// SCHMU 90s · utilidades de DSP comunes (sin dependencias: ni JUCE ni nada fuera de la stdlib).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace schmu {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr int kBlock = 16;  // tamaño del sub-bloque de control (coeficientes de filtro, envolventes lentas)

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float midiToHz(float note) { return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f); }
inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linToDb(float x) { return 20.0f * std::log10(std::max(x, 1e-9f)); }
inline float flushDenormal(float x) { return std::fabs(x) < 1e-20f ? 0.0f : x; }

// Ruido blanco determinista (para que los renders y los tests sean reproducibles).
struct Rng {
  uint32_t s = 0x9E3779B9u;
  void seed(uint32_t v) { s = v ? v : 1u; }
  float next() {  // [-1, 1)
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return static_cast<float>(static_cast<int32_t>(s)) * (1.0f / 2147483648.0f);
  }
  float uni() { return next() * 0.5f + 0.5f; }  // [0, 1)
};

// Tabla de seno con interpolación lineal (fase normalizada 0..1).
struct SineTable {
  static constexpr int N = 4096;
  std::array<float, N + 1> t{};
  SineTable() {
    for (int i = 0; i <= N; ++i) t[i] = static_cast<float>(std::sin(kTwoPi * i / N));
  }
  float at(double phase) const {
    double p = phase - std::floor(phase);
    double x = p * N;
    int i = static_cast<int>(x);
    float f = static_cast<float>(x - i);
    return t[i] + (t[i + 1] - t[i]) * f;
  }
};
inline const SineTable& sineTable() { static const SineTable s; return s; }
inline float sinCycle(double phase) { return sineTable().at(phase); }

// PolyBLEP: corrige el salto de un diente de sierra/pulso para reducir el aliasing.
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}
inline float sawBlep(double phase, double dt) {  // -1..1
  float t = static_cast<float>(phase);
  return 2.0f * t - 1.0f - polyBlep(t, static_cast<float>(dt));
}
inline float pulseBlep(double phase, double dt, float pw) {  // pulso con ancho pw (0.05..0.95)
  float t = static_cast<float>(phase);
  float v = t < pw ? 1.0f : -1.0f;
  v += polyBlep(t, static_cast<float>(dt));
  float t2 = t - pw; if (t2 < 0.0f) t2 += 1.0f;
  v -= polyBlep(t2, static_cast<float>(dt));
  return v;
}

// Filtro de variable de estado TPT (Zavalishin): estable y suave al modular el corte.
struct Svf {
  float ic1 = 0, ic2 = 0;
  float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
  void set(float cutoffHz, float res01, double sr) {  // res01: 0 (sin resonancia) .. 1 (casi autooscila)
    float fc = clampf(cutoffHz, 15.0f, static_cast<float>(sr) * 0.45f);
    g = std::tan(static_cast<float>(kPi) * fc / static_cast<float>(sr));
    k = 2.0f - 1.96f * clampf(res01, 0.0f, 1.0f);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
  }
  void reset() { ic1 = ic2 = 0; }
  // devuelve {low, band, high}
  void tick(float v0, float& lp, float& bp, float& hp) {
    float v3 = v0 - ic2;
    float v1 = a1 * ic1 + a2 * v3;
    float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = flushDenormal(2.0f * v1 - ic1);
    ic2 = flushDenormal(2.0f * v2 - ic2);
    lp = v2; bp = v1; hp = v0 - k * v1 - v2;
  }
  float lowpass(float x) { float l, b, h; tick(x, l, b, h); return l; }
  float highpass(float x) { float l, b, h; tick(x, l, b, h); return h; }
  float bandpass(float x) { float l, b, h; tick(x, l, b, h); return b; }
};

// Filtro de un polo (suavizado de parámetros, caída de agudos de la cinta, etc.).
struct OnePole {
  float z = 0, a = 0.0f;
  void setCutoff(float hz, double sr) { a = 1.0f - std::exp(-static_cast<float>(kTwoPi) * hz / static_cast<float>(sr)); }
  void setTime(float seconds, double sr) { a = 1.0f - std::exp(-1.0f / std::max(1e-5f, seconds * static_cast<float>(sr))); }
  float lowpass(float x) { z += a * (x - z); z = flushDenormal(z); return z; }
  void reset(float v = 0) { z = v; }
};

// Envolvente ADSR: ataque lineal, decaimiento y liberación exponenciales (-60 dB en el tiempo indicado).
struct Adsr {
  enum Stage { Idle, Attack, Decay, Sustain, Release };
  Stage stage = Idle;
  float level = 0, sustain = 1, attackInc = 0.01f, decayCoef = 0.999f, releaseCoef = 0.999f;
  static constexpr float kEnd = 1e-4f;  // -80 dB: ahí se considera terminada
  static float coefFor(float seconds, double sr) { return std::exp(-6.9078f / std::max(1.0f, seconds * static_cast<float>(sr))); }
  void set(float a, float d, float s, float r, double sr) {
    attackInc = 1.0f / std::max(1.0f, std::max(a, 0.001f) * static_cast<float>(sr));
    decayCoef = coefFor(std::max(d, 0.005f), sr);
    releaseCoef = coefFor(std::max(r, 0.005f), sr);
    sustain = clampf(s, 0.0f, 1.0f);
  }
  void gateOn() { stage = Attack; }
  void gateOff() { if (stage != Idle) stage = Release; }
  void kill(double sr) { releaseCoef = coefFor(0.006f, sr); stage = Release; }  // robo de voz: release de 6 ms
  bool active() const { return stage != Idle; }
  float tick() {
    switch (stage) {
      case Idle: return 0;
      case Attack: level += attackInc; if (level >= 1.0f) { level = 1.0f; stage = Decay; } break;
      case Decay:
        level = sustain + (level - sustain) * decayCoef;
        if (sustain > 0.0f) { if (level - sustain < 1e-4f) { level = sustain; stage = Sustain; } }
        else if (level < kEnd) { level = 0; stage = Idle; }
        break;
      case Sustain: level = sustain; break;
      case Release: level *= releaseCoef; if (level < kEnd) { level = 0; stage = Idle; } break;
    }
    return level;
  }
};

// Decaimiento exponencial simple (envolvente de filtro, de tono de bombo, etc.).
struct ExpDecay {
  float v = 0, c = 0.999f;
  void trigger(float start, float seconds, double sr) { v = start; c = std::exp(-6.9078f / std::max(1.0f, seconds * static_cast<float>(sr))); }
  float tick() { v *= c; if (v < 1e-7f) v = 0; return v; }
};

}  // namespace schmu
