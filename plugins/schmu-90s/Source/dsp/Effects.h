// SCHMU 90s · efectos: saturación de cinta, chorus estéreo, delay sincronizado (ping-pong), reverb y swing global.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "Common.h"

namespace schmu {

// ---------- Saturación de cinta ----------
struct Tape {
  OnePole lpL, lpR, dcL, dcR;
  double sr = 44100;
  void prepare(double s) { sr = s; lpL.reset(); lpR.reset(); dcL.reset(); dcR.reset(); dcL.setCutoff(20.0f, s); dcR.setCutoff(20.0f, s); }
  static float shape(float x, float d, float bias) {
    return std::tanh(d * (x + bias)) - std::tanh(d * bias);  // con sesgo: armónicos pares, "calor"
  }
  void process(float& l, float& r, float amount) {
    if (amount <= 0.0001f) return;
    float d = 1.0f + amount * 7.0f;
    float bias = 0.08f * amount;
    float norm = 1.0f / (1.0f + (d - 1.0f) * 0.3f);
    float cut = lerpf(16000.0f, 5500.0f, amount);
    lpL.setCutoff(cut, sr); lpR.setCutoff(cut, sr);
    float yl = shape(l, d, bias) * norm, yr = shape(r, d, bias) * norm;
    yl = lpL.lowpass(yl); yr = lpR.lowpass(yr);
    yl -= dcL.lowpass(yl); yr -= dcR.lowpass(yr);  // quita el DC que mete el sesgo
    l = yl; r = yr;
  }
};

// ---------- Chorus estéreo (dos líneas con LFO en cuadratura, estilo ensemble de los 80/90) ----------
struct Chorus {
  std::vector<float> bufL, bufR;
  int pos = 0, size = 0;
  double sr = 44100, phase = 0;
  void prepare(double s) {
    sr = s;
    size = static_cast<int>(s * 0.05) + 8;
    bufL.assign(size, 0.0f); bufR.assign(size, 0.0f);
    pos = 0; phase = 0;
  }
  static float readFrac(const std::vector<float>& b, int size, int pos, float delay) {
    float rp = static_cast<float>(pos) - delay;
    while (rp < 0) rp += static_cast<float>(size);
    int i0 = static_cast<int>(rp); int i1 = (i0 + 1) % size;
    float f = rp - static_cast<float>(i0);
    return b[i0 % size] + (b[i1] - b[i0 % size]) * f;
  }
  void process(float& l, float& r, float mix, float depth) {
    if (mix <= 0.0001f) return;
    bufL[pos] = l; bufR[pos] = r;
    phase += 0.7 / sr; if (phase >= 1.0) phase -= 1.0;
    float base = 0.0075f * static_cast<float>(sr);
    float dev = (0.0008f + 0.0032f * depth) * static_cast<float>(sr);
    float dl = base + dev * static_cast<float>(sinCycle(phase));
    float dr = base + dev * static_cast<float>(sinCycle(phase + 0.5));
    float wl = readFrac(bufL, size, pos, dl);
    float wr = readFrac(bufR, size, pos, dr);
    // cruzado: cada lado también mete un poco del otro para abrir el estéreo
    float cl = 0.8f * wl + 0.2f * wr, cr = 0.8f * wr + 0.2f * wl;
    l = lerpf(l, cl, 0.5f * mix); r = lerpf(r, cr, 0.5f * mix);
    if (++pos >= size) pos = 0;
  }
};

// ---------- Delay ping-pong sincronizado al tempo ----------
struct Delay {
  std::vector<float> bufL, bufR;
  int pos = 0, size = 0;
  double sr = 44100;
  OnePole timeSmooth, fbLpL, fbLpR;
  float curDelay = 0;
  void prepare(double s) {
    sr = s;
    size = static_cast<int>(s * 4.0) + 8;
    bufL.assign(size, 0.0f); bufR.assign(size, 0.0f);
    pos = 0;
    timeSmooth.setTime(0.08f, s);
    fbLpL.setCutoff(4500.0f, s); fbLpR.setCutoff(4500.0f, s);
    fbLpL.reset(); fbLpR.reset();
    curDelay = 0.25f * static_cast<float>(s);
    timeSmooth.reset(curDelay);
  }
  static float beats(int div) {
    static const float b[6] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
    return b[std::min(5, std::max(0, div))];
  }
  static float delaySeconds(int div, double bpm) { return beats(div) * 60.0f / static_cast<float>(std::max(20.0, bpm)); }
  void process(float& l, float& r, float mix, int div, float fb, double bpm) {
    if (mix <= 0.0001f && fb <= 0.0001f) { bufL[pos] = bufR[pos] = 0; if (++pos >= size) pos = 0; return; }
    float target = std::min(delaySeconds(div, bpm), 3.9f) * static_cast<float>(sr);
    curDelay = timeSmooth.lowpass(target);
    float dl = Chorus::readFrac(bufL, size, pos, curDelay);
    float dr = Chorus::readFrac(bufR, size, pos, curDelay);
    // ping-pong: la entrada va al L, el feedback cruza
    float fl = fbLpL.lowpass(dr) * fb, fr = fbLpR.lowpass(dl) * fb;
    bufL[pos] = flushDenormal(std::tanh((l + r) * 0.5f + fl));
    bufR[pos] = flushDenormal(std::tanh(fr));
    l += dl * mix; r += dr * mix;
    if (++pos >= size) pos = 0;
  }
};

// ---------- Reverb (esquema de combs + allpass tipo Schroeder/Freeverb; implementación propia) ----------
struct Reverb {
  struct Comb {
    std::vector<float> b; int i = 0; float store = 0;
    void init(int n) { b.assign(std::max(1, n), 0.0f); i = 0; store = 0; }
    float tick(float x, float fb, float damp) {
      float y = b[i];
      store = flushDenormal(y * (1.0f - damp) + store * damp);
      b[i] = flushDenormal(x + store * fb);
      if (++i >= static_cast<int>(b.size())) i = 0;
      return y;
    }
  };
  struct Allpass {
    std::vector<float> b; int i = 0;
    void init(int n) { b.assign(std::max(1, n), 0.0f); i = 0; }
    float tick(float x) {
      float y = b[i];
      float out = -x + y;
      b[i] = flushDenormal(x + y * 0.5f);
      if (++i >= static_cast<int>(b.size())) i = 0;
      return out;
    }
  };
  static constexpr int NC = 8, NA = 4;
  Comb cl[NC], cr[NC];
  Allpass al[NA], ar[NA];
  void prepare(double sr) {
    static const int ct[NC] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static const int at[NA] = {556, 441, 341, 225};
    double k = sr / 44100.0;
    for (int i = 0; i < NC; ++i) { cl[i].init(static_cast<int>(ct[i] * k)); cr[i].init(static_cast<int>((ct[i] + 23) * k)); }
    for (int i = 0; i < NA; ++i) { al[i].init(static_cast<int>(at[i] * k)); ar[i].init(static_cast<int>((at[i] + 23) * k)); }
  }
  void process(float& l, float& r, float mix, float size) {
    if (mix <= 0.0001f) return;
    float in = (l + r) * 0.015f;
    float fb = 0.70f + 0.27f * clampf(size, 0.0f, 1.0f);
    float damp = 0.35f;
    float wl = 0, wr = 0;
    for (int i = 0; i < NC; ++i) { wl += cl[i].tick(in, fb, damp); wr += cr[i].tick(in, fb, damp); }
    for (int i = 0; i < NA; ++i) { wl = al[i].tick(wl); wr = ar[i].tick(wr); }
    l += wl * mix * 3.0f; r += wr * mix * 3.0f;
  }
};

// ---------- Limitador suave de salida: nunca pasa de ±1 ----------
inline float softLimit(float x) {
  constexpr float knee = 0.85f;
  float a = std::fabs(x);
  if (a <= knee) return x;
  float o = knee + (1.0f - knee) * std::tanh((a - knee) / (1.0f - knee));
  return x < 0 ? -o : o;
}

// ---------- Swing global ----------
// Retraso (en semicorcheas) de una nota que cae en una semicorchea impar (contratiempo) de la grilla.
// swing 0.50 = recto; 0.667 = tresillo; 0.75 = muy arrastrado. Solo mueve notas cercanas a la grilla.
inline double swingDelay16(double ppq, double swing) {
  double pos16 = ppq * 4.0;
  double nearest = std::floor(pos16 + 0.5);
  if (std::fabs(pos16 - nearest) > 0.25) return 0.0;
  long long idx = static_cast<long long>(nearest);
  if ((idx & 1LL) == 0) return 0.0;
  return (std::min(0.75, std::max(0.5, swing)) - 0.5) * 2.0;
}
inline double swingDelaySamples(double ppq, double bpm, double sr, double swing) {
  return swingDelay16(ppq, swing) * (60.0 / std::max(20.0, bpm) / 4.0) * sr;
}

// Cola de eventos MIDI con retraso. Los note-off de una nota retrasada se retrasan igual, para no acortarla.
class SwingScheduler {
 public:
  struct Event { bool on; int note; float vel; long long due; };
  void reset() { pending_.clear(); for (auto& d : noteDelay_) d = 0; }
  // Registra un evento entrante; ppq < 0 significa "transporte parado": sin swing.
  void add(bool on, int note, float vel, long long nowSample, double ppq, double bpm, double sr, double swing) {
    if (note < 0 || note > 127) return;
    long long delay = 0;
    if (on) {
      if (ppq >= 0.0 && swing > 0.5001) delay = static_cast<long long>(std::llround(swingDelaySamples(ppq, bpm, sr, swing)));
      noteDelay_[note] = delay;
    } else {
      delay = noteDelay_[note];
      noteDelay_[note] = 0;
    }
    Event e{on, note, vel, nowSample + delay};
    auto it = pending_.begin();
    while (it != pending_.end() && it->due <= e.due) ++it;  // estable: respeta el orden de llegada con igual tiempo
    pending_.insert(it, e);
  }
  // Saca el próximo evento vencido antes de `until` (exclusivo); false si no hay.
  bool pop(long long until, Event& out) {
    if (pending_.empty() || pending_.front().due >= until) return false;
    out = pending_.front(); pending_.erase(pending_.begin());
    return true;
  }
  bool peek(long long& due) const { if (pending_.empty()) return false; due = pending_.front().due; return true; }
  size_t size() const { return pending_.size(); }
 private:
  std::vector<Event> pending_;
  long long noteDelay_[128] = {};
};

}  // namespace schmu
