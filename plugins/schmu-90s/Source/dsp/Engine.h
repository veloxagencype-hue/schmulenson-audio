// SCHMU 90s · motor del instrumento: voces (organ, piano, stab, bajo, chops), batería, chord memory y FX.
// No depende de JUCE: el plugin solo le pasa notas, parámetros y tempo.
#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <vector>

#include "Common.h"
#include "Drums.h"
#include "Effects.h"
#include "Params.h"
#include "Sampler.h"

namespace schmu {

constexpr int kMaxVoices = 16;

class SchmuEngine {
 public:
  SchmuEngine() { setDefaults(); sample_ = makeDefaultChop(); }

  void setDefaults() { for (int i = 0; i < P_COUNT; ++i) p_[i] = paramTable()[i].def; }
  void prepare(double sr) {
    sr_ = sr;
    for (auto& v : voices_) v = Voice{};
    drums_.prepare(sr);
    tape_.prepare(sr); chorus_.prepare(sr); delay_.prepare(sr); reverb_.prepare(sr);
    rng_.seed(12345);
    heldBass_.clear();
    vol_ = p_[P_VOLUME]; cutoff_ = p_[P_CUTOFF]; reso_ = p_[P_RESO];
    tick_ = 0;
  }
  void setParam(int id, float v) {
    const auto& d = paramTable()[id];
    p_[id] = clampf(v, d.min, d.max);
  }
  float param(int id) const { return p_[id]; }
  void setTempo(double bpm) { bpm_ = clampf(static_cast<float>(bpm), 20.0f, 400.0f); }
  void setSample(std::shared_ptr<const SampleData> s) { std::lock_guard<std::mutex> g(smpMutex_); pendingSample_ = std::move(s); hasPending_ = true; }
  std::shared_ptr<const SampleData> sample() const { return sample_; }

  int engine() const { return static_cast<int>(p_[P_ENGINE] + 0.5f); }
  int activeVoices() const { int n = 0; for (auto& v : voices_) if (v.env.active()) ++n; return n + (drums_.anyActive() ? 1 : 0); }

  // ---- MIDI ----
  void noteOn(int note, float vel01) {
    vel01 = clampf(vel01, 0.0f, 1.0f);
    if (vel01 <= 0.0f) { noteOff(note); return; }
    switch (engine()) {
      case E_DRUMS: { drums_.noteOn(note, vel01, drumParams()); break; }
      case E_BASS: bassOn(note, vel01); break;
      case E_CHOPS: updateSampleSwap(); chopOn(note, vel01); break;
      default: chordOn(note, vel01); break;
    }
  }
  void noteOff(int note) {
    if (engine() == E_BASS) { bassOff(note); return; }
    for (auto& v : voices_) if (v.env.active() && v.root == note && v.gate) { v.gate = false; v.env.gateOff(); }
  }
  void allNotesOff() {
    heldBass_.clear();
    for (auto& v : voices_) if (v.env.active()) { v.gate = false; v.env.gateOff(); }
  }

  // ---- Audio ----
  void process(float* outL, float* outR, int n) {
    updateSampleSwap();
    int done = 0;
    while (done < n) {
      int m = std::min(kBlock, n - done);
      renderChunk(outL + done, outR + done, m);
      done += m;
    }
  }

  // Convierte una nota en las notas que realmente suenan (chord memory). Pública para poder testearla.
  static int chordNotes(int chordIdx, int rootNote, std::array<int, 6>& out) {
    chordIdx = std::min<int>(std::max(chordIdx, 0), static_cast<int>(chordTable().size()) - 1);
    const ChordDef& c = chordTable()[chordIdx];
    for (int i = 0; i < c.n; ++i) out[i] = std::min(127, rootNote + c.semis[i]);
    return c.n;
  }

