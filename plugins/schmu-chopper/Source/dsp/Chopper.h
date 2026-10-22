// SCHMU Chopper · motor del sampler: análisis (auto-chop por transientes, tono, tonalidad del nombre de archivo),
// voces (repitch o pitch-lock granular alineado tipo WSOLA), secuenciador de 16 pasos y cadena de FX.
// Sin JUCE: todo se puede renderizar y testear offline.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Common.h"
#include "Effects.h"
#include "Params.h"

namespace schmu {

constexpr int kMaxSlices = 16;
constexpr int kMaxVoices = 24;
constexpr int kPadBase = 36;   // C1: 16 pads desde acá (mismo mapa que el Drum Rack de Vocal Chops Vol.1)

// ------------------------------------------------------------------ datos
struct SampleData {
  std::vector<float> l, r;   // r puede ser igual a l (mono)
  double sampleRate = 44100;
  int length() const { return static_cast<int>(l.size()); }
  bool empty() const { return l.empty(); }
};

enum PlayMode { PM_ONESHOT, PM_GATE, PM_LOOP };
enum FilterType { FT_OFF, FT_LP, FT_HP };

struct SliceParams {
  float gainDb = 0, pan = 0, pitch = 0;       // dB, -1..1, semitonos (±12)
  bool reverse = false;
  int mode = PM_ONESHOT;
  float attack = 0.002f, decay = 0.2f, sustain = 1.0f, release = 0.08f;
  int filter = FT_OFF;
  float cutoff = 8000.0f;
  int choke = 0;                              // 0 = sin grupo; 1..8
};

struct Slice {
  int start = 0, end = 0;                     // en muestras del sample original
  int root = -1;                              // nota MIDI de origen (modo Key); -1 = desconocida (usa C3)
  SliceParams p;
};

struct Step {
  int slice = -1;                             // -1 = paso vacío
  float prob = 1.0f;                          // 0..1
  int pitch = 0;                              // ±12 st sobre el pitch del slice
};

struct Bank {
  std::shared_ptr<const SampleData> data;
  std::vector<Slice> slices;                  // 0..16, ordenados y sin solaparse
  std::array<Step, 16> steps{};
  int keySlice = 0;                           // slice que se toca en modo Key
};

// ------------------------------------------------------------------ análisis
inline float monoAt(const SampleData& d, int i) { return 0.5f * (d.l[static_cast<size_t>(i)] + d.r[static_cast<size_t>(i)]); }

// Detección de transientes: energía en dB por tramos de ~2,5 ms; un onset es un aumento de `umbral` dB respecto de
// 7,5 ms antes. `sens` 0..1 (más sensibilidad = umbral más bajo). Devuelve las posiciones de inicio (ordenadas, <= maxN):
// si hay más, se queda con las más fuertes. Determinista.
inline std::vector<int> detectOnsets(const SampleData& d, float sens, int maxN = kMaxSlices) {
  std::vector<int> out;
  const int n = d.length();
  if (n < 256) return out;
  const int hop = std::max(32, static_cast<int>(d.sampleRate / 400.0));
  const int frames = n / hop;
  if (frames < 8) return out;
  std::vector<float> e(static_cast<size_t>(frames));
  for (int f = 0; f < frames; ++f) {
    double s = 0;
    int a = f * hop, b = std::min(n, a + 2 * hop);
    for (int i = a; i < b; ++i) { float v = monoAt(d, i); s += static_cast<double>(v) * v; }
    e[static_cast<size_t>(f)] = 10.0f * std::log10(static_cast<float>(s / std::max(1, b - a)) + 1e-10f);
  }
  const float thr = lerpf(20.0f, 4.0f, clampf(sens, 0.0f, 1.0f));
  const float floorDb = -55.0f;
  const int back = 3, minGap = std::max(1, static_cast<int>(0.05 * d.sampleRate / hop));
  std::vector<float> o(static_cast<size_t>(frames), 0.0f);
  for (int f = back; f < frames; ++f) {
    float ref = std::min(e[static_cast<size_t>(f - back)], e[static_cast<size_t>(f - 1)]);
    if (e[static_cast<size_t>(f)] > floorDb) o[static_cast<size_t>(f)] = std::max(0.0f, e[static_cast<size_t>(f)] - std::max(ref, floorDb));
  }
  struct Cand { int frame; float strength; };
  std::vector<Cand> cands;
  for (int f = back; f < frames; ++f) {
    if (o[static_cast<size_t>(f)] < thr) continue;
    // máximo local dentro de la ventana de separación mínima
    bool isMax = true;
    for (int k = std::max(0, f - minGap); k <= std::min(frames - 1, f + minGap); ++k)
      if (o[static_cast<size_t>(k)] > o[static_cast<size_t>(f)] || (o[static_cast<size_t>(k)] == o[static_cast<size_t>(f)] && k < f)) { isMax = false; break; }
    if (isMax) cands.push_back({f, o[static_cast<size_t>(f)]});
  }
  std::stable_sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.strength > b.strength; });
  if (static_cast<int>(cands.size()) > maxN) cands.resize(static_cast<size_t>(maxN));
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.frame < b.frame; });
  for (const auto& c : cands) {
    // el onset real empieza unos tramos antes del pico de la derivada; se acomoda al cruce por cero anterior
    int pos = std::max(0, (c.frame - 2) * hop);
    int lim = std::max(0, pos - static_cast<int>(0.002 * d.sampleRate));
    int z = pos;
    for (int i = pos; i > lim; --i) if ((monoAt(d, i) <= 0.0f) != (monoAt(d, i - 1) <= 0.0f)) { z = i; break; }
    if (!out.empty() && z <= out.back()) continue;
    out.push_back(z);
  }
  return out;
}

