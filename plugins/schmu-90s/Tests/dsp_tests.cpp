// SCHMU 90s · tests de DSP (sin JUCE, sin audio real: todo se renderiza offline en memoria).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "../Source/dsp/Engine.h"
#include "../Source/dsp/Presets.h"

using namespace schmu;

static int g_checks = 0, g_fails = 0;
#define CHECK(cond, ...) do { ++g_checks; if (!(cond)) { ++g_fails; std::printf("  FALLA [%s:%d] %s  -> ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Buf { std::vector<float> l, r; };

static void applyPreset(SchmuEngine& e, const Preset& p) {
  auto v = presetValues(p);
  for (int i = 0; i < P_COUNT; ++i) e.setParam(i, v[i]);
}
static void render(SchmuEngine& e, Buf& b, int n) {
  size_t o = b.l.size();
  b.l.resize(o + n); b.r.resize(o + n);
  int done = 0;
  while (done < n) { int m = std::min(333, n - done); e.process(&b.l[o + done], &b.r[o + done], m); done += m; }  // bloques irregulares a propósito
}
static float peakOf(const Buf& b, int from = 0, int to = -1) {
  if (to < 0) to = static_cast<int>(b.l.size());
  float p = 0;
  for (int i = from; i < to; ++i) p = std::max(p, std::max(std::fabs(b.l[i]), std::fabs(b.r[i])));
  return p;
}
static double rmsOf(const Buf& b, int from, int to) {
  double s = 0; for (int i = from; i < to; ++i) s += 0.5 * (b.l[i] * b.l[i] + b.r[i] * b.r[i]);
  return std::sqrt(s / std::max(1, to - from));
}
static bool finiteAll(const Buf& b) {
  for (size_t i = 0; i < b.l.size(); ++i) if (!std::isfinite(b.l[i]) || !std::isfinite(b.r[i])) return false;
  return true;
}
static double goertzel(const std::vector<float>& x, int from, int to, double f, double sr) {
  double w = 2.0 * kPi * f / sr, c = 2.0 * std::cos(w), s1 = 0, s2 = 0;
  for (int i = from; i < to; ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
  double p = s1 * s1 + s2 * s2 - c * s1 * s2;
  return std::sqrt(std::max(0.0, p)) / std::max(1, to - from);
}
static double peakFreq(const std::vector<float>& x, int from, int to, double lo, double hi, double step, double sr) {
  double best = lo, bm = -1;
  for (double f = lo; f <= hi; f += step) { double m = goertzel(x, from, to, f, sr); if (m > bm) { bm = m; best = f; } }
  return best;
}

// Preset "limpio" para medir un motor en seco (sin FX).
static void dry(SchmuEngine& e, int engine) {
  e.setDefaults();
  e.setParam(P_ENGINE, static_cast<float>(engine));
  e.setParam(P_VOLUME, 1.0f);
}

static void test_params() {
  std::printf("[params]\n");
  std::set<std::string> ids;
  for (int i = 0; i < P_COUNT; ++i) {
    const auto& d = paramTable()[i];
    CHECK(ids.insert(d.id).second, "id duplicado %s", d.id);
    CHECK(d.min < d.max, "rango %s", d.id);
    CHECK(d.def >= d.min && d.def <= d.max, "default fuera de rango %s", d.id);
    CHECK(d.skew > 0 && d.skew <= 1.0f, "skew %s", d.id);
    if (d.choices) {
      auto c = choiceList(i);
      CHECK(static_cast<int>(c.size()) == static_cast<int>(d.max) + 1, "%s: %zu opciones vs max %g", d.id, c.size(), d.max);
      CHECK(d.min == 0, "lista %s debe arrancar en 0", d.id);
    }
    CHECK(paramIndexById(d.id) == i, "indice por id %s", d.id);
  }
  for (int e = 0; e < E_DRUMS; ++e) for (int m = 0; m < 4; ++m) CHECK(std::string(macroLabel(e, m)).size() > 0, "macro %d/%d sin nombre", e, m);
}

static void test_presets() {
  std::printf("[presets]\n");
  const auto& L = presetList();
  CHECK(L.size() >= 40, "hacen falta 40+ presets, hay %zu", L.size());
  std::set<std::string> names; int perEngine[E_COUNT] = {};
  std::set<std::string> cats;
  for (const auto& p : L) {
    CHECK(names.insert(p.name).second, "nombre repetido %s", p.name.c_str());
    CHECK(!p.name.empty() && !p.category.empty(), "preset sin nombre/categoría");
    cats.insert(p.category);
    auto v = presetValues(p);
    for (int i = 0; i < P_COUNT; ++i) CHECK(v[i] >= paramTable()[i].min - 1e-6f && v[i] <= paramTable()[i].max + 1e-6f, "%s.%s fuera de rango (%g)", p.name.c_str(), paramTable()[i].id, v[i]);
    for (const auto& kv : p.values) CHECK(kv.first >= 0 && kv.first < P_COUNT, "id inválido en %s", p.name.c_str());
    ++perEngine[static_cast<int>(v[P_ENGINE] + 0.5f)];
  }
  for (int e = 0; e < E_COUNT; ++e) CHECK(perEngine[e] >= 4, "motor %d con %d presets (mínimo 4)", e, perEngine[e]);
  CHECK(cats.size() == 6, "categorías: %zu", cats.size());
}

static void test_pitch() {
  std::printf("[afinación]\n");
  for (double sr : {44100.0, 48000.0, 96000.0}) {
    struct Case { int engine; int note; double hz; const char* name; };
    Case cs[] = {{E_ORGAN, 69, 440.0, "organ"}, {E_PIANO, 69, 440.0, "piano"}, {E_STAB, 69, 440.0, "stab"}, {E_BASS, 45, 110.0, "bass"}};
    for (const auto& c : cs) {
      SchmuEngine e; e.prepare(sr); dry(e, c.engine);
      e.setParam(P_M2, 0.0f);   // sin pitch drop en el bajo
      if (c.engine == E_STAB) e.setParam(P_M3, 0.0f);  // unison mínimo
      if (c.engine == E_BASS) { e.setParam(P_M1, 0.0f); e.setParam(P_M2, 0.0f); }
      if (c.engine == E_ORGAN) { e.setParam(P_M1, 0.2f); e.setParam(P_M3, 0.0f); e.setParam(P_M4, 0.0f); }
      e.setParam(P_SUSTAIN, 1.0f);
      e.noteOn(c.note, 0.9f);
      Buf b; render(e, b, static_cast<int>(sr * 0.8));
      int from = static_cast<int>(sr * 0.3), to = static_cast<int>(sr * 0.8);
      double f = peakFreq(b.l, from, to, c.hz * 0.97, c.hz * 1.03, c.hz * 0.0005, sr);
      double cents = 1200.0 * std::log2(f / c.hz);
      CHECK(std::fabs(cents) < (c.engine == E_STAB ? 8.0 : 4.0), "%s @%g Hz: pico en %.2f Hz (%.1f cents)", c.name, sr, f, cents);
    }
  }
}

static void test_all_presets_render() {
  std::printf("[render de todos los presets]\n");
  for (double sr : {44100.0, 48000.0}) {
    for (const auto& p : presetList()) {
      SchmuEngine e; e.prepare(sr); applyPreset(e, p);
      auto v = presetValues(p);
      int eng = static_cast<int>(v[P_ENGINE] + 0.5f);
      std::vector<int> notes = eng == E_DRUMS ? std::vector<int>{36, 38, 39, 42, 46, 37}
                             : eng == E_BASS ? std::vector<int>{36, 43, 41}
                             : eng == E_CHOPS ? (v[P_CHOPMODE] > 0.5f ? std::vector<int>{36, 38, 41} : std::vector<int>{60, 64, 67}) : std::vector<int>{60, 63, 67, 70};
      Buf b;
      for (int k = 0; k < static_cast<int>(notes.size()); ++k) { e.noteOn(notes[k], 0.8f); render(e, b, static_cast<int>(sr * 0.12)); }
      render(e, b, static_cast<int>(sr * 0.3));
      for (int n : notes) e.noteOff(n);
      render(e, b, static_cast<int>(sr * 20));  // cola de FX
      CHECK(finiteAll(b), "%s @%g: NaN/Inf", p.name.c_str(), sr);
      float pk = peakOf(b);
      CHECK(pk > 0.02f, "%s @%g: casi mudo (pico %.4f)", p.name.c_str(), sr, pk);
      CHECK(pk <= 1.0f + 1e-6f, "%s @%g: pasa de 0 dBFS (%.3f)", p.name.c_str(), sr, pk);
      int n = static_cast<int>(b.l.size());
      double tail = rmsOf(b, n - static_cast<int>(sr), n);
      CHECK(tail < 1e-3, "%s @%g: no se apaga (RMS final %.5f)", p.name.c_str(), sr, tail);
      CHECK(e.activeVoices() == 0 || eng == E_CHOPS, "%s: quedaron %d voces activas", p.name.c_str(), e.activeVoices());
    }
  }
}

static void test_filter() {
  std::printf("[filtro]\n");
  auto hf = [&](float cutoff) {
    SchmuEngine e; e.prepare(44100); dry(e, E_STAB);
    e.setParam(P_SUSTAIN, 1.0f); e.setParam(P_CUTOFF, cutoff); e.setParam(P_M1, 0.0f);
    e.noteOn(48, 0.9f);
    Buf b; render(e, b, 44100);
    double s = 0; for (int h = 5; h <= 20; ++h) s += goertzel(b.l, 8000, 44100, 130.8128 * h, 44100);
    return s;
  };
  double open = hf(18000.0f), closed = hf(500.0f);
  CHECK(open / std::max(closed, 1e-12) > 10.0, "el filtro a 500 Hz debe bajar >20 dB los agudos (ratio %.1f)", open / std::max(closed, 1e-12));
  // la resonancia sube el pico cerca del corte
  auto peakAtCut = [&](float res) {
    SchmuEngine e; e.prepare(44100); dry(e, E_STAB);
    e.setParam(P_SUSTAIN, 1.0f); e.setParam(P_CUTOFF, 1200.0f); e.setParam(P_RESO, res); e.setParam(P_M1, 0.0f); e.setParam(P_M3, 0.0f);
    e.noteOn(36, 0.9f);   // 65 Hz: armónicos cada 65 Hz
    Buf b; render(e, b, 44100);
    return peakOf(b, 8000, 44100);
  };
  CHECK(peakAtCut(0.9f) > peakAtCut(0.0f) * 1.2f, "la resonancia debe resaltar el corte");
  // el envolvente de filtro mueve el corte
  SchmuEngine e; e.prepare(44100); dry(e, E_STAB);
  e.setParam(P_SUSTAIN, 1.0f); e.setParam(P_CUTOFF, 400.0f); e.setParam(P_ENVAMT, 1.0f); e.setParam(P_FDECAY, 0.15f);
  e.noteOn(48, 0.9f); Buf b; render(e, b, 44100);
  double early = 0, late = 0; for (int h = 5; h <= 20; ++h) { early += goertzel(b.l, 0, 4000, 130.8128 * h, 44100); late += goertzel(b.l, 30000, 44100, 130.8128 * h, 44100); }
  CHECK(early > late * 2.0, "el sweep del filtro debe cerrarse con el tiempo (%.4f vs %.4f)", early, late);
}

static void test_chords_and_voices() {
  std::printf("[chord memory y polifonía]\n");
  std::array<int, 6> ns{};
  int n = SchmuEngine::chordNotes(CH_MIN9, 60, ns);
  CHECK(n == 5 && ns[0] == 60 && ns[1] == 63 && ns[2] == 67 && ns[3] == 70 && ns[4] == 74, "Min9 sobre C");
  n = SchmuEngine::chordNotes(CH_DOM7, 55, ns);
  CHECK(n == 4 && ns[1] == 59 && ns[2] == 62 && ns[3] == 65, "Dom7 sobre G");
  n = SchmuEngine::chordNotes(CH_MIN11, 62, ns);
  CHECK(n == 6 && ns[5] == 79, "Min11 llega a la 11");
  n = SchmuEngine::chordNotes(CH_OFF, 40, ns);
  CHECK(n == 1 && ns[0] == 40, "Off = nota sola");
  n = SchmuEngine::chordNotes(CH_POWER, 125, ns);
  CHECK(ns[2] <= 127, "las notas se limitan a 127");
  for (const auto& c : chordTable()) CHECK(c.n >= 1 && c.n <= 6 && c.semis[0] == 0, "acorde %s", c.name);

  SchmuEngine e; e.prepare(44100); dry(e, E_STAB);
  e.setParam(P_CHORD, CH_MIN9); e.setParam(P_SUSTAIN, 1.0f);
  e.noteOn(60, 1.0f); Buf b; render(e, b, 4410);
  CHECK(e.activeVoices() == 5, "Min9 = 5 voces (hay %d)", e.activeVoices());
  e.noteOff(60); render(e, b, 44100);
  CHECK(e.activeVoices() == 0, "tras el release no quedan voces (%d)", e.activeVoices());
  // robo de voces
  SchmuEngine s; s.prepare(44100); dry(s, E_ORGAN); s.setParam(P_SUSTAIN, 1.0f);
  Buf sb;
  for (int i = 0; i < 60; ++i) { s.noteOn(36 + i, 0.8f); render(s, sb, 200); }
  CHECK(s.activeVoices() <= kMaxVoices, "polifonía acotada (%d)", s.activeVoices());
  CHECK(finiteAll(sb) && peakOf(sb) <= 1.0f, "sin NaN ni clipping con 60 notas");
}

static void test_bass_mono_glide() {
  std::printf("[bajo mono + glide]\n");
  SchmuEngine e; e.prepare(44100); dry(e, E_BASS);
  e.setParam(P_M1, 0.0f); e.setParam(P_M2, 0.0f); e.setParam(P_GLIDE, 0.12f); e.setParam(P_SUSTAIN, 1.0f);
  e.noteOn(36, 0.9f); Buf b; render(e, b, 22050);
  e.noteOn(48, 0.9f);   // legato: la anterior sigue apretada
  render(e, b, 4000);
  CHECK(e.activeVoices() == 1, "el bajo es monofónico (%d voces)", e.activeVoices());
  render(e, b, 44100);
  double f = peakFreq(b.l, b.l.size() - 22050, b.l.size(), 120.0, 140.0, 0.1, 44100);
  CHECK(std::fabs(f - 130.81) < 1.0, "tras el glide llega a C3 (%.2f Hz)", f);
  // durante el glide hay frecuencias intermedias (no es un salto)
  double mid = peakFreq(b.l, 22050 + 500, 22050 + 3000, 60.0, 140.0, 0.5, 44100);
  CHECK(mid > 66.0 && mid < 128.0, "durante el glide la frecuencia es intermedia (%.1f Hz)", mid);
  // soltar la nota superior con la inferior todavía apretada vuelve a ella
  e.noteOff(48); render(e, b, 44100);
  CHECK(e.activeVoices() == 1, "sigue sonando la nota apretada");
  double f2 = peakFreq(b.l, b.l.size() - 22050, b.l.size(), 60.0, 75.0, 0.05, 44100);
  CHECK(std::fabs(f2 - 65.41) < 0.8, "vuelve a C2 (%.2f Hz)", f2);
  // pitch drop: arranca más agudo que su nota
  SchmuEngine d; d.prepare(44100); dry(d, E_BASS);
  d.setParam(P_M1, 0.0f); d.setParam(P_M2, 0.5f); d.setParam(P_M3, 0.5f); d.setParam(P_SUSTAIN, 1.0f);
  d.noteOn(36, 0.9f); Buf db; render(d, db, 44100);
  double f0 = peakFreq(db.l, 200, 2500, 60.0, 400.0, 1.0, 44100);
  double f1 = peakFreq(db.l, 30000, 44100, 60.0, 70.0, 0.05, 44100);
  CHECK(f0 > f1 * 1.2, "el pitch drop empieza agudo (%.1f -> %.1f)", f0, f1);
  CHECK(std::fabs(f1 - 65.41) < 0.8, "y termina en la nota (%.2f)", f1);
}

static void test_drums() {
  std::printf("[batería]\n");
  for (int kit = 0; kit < 2; ++kit) {
    SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.setParam(P_KIT, static_cast<float>(kit)); e.setParam(P_KICKTUNE, 55.0f);
    e.noteOn(36, 1.0f); Buf b; render(e, b, 44100);
    double f = peakFreq(b.l, 4410, 13230, 35.0, 90.0, 0.25, 44100);
    CHECK(std::fabs(f - 55.0) < 6.0, "kit %d: el cuerpo del bombo ronda su tune (%.1f Hz)", kit, f);
    CHECK(peakOf(b, 0, 2000) > 0.2f, "kit %d: el bombo pega fuerte", kit);
    float early = peakOf(b, 2000, 6000), late = peakOf(b, 30000, 44100);
    CHECK(early > late * 3.0f, "kit %d: el bombo decae (%.3f -> %.3f)", kit, early, late);
  }
  // decay más largo = cola más larga
  auto tailPeak = [&](float dec) { SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.setParam(P_KICKDECAY, dec); e.noteOn(36, 1.0f); Buf b; render(e, b, 44100); return peakOf(b, 15000, 20000); };
  CHECK(tailPeak(1.2f) > tailPeak(0.15f) * 3.0f, "kick decay controla la cola");
  // timbres: ratio de energía de alta frecuencia (diferencia primera)
  auto hfRatio = [&](int note) {
    SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.noteOn(note, 1.0f); Buf b; render(e, b, 22050);
    double a = 0, d = 0; for (size_t i = 1; i < b.l.size(); ++i) { a += b.l[i] * b.l[i]; double x = b.l[i] - b.l[i - 1]; d += x * x; }
    return std::make_pair(a, d / std::max(a, 1e-12));
  };
  CHECK(hfRatio(36).first > 1.0, "bombo con energía");
  CHECK(hfRatio(36).second < 0.02, "bombo grave (hf %.4f)", hfRatio(36).second);
  for (int n : {38, 39, 37, 42, 46}) CHECK(hfRatio(n).first > 0.01, "nota %d suena", n);
  CHECK(hfRatio(42).second > 1.0, "hat cerrado brillante (hf %.2f)", hfRatio(42).second);
  CHECK(hfRatio(46).second > 1.0, "hat abierto brillante (hf %.2f)", hfRatio(46).second);
  CHECK(hfRatio(39).second > 0.1, "clap con contenido medio-agudo (hf %.2f)", hfRatio(39).second);
  // choke: el hat cerrado corta al abierto
  auto openTail = [&](bool choke) {
    SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.setParam(P_HATDECAY, 0.03f);
    e.noteOn(46, 1.0f); Buf b; render(e, b, 4410);
    if (choke) e.noteOn(42, 1.0f);
    render(e, b, 22050);
    return rmsOf(b, 4410 + 6000, 4410 + 22050);
  };
  CHECK(openTail(false) > openTail(true) * 3.0, "choke del hat abierto (%.5f vs %.5f)", openTail(false), openTail(true));
  // las notas fuera del mapa no suenan ni rompen nada
  SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.noteOn(100, 1.0f); Buf b; render(e, b, 4410);
  CHECK(peakOf(b) == 0.0f, "nota fuera del mapa = silencio");
  // 808 vs 909: el 808 dura más
  auto kickLen = [&](int kit) { SchmuEngine e; e.prepare(44100); dry(e, E_DRUMS); e.setParam(P_KIT, static_cast<float>(kit)); e.noteOn(36, 1.0f); Buf b; render(e, b, 44100); return rmsOf(b, 10000, 20000); };
  CHECK(kickLen(1) > kickLen(0), "el 808 sostiene más que el 909");
}

static void test_fx() {
  std::printf("[FX]\n");
  // tape: agrega armónicos
  auto thd = [&](float amount) {
    Tape t; t.prepare(44100);
    std::vector<float> x(44100), y(44100);
    for (int i = 0; i < 44100; ++i) { float s = 0.5f * static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / 44100.0)); float l = s, r = s; t.process(l, r, amount); y[i] = l; }
    double f1 = goertzel(y, 4410, 44100, 440, 44100), h = goertzel(y, 4410, 44100, 880, 44100) + goertzel(y, 4410, 44100, 1320, 44100);
    return h / std::max(f1, 1e-9);
  };
  CHECK(thd(0.0f) < 1e-4, "tape en 0 es transparente");
  CHECK(thd(0.8f) > 0.02, "tape en 0.8 agrega armónicos (%.4f)", thd(0.8f));
  // delay sincronizado: eco a 1/8 @ 120 bpm = 0.25 s
  {
    Delay d; d.prepare(48000);
    std::vector<float> out(48000);
    for (int i = 0; i < 48000; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; d.process(l, r, 1.0f, DV_8, 0.0f, 120.0); out[i] = l; }
    int idx = -1; for (int i = 100; i < 48000; ++i) if (std::fabs(out[i]) > 0.1f) { idx = i; break; }
    CHECK(idx >= 0 && std::abs(idx - 12000) <= 2, "eco del delay en la muestra %d (esperado 12000)", idx);
    CHECK(std::fabs(Delay::delaySeconds(DV_4, 120.0) - 0.5f) < 1e-6 && std::fabs(Delay::delaySeconds(DV_8D, 120.0) - 0.375f) < 1e-6, "divisiones de delay");
    CHECK(std::fabs(Delay::delaySeconds(DV_2, 90.0) - 1.3333f) < 1e-3, "1/2 a 90 bpm");
  }
  // delay: el feedback decae y no explota
  {
    Delay d; d.prepare(44100); double e0 = 0, e1 = 0;
    for (int i = 0; i < 44100 * 12; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; d.process(l, r, 1.0f, DV_16, 0.9f, 140.0); if (i < 44100 * 2) e0 += l * l; else if (i >= 44100 * 10) e1 += l * l; CHECK(std::isfinite(l), "delay NaN"); }
    CHECK(e1 < e0, "feedback 0.9 decae");
  }
  // reverb: estable y con cola
  {
    Reverb rv; rv.prepare(44100); double e0 = 0, e1 = 0; double t1 = 0;
    for (int i = 0; i < 44100 * 14; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; rv.process(l, r, 0.5f, 1.0f); if (i > 0 && i < 44100) e0 += l * l; if (i >= 44100 * 12) e1 += l * l; if (i >= 4410 && i < 44100) t1 += std::fabs(l); CHECK(std::isfinite(l), "reverb NaN"); }
    CHECK(e0 > 1e-6, "la reverb tiene cola (%.8f)", e0);
    CHECK(e1 < e0 * 1e-2, "la reverb decae (%.3g -> %.3g)", e0, e1);
  }
  // chorus: ensancha (L y R se vuelven distintos) y conserva nivel
  {
    Chorus c; c.prepare(44100); double diff = 0, ein = 0, eout = 0; Rng nz; nz.seed(5);
    for (int i = 0; i < 44100; ++i) { float s = 0.4f * nz.next(); float l = s, r = s; c.process(l, r, 1.0f, 0.8f); if (i > 8000) { diff += std::fabs(l - r); ein += s * s; eout += 0.5 * (l * l + r * r); } }
    CHECK(diff > 10.0, "el chorus abre el estéreo (%.2f)", diff);
    CHECK(eout / ein > 0.3 && eout / ein < 1.2, "el chorus conserva el nivel (%.2f)", eout / ein);
  }
  // limitador
  for (float x : {0.0f, 0.5f, 0.85f, 1.0f, 3.0f, 100.0f, -100.0f}) CHECK(std::fabs(softLimit(x)) <= 1.0f, "softLimit(%g)", x);
  CHECK(softLimit(0.3f) == 0.3f, "softLimit transparente por debajo de la rodilla");
}