 private:
  struct Voice {
    Adsr env;
    ExpDecay fenv, aux, aux2;
    Svf f1, f2, f3, f4;
    int root = -1, note = 60, engine = 0;
    bool gate = false;
    float vel = 1, gain = 1, curNote = 60, targetNote = 60, glideInc = 0;
    long long age = 0, t = 0;
    double ph[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double sub = 0, vib = 0;
    // sampler
    double pos = 0, endPos = 0, startPos = 0, rate = 1;
    bool slice = false;
    float pan = 0;
    int stage = 0;
    OnePole click;
    uint32_t seed = 1;
  };

  double sr_ = 44100;
  float bpm_ = 124.0f;
  std::array<float, P_COUNT> p_{};
  Voice voices_[kMaxVoices];
  std::vector<int> heldBass_;
  DrumKit drums_;
  Tape tape_; Chorus chorus_; Delay delay_; Reverb reverb_;
  Rng rng_;
  float vol_ = 0.8f, cutoff_ = 18000.0f, reso_ = 0.1f;
  long long tick_ = 0;
  std::shared_ptr<const SampleData> sample_;
  std::shared_ptr<const SampleData> pendingSample_;
  bool hasPending_ = false;
  std::mutex smpMutex_;

  void updateSampleSwap() {
    if (!smpMutex_.try_lock()) return;
    if (hasPending_) { sample_ = pendingSample_; hasPending_ = false; for (auto& v : voices_) if (v.engine == E_CHOPS) v.env.kill(sr_); }
    smpMutex_.unlock();
  }

  DrumParams drumParams() const {
    DrumParams d;
    d.is808 = p_[P_KIT] > 0.5f;
    d.kickTune = p_[P_KICKTUNE]; d.kickDecay = p_[P_KICKDECAY]; d.kickPunch = p_[P_KICKPUNCH];
    d.tone = p_[P_SNARETONE]; d.hatDecay = p_[P_HATDECAY];
    return d;
  }

  Voice* allocVoice() {
    Voice* free = nullptr; Voice* oldest = nullptr;
    for (auto& v : voices_) {
      if (!v.env.active()) { if (!free) free = &v; }
      if (!oldest || v.age < oldest->age) oldest = &v;  // 'age' crece por tick; la más vieja = menor sello
    }
    if (free) return free;
    return oldest;
  }

  void startEnvelopes(Voice& v, float noteHz) {
    (void)noteHz;
    int eng = v.engine;
    float a = p_[P_ATTACK], d = p_[P_DECAY], s = p_[P_SUSTAIN], r = p_[P_RELEASE];
    if (eng == E_PIANO) d *= std::pow(2.0f, -(v.note - 60) / 30.0f);   // las notas agudas se apagan antes
    v.env.set(a, d, s, r, sr_);
    v.env.level = 0; v.env.gateOn();
    v.fenv.trigger(1.0f, p_[P_FDECAY], sr_);
    v.gate = true; v.t = 0; v.age = ++tick_;
  }

  void initVoice(Voice& v, int root, int note, float vel, int engine, float chordGain) {
    v = Voice{};
    v.root = root; v.note = note; v.engine = engine; v.vel = vel; v.gain = chordGain;
    v.curNote = v.targetNote = static_cast<float>(note);
    v.seed = 0x1234567u + static_cast<uint32_t>(note * 7919 + tick_ * 104729);
    v.pan = ((note * 37) % 100) / 100.0f - 0.5f;
    v.f1.reset(); v.f2.reset(); v.f3.reset(); v.f4.reset();
    startEnvelopes(v, midiToHz(static_cast<float>(note)));
  }