// Convierte inicios en slices: cada uno termina donde empieza el siguiente; el último, donde se acaba el sonido útil.
inline std::vector<Slice> slicesFromStarts(const SampleData& d, const std::vector<int>& starts) {
  std::vector<Slice> out;
  const int n = d.length();
  if (n <= 0) return out;
  std::vector<int> s = starts;
  if (s.empty()) s.push_back(0);
  int lastEnd = n;
  {  // recorta el silencio final (< -55 dB)
    int hop = std::max(32, static_cast<int>(d.sampleRate / 400.0));
    int f = n / hop;
    while (f > 0) {
      double acc = 0; int a = (f - 1) * hop, b = std::min(n, a + hop);
      for (int i = a; i < b; ++i) { float v = monoAt(d, i); acc += static_cast<double>(v) * v; }
      if (10.0 * std::log10(acc / std::max(1, b - a) + 1e-10) > -55.0) break;
      --f;
    }
    lastEnd = std::max(s.back() + 64, std::min(n, f * hop + hop * 4));
  }
  for (size_t i = 0; i < s.size() && static_cast<int>(i) < kMaxSlices; ++i) {
    Slice sl;
    sl.start = s[i];
    sl.end = i + 1 < s.size() ? s[i + 1] : lastEnd;
    if (sl.end - sl.start < 16) continue;
    out.push_back(sl);
  }
  return out;
}

inline std::vector<Slice> autoChop(const SampleData& d, float sens) { return slicesFromStarts(d, detectOnsets(d, sens)); }

inline std::vector<Slice> evenChop(const SampleData& d, int n) {
  n = std::max(1, std::min(kMaxSlices, n));
  std::vector<int> st;
  for (int i = 0; i < n; ++i) st.push_back(static_cast<int>(static_cast<long long>(d.length()) * i / n));
  auto v = slicesFromStarts(d, st);
  for (size_t i = 0; i + 1 < v.size(); ++i) v[i].end = v[i + 1].start;
  if (!v.empty()) v.back().end = d.length();
  return v;
}

