// SCHMU 90s · batería sintetizada (modelos propios, sin samples) en la onda de 909 y 808.
// Mapa de notas (GM): 36 bombo · 37 rim · 38 caja · 39 clap · 42/44 hat cerrado · 46 hat abierto · 41/43/45 toms.
#pragma once
#include "Common.h"
#include "Params.h"

namespace schmu {

struct DrumParams {
  bool is808 = false;
  float kickTune = 52, kickDecay = 0.45f, kickPunch = 0.5f, tone = 0.5f, hatDecay = 0.07f;
};

class DrumKit {
 public:
  void prepare(double sr) {
    sr_ = sr;
    rng_.seed(0xC0FFEE);
    reset();
  }
  void reset() {
    for (auto& v : voices_) v = Voice{};
  }
  bool anyActive() const { for (auto& v : voices_) if (v.on) return true; return false; }

  void noteOn(int note, float vel, const DrumParams& p) {
    Kind k;
    switch (note) {
      case 36: case 35: k = KICK; break;
      case 37: k = RIM; break;
      case 38: case 40: k = SNARE; break;
      case 39: k = CLAP; break;
      case 42: case 44: k = HAT_C; break;
      case 46: k = HAT_O; break;
      case 41: case 43: case 45: case 47: k = TOM; break;
      default: return;
    }
    if (k == HAT_C) { voices_[HAT_O].on = false; }          // el hat cerrado corta al abierto
    Voice& v = voices_[k];
    v = Voice{};
    v.on = true; v.vel = vel; v.note = note;
    v.is808 = p.is808; v.tune = p.kickTune; v.decay = p.kickDecay; v.punch = p.kickPunch;
    v.tone = p.tone; v.hatDecay = p.hatDecay;
    v.bp.reset(); v.hp.reset(); v.hp2.reset();
    if (k == KICK) {
      float f0 = p.kickTune;
      v.f0 = f0;
      v.pitchAmt = p.is808 ? 2.2f + 1.0f * p.kickPunch : 3.0f + 5.0f * p.kickPunch;
      v.pitchTau = p.is808 ? 0.05f : 0.014f + 0.01f * (1.0f - p.kickPunch);
      v.ampTau = p.kickDecay * (p.is808 ? 1.5f : 1.0f) * 0.4f;  // tau ≈ decay/ ~2.5 → llega a -60 dB cerca de 'decay'
    } else if (k == TOM) {
      static const float f[4] = {82.0f, 110.0f, 146.0f, 196.0f};
      v.f0 = f[std::min(3, std::max(0, (note - 41) / 2))] * (p.is808 ? 1.0f : 1.15f);
      v.pitchAmt = 1.2f; v.pitchTau = 0.04f; v.ampTau = 0.12f + 0.2f * p.kickDecay;
    } else if (k == SNARE) {
      v.f0 = p.is808 ? 238.0f : 185.0f;
    } else if (k == HAT_C) {
      v.ampTau = std::max(0.01f, p.hatDecay) * 0.45f;
    } else if (k == HAT_O) {
      v.ampTau = std::max(0.12f, p.hatDecay * 5.0f) * 0.45f;
    }
    v.bp.set(k == CLAP ? 1150.0f + 900.0f * p.tone : 3000.0f, 0.55f, sr_);
    v.hp.set(k == HAT_C || k == HAT_O ? 7500.0f : (k == SNARE ? 1200.0f + 3000.0f * p.tone : 1500.0f), 0.1f, sr_);
    v.hp2.set(k == HAT_C || k == HAT_O ? 6500.0f : 800.0f, 0.1f, sr_);
  }

  // Suma en L/R. pan fijo por pieza para que el kit tenga cuerpo estéreo sin tocar el bombo.
  void process(float* l, float* r, int n) {
    for (int k = 0; k < KINDS; ++k) {
      Voice& v = voices_[k];
      if (!v.on) continue;
      float pl = 1.0f, pr = 1.0f;
      if (k == HAT_C) { pl = 0.75f; pr = 1.0f; } else if (k == HAT_O) { pl = 1.0f; pr = 0.75f; }
      else if (k == RIM) { pl = 0.9f; pr = 0.8f; }
      for (int i = 0; i < n; ++i) {
        float s = render(k, v) * v.vel * 0.9f;
        l[i] += s * pl; r[i] += s * pr;
        if (!v.on) break;
      }
    }
  }

 private:
  enum Kind { KICK, RIM, SNARE, CLAP, HAT_C, HAT_O, TOM, KINDS };
  struct Voice {
    bool on = false, is808 = false;
    int note = 0;
    float vel = 1, tune = 52, decay = 0.4f, punch = 0.5f, tone = 0.5f, hatDecay = 0.07f;
    float f0 = 50, pitchAmt = 4, pitchTau = 0.02f, ampTau = 0.2f;
    long long t = 0;               // muestras desde el disparo
    double ph = 0, ph2 = 0, mph[6] = {0, 0, 0, 0, 0, 0};
    Svf bp, hp, hp2;
    float env = 1;
  };
  double sr_ = 44100;
  Rng rng_;
  Voice voices_[KINDS];