  void chordOn(int note, float vel) {
    std::array<int, 6> ns{};
    int idx = static_cast<int>(p_[P_CHORD] + 0.5f);
    int n = chordNotes(idx, note, ns);
    // retrigger: si esa tecla ya suena, se corta primero
    for (auto& v : voices_) if (v.env.active() && v.root == note && v.gate) { v.gate = false; v.env.kill(sr_); }
    float g = 1.0f / std::sqrt(static_cast<float>(n));
    for (int i = 0; i < n; ++i) {
      Voice* v = allocVoice();
      bool stolen = v->env.active();
      Voice nv; initVoice(nv, note, ns[i], vel, engine(), g);
      nv.pan = (n == 1 ? nv.pan * 0.5f : ((i % 2 ? 1.0f : -1.0f) * (0.1f + 0.1f * (i / 2))));
      if (stolen) { v->env.kill(sr_); }
      *v = nv;
    }
  }

  void chopOn(int note, float vel) {
    if (!sample_ || sample_->empty()) return;
    int mode = static_cast<int>(p_[P_CHOPMODE] + 0.5f);
    int slicesIdx = static_cast<int>(p_[P_CHOPSLICES] + 0.5f);
    int nSlices = slicesIdx == 0 ? 4 : (slicesIdx == 1 ? 8 : 16);
    bool rev = p_[P_CHOPREV] > 0.5f;
    int len = sample_->length();
    double s0 = static_cast<double>(p_[P_CHOPSTART]) * (len - 1);
    double s1 = static_cast<double>(len - 1);
    double rate = sample_->sampleRate / sr_;
    bool slice = (mode == 1);
    if (slice) {
      int idx = note - 36;
      if (idx < 0 || idx >= nSlices) return;
      double span = (s1 - s0) / nSlices;
      s0 = s0 + span * idx; s1 = s0 + span;
      rate *= std::pow(2.0, p_[P_TUNE] / 12.0);
    } else {
      rate *= std::pow(2.0, (note - 60 + p_[P_TUNE]) / 12.0);
    }
    Voice* v = allocVoice();
    Voice nv; initVoice(nv, note, note, vel, E_CHOPS, 1.0f);
    nv.slice = slice; nv.rate = rev ? -rate : rate;
    nv.startPos = s0; nv.endPos = s1;
    nv.pos = rev ? s1 : s0;
    nv.pan = 0;
    if (v->env.active()) v->env.kill(sr_);
    *v = nv;
  }

  void bassOn(int note, float vel) {
    heldBass_.erase(std::remove(heldBass_.begin(), heldBass_.end(), note), heldBass_.end());
    bool legato = !heldBass_.empty() && p_[P_GLIDE] > 0.0005f;
    heldBass_.push_back(note);
    Voice* mono = nullptr;
    for (auto& v : voices_) if (v.engine == E_BASS && v.env.active()) { mono = &v; break; }
    if (mono) {
      // misma voz: se desliza (glide) y, si no hay glide, se retrigueriza sin cortar la fase
      mono->root = note;
      mono->targetNote = static_cast<float>(note);
      float g = std::max(0.0005f, p_[P_GLIDE]);
      mono->glideInc = (mono->targetNote - mono->curNote) / (g * static_cast<float>(sr_));
      if (!legato) {
        mono->curNote = mono->targetNote; mono->glideInc = 0;
        mono->note = note; mono->vel = vel;
        mono->env.set(p_[P_ATTACK], p_[P_DECAY], p_[P_SUSTAIN], p_[P_RELEASE], sr_);
        mono->env.gateOn(); mono->fenv.trigger(1.0f, p_[P_FDECAY], sr_); mono->aux.trigger(1.0f, 0.1f, sr_);
        mono->t = 0; mono->gate = true;
      }
      mono->note = note;
      return;
    }
    Voice* v = allocVoice();
    if (v->env.active()) v->env.kill(sr_);
    Voice nv; initVoice(nv, note, note, vel, E_BASS, 1.0f);
    nv.pan = 0;
    *v = nv;
  }
  void bassOff(int note) {
    bool wasLast = !heldBass_.empty() && heldBass_.back() == note;
    heldBass_.erase(std::remove(heldBass_.begin(), heldBass_.end(), note), heldBass_.end());
    for (auto& v : voices_) {
      if (v.engine != E_BASS || !v.env.active() || !v.gate) continue;
      if (heldBass_.empty()) { v.gate = false; v.env.gateOff(); }
      else if (wasLast) {
        v.targetNote = static_cast<float>(heldBass_.back()); v.root = heldBass_.back(); v.note = heldBass_.back();
        float g = std::max(0.0005f, p_[P_GLIDE]);
        v.glideInc = (v.targetNote - v.curNote) / (g * static_cast<float>(sr_));
      }
    }
  }