// Edición manual de slices (marcadores). Los parámetros de cada slice acompañan a su marcador.
inline int sliceAt(const std::vector<Slice>& v, int pos) {
  for (size_t i = 0; i < v.size(); ++i) if (pos >= v[i].start && pos < v[i].end) return static_cast<int>(i);
  return -1;
}
inline bool insertMarker(std::vector<Slice>& v, int len, int pos) {
  if (static_cast<int>(v.size()) >= kMaxSlices || pos < 16 || pos > len - 16) return false;
  if (v.empty()) {
    Slice a; a.start = 0; a.end = pos; Slice b; b.start = pos; b.end = len;
    v = {a, b};
    return true;
  }
  int i = sliceAt(v, pos);
  if (i < 0 || pos - v[static_cast<size_t>(i)].start < 16 || v[static_cast<size_t>(i)].end - pos < 16) return false;
  Slice nw = v[static_cast<size_t>(i)];
  nw.start = pos;
  v[static_cast<size_t>(i)].end = pos;
  v.insert(v.begin() + i + 1, nw);
  return true;
}
inline bool removeMarker(std::vector<Slice>& v, int index) {   // une el slice `index` con el anterior
  if (index <= 0 || index >= static_cast<int>(v.size())) return false;
  v[static_cast<size_t>(index) - 1].end = v[static_cast<size_t>(index)].end;
  v.erase(v.begin() + index);
  return true;
}
inline bool moveMarker(std::vector<Slice>& v, int index, int pos) {
  if (index <= 0 || index >= static_cast<int>(v.size())) return false;
  int lo = v[static_cast<size_t>(index) - 1].start + 16, hi = v[static_cast<size_t>(index)].end - 16;
  if (hi < lo) return false;
  pos = std::max(lo, std::min(hi, pos));
  v[static_cast<size_t>(index)].start = pos;
  v[static_cast<size_t>(index) - 1].end = pos;
  return true;
}

// ---- tono ----
// Frecuencia fundamental por autocorrelación normalizada en el tramo más fuerte; 0 si no hay tono claro.
inline float detectF0(const float* x, int n, double sr) {
  const int win = 1024;
  const int lagMin = std::max(8, static_cast<int>(sr / 1000.0));
  const int lagMax = static_cast<int>(sr / 70.0);
  if (n < win + lagMax + 8) return 0.0f;
  int a = 0; double bestE = -1;   // tramo más fuerte
  for (int p = 0; p + win + lagMax <= n; p += win / 4) {
    double s = 0; for (int i = p; i < p + win; ++i) s += static_cast<double>(x[i]) * x[i];
    if (s > bestE) { bestE = s; a = p; }
  }
  if (bestE < 1e-4) return 0.0f;
  std::vector<double> r(static_cast<size_t>(lagMax + 2), 0.0);
  double e0 = 0; for (int i = 0; i < win; ++i) e0 += static_cast<double>(x[a + i]) * x[a + i];
  double best = 0;
  for (int lag = lagMin; lag <= lagMax; ++lag) {
    double c = 0, e1 = 0;
    for (int i = 0; i < win; ++i) { c += static_cast<double>(x[a + i]) * x[a + i + lag]; e1 += static_cast<double>(x[a + i + lag]) * x[a + i + lag]; }
    r[static_cast<size_t>(lag)] = c / std::sqrt(e0 * e1 + 1e-12);
    best = std::max(best, r[static_cast<size_t>(lag)]);
  }
  if (best < 0.6) return 0.0f;
  for (int lag = lagMin + 1; lag < lagMax; ++lag) {   // el primer máximo local cercano al máximo global evita errores de octava
    double v = r[static_cast<size_t>(lag)];
    if (v >= 0.9 * best && v >= r[static_cast<size_t>(lag) - 1] && v >= r[static_cast<size_t>(lag) + 1]) {
      double y0 = r[static_cast<size_t>(lag) - 1], y1 = v, y2 = r[static_cast<size_t>(lag) + 1];
      double den = y0 - 2 * y1 + y2;
      double off = std::fabs(den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0;
      return static_cast<float>(sr / (lag + off));
    }
  }
  return 0.0f;
}

// Nombre del archivo SCHMU_VC1_<CAT>_<nn>_<key>_<bpm>.wav → clase de tono 0..11 (C=0, Cs=1 … B=11); -1 si NOKEY/no hay.
inline int keyFromFilename(const std::string& name) {
  static const char* K[12] = {"C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B"};
  std::string base = name;
  size_t dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);
  size_t slash = base.find_last_of("/\\");
  if (slash != std::string::npos) base = base.substr(slash + 1);
  std::vector<std::string> tok;
  std::string cur;
  for (char c : base) { if (c == '_') { tok.push_back(cur); cur.clear(); } else cur.push_back(c); }
  tok.push_back(cur);
  if (tok.size() < 3 || tok[0] != "SCHMU") return -1;
  const std::string& k = tok[tok.size() - 2];
  for (int i = 0; i < 12; ++i) if (k == K[i]) return i;
  return -1;
}
inline int bpmFromFilename(const std::string& name) {
  std::string base = name;
  size_t dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);
  size_t us = base.find_last_of('_');
  if (us == std::string::npos) return 0;
  std::string t = base.substr(us + 1);
  if (t.empty() || t.size() > 3) return 0;
  int v = 0;
  for (char c : t) { if (c < '0' || c > '9') return 0; v = v * 10 + (c - '0'); }
  return v;
}