  float expEnv(long long t, float tau) const { return std::exp(-static_cast<float>(t) / (tau * static_cast<float>(sr_))); }

  float render(int k, Voice& v) {
    float tsec = static_cast<float>(v.t) / static_cast<float>(sr_);
    float out = 0;
    switch (k) {
      case KICK: case TOM: {
        float pe = std::exp(-tsec / v.pitchTau);
        double f = v.f0 * (1.0 + v.pitchAmt * pe);
        v.ph += f / sr_;
        float body = static_cast<float>(sinCycle(v.ph));
        float a = expEnv(v.t, v.ampTau);
        // ataque lineal cortísimo (evita click de arranque) + sobre decaimiento
        float att = std::min(1.0f, static_cast<float>(v.t) / (0.0008f * static_cast<float>(sr_)));
        out = body * a * att;
        if (k == KICK) {
          if (!v.is808) {  // 909: clic de golpe + saturación
            float click = rng_.next() * std::exp(-tsec / 0.0018f) * (0.25f + 0.5f * v.punch);
            out = std::tanh((out + click) * (1.4f + 1.2f * v.punch)) * 0.85f;
          } else {         // 808: casi seno puro, saturación suave
            out = std::tanh(out * 1.15f) * 0.95f;
          }
        } else {
          out *= 0.8f;
        }
        if (a < 1e-4f) v.on = false;
        break;
      }
      case SNARE: {
        float f = v.f0 * (1.0f + 0.25f * std::exp(-tsec / 0.012f));
        v.ph += f / sr_; v.ph2 += f * 1.9 / sr_;
        float tonal = (static_cast<float>(sinCycle(v.ph)) + 0.5f * static_cast<float>(sinCycle(v.ph2))) * expEnv(v.t, v.is808 ? 0.05f : 0.065f);
        float nz = v.hp.highpass(rng_.next());
        float snappy = 0.35f + 0.65f * v.tone;
        float noise = nz * expEnv(v.t, 0.045f + 0.1f * snappy) * snappy * 1.3f;
        out = (tonal * 0.75f + noise) * 0.9f;
        if (v.t > static_cast<long long>(0.6 * sr_)) v.on = false;
        break;
      }
      case CLAP: {
        // tres ráfagas cortas (a 0, 10 y 20 ms) y una cola más larga
        float nz = v.bp.bandpass(rng_.next()) * 2.2f;
        float e = 0;
        float ms = tsec * 1000.0f;
        if (ms < 10.0f) e = std::exp(-ms / 2.2f);
        else if (ms < 20.0f) e = std::exp(-(ms - 10.0f) / 2.2f);
        else if (ms < 30.0f) e = std::exp(-(ms - 20.0f) / 2.2f);
        float tail = (ms >= 30.0f) ? std::exp(-(ms - 30.0f) / (1000.0f * (0.04f + 0.13f * v.tone))) : 0.0f;
        out = nz * std::max(e, tail * 0.8f);
        if (ms > 600.0f) v.on = false;
        break;
      }
      case RIM: {
        v.ph += 455.0 / sr_; v.ph2 += 1667.0 / sr_;
        float e = expEnv(v.t, 0.006f);
        float s = (static_cast<float>(sinCycle(v.ph)) * 0.6f + static_cast<float>(sinCycle(v.ph2)) * 0.8f) * e;
        s += v.hp.highpass(rng_.next()) * expEnv(v.t, 0.002f) * 0.5f;
        out = s * 0.9f;
        if (e < 1e-4f && v.t > 64) v.on = false;
        break;
      }
      case HAT_C: case HAT_O: {
        static const double fr[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
        float m = 0;
        double mul = 2.0 + 0.4 * v.tone;   // multiplicador de la banda metálica (909 más agudo)
        for (int i = 0; i < 6; ++i) {
          v.mph[i] += fr[i] * mul / sr_; if (v.mph[i] >= 1.0) v.mph[i] -= 1.0;
          m += v.mph[i] < 0.5 ? 1.0f : -1.0f;
        }
        m *= 0.18f;
        float x = v.hp.highpass(m + rng_.next() * 0.1f);
        x = v.hp2.highpass(x);
        float e = expEnv(v.t, v.ampTau);
        float att = std::min(1.0f, static_cast<float>(v.t) / (0.0004f * static_cast<float>(sr_)));
        out = x * e * att * 0.9f;
        if (e < 1e-4f) v.on = false;
        break;
      }
    }
    ++v.t;
    return flushDenormal(out);
  }
};

}  // namespace schmu