  // ---------- render ----------
  void renderChunk(float* L, float* R, int n) {
    // suavizado de controles (un paso por sub-bloque)
    float a = 1.0f - std::exp(-static_cast<float>(n) / (0.01f * static_cast<float>(sr_)));
    vol_ += a * (p_[P_VOLUME] - vol_);
    cutoff_ += a * (p_[P_CUTOFF] - cutoff_);
    reso_ += a * (p_[P_RESO] - reso_);
    for (int i = 0; i < n; ++i) { L[i] = 0; R[i] = 0; }

    int eng = engine();
    for (auto& v : voices_) {
      if (!v.env.active()) continue;
      renderVoice(v, L, R, n);
    }
    if (eng == E_DRUMS || drums_.anyActive()) drums_.process(L, R, n);

    float tape = p_[P_TAPE], chor = p_[P_CHORUS], chd = p_[P_CHORUSDEPTH];
    float dmix = p_[P_DELAYMIX], dfb = p_[P_DELAYFB], rmix = p_[P_REVERBMIX], rsz = p_[P_REVERBSIZE];
    int ddiv = static_cast<int>(p_[P_DELAYDIV] + 0.5f);
    for (int i = 0; i < n; ++i) {
      float l = L[i], r = R[i];
      tape_.process(l, r, tape);
      chorus_.process(l, r, chor, chd);
      delay_.process(l, r, dmix, ddiv, dfb, bpm_);
      reverb_.process(l, r, rmix, rsz);
      L[i] = softLimit(l * vol_);
      R[i] = softLimit(r * vol_);
    }
  }

  void renderVoice(Voice& v, float* L, float* R, int n) {
    const int eng = v.engine;
    // filtro: corte + envolvente (envamt negativo cierra el filtro en el ataque)
    float envAmt = p_[P_ENVAMT];
    float fcBase = cutoff_;
    float tune = (eng == E_CHOPS) ? 0.0f : p_[P_TUNE];
    float panL = 1.0f - 0.5f * std::max(0.0f, v.pan), panR = 1.0f + 0.5f * std::min(0.0f, v.pan);
    // coeficientes al inicio del sub-bloque
    float fe = v.fenv.v;
    float kt = std::pow(2.0f, (v.note - 60) / 24.0f) ;  // seguimiento de teclado suave
    float oct = envAmt * fe * 6.0f;
    float fc = fcBase * std::pow(2.0f, oct) * (eng == E_STAB || eng == E_BASS ? kt : 1.0f);
    v.f1.set(fc, reso_, sr_); v.f2.set(fc, reso_, sr_);
    v.f3.set(fc, reso_ * 0.5f, sr_); v.f4.set(fc, reso_ * 0.5f, sr_);
    bool twoPole = (eng == E_STAB || eng == E_BASS);
    bool stereoEng = (eng == E_STAB || eng == E_PIANO || eng == E_CHOPS);
    bool filterOpen = (fcBase >= 17000.0f && std::fabs(envAmt) < 0.001f);
    float amp = v.vel * v.gain * v.vel;  // la velocity pesa al cuadrado (más dinámica)

    for (int i = 0; i < n; ++i) {
      if (v.glideInc != 0.0f) {
        v.curNote += v.glideInc;
        if ((v.glideInc > 0 && v.curNote >= v.targetNote) || (v.glideInc < 0 && v.curNote <= v.targetNote)) { v.curNote = v.targetNote; v.glideInc = 0; }
      }
      float noteF = v.curNote + tune;
      double f = midiToHz(noteF);
      double dt = f / sr_;
      float s = 0.0f, s2 = 0.0f;
      switch (eng) {
        case E_ORGAN: s = organ(v, dt, f); s2 = s; break;
        case E_PIANO: s = piano(v, dt, f, s2); break;
        case E_STAB: s = stab(v, dt, s2); break;
        case E_BASS: s = bass(v, dt); s2 = s; break;
        case E_CHOPS: s = chop(v, s2); break;
      }
      // sub opcional: seno una octava abajo
      float sub = p_[P_SUB];
      if (sub > 0.001f && eng != E_CHOPS && eng != E_BASS) {
        v.sub += dt * 0.5; if (v.sub >= 1.0) v.sub -= 1.0;
        float sb = static_cast<float>(sinCycle(v.sub)) * sub * 0.7f;
        s += sb; s2 += sb;
      }
      if (!filterOpen) {
        s = v.f1.lowpass(s);
        if (stereoEng) s2 = v.f2.lowpass(s2);
        if (twoPole) { s = v.f3.lowpass(s); if (stereoEng) s2 = v.f4.lowpass(s2); }  // 24 dB/oct
        if (!stereoEng) s2 = s;
      }
      float e = v.env.tick();
      float g = e * amp;
      L[i] += s * g * panL;
      R[i] += s2 * g * panR;
      v.fenv.tick();
      ++v.t;
      if (!v.env.active()) break;
    }
  }