// Nota MIDI de origen de un slice: el tono medido, forzado a la clase de tono del nombre del archivo (si la hay).
inline int rootNoteFor(const SampleData& d, int start, int end, int pitchClass) {
  start = std::max(0, start); end = std::min(d.length(), end);
  std::vector<float> m(static_cast<size_t>(std::max(0, end - start)));
  for (int i = start; i < end; ++i) m[static_cast<size_t>(i - start)] = monoAt(d, i);
  float f0 = m.empty() ? 0.0f : detectF0(m.data(), static_cast<int>(m.size()), d.sampleRate);
  if (f0 > 0.0f) {
    int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(f0 / 440.0)));
    if (pitchClass < 0) return midi;
    int best = midi, bd = 99;
    for (int dl = -6; dl <= 5; ++dl) if (((midi + dl) % 12 + 12) % 12 == pitchClass && std::abs(dl) < bd) { bd = std::abs(dl); best = midi + dl; }
    return best;
  }
  return pitchClass >= 0 ? 60 + pitchClass : 60;
}

// ------------------------------------------------------------------ región (lectura de un slice)
// Un slice leído en tiempo virtual u ∈ [0, len): hacia adelante o invertido, con loop cruzado (sin clics).
struct Region {
  const SampleData* d = nullptr;
  int s = 0, e = 0, xf = 0;
  bool rev = false, loop = false;
  int len() const { return e - s; }
  double period() const { return static_cast<double>(len() - xf); }

  static float cubic(float y0, float y1, float y2, float y3, float t) {
    float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c = -0.5f * y0 + 0.5f * y2;
    return ((a * t + b) * t + c) * t + y1;
  }
  void fetch(double u, float& l, float& r) const {
    double idx = rev ? static_cast<double>(e - 1) - u : static_cast<double>(s) + u;
    int i1 = static_cast<int>(std::floor(idx));
    float t = static_cast<float>(idx - i1);
    int i0 = std::max(s, std::min(e - 1, i1 - 1)), i2 = std::max(s, std::min(e - 1, i1 + 1)), i3 = std::max(s, std::min(e - 1, i1 + 2));
    int c1 = std::max(s, std::min(e - 1, i1));
    const float* L = d->l.data(); const float* R = d->r.data();
    l = cubic(L[i0], L[c1], L[i2], L[i3], t);
    r = cubic(R[i0], R[c1], R[i2], R[i3], t);
  }
  // Lectura en tiempo virtual t (>= 0 normalmente). Sin loop, fuera de [0,len) es silencio.
  void read(double t, float& l, float& r) const {
    if (!loop) {
      if (t < 0.0 || t >= static_cast<double>(len())) { l = r = 0; return; }
      fetch(t, l, r);
      return;
    }
    const double P = period();
    double u = std::fmod(t, P);
    if (u < 0) u += P;
    if (xf > 0 && u < xf) {   // el comienzo se mezcla con la cola, así el salto de P a 0 es continuo
      float w = static_cast<float>(u / xf), l1, r1, l2, r2;
      fetch(P + u, l1, r1); fetch(u, l2, r2);
      l = l1 * (1.0f - w) + l2 * w; r = r1 * (1.0f - w) + r2 * w;
    } else fetch(u, l, r);
  }
  // Lectura mono lineal y barata, solo para buscar el punto de arranque del grano (sin crossfade del loop).
  float readMonoFast(double t) const {
    const double L = static_cast<double>(len());
    if (loop) { double P = period(); t = std::fmod(t, P); if (t < 0) t += P; }
    else if (t < 0.0 || t >= L - 1.0) return 0.0f;
    double idx = rev ? static_cast<double>(e - 1) - t : static_cast<double>(s) + t;
    int i = static_cast<int>(idx);
    float f = static_cast<float>(idx - i);
    int j = std::min(e - 1, i + 1); i = std::max(s, std::min(e - 1, i));
    float a = d->l[static_cast<size_t>(i)] + d->r[static_cast<size_t>(i)], b = d->l[static_cast<size_t>(j)] + d->r[static_cast<size_t>(j)];
    return 0.5f * (a + (b - a) * f);
  }
};