static void test_swing() {
  std::printf("[swing]\n");
  CHECK(swingDelay16(0.0, 0.667) == 0.0, "el tiempo fuerte no se mueve");
  CHECK(std::fabs(swingDelay16(0.25, 0.5)) < 1e-12, "swing 50%% = recto");
  CHECK(std::fabs(swingDelay16(0.25, 0.6667) - 0.3334) < 1e-3, "la 2ª semicorchea se retrasa 1/3 con swing 66.7%% (%.4f)", swingDelay16(0.25, 0.6667));
  CHECK(swingDelay16(0.5, 0.7) == 0.0, "la 3ª semicorchea (corchea) no se mueve");
  CHECK(std::fabs(swingDelay16(0.75, 0.75) - 0.5) < 1e-9, "swing 75%% retrasa media semicorchea");
  CHECK(swingDelay16(0.125, 0.7) == 0.0, "una nota lejos de la grilla no se mueve");
  CHECK(std::fabs(swingDelay16(0.26, 0.6667) - 0.3334) < 1e-3, "nota apenas fuera de grilla igual se mueve");
  CHECK(swingDelay16(4.25, 0.6667) > 0.3 && swingDelay16(4.0, 0.6667) == 0.0, "compases siguientes");
  double s = swingDelaySamples(0.25, 120.0, 48000.0, 0.6667);
  CHECK(std::fabs(s - 2000.0) < 3.0, "a 120 bpm / 48k el retraso es ~2000 muestras (%.1f)", s);
  // cola
  SwingScheduler q;
  q.add(true, 60, 1.0f, 1000, 0.25, 120.0, 48000.0, 0.6667);   // contratiempo -> retrasada
  q.add(true, 62, 1.0f, 1000, 0.0, 120.0, 48000.0, 0.6667);    // tiempo fuerte -> igual
  q.add(false, 60, 0.0f, 5000, 0.5, 120.0, 48000.0, 0.6667);   // su note-off se retrasa lo mismo
  q.add(true, 64, 1.0f, 3000, -1.0, 120.0, 48000.0, 0.7);      // transporte parado -> sin swing
  SwingScheduler::Event ev;
  CHECK(q.pop(10000, ev) && ev.note == 62 && ev.due == 1000, "primero la nota sin retraso");
  CHECK(q.pop(10000, ev) && ev.note == 60 && ev.on && std::llabs(ev.due - 3000) <= 2, "la nota en contratiempo llega 2000 muestras después (%lld)", ev.due);
  CHECK(q.pop(10000, ev) && ev.note == 64 && ev.due == 3000, "transporte parado: sin swing");
  CHECK(q.pop(10000, ev) && ev.note == 60 && !ev.on && std::llabs(ev.due - 7000) <= 2, "su note-off también se retrasa 2000 (%lld)", ev.due);
  CHECK(!q.pop(10000, ev), "cola vacía");
}