  // ---- Organ: aditivo de 8 parciales, click de tecla, percusión y vibrato ----
  float organ(Voice& v, double dt, double f) {
    static const float ratio[8] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
    static const float soft[8] = {0.5f, 1.0f, 0.25f, 0.08f, 0.0f, 0.0f, 0.0f, 0.0f};
    static const float house[8] = {0.55f, 1.0f, 0.85f, 0.5f, 0.45f, 0.3f, 0.2f, 0.15f};
    static const float bright[8] = {0.2f, 1.0f, 0.9f, 0.85f, 0.75f, 0.65f, 0.55f, 0.45f};
    float draw = p_[P_M1], click = p_[P_M2], perc = p_[P_M3], vibAmt = p_[P_M4];
    // vibrato: LFO de 6 Hz que entra suave
    v.vib += 6.0 / sr_; if (v.vib >= 1.0) v.vib -= 1.0;
    float fade = std::min(1.0f, static_cast<float>(v.t) / (0.25f * static_cast<float>(sr_)));
    double vibMul = 1.0 + 0.006 * vibAmt * fade * sinCycle(v.vib);
    float tsec = static_cast<float>(v.t) / static_cast<float>(sr_);
    float percEnv = std::exp(-tsec / 0.16f) * perc;
    float acc = 0, norm = 0, gsum = 0;
    for (int k = 0; k < 8; ++k) {
      float g = draw < 0.5f ? lerpf(soft[k], house[k], draw * 2.0f) : lerpf(house[k], bright[k], (draw - 0.5f) * 2.0f);
      if (k == 3) g += percEnv * 0.9f;       // 3ª armónica percusiva (el "pluc" del órgano)
      if (k == 2) g += percEnv * 0.5f;
      double pf = f * ratio[k] * vibMul;
      if (pf > 0.45 * sr_) continue;
      v.ph[k] += pf / sr_; if (v.ph[k] >= 1.0) v.ph[k] -= 1.0;
      acc += static_cast<float>(sinCycle(v.ph[k])) * g;
      norm += g * g; gsum += g;
    }
    (void)dt;
    float out = acc / std::max(0.5f, 0.6f * gsum + 0.4f * std::sqrt(norm)) * 0.6f;
    // click de tecla: ruido filtrado muy corto
    if (click > 0.001f) {
      v.seed = v.seed * 1664525u + 1013904223u;
      float nz = (static_cast<float>(v.seed >> 8) / 8388608.0f - 1.0f);
      v.click.setCutoff(2500.0f, sr_);
      float e = std::exp(-tsec / 0.007f);
      out += (nz - v.click.lowpass(nz)) * e * click * 1.2f;
    }
    return out;
  }