// ------------------------------------------------------------------ voz
struct Voice {
  bool active = false;
  int slice = -1, note = -1, group = 0;
  bool fromSeq = false;
  long long age = 0;
  Region reg;
  SliceParams p;
  double t = 0;              // tiempo virtual (en muestras del sample original)
  double srcStep = 1.0;      // muestras de origen por muestra de salida (conversión de frecuencia de muestreo)
  double ratio = 1.0;        // relación de tono
  bool lock = false;         // mantiene la duración (granular) en vez de repitch
  float vel = 1.0f, gL = 1.0f, gR = 1.0f;
  int delay = 0;             // retraso de inicio (humanize)
  int autoRelease = -1;      // muestras hasta soltar solo (pasos del secuenciador)
  Adsr env;
  Svf filt, filtR;
  int blockCount = 0;
  float fadeLen = 100.0f;

  // pitch-lock: granos de 46 ms con 50 % de solape y alineación por correlación (WSOLA)
  struct Grain { bool on = false, first = false; double pos = 0; int age = 0; };
  Grain grains[3];
  int grainLen = 2048, grainHop = 1024, sinceSpawn = 0;

  void start(const Region& rg, const SliceParams& sp, double hostSr, double ratioIn, bool lockIn, float velocity, int note_, int slice_, bool seq, double resonance) {
    reg = rg; p = sp; note = note_; slice = slice_; fromSeq = seq; group = sp.choke;
    ratio = ratioIn; lock = lockIn && std::fabs(ratioIn - 1.0) > 1e-4;
    srcStep = reg.d->sampleRate / hostSr;
    t = 0; age = 0; delay = 0; autoRelease = -1; vel = velocity;
    float g = dbToLin(sp.gainDb) * velocity;
    float ang = (clampf(sp.pan, -1.0f, 1.0f) * 0.5f + 0.5f) * static_cast<float>(kPi) * 0.5f;
    gL = g * std::cos(ang) * 1.4142136f; gR = g * std::sin(ang) * 1.4142136f;   // ley de potencia constante: centro = g
    env.set(sp.attack, sp.decay, sp.sustain, sp.release, hostSr);
    env.level = 0; env.gateOn();
    filt.reset(); filtR.reset(); filt.set(sp.cutoff, static_cast<float>(resonance), hostSr); filtR = filt; filtR.reset();
    blockCount = 0;
    fadeLen = static_cast<float>(std::max(32.0, 0.003 * reg.d->sampleRate));
    grainLen = std::max(256, static_cast<int>(0.046 * hostSr)); grainHop = grainLen / 2; sinceSpawn = 0;
    for (auto& gr : grains) gr = Grain{};
    active = true;
  }

  void spawnGrain() {
    int slot = 0;
    for (int i = 0; i < 3; ++i) if (!grains[i].on) { slot = i; break; }
    double rate = srcStep * ratio;
    double start = t;
    const Grain* prev = nullptr;
    for (const auto& g : grains) if (g.on && g.age == grainHop) prev = &g;
    if (prev) {   // busca el punto de arranque que continúa la fase del grano anterior (grueso y luego fino)
      const int S = std::max(48, static_cast<int>(0.010 * reg.d->sampleRate));
      const int J = 128, stepJ = 4;
      float pv[128];
      for (int j = 0; j < J; ++j) pv[j] = reg.readMonoFast(prev->pos + j * stepJ * rate);
      auto score = [&](int dl) {
        double c = t + dl, num = 0, den = 1e-9;
        for (int j = 0; j < J; ++j) { float q = reg.readMonoFast(c + j * stepJ * rate); num += static_cast<double>(pv[j]) * q; den += static_cast<double>(q) * q; }
        return num / std::sqrt(den) - 1e-5 * std::fabs(static_cast<double>(dl));
      };
      int bestD = 0; double bestScore = -1e30;
      for (int dl = -S; dl <= S; dl += 6) { double sc = score(dl); if (sc > bestScore) { bestScore = sc; bestD = dl; } }
      int fineBest = bestD;
      for (int dl = bestD - 5; dl <= bestD + 5; ++dl) { double sc = score(dl); if (sc > bestScore) { bestScore = sc; fineBest = dl; } }
      start = t + fineBest;
    }
    Grain& g = grains[slot];
    g.on = true; g.first = (prev == nullptr && age == 0); g.pos = start; g.age = 0;
  }