static void test_chops() {
  std::printf("[chops]\n");
  auto def = makeDefaultChop(44100.0);
  CHECK(def && !def->empty() && def->length() == 44100, "chop por defecto");
  double pk = 0; for (float x : def->l) pk = std::max(pk, static_cast<double>(std::fabs(x)));
  CHECK(pk > 0.05 && pk < 1.0, "chop con nivel razonable (%.3f)", pk);
  auto len = [&](int note) {
    SchmuEngine e; e.prepare(44100); dry(e, E_CHOPS); e.setParam(P_SUSTAIN, 1.0f); e.setParam(P_RELEASE, 0.05f);
    e.noteOn(note, 1.0f); Buf b; render(e, b, 88200);
    int last = 0; for (size_t i = 0; i < b.l.size(); ++i) if (std::fabs(b.l[i]) > 1e-3f) last = static_cast<int>(i);
    return last;
  };
  int l60 = len(60), l72 = len(72);
  CHECK(l60 > 40000 && l60 < 46000, "C3 reproduce el sample a su velocidad (%d muestras)", l60);
  CHECK(std::abs(l72 - l60 / 2) < 2500, "una octava arriba dura la mitad (%d vs %d)", l72, l60);
  // slices
  SchmuEngine e; e.prepare(44100); dry(e, E_CHOPS); e.setParam(P_CHOPMODE, 1.0f); e.setParam(P_CHOPSLICES, 1.0f); e.setParam(P_SUSTAIN, 1.0f);
  e.noteOn(36, 1.0f); Buf b; render(e, b, 44100);
  int last = 0; for (size_t i = 0; i < b.l.size(); ++i) if (std::fabs(b.l[i]) > 1e-3f) last = static_cast<int>(i);
  CHECK(last > 4000 && last < 6500, "slice 1 de 8 dura 1/8 del sample (%d)", last);
  e.noteOn(100, 1.0f); Buf c; render(e, c, 4410);
  e.allNotesOff();
  SchmuEngine f; f.prepare(44100); dry(f, E_CHOPS); f.setParam(P_CHOPMODE, 1.0f); f.setParam(P_CHOPSLICES, 0.0f); f.noteOn(60, 1.0f); Buf z; render(f, z, 4410);
  CHECK(peakOf(z) == 0.0f, "una tecla fuera de las slices no suena");
  // reverse da otra señal
  SchmuEngine g; g.prepare(44100); dry(g, E_CHOPS); g.setParam(P_SUSTAIN, 1.0f); g.setParam(P_CHOPREV, 1.0f); g.noteOn(60, 1.0f); Buf r; render(g, r, 22050);
  SchmuEngine h; h.prepare(44100); dry(h, E_CHOPS); h.setParam(P_SUSTAIN, 1.0f); h.noteOn(60, 1.0f); Buf fw; render(h, fw, 22050);
  double d = 0; for (size_t i = 0; i < r.l.size(); ++i) d += std::fabs(r.l[i] - fw.l[i]);
  CHECK(d > 10.0 && peakOf(r) > 0.02f, "reverse suena distinto (%.1f)", d);
  // cargar un sample propio
  auto s = std::make_shared<SampleData>(); s->sampleRate = 44100; s->l.resize(22050); for (int i = 0; i < 22050; ++i) s->l[i] = 0.5f * static_cast<float>(std::sin(2.0 * kPi * 220.0 * i / 44100.0)); s->r = s->l;
  SchmuEngine k; k.prepare(44100); dry(k, E_CHOPS); k.setParam(P_SUSTAIN, 1.0f); k.setSample(s); k.noteOn(60, 1.0f);
  Buf kb; render(k, kb, 22050);
  double fq = peakFreq(kb.l, 2000, 18000, 200.0, 240.0, 0.2, 44100);
  CHECK(std::fabs(fq - 220.0) < 1.0, "el sample cargado suena a su frecuencia (%.2f Hz)", fq);
  // sample vacío no rompe
  k.setSample(std::make_shared<SampleData>()); Buf kz; render(k, kz, 2048); k.noteOn(60, 1.0f); render(k, kz, 2048);
  CHECK(finiteAll(kz), "sample vacío es seguro");
}