  // ---- Piano: FM de 2 operadores (cuerpo) + 2º par sobre-agudo (martillo/tine) + capa desafinada ----
  float piano(Voice& v, double dt, double f, float& outR) {
    float bright = p_[P_M1], tine = p_[P_M2], body = p_[P_M3], shimmer = p_[P_M4];
    float tsec = static_cast<float>(v.t) / static_cast<float>(sr_);
    float keyScale = clampf(1.6f - (v.note - 24) / 72.0f, 0.35f, 1.5f);  // menos brillo arriba
    float idxTau = 0.05f + 1.4f * body;
    float idx = (0.4f + 3.4f * bright) * v.vel * keyScale * std::exp(-tsec / idxTau);
    // capa A: FM 1:1 (la modulación de fase se mide en ciclos)
    v.ph[0] += dt; if (v.ph[0] >= 1.0) v.ph[0] -= 1.0;
    v.ph[1] += dt; if (v.ph[1] >= 1.0) v.ph[1] -= 1.0;
    float a = static_cast<float>(sinCycle(v.ph[0] + 0.28 * idx * sinCycle(v.ph[1])));
    // capa B: ligeramente desafinada (shimmer)
    double dt2 = dt * (1.0 + 0.0035 * shimmer);
    v.ph[2] += dt2; if (v.ph[2] >= 1.0) v.ph[2] -= 1.0;
    v.ph[3] += dt2 * 2.0; if (v.ph[3] >= 1.0) v.ph[3] -= 1.0;
    float b = static_cast<float>(sinCycle(v.ph[2] + 0.2 * idx * sinCycle(v.ph[3])));
    // tine: par 1:14 que muere en ~28 ms
    float tn = 0;
    if (14.0 * f < 0.45 * sr_) {
      v.ph[4] += dt * 14.0; if (v.ph[4] >= 1.0) v.ph[4] -= 1.0;
      tn = static_cast<float>(sinCycle(v.ph[4])) * std::exp(-tsec / 0.028f) * tine * v.vel * 0.55f;
    }
    // L lleva más capa A, R más capa B: abre el estéreo
    outR = (0.5f * a + 0.5f * b * (0.5f + 0.5f * shimmer)) * 0.9f + tn;
    return (0.7f * a + 0.3f * b * (0.5f + 0.5f * shimmer)) * 0.9f + tn;
  }

  // ---- Stab: 3 sierras/pulsos PolyBLEP desafinadas, estéreo ----
  float stab(Voice& v, double dt, float& outR) {
    float wave = p_[P_M1], pw = 0.1f + 0.4f * (1.0f - p_[P_M2]), uni = p_[P_M3], air = p_[P_M4];
    float detCents = 3.0f + 22.0f * uni;
    double dtc[3] = {dt * std::pow(2.0, -detCents / 1200.0), dt, dt * std::pow(2.0, detCents / 1200.0)};
    float o[3];
    for (int k = 0; k < 3; ++k) {
      v.ph[k] += dtc[k]; if (v.ph[k] >= 1.0) v.ph[k] -= 1.0;
      float sw = sawBlep(v.ph[k], dtc[k]);
      float pu = pulseBlep(v.ph[k], dtc[k], pw);
      o[k] = lerpf(sw, pu, wave);
    }
    float m = (o[0] + o[1] + o[2]) * 0.33f;
    float l = (o[0] * 0.6f + o[1] * 0.5f + o[2] * 0.2f) * 0.62f;
    float r = (o[2] * 0.6f + o[1] * 0.5f + o[0] * 0.2f) * 0.62f;
    (void)m;
    if (air > 0.001f) {
      v.seed = v.seed * 1664525u + 1013904223u;
      float nz = (static_cast<float>(v.seed >> 8) / 8388608.0f - 1.0f) * air * 0.35f;
      l += nz; r += nz;
    }
    outR = r;
    return l;
  }