  // Suma `n` muestras de la voz en L/R.
  void render(float* L, float* R, int n, double hostSr, double resonance) {
    int i = 0;
    if (delay > 0) { int d = std::min(delay, n); delay -= d; i = d; }
    for (; i < n && active; ++i) {
      if (autoRelease == 0) { env.gateOff(); autoRelease = -1; }
      else if (autoRelease > 0) --autoRelease;
      if (blockCount == 0) {
        if (p.filter != FT_OFF) { filt.set(p.cutoff, static_cast<float>(resonance), hostSr); filtR.set(p.cutoff, static_cast<float>(resonance), hostSr); }
        blockCount = kBlock;
      }
      --blockCount;
      float l = 0, r = 0;
      if (!lock) {
        reg.read(t, l, r);
        t += srcStep * ratio;
      } else {
        if (sinceSpawn == 0) spawnGrain();
        for (auto& g : grains) {
          if (!g.on) continue;
          float gl, gr;
          reg.read(g.pos, gl, gr);
          float w = (g.first && g.age < grainHop) ? 1.0f : 0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi) * static_cast<float>(g.age) / static_cast<float>(grainLen));
          l += gl * w; r += gr * w;
          g.pos += srcStep * ratio;
          if (++g.age >= grainLen) g.on = false;
        }
        if (++sinceSpawn >= grainHop) sinceSpawn = 0;
        t += srcStep;
      }
      // fin del sample (sin loop): fade corto y baja la voz
      if (!reg.loop) {
        double remain = static_cast<double>(reg.len()) - t;
        if (remain < fadeLen) { float f = static_cast<float>(std::max(0.0, remain) / fadeLen); l *= f; r *= f; }
        if (remain <= 0.0) { active = false; break; }
      }
      float e = env.tick();
      if (!env.active()) { active = false; break; }
      if (p.filter == FT_LP) { l = filt.lowpass(l); r = filtR.lowpass(r); }
      else if (p.filter == FT_HP) { l = filt.highpass(l); r = filtR.highpass(r); }
      l *= e; r *= e;
      L[i] += l * gL; R[i] += r * gR;
    }
    age += n;
  }
};

// ------------------------------------------------------------------ bitcrusher suave
struct Crusher {
  float holdL = 0, holdR = 0; int cnt = 0;
  void reset() { holdL = holdR = 0; cnt = 0; }
  void process(float& l, float& r, float amount) {
    if (amount <= 0.001f) return;
    const int div = 1 + static_cast<int>(amount * amount * 7.0f);
    const float bits = 16.0f - 11.0f * amount;
    const float q = std::pow(2.0f, bits - 1.0f);
    if (cnt <= 0) {
      holdL = std::round(l * q) / q; holdR = std::round(r * q) / q;
      cnt = div;
    }
    --cnt;
    float mix = std::min(1.0f, amount * 5.0f);
    l = lerpf(l, holdL, mix); r = lerpf(r, holdR, mix);
  }
};

// ------------------------------------------------------------------ motor
class ChopEngine {
 public:
  void prepare(double sr) {
    sr_ = sr;
    tape_.prepare(sr); delay_.prepare(sr); reverb_.prepare(sr); crusher_.reset();
    for (auto& v : voices_) v.active = false;
    rng_.seed(12345);
    bank_.slices.reserve(kMaxSlices);   // setBank() reasigna sin pedir memoria nueva (se llama desde el hilo de audio)
    seqReady_ = false; seqRunning_ = false;
    internalPpq_ = 0.0;
  }

  void setBank(const Bank& b) {
    const bool sampleChanged = bank_.data != b.data;
    bank_ = b;
    if (sampleChanged) allNotesOff();
  }
  const Bank& bank() const { return bank_; }
  void setParam(int id, float v) { params_[static_cast<size_t>(id)] = v; }
  float param(int id) const { return params_[static_cast<size_t>(id)]; }