static void test_robustness() {
  std::printf("[robustez]\n");
  // determinismo
  auto run = []() {
    SchmuEngine e; e.prepare(44100); applyPreset(e, presetList()[0]);
    e.noteOn(60, 0.7f); Buf b; render(e, b, 8000); e.noteOn(67, 0.9f); render(e, b, 8000); e.noteOff(60); render(e, b, 8000);
    return b;
  };
  Buf a = run(), c = run();
  CHECK(a.l == c.l && a.r == c.r, "el render es determinista");
  // parámetros fuera de rango se acotan
  SchmuEngine e; e.prepare(44100);
  e.setParam(P_CUTOFF, -5.0f); CHECK(e.param(P_CUTOFF) == paramTable()[P_CUTOFF].min, "clamp inferior");
  e.setParam(P_CUTOFF, 1e9f); CHECK(e.param(P_CUTOFF) == paramTable()[P_CUTOFF].max, "clamp superior");
  // abuso: todo al máximo con 16 voces, saturación y reverb
  SchmuEngine x; x.prepare(48000); dry(x, E_STAB);
  for (int i = 0; i < P_COUNT; ++i) if (!paramTable()[i].choices && i != P_ENGINE) x.setParam(i, paramTable()[i].max);
  x.setParam(P_ENGINE, E_STAB); x.setParam(P_CHORD, CH_MIN11);
  Buf xb; for (int i = 0; i < 12; ++i) { x.noteOn(40 + i * 3, 1.0f); render(x, xb, 3000); }
  CHECK(finiteAll(xb) && peakOf(xb) <= 1.0f, "parámetros al máximo: sin NaN ni clipping (pico %.3f)", peakOf(xb));
  // cambio de motor en caliente + panic
  SchmuEngine y; y.prepare(44100); Buf yb;
  for (int eng = 0; eng < E_COUNT; ++eng) { y.setParam(P_ENGINE, static_cast<float>(eng)); y.noteOn(48, 1.0f); render(y, yb, 2000); y.allNotesOff(); render(y, yb, 500); }
  CHECK(finiteAll(yb), "cambiar de motor en caliente es seguro");
  // velocity 0 = note off
  SchmuEngine z; z.prepare(44100); z.setParam(P_SUSTAIN, 1.0f); z.noteOn(60, 1.0f); Buf zb; render(z, zb, 2000); z.noteOn(60, 0.0f); render(z, zb, 44100);
  CHECK(z.activeVoices() == 0, "velocity 0 libera la nota");
  // un solo bloque de tamaño 1 y de 4096
  SchmuEngine w; w.prepare(44100); w.noteOn(60, 1.0f); float l1 = 0, r1 = 0; for (int i = 0; i < 100; ++i) w.process(&l1, &r1, 1);
  std::vector<float> L(4096), R(4096); w.process(L.data(), R.data(), 4096);
  CHECK(std::isfinite(l1) && std::isfinite(L[100]), "tamaños de bloque extremos");
  // nivel de una nota sola: cómodo (ni mudo ni al palo)
  for (int eng : {E_ORGAN, E_PIANO, E_STAB, E_BASS}) {
    SchmuEngine n; n.prepare(44100); dry(n, eng); n.setParam(P_VOLUME, 0.8f); n.noteOn(eng == E_BASS ? 36 : 60, 1.0f); Buf nb; render(n, nb, 22050);
    float pk = peakOf(nb);
    CHECK(pk > 0.15f && pk < 0.98f, "motor %d: nivel de una nota a velocity máx = %.3f", eng, pk);
  }
}

int main() {
  auto t0 = std::chrono::steady_clock::now();
  test_params(); test_presets(); test_pitch(); test_filter(); test_chords_and_voices(); test_bass_mono_glide();
  test_drums(); test_fx(); test_swing(); test_chops(); test_robustness(); test_all_presets_render();
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("\n%d chequeos, %d fallas (%.1f s)\n", g_checks, g_fails, sec);
  return g_fails ? 1 : 0;
}