  // ---- Bajo 2-step/garage: seno con caída de pitch + capa reese (dos sierras desafinadas) ----
  float bass(Voice& v, double dt) {
    float wave = p_[P_M1], drop = p_[P_M2] * 24.0f, dropT = 0.01f + 0.29f * p_[P_M3], reese = p_[P_M4];
    float tsec = static_cast<float>(v.t) / static_cast<float>(sr_);
    double pm = std::pow(2.0, drop * std::exp(-tsec / dropT) / 12.0);
    double d = dt * pm;
    v.ph[0] += d; if (v.ph[0] >= 1.0) v.ph[0] -= 1.0;
    float sn = static_cast<float>(sinCycle(v.ph[0]));
    float det = 4.0f + 26.0f * reese;
    double d1 = d * std::pow(2.0, -det / 1200.0), d2 = d * std::pow(2.0, det / 1200.0);
    v.ph[1] += d1; if (v.ph[1] >= 1.0) v.ph[1] -= 1.0;
    v.ph[2] += d2; if (v.ph[2] >= 1.0) v.ph[2] -= 1.0;
    float rs = (sawBlep(v.ph[1], d1) + sawBlep(v.ph[2], d2)) * 0.5f;
    float saturated = std::tanh(sn * 1.8f) * 0.8f;
    float mid = lerpf(sn, saturated, clampf(wave * 2.0f, 0.0f, 1.0f));
    float out = lerpf(mid, rs * 0.7f + sn * 0.35f, clampf(wave * 2.0f - 1.0f, 0.0f, 1.0f));
    return out * 0.95f;
  }

  // ---- Chops: reproduce el sample cargado (teclas = pitch, o 4/8/16 slices) ----
  float chop(Voice& v, float& outR) {
    const SampleData* s = sample_.get();
    if (!s || s->empty()) { v.env.kill(sr_); outR = 0; return 0; }
    int len = s->length();
    double pos = v.pos;
    bool loop = (p_[P_CHOPLOOP] > 0.5f) && !v.slice;
    if (v.rate >= 0) {
      if (pos >= v.endPos) {
        if (loop) pos = v.startPos + (pos - v.endPos); else { v.env.kill(sr_); outR = 0; return 0; }
      }
    } else {
      if (pos <= v.startPos) {
        if (loop) pos = v.endPos - (v.startPos - pos); else { v.env.kill(sr_); outR = 0; return 0; }
      }
    }
    int i1 = static_cast<int>(pos); double fr = pos - i1;
    auto at = [&](const std::vector<float>& b, int i) { return b[static_cast<size_t>(std::min(std::max(i, 0), len - 1))]; };
    auto herm = [&](const std::vector<float>& b) {
      float y0 = at(b, i1 - 1), y1 = at(b, i1), y2 = at(b, i1 + 1), y3 = at(b, i1 + 2);
      float x = static_cast<float>(fr);
      float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
      return ((c3 * x + c2) * x + c1) * x + c0;
    };
    float l = herm(s->l);
    float r = (s->r.size() == s->l.size()) ? herm(s->r) : l;
    v.pos = pos + v.rate;
    // fundido corto al final del recorte para evitar clics (en slices sobre todo)
    double remaining = v.rate >= 0 ? (v.endPos - v.pos) : (v.pos - v.startPos);
    double fadeLen = 0.004 * sr_ * std::fabs(v.rate);
    float fg = (!loop && remaining < fadeLen) ? static_cast<float>(std::max(0.0, remaining / fadeLen)) : 1.0f;
    outR = r * fg;
    return l * fg;
  }
};

}  // namespace schmu