  void allNotesOff() { for (auto& v : voices_) v.active = false; }

  // Transporte del host. hasHost=false → reloj interno (tempo del parámetro), solo en modo Sequence.
  void setHost(bool hasHost, bool playing, double ppq, double bpm) {
    hasHost_ = hasHost; playing_ = playing; hostPpq_ = ppq; bpm_ = std::max(20.0, hasHost ? bpm : static_cast<double>(params_[P_TEMPO]));
  }

  void noteOn(int note, float vel) {
    const int mode = static_cast<int>(params_[P_MODE] + 0.5f);
    if (!bank_.data || bank_.data->empty()) return;
    if (mode == M_KEY) {
      if (bank_.slices.empty()) return;
      int si = std::max(0, std::min(static_cast<int>(bank_.slices.size()) - 1, bank_.keySlice));
      const Slice& s = bank_.slices[static_cast<size_t>(si)];
      int root = s.root >= 0 ? s.root : 60;
      double semis = (note - root) + params_[P_ROOTOFS] + s.p.pitch;
      trigger(si, note, vel, semis, false, -1);
    } else {
      int si = note - kPadBase;
      if (si < 0 || si >= static_cast<int>(bank_.slices.size())) return;
      trigger(si, note, vel, bank_.slices[static_cast<size_t>(si)].p.pitch, false, -1);
    }
  }

  void noteOff(int note) {
    for (auto& v : voices_) if (v.active && !v.fromSeq && v.note == note && v.p.mode != PM_ONESHOT) v.env.gateOff();
  }

  // Renderiza n muestras (suma el resultado de las voces y la cadena de FX; escribe en L/R).
  void process(float* L, float* R, int n) {
    std::fill(L, L + n, 0.0f); std::fill(R, R + n, 0.0f);
    const int mode = static_cast<int>(params_[P_MODE] + 0.5f);
    const bool seqMode = mode == M_SEQ;
    const double spb = stepsPerBeat(static_cast<int>(params_[P_SEQRATE] + 0.5f));
    const double swingFrac = (clampf(params_[P_SWING], 0.5f, 0.75f) - 0.5f) * 2.0;
    const bool clockOn = seqMode && (hasHost_ ? playing_ : true);
    if (!clockOn && seqRunning_) { releaseSeqVoices(); seqRunning_ = false; seqReady_ = false; }

    int done = 0;
    double ppq = hasHost_ ? hostPpq_ : internalPpq_;
    const double inc = bpm_ / 60.0 / sr_;
    if (clockOn) {
      if (!seqReady_ || std::fabs(ppq - expectedPpq_) > 0.02) {   // arranque o salto de posición
        nextStep_ = static_cast<long long>(std::ceil(ppq * spb - 1e-9));
        seqReady_ = true;
      }
      seqRunning_ = true;
    }
    while (done < n) {
      int len = n - done;
      if (clockOn) {
        // muestras hasta el próximo disparo
        double trig = (static_cast<double>(nextStep_) + ((nextStep_ & 1LL) ? swingFrac : 0.0)) / spb;
        double need = (trig - ppq) / inc;
        int toTrig = need <= 0 ? 0 : static_cast<int>(std::ceil(need));
        len = std::min(len, std::max(0, toTrig));
        if (toTrig <= 0) {
          fireStep(nextStep_, spb);
          ++nextStep_;
          continue;
        }
      }
      if (len <= 0) len = 1;
      renderVoices(L + done, R + done, len);
      done += len;
      ppq += inc * len;
    }
    expectedPpq_ = ppq;
    if (!hasHost_) internalPpq_ = ppq;

    const float vol = params_[P_VOLUME];
    const int dd = static_cast<int>(params_[P_DELAYDIV] + 0.5f);
    for (int i = 0; i < n; ++i) {
      float l = L[i], r = R[i];
      tape_.process(l, r, params_[P_TAPE]);
      crusher_.process(l, r, params_[P_CRUSH]);
      delay_.process(l, r, params_[P_DELAYMIX], dd, params_[P_DELAYFB], bpm_);
      reverb_.process(l, r, params_[P_REVERBMIX], params_[P_REVERBSIZE]);
      L[i] = softLimit(l * vol); R[i] = softLimit(r * vol);
    }
  }

  long long triggerCount() const { return triggers_; }
  int activeVoices() const { int c = 0; for (const auto& v : voices_) if (v.active) ++c; return c; }

 private:
  void releaseSeqVoices() { for (auto& v : voices_) if (v.active && v.fromSeq) v.env.gateOff(); }

  void renderVoices(float* L, float* R, int n) {
    for (auto& v : voices_) if (v.active) v.render(L, R, n, sr_, params_[P_RESO]);
  }

  void fireStep(long long idx, double spb) {
    const int len = std::max(1, std::min(16, static_cast<int>(params_[P_SEQLEN] + 0.5f)));
    const Step& st = bank_.steps[static_cast<size_t>(((idx % len) + len) % len)];
    if (st.slice < 0 || st.slice >= static_cast<int>(bank_.slices.size())) return;
    if (st.prob < 0.999f && rng_.uni() >= st.prob) return;
    const int stepSamples = static_cast<int>(sr_ * 60.0 / bpm_ / spb);
    const SliceParams& sp = bank_.slices[static_cast<size_t>(st.slice)].p;
    trigger(st.slice, 1000 + st.slice, 1.0f, sp.pitch + st.pitch, true, sp.mode == PM_ONESHOT ? -1 : static_cast<int>(stepSamples * 0.95));
  }

  void trigger(int si, int note, float vel, double semis, bool fromSeq, int autoRel) {
    const Slice& s = bank_.slices[static_cast<size_t>(si)];
    const SampleData& d = *bank_.data;
    const double hum = params_[P_HUMANIZE];
    int delaySamples = 0;
    if (hum > 0.0) {
      vel = clampf(vel * (1.0f - static_cast<float>(hum) * 0.3f * rng_.uni()), 0.05f, 1.0f);
      semis += hum * 0.12 * rng_.next();                       // hasta ±12 cents
      delaySamples = static_cast<int>(hum * 0.006 * sr_ * rng_.uni());   // hasta 6 ms
    }
    // en Chop/Sequence cada slice es monofónico (un golpe nuevo corta el anterior); en Key es polifónico. Choke por grupo.
    const bool keyMode = static_cast<int>(params_[P_MODE] + 0.5f) == M_KEY && !fromSeq;
    for (auto& o : voices_) {
      if (!o.active) continue;
      if ((!keyMode && o.slice == si) || (s.p.choke > 0 && o.group == s.p.choke)) o.env.kill(sr_);
    }
    Voice* v = nullptr;
    for (auto& c : voices_) if (!c.active) { v = &c; break; }
    if (!v) { v = &voices_[0]; for (auto& c : voices_) if (c.age > v->age) v = &c; }
    Region rg;
    rg.d = &d; rg.s = s.start; rg.e = s.end; rg.rev = s.p.reverse; rg.loop = s.p.mode == PM_LOOP;
    rg.xf = rg.loop ? std::max(0, std::min(static_cast<int>(0.005 * d.sampleRate), (rg.e - rg.s) / 4)) : 0;
    const bool lockOn = params_[P_PITCHLOCK] > 0.5f;
    v->start(rg, s.p, sr_, std::pow(2.0, semis / 12.0), lockOn, vel, note, si, fromSeq, params_[P_RESO]);
    v->delay = delaySamples;
    ++triggers_;
    v->autoRelease = autoRel;
  }

  double sr_ = 44100.0;
  Bank bank_;
  std::array<float, P_COUNT> params_ = [] { std::array<float, P_COUNT> a{}; for (int i = 0; i < P_COUNT; ++i) a[static_cast<size_t>(i)] = paramTable()[static_cast<size_t>(i)].def; return a; }();
  std::array<Voice, kMaxVoices> voices_;
  Tape tape_; Crusher crusher_; Delay delay_; Reverb reverb_;
  Rng rng_;
  bool hasHost_ = false, playing_ = false, seqReady_ = false, seqRunning_ = false;
  double hostPpq_ = 0, bpm_ = 126.0, internalPpq_ = 0, expectedPpq_ = 0;
  long long nextStep_ = 0, triggers_ = 0;
};

}  // namespace schmu
