// SCHMU Chopper · tests de DSP (sin JUCE ni audio real: todo se genera y renderiza offline en memoria, con audio sintético).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../Source/dsp/Chopper.h"
#include "../Source/dsp/Presets.h"

using namespace schmu;

static int g_checks = 0, g_fails = 0;
#define CHECK(cond, ...) do { ++g_checks; if (!(cond)) { ++g_fails; std::printf("  FALLA [%s:%d] %s  -> ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static constexpr double SR = 44100.0;
struct Buf { std::vector<float> l, r; };

static std::shared_ptr<SampleData> mkSample(int n, double sr = SR) {
  auto d = std::make_shared<SampleData>();
  d->sampleRate = sr; d->l.assign(static_cast<size_t>(n), 0.0f); d->r = d->l;
  return d;
}
// ráfaga tonal con ataque rápido y caída exponencial
static void burst(SampleData& d, int at, double secs, double freq, float amp, double decay = 12.0) {
  int n = static_cast<int>(secs * d.sampleRate);
  for (int i = 0; i < n && at + i < d.length(); ++i) {
    double t = i / d.sampleRate;
    float env = static_cast<float>(std::min(1.0, t * 800.0) * std::exp(-decay * t));
    d.l[static_cast<size_t>(at + i)] += amp * env * static_cast<float>(std::sin(kTwoPi * freq * t));
  }
  d.r = d.l;
}
static std::shared_ptr<SampleData> sine(double freq, double secs, float amp = 0.5f) {
  auto d = mkSample(static_cast<int>(secs * SR));
  for (int i = 0; i < d->length(); ++i) d->l[static_cast<size_t>(i)] = amp * static_cast<float>(std::sin(kTwoPi * freq * i / SR));
  d->r = d->l;
  return d;
}
static Bank oneSlice(std::shared_ptr<SampleData> d, int root = 69) {
  Bank b; b.data = d;
  Slice s; s.start = 0; s.end = d->length(); s.root = root;
  b.slices.push_back(s);
  return b;
}
static void render(ChopEngine& e, Buf& b, int n, int block = 333) {
  size_t o = b.l.size();
  b.l.resize(o + static_cast<size_t>(n)); b.r.resize(o + static_cast<size_t>(n));
  int done = 0;
  while (done < n) { int m = std::min(block, n - done); e.process(&b.l[o + static_cast<size_t>(done)], &b.r[o + static_cast<size_t>(done)], m); done += m; }
}
static ChopEngine mkEngine(const Bank& b, int mode = M_CHOP) {
  ChopEngine e; e.prepare(SR); e.setParam(P_MODE, static_cast<float>(mode)); e.setParam(P_VOLUME, 1.0f); e.setBank(b);
  return e;
}
static double goertzel(const std::vector<float>& x, int from, int to, double f) {
  double w = 2.0 * kPi * f / SR, c = 2.0 * std::cos(w), s1 = 0, s2 = 0;
  for (int i = from; i < to; ++i) { double s0 = x[static_cast<size_t>(i)] + c * s1 - s2; s2 = s1; s1 = s0; }
  return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / std::max(1, to - from);
}
// frecuencia con más energía en [lo,hi] (paso fino de 0,5 cent) → en Hz
static double peakFreq(const std::vector<float>& x, int from, int to, double lo, double hi) {
  double best = 0, bf = lo;
  for (double f = lo; f <= hi; f *= std::pow(2.0, 0.5 / 1200.0)) { double g = goertzel(x, from, to, f); if (g > best) { best = g; bf = f; } }
  return bf;
}
static double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }
static double rmsOf(const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += static_cast<double>(x[static_cast<size_t>(i)]) * x[static_cast<size_t>(i)]; return std::sqrt(s / std::max(1, b - a)); }
static bool finiteAll(const Buf& b) { for (size_t i = 0; i < b.l.size(); ++i) if (!std::isfinite(b.l[i]) || !std::isfinite(b.r[i])) return false; return true; }
static float peakOf(const Buf& b) { float p = 0; for (size_t i = 0; i < b.l.size(); ++i) p = std::max(p, std::max(std::fabs(b.l[i]), std::fabs(b.r[i]))); return p; }

// ------------------------------------------------------------------ auto-chop
static void testAutoChop() {
  std::printf("auto-chop...\n");
  auto d = mkSample(static_cast<int>(3.0 * SR));
  const double at[5] = {0.20, 0.70, 1.20, 1.75, 2.30};
  const double fr[5] = {300, 440, 350, 520, 400};
  for (int i = 0; i < 5; ++i) burst(*d, static_cast<int>(at[i] * SR), 0.35, fr[i], 0.8f);
  auto on = detectOnsets(*d, 0.5f);
  CHECK(on.size() == 5, "onsets=%zu", on.size());
  for (size_t i = 0; i < on.size() && i < 5; ++i) CHECK(std::fabs(on[i] / SR - at[i]) < 0.015, "onset %zu en %.4f s, esperado %.2f", i, on[i] / SR, at[i]);
  CHECK(detectOnsets(*d, 0.5f) == on, "determinista");

  // sensibilidad: un golpe suave (-26 dB) aparece solo con sens alta
  auto d2 = mkSample(static_cast<int>(2.0 * SR));
  burst(*d2, static_cast<int>(0.2 * SR), 0.3, 440, 0.8f);
  burst(*d2, static_cast<int>(0.9 * SR), 0.3, 440, 0.012f);
  burst(*d2, static_cast<int>(1.5 * SR), 0.3, 440, 0.8f);
  size_t lo = detectOnsets(*d2, 0.0f).size(), hi = detectOnsets(*d2, 1.0f).size();
  CHECK(lo == 2 && hi == 3, "sens 0 → %zu (esp 2), sens 1 → %zu (esp 3)", lo, hi);

  // más de 16: se queda con 16, ordenados
  auto d3 = mkSample(static_cast<int>(10.0 * SR));
  for (int i = 0; i < 24; ++i) burst(*d3, static_cast<int>((0.2 + 0.4 * i) * SR), 0.2, 400.0 + 10 * i, 0.3f + 0.02f * i, 20.0);
  auto o3 = detectOnsets(*d3, 1.0f);
  CHECK(o3.size() == 16, "tope de 16: %zu", o3.size());
  CHECK(std::is_sorted(o3.begin(), o3.end()), "ordenados");

  auto sl = autoChop(*d, 0.5f);
  CHECK(sl.size() == 5, "slices=%zu", sl.size());
  bool contiguous = true;
  for (size_t i = 0; i + 1 < sl.size(); ++i) contiguous &= sl[i].end == sl[i + 1].start;
  CHECK(contiguous && sl.back().end <= d->length() && sl.back().end > sl.back().start + 1000, "slices contiguos");

  auto ev = evenChop(*d, 16);
  CHECK(ev.size() == 16 && ev.front().start == 0 && ev.back().end == d->length(), "evenChop 16");
  auto none = autoChop(*mkSample(2000), 0.5f);
  CHECK(none.size() <= 1, "silencio: %zu", none.size());

  // edición manual
  std::vector<Slice> v = ev;
  v[3].p.gainDb = -6.0f;
  CHECK(!insertMarker(v, d->length(), 5), "no inserta pegado al borde");
  v = {}; CHECK(insertMarker(v, 10000, 4000) && v.size() == 2 && v[0].end == 4000 && v[1].start == 4000 && v[1].end == 10000, "primer marcador");
  v[1].p.gainDb = -3.0f;
  CHECK(insertMarker(v, 10000, 7000) && v.size() == 3 && v[2].p.gainDb == -3.0f && v[1].end == 7000, "insertar copia los parámetros");
  CHECK(moveMarker(v, 1, 5000) && v[0].end == 5000 && v[1].start == 5000, "mover");
  CHECK(moveMarker(v, 1, 99999) && v[1].start <= v[1].end - 16, "mover se limita");
  CHECK(removeMarker(v, 1) && v.size() == 2 && v[0].end == v[1].start, "quitar");
  v = {};
  while (insertMarker(v, 100000, 100 + static_cast<int>(v.size()) * 5000)) {}
  CHECK(v.size() == static_cast<size_t>(kMaxSlices), "tope %zu", v.size());
}

// ------------------------------------------------------------------ tono y nombres de archivo
static void testPitchAndNames() {
  std::printf("tono y nombres...\n");
  CHECK(keyFromFilename("SCHMU_VC1_SYL_03_A_126.wav") == 9, "A");
  CHECK(keyFromFilename("C:\\x\\SCHMU_VC1_SHT_12_Fs_126.wav") == 6, "Fs");
  CHECK(keyFromFilename("/a/b/SCHMU_VC1_NTE_01_Gs_000.WAV") == 8, "Gs");
  CHECK(keyFromFilename("SCHMU_VC1_WHS_02_NOKEY_000.wav") == -1, "NOKEY");
  CHECK(keyFromFilename("cualquier_cosa.wav") == -1, "otro nombre");
  CHECK(bpmFromFilename("SCHMU_VC1_SYL_03_A_126.wav") == 126 && bpmFromFilename("x.wav") == 0, "bpm");

  const double fs[] = {110, 147, 196, 220, 261.63, 330, 440, 523.25};
  for (double f : fs) {
    auto s = sine(f, 0.5f);
    float got = detectF0(s->l.data(), s->length(), SR);
    CHECK(got > 0 && std::fabs(cents(got, f)) < 5.0, "f0 de %.1f Hz → %.2f (%.1f cents)", f, got, got > 0 ? cents(got, f) : 0);
  }
  // un seno + armónico: sigue dando la fundamental
  auto h = mkSample(static_cast<int>(0.5 * SR));
  for (int i = 0; i < h->length(); ++i) h->l[static_cast<size_t>(i)] = 0.4f * std::sin(kTwoPi * 220 * i / SR) + 0.3f * std::sin(kTwoPi * 440 * i / SR) + 0.2f * std::sin(kTwoPi * 660 * i / SR);
  h->r = h->l;
  float f0 = detectF0(h->l.data(), h->length(), SR);
  CHECK(std::fabs(cents(f0, 220)) < 5.0, "armónicos → %.2f Hz", f0);
  auto noise = mkSample(static_cast<int>(0.5 * SR)); Rng rn; rn.seed(3);
  for (auto& x : noise->l) x = 0.3f * rn.next();
  noise->r = noise->l;
  CHECK(detectF0(noise->l.data(), noise->length(), SR) == 0.0f, "ruido: sin tono");

  auto a3 = sine(220, 0.5f);   // A3 = 57
  CHECK(rootNoteFor(*a3, 0, a3->length(), 9) == 57, "root con key A: %d", rootNoteFor(*a3, 0, a3->length(), 9));
  CHECK(rootNoteFor(*a3, 0, a3->length(), -1) == 57, "root sin key: %d", rootNoteFor(*a3, 0, a3->length(), -1));
  auto a4 = sine(440 * 1.0, 0.5f);
  CHECK(rootNoteFor(*a4, 0, a4->length(), 9) == 69, "A4 → 69");
  CHECK(rootNoteFor(*a4, 0, a4->length(), 0) % 12 == 0, "se fuerza la clase de tono del nombre");
  CHECK(rootNoteFor(*noise, 0, noise->length(), 4) == 64, "sin tono: usa la clase del nombre cerca de C4 (%d)", rootNoteFor(*noise, 0, noise->length(), 4));
}

// ------------------------------------------------------------------ afinado y duración
static void testPitch() {
  std::printf("afinado (modo Key)...\n");
  auto s = sine(440, 2.0);
  for (int lock = 0; lock <= 1; ++lock) {
    for (int semis : {-12, -7, -3, 0, 3, 7, 12}) {
      Bank b = oneSlice(s, 69);
      b.slices[0].p.mode = PM_ONESHOT;
      ChopEngine e = mkEngine(b, M_KEY);
      e.setParam(P_PITCHLOCK, static_cast<float>(lock));
      e.noteOn(69 + semis, 1.0f);
      Buf out; render(e, out, static_cast<int>(0.9 * SR));
      double target = 440.0 * std::pow(2.0, semis / 12.0);
      double f = peakFreq(out.l, static_cast<int>(0.2 * SR), static_cast<int>(0.8 * SR), target * 0.97, target * 1.03);
      CHECK(std::fabs(cents(f, target)) < 5.0, "lock=%d %+d st: %.2f Hz vs %.2f (%.2f cents)", lock, semis, f, target, cents(f, target));
      CHECK(rmsOf(out.l, static_cast<int>(0.2 * SR), static_cast<int>(0.8 * SR)) > 0.1, "suena");
    }
  }
  // pitch por slice (±12) y root offset
  {
    Bank b = oneSlice(s, 69); b.slices[0].p.pitch = 5.0f;
    ChopEngine e = mkEngine(b, M_CHOP);
    e.setParam(P_PITCHLOCK, 1.0f);
    e.noteOn(kPadBase, 1.0f);
    Buf out; render(e, out, static_cast<int>(0.6 * SR));
    double target = 440.0 * std::pow(2.0, 5.0 / 12.0);
    CHECK(std::fabs(cents(peakFreq(out.l, 4000, 22000, target * 0.97, target * 1.03), target)) < 5.0, "pitch de slice +5 st");
    Bank b2 = oneSlice(s, 69);
    ChopEngine e2 = mkEngine(b2, M_KEY);
    e2.setParam(P_ROOTOFS, 12.0f); e2.setParam(P_PITCHLOCK, 0.0f);
    e2.noteOn(69, 1.0f);
    Buf o2; render(e2, o2, static_cast<int>(0.5 * SR));
    CHECK(std::fabs(cents(peakFreq(o2.l, 4000, 20000, 400, 1000), 880.0)) < 5.0, "transpose +12 → octava arriba");
  }
  std::printf("duración con pitch-lock...\n");
  for (int semis : {-12, 7, 12}) {
    Bank b = oneSlice(s, 69);
    ChopEngine e = mkEngine(b, M_KEY); e.setParam(P_PITCHLOCK, 1.0f);
    e.noteOn(69 + semis, 1.0f);
    Buf out; render(e, out, static_cast<int>(1.85 * SR));
    CHECK(e.activeVoices() == 1, "lock %+d: sigue sonando a 1,85 s", semis);
    render(e, out, static_cast<int>(0.3 * SR));
    CHECK(e.activeVoices() == 0, "lock %+d: terminó a ~2 s", semis);
    // amplitud estable (sin batidos por mala alineación de los granos)
    double mn = 1e9, mx = 0;
    for (int w = 4; w < 17; ++w) { double r = rmsOf(out.l, static_cast<int>(w * 0.1 * SR), static_cast<int>((w + 1) * 0.1 * SR)); mn = std::min(mn, r); mx = std::max(mx, r); }
    CHECK(20.0 * std::log10(mx / mn) < 2.0, "lock %+d: variación de nivel %.2f dB", semis, 20.0 * std::log10(mx / mn));
  }
  {   // sin lock, el repitch acorta/alarga
    Bank b = oneSlice(s, 69);
    ChopEngine e = mkEngine(b, M_KEY); e.setParam(P_PITCHLOCK, 0.0f);
    e.noteOn(81, 1.0f);   // +12 → la mitad
    Buf out; render(e, out, static_cast<int>(1.1 * SR));
    CHECK(e.activeVoices() == 0, "repitch +12 termina a ~1 s");
  }
  {   // las voces con lock mantienen el timbre de un chop con transiente: el ataque no se pierde
    auto d = mkSample(static_cast<int>(1.0 * SR));
    burst(*d, 4000, 0.5, 300, 0.8f, 8.0);
    Bank b = oneSlice(d, 62);
    ChopEngine e = mkEngine(b, M_KEY); e.setParam(P_PITCHLOCK, 1.0f);
    e.noteOn(62 + 5, 1.0f);
    Buf out; render(e, out, static_cast<int>(0.5 * SR));
    int firstLoud = -1;
    for (int i = 0; i < static_cast<int>(out.l.size()); ++i) if (std::fabs(out.l[static_cast<size_t>(i)]) > 0.1f) { firstLoud = i; break; }
    CHECK(firstLoud >= 2500 && firstLoud < 4600, "el ataque llega a tiempo (muestra %d)", firstLoud);
  }
}

// ------------------------------------------------------------------ loops, reverse, pads, choke, gate
static void testPlayback() {
  std::printf("loops y reverse...\n");
  auto s = sine(330.37, 0.31, 0.5f);
  for (int rev = 0; rev <= 1; ++rev) {
    for (int lock = 0; lock <= 1; ++lock) {
      Bank b = oneSlice(s, 64); b.slices[0].p.mode = PM_LOOP; b.slices[0].p.reverse = rev != 0;
      b.slices[0].p.attack = 0.001f; b.slices[0].p.release = 0.05f;
      ChopEngine e = mkEngine(b, M_CHOP);
      e.setParam(P_PITCHLOCK, static_cast<float>(lock));
      b.slices[0].p.pitch = lock ? 3.0f : 0.0f; e.setBank(b);
      e.noteOn(kPadBase, 1.0f);
      Buf out; render(e, out, static_cast<int>(3.0 * SR), 257);
      CHECK(e.activeVoices() == 1, "el loop sigue sonando");
      float maxJump = 0, lvlMin = 1e9f;
      for (size_t i = static_cast<size_t>(0.1 * SR) + 1; i < out.l.size(); ++i) maxJump = std::max(maxJump, std::fabs(out.l[i] - out.l[i - 1]));
      CHECK(maxJump < 0.06f, "rev=%d lock=%d: salto máximo entre muestras %.4f", rev, lock, maxJump);
      for (int w = 2; w < 29; ++w) lvlMin = std::min(lvlMin, static_cast<float>(rmsOf(out.l, static_cast<int>(w * 0.1 * SR), static_cast<int>((w + 1) * 0.1 * SR))));
      CHECK(lvlMin > 0.12f, "rev=%d lock=%d: nivel mínimo del loop %.3f", rev, lock, lvlMin);
      e.noteOff(kPadBase);
      render(e, out, static_cast<int>(0.2 * SR));
      CHECK(e.activeVoices() == 0, "note-off libera el loop");
    }
  }
  {   // reverse: una rampa se escucha de arriba hacia abajo
    auto r = mkSample(4000);
    for (int i = 0; i < 4000; ++i) r->l[static_cast<size_t>(i)] = static_cast<float>(i) / 4000.0f * 0.8f;
    r->r = r->l;
    Bank b = oneSlice(r, 60); b.slices[0].p.reverse = true; b.slices[0].p.attack = 0.0005f;
    ChopEngine e = mkEngine(b, M_CHOP); e.setParam(P_PITCHLOCK, 0.0f);
    e.noteOn(kPadBase, 1.0f);
    Buf out; render(e, out, 3000);
    CHECK(out.l[200] > out.l[1500] && out.l[1500] > out.l[2800], "reverse descendente: %.3f %.3f %.3f", out.l[200], out.l[1500], out.l[2800]);
  }
  std::printf("pads, gate, choke, pan, filtro...\n");
  auto d = mkSample(static_cast<int>(4.0 * SR));
  const double fr[4] = {220, 330, 440, 660};
  Bank b; b.data = d;
  for (int i = 0; i < 4; ++i) {
    burst(*d, static_cast<int>(i * SR), 0.9, fr[i], 0.7f, 2.0);
    Slice sl; sl.start = static_cast<int>(i * SR); sl.end = static_cast<int>((i + 1) * SR); sl.root = 57; b.slices.push_back(sl);
  }
  for (int i = 0; i < 4; ++i) {
    ChopEngine e = mkEngine(b, M_CHOP);
    e.noteOn(kPadBase + i, 1.0f);
    Buf out; render(e, out, static_cast<int>(0.6 * SR));
    double f = peakFreq(out.l, 2000, 20000, fr[i] * 0.9, fr[i] * 1.1);
    CHECK(std::fabs(cents(f, fr[i])) < 5.0, "pad %d suena su slice: %.1f Hz vs %.1f", i, f, fr[i]);
    // no suena nada de los otros
    for (int k = 0; k < 4; ++k) if (k != i) CHECK(goertzel(out.l, 2000, 20000, fr[k]) < 0.1 * goertzel(out.l, 2000, 20000, fr[i]), "pad %d sin fuga del slice %d", i, k);
  }
  { ChopEngine e = mkEngine(b, M_CHOP); e.noteOn(kPadBase + 9, 1.0f); Buf o; render(e, o, 4000); CHECK(peakOf(o) == 0.0f, "pad sin slice no suena"); }
  { ChopEngine e = mkEngine(b, M_CHOP); e.noteOn(kPadBase - 1, 1.0f); Buf o; render(e, o, 4000); CHECK(peakOf(o) == 0.0f, "nota fuera del mapa no suena"); }
  {   // gate vs one-shot
    Bank g = b; g.slices[0].p.mode = PM_GATE; g.slices[0].p.release = 0.02f; g.slices[1].p.mode = PM_ONESHOT;
    ChopEngine e = mkEngine(g, M_CHOP);
    e.noteOn(kPadBase, 1.0f); e.noteOn(kPadBase + 1, 1.0f);
    Buf o; render(e, o, 2000);
    e.noteOff(kPadBase); e.noteOff(kPadBase + 1);
    render(e, o, static_cast<int>(0.1 * SR));
    CHECK(e.activeVoices() == 1, "gate se corta con note-off, one-shot sigue (activas: %d)", e.activeVoices());
  }
  {   // choke
    Bank c = b; c.slices[0].p.choke = 2; c.slices[1].p.choke = 2; c.slices[2].p.choke = 0;
    ChopEngine e = mkEngine(c, M_CHOP);
    e.noteOn(kPadBase, 1.0f);
    Buf o; render(e, o, 4000);
    e.noteOn(kPadBase + 1, 1.0f); e.noteOn(kPadBase + 2, 1.0f);
    render(e, o, 2000);
    CHECK(e.activeVoices() == 2, "choke: mata solo al mismo grupo (activas: %d)", e.activeVoices());
    // el slice 0 ya no suena unos ms después
    render(e, o, 2000);
    double fz = goertzel(o.l, static_cast<int>(o.l.size()) - 1500, static_cast<int>(o.l.size()), 220);
    double g330 = goertzel(o.l, static_cast<int>(o.l.size()) - 1500, static_cast<int>(o.l.size()), 330);
    CHECK(fz < 0.25 * g330, "choke: 220 Hz apagado (%.4f vs 330 Hz %.4f)", fz, g330);
    // mismo slice se retrigea sin acumular voces
    ChopEngine e2 = mkEngine(c, M_CHOP);
    for (int i = 0; i < 6; ++i) { e2.noteOn(kPadBase + 3, 1.0f); Buf t; render(e2, t, 3000); }
    CHECK(e2.activeVoices() <= 2, "retrigger monofónico por slice (%d)", e2.activeVoices());
  }
  {   // pan, ganancia y filtro
    Bank p = b; p.slices[0].p.pan = -1.0f; p.slices[0].p.gainDb = -6.0f;
    ChopEngine e = mkEngine(p, M_CHOP); e.noteOn(kPadBase, 1.0f);
    Buf o; render(e, o, 8000);
    CHECK(rmsOf(o.r, 1000, 8000) < 0.001 && rmsOf(o.l, 1000, 8000) > 0.1, "pan izquierda");
    Bank q = b; ChopEngine e0 = mkEngine(q, M_CHOP); e0.noteOn(kPadBase, 1.0f); Buf o0; render(e0, o0, 8000);
    Bank q2 = b; q2.slices[0].p.gainDb = -6.0f; ChopEngine e1 = mkEngine(q2, M_CHOP); e1.noteOn(kPadBase, 1.0f); Buf o1; render(e1, o1, 8000);
    CHECK(std::fabs(20.0 * std::log10(rmsOf(o1.l, 1000, 8000) / rmsOf(o0.l, 1000, 8000)) + 6.0) < 0.3, "ganancia -6 dB");
    Bank lp = b; lp.slices[2].p.filter = FT_LP; lp.slices[2].p.cutoff = 200.0f;   // el slice 2 es de 440 Hz
    ChopEngine e3 = mkEngine(lp, M_CHOP); e3.noteOn(kPadBase + 2, 1.0f); Buf o3; render(e3, o3, 16000);
    Bank ref = b; ChopEngine e4 = mkEngine(ref, M_CHOP); e4.noteOn(kPadBase + 2, 1.0f); Buf o4; render(e4, o4, 16000);
    CHECK(rmsOf(o3.l, 4000, 16000) < 0.25 * rmsOf(o4.l, 4000, 16000), "filtro LP baja los 440 Hz");
    Bank hp = b; hp.slices[2].p.filter = FT_HP; hp.slices[2].p.cutoff = 3000.0f;
    ChopEngine e5 = mkEngine(hp, M_CHOP); e5.noteOn(kPadBase + 2, 1.0f); Buf o5; render(e5, o5, 16000);
    CHECK(rmsOf(o5.l, 4000, 16000) < 0.25 * rmsOf(o4.l, 4000, 16000), "filtro HP quita los 440 Hz");
  }
  {   // ADSR: attack lento
    Bank a = b; a.slices[0].p.attack = 0.2f;
    ChopEngine e = mkEngine(a, M_CHOP); e.noteOn(kPadBase, 1.0f);
    Buf o; render(e, o, static_cast<int>(0.4 * SR));
    CHECK(rmsOf(o.l, 0, 2000) < 0.3 * rmsOf(o.l, static_cast<int>(0.25 * SR), static_cast<int>(0.3 * SR)), "attack de 200 ms");
  }
}

// ------------------------------------------------------------------ secuenciador
static void testSequencer() {
  std::printf("secuenciador...\n");
  auto d = mkSample(static_cast<int>(2.0 * SR));
  Bank b; b.data = d;
  for (int i = 0; i < 4; ++i) {
    burst(*d, static_cast<int>(i * 0.5 * SR), 0.3, 300.0 + 100 * i, 0.8f, 6.0);
    Slice sl; sl.start = static_cast<int>(i * 0.5 * SR); sl.end = static_cast<int>((i + 1) * 0.5 * SR); b.slices.push_back(sl);
  }
  b.steps[0].slice = 0; b.steps[4].slice = 1; b.steps[8].slice = 2; b.steps[13].slice = 3;
  const double bpm = 120.0;   // 1/16 = 0.125 s = 5512,5 muestras
  const double stepSamp = SR * 60.0 / bpm / 4.0;

  auto firstLoud = [](const Buf& o, int from) { for (size_t i = static_cast<size_t>(from); i < o.l.size(); ++i) if (std::fabs(o.l[i]) > 0.02f) return static_cast<int>(i); return -1; };
  {   // posiciones exactas de disparo
    ChopEngine e = mkEngine(b, M_SEQ);
    e.setParam(P_VOLUME, 1.0f);
    e.setHost(true, true, 0.0, bpm);
    Buf o; int total = static_cast<int>(2.0 * SR);
    int done = 0; double ppq = 0;
    while (done < total) { int n = std::min(512, total - done); e.setHost(true, true, ppq, bpm); render(e, o, n, 512); done += n; ppq += n * bpm / 60.0 / SR; }
    int t0 = firstLoud(o, 0);
    CHECK(t0 >= 0 && t0 < 40, "paso 0 dispara al inicio (%d)", t0);
    // el paso 4 = 0,5 s: aparece un nuevo ataque de 400 Hz
    double f1 = peakFreq(o.l, static_cast<int>(0.52 * SR), static_cast<int>(0.60 * SR), 360, 440);
    CHECK(std::fabs(cents(f1, 400.0)) < 15.0, "paso 4 toca el slice 1 (%.1f Hz)", f1);
    double f3 = peakFreq(o.l, static_cast<int>((13 * 0.125 + 0.02) * SR), static_cast<int>((13 * 0.125 + 0.1) * SR), 560, 640);
    CHECK(std::fabs(cents(f3, 600.0)) < 15.0, "paso 13 toca el slice 3 (%.1f Hz)", f3);
    CHECK(e.triggerCount() == 4, "4 disparos en 1 compás (%lld)", e.triggerCount());
    (void)stepSamp;
  }
  {   // independiente del tamaño de bloque
    Buf a, c;
    for (int pass = 0; pass < 2; ++pass) {
      ChopEngine e = mkEngine(b, M_SEQ); e.setParam(P_TAPE, 0.2f); e.setParam(P_DELAYMIX, 0.2f); e.setParam(P_REVERBMIX, 0.1f);
      Buf& o = pass == 0 ? a : c;
      int blk = pass == 0 ? 64 : 509, done = 0, total = static_cast<int>(2.0 * SR); double ppq = 0;
      while (done < total) { int n = std::min(blk, total - done); e.setHost(true, true, ppq, bpm); render(e, o, n, 4096); done += n; ppq += n * bpm / 60.0 / SR; }
    }
    double worst = 0;   // el disparo puede variar 1 muestra según el bloque del host: se compara la envolvente (RMS cada 25 ms)
    for (int w = 0; w + 1100 < static_cast<int>(a.l.size()); w += 1100) {
      double ra = rmsOf(a.l, w, w + 1100), rc = rmsOf(c.l, w, w + 1100);
      if (ra > 1e-4 || rc > 1e-4) worst = std::max(worst, std::fabs(20.0 * std::log10((ra + 1e-9) / (rc + 1e-9))));
    }
    CHECK(worst < 0.3, "bloques de 64 y de 509 dan lo mismo (dif de nivel máx %.3f dB)", worst);
  }
  {   // swing: los pasos impares se retrasan
    Bank s2 = b; s2.steps.fill(Step{}); s2.steps[1].slice = 0; s2.steps[0].slice = 0;
    for (double sw : {0.5, 0.667}) {
      ChopEngine e = mkEngine(s2, M_SEQ); e.setParam(P_SWING, static_cast<float>(sw)); e.setParam(P_SEQLEN, 2.0f);
      Buf o; int done = 0, total = static_cast<int>(0.4 * SR); double ppq = 0;
      while (done < total) { int n = 256; e.setHost(true, true, ppq, bpm); render(e, o, n, 256); done += n; ppq += n * bpm / 60.0 / SR; }
      // el paso 1 cae en 0,125 s (+ swing·0,125); la detección busca el segundo ataque tras el primero
      int second = -1;
      for (int i = static_cast<int>(0.06 * SR); i < static_cast<int>(0.4 * SR) - 1; ++i) if (std::fabs(o.l[static_cast<size_t>(i)]) < 0.01f && std::fabs(o.l[static_cast<size_t>(i) + 1]) >= 0.01f && i > static_cast<int>(0.08 * SR)) { second = i; break; }
      (void)second;
      CHECK(e.triggerCount() >= 2, "swing %.3f: dispara", sw);
    }
    // medición directa con slice de impulso
    auto imp = mkSample(8000); imp->l[500] = 0.9f; imp->l[501] = -0.9f; imp->r = imp->l;
    Bank ib; ib.data = imp; Slice sl; sl.start = 0; sl.end = 8000; ib.slices.push_back(sl);
    ib.steps.fill(Step{}); ib.steps[0].slice = 0; ib.steps[1].slice = 0;
    int pos[2] = {0, 0};
    for (int k = 0; k < 2; ++k) {
      ChopEngine e = mkEngine(ib, M_SEQ); e.setParam(P_SWING, k ? 0.667f : 0.5f); e.setParam(P_SEQLEN, 2.0f); e.setParam(P_PITCHLOCK, 0.0f);
      Buf o; double ppq = 0; int done = 0;
      while (done < 12000) { e.setHost(true, true, ppq, bpm); render(e, o, 100, 100); done += 100; ppq += 100 * bpm / 60.0 / SR; }
      int cnt = 0;
      for (size_t i = 0; i + 1 < o.l.size(); ++i) if (o.l[i] > 0.15f && i > 3000 && cnt == 0) { pos[k] = static_cast<int>(i); ++cnt; }
    }
    double expectedShift = 0.667 - 0.5; (void)expectedShift;
    double delta = (pos[1] - pos[0]) / stepSamp;
    CHECK(std::fabs(delta - (0.667 - 0.5) * 2.0) < 0.02, "swing 66,7 %% retrasa el paso impar %.3f pasos (esp. %.3f)", delta, (0.667 - 0.5) * 2.0);
  }
  {   // probabilidad
    Bank pb = b; pb.steps.fill(Step{}); pb.steps[0].slice = 0; pb.steps[0].prob = 0.5f;
    ChopEngine e = mkEngine(pb, M_SEQ); e.setParam(P_SEQLEN, 1.0f); e.setParam(P_SEQRATE, 2.0f);
    Buf o; int total = static_cast<int>(40.0 * SR), done = 0; double ppq = 0;
    while (done < total) { e.setHost(true, true, ppq, bpm); render(e, o, 1024, 1024); o.l.clear(); o.r.clear(); done += 1024; ppq += 1024 * bpm / 60.0 / SR; }
    long long steps = static_cast<long long>(40.0 * bpm / 60.0 * 8.0);
    double frac = static_cast<double>(e.triggerCount()) / static_cast<double>(steps);
    CHECK(frac > 0.42 && frac < 0.58, "prob 50 %%: %.3f", frac);
    pb.steps[0].prob = 0.0001f; ChopEngine e2 = mkEngine(pb, M_SEQ);
    e2.setHost(true, true, 0, bpm); Buf o2; render(e2, o2, 44100, 512);
    CHECK(e2.triggerCount() == 0, "prob ~0 no dispara");
  }
  {   // transporte detenido = silencio; reloj interno cuando no hay host; salto de posición
    ChopEngine e = mkEngine(b, M_SEQ);
    e.setHost(true, false, 0.0, bpm); Buf o; render(e, o, 20000, 512);
    CHECK(peakOf(o) == 0.0f && e.triggerCount() == 0, "con el transporte parado no suena");
    ChopEngine f = mkEngine(b, M_SEQ); f.setParam(P_TEMPO, 120.0f);
    for (int i = 0; i < 100; ++i) { f.setHost(false, false, 0.0, 0.0); Buf t; render(f, t, 512, 512); }
    CHECK(f.triggerCount() >= 2, "reloj interno sin host (%lld)", f.triggerCount());
    ChopEngine g = mkEngine(b, M_SEQ);
    g.setHost(true, true, 0.0, bpm); Buf t1; render(g, t1, 512, 512);
    g.setHost(true, true, 7.0, bpm); Buf t2; render(g, t2, 512, 512);   // salto a 7 beats = paso 28 = 28 mod 16 = 12 (vacío) → próximo 13
    { double q = 7.0 + 512 * bpm / 60.0 / SR; Buf t3; for (int i = 0; i < 40; ++i) { g.setHost(true, true, q, bpm); render(g, t3, 512, 512); q += 512 * bpm / 60.0 / SR; } }
    CHECK(g.triggerCount() >= 2, "salto de posición sigue disparando (%lld)", g.triggerCount());
  }
  {   // en modo Chop el secuenciador no corre aunque haya transporte
    ChopEngine e = mkEngine(b, M_CHOP); e.setHost(true, true, 0.0, bpm); Buf o; render(e, o, 30000, 512);
    CHECK(e.triggerCount() == 0, "modo Chop ignora el patrón");
  }
}

// ------------------------------------------------------------------ presets, robustez, humanize, rendimiento
static void testPresetsAndRobustness() {
  std::printf("presets y robustez...\n");
  auto d = mkSample(static_cast<int>(4.0 * SR));
  Bank base; base.data = d;
  for (int i = 0; i < 16; ++i) {
    burst(*d, static_cast<int>(i * 0.25 * SR), 0.24, 200.0 + 25.0 * i, 0.7f, 8.0);
    Slice sl; sl.start = static_cast<int>(i * 0.25 * SR); sl.end = static_cast<int>((i + 1) * 0.25 * SR); sl.root = 52 + i % 12; base.slices.push_back(sl);
  }
  const auto& L = presetList();
  CHECK(L.size() >= 10, "presets: %zu", L.size());
  std::set<std::string> names; std::set<std::string> cats;
  for (const auto& p : L) { names.insert(p.name); cats.insert(p.category); }
  CHECK(names.size() == L.size(), "nombres únicos");
  CHECK(cats.size() >= 3, "categorías: %zu", cats.size());
  for (const auto& p : L) {
    Bank b = base;
    for (auto& s : b.slices) s.p = p.slice;
    b.steps = presetSteps(p);
    ChopEngine e = mkEngine(b);
    auto vals = presetValues(p);
    for (int i = 0; i < P_COUNT; ++i) e.setParam(i, vals[static_cast<size_t>(i)]);
    int mode = static_cast<int>(vals[P_MODE] + 0.5f);
    Buf o; double ppq = 0; int done = 0;
    if (mode == M_KEY) e.noteOn(60, 1.0f); else if (mode == M_CHOP) e.noteOn(kPadBase + 2, 1.0f);
    while (done < static_cast<int>(2.0 * SR)) { e.setHost(true, true, ppq, 126.0); render(e, o, 512, 512); done += 512; ppq += 512 * 126.0 / 60.0 / SR; }
    float pk = peakOf(o);
    CHECK(finiteAll(o) && pk > 0.02f && pk <= 1.0f, "preset '%s': pico %.3f finito=%d", p.name.c_str(), pk, finiteAll(o));
  }
  {   // esfuerzo: 16 pads a la vez, todo el FX, pitch-lock y loop
    Bank b = base;
    for (auto& s : b.slices) { s.p.mode = PM_LOOP; s.p.pitch = 7.0f; s.p.filter = FT_LP; s.p.cutoff = 1500.0f; }
    ChopEngine e = mkEngine(b);
    e.setParam(P_TAPE, 1.0f); e.setParam(P_CRUSH, 1.0f); e.setParam(P_DELAYMIX, 1.0f); e.setParam(P_DELAYFB, 0.9f); e.setParam(P_REVERBMIX, 1.0f); e.setParam(P_HUMANIZE, 1.0f); e.setParam(P_VOLUME, 1.0f);
    for (int i = 0; i < 16; ++i) e.noteOn(kPadBase + i, 1.0f);
    Buf o; auto t0 = std::chrono::steady_clock::now();
    render(e, o, static_cast<int>(10.0 * SR), 480);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    CHECK(finiteAll(o) && peakOf(o) <= 1.0f, "esfuerzo: finito y sin pasar de 0 dBFS (pico %.3f)", peakOf(o));
    std::printf("  rendimiento: 16 voces con pitch-lock y FX completos: %.1fx tiempo real\n", 10.0 / secs);
    CHECK(10.0 / secs > 2.0, "rendimiento %.1fx tiempo real", 10.0 / secs);
  }
  {   // humanize determinista y acotado
    Buf a, c;
    for (int pass = 0; pass < 2; ++pass) {
      ChopEngine e = mkEngine(base); e.setParam(P_HUMANIZE, 0.8f);
      for (int i = 0; i < 8; ++i) { e.noteOn(kPadBase + i, 1.0f); render(e, pass ? c : a, 2000, 500); }
    }
    bool same = a.l == c.l;
    CHECK(same, "humanize reproducible");
    ChopEngine e = mkEngine(base); e.setParam(P_HUMANIZE, 1.0f);
    e.noteOn(kPadBase, 1.0f); Buf o; render(e, o, 600);
    int first = -1; for (size_t i = 0; i < o.l.size(); ++i) if (std::fabs(o.l[i]) > 0.001f) { first = static_cast<int>(i); break; }
    CHECK(first >= 0 && first < static_cast<int>(0.0065 * SR) + 30, "humanize retrasa como mucho 6 ms (%d)", first);
  }
  {   // sin sample: silencio y sin caídas
    ChopEngine e; e.prepare(SR); e.noteOn(36, 1.0f); Buf o; render(e, o, 4000);
    CHECK(peakOf(o) == 0.0f, "sin sample no suena");
    Bank empty; empty.data = mkSample(0); ChopEngine e2 = mkEngine(empty); e2.noteOn(36, 1.0f); render(e2, o, 100);
    Bank noSlices; noSlices.data = sine(440, 0.2); ChopEngine e3 = mkEngine(noSlices, M_KEY); e3.noteOn(60, 1.0f); render(e3, o, 100);
    CHECK(true, "ok");
  }
  {   // otras frecuencias de muestreo: el sample de 48 k suena a su tono en un host de 44,1 k y viceversa
    auto s48 = std::make_shared<SampleData>(); s48->sampleRate = 48000.0;
    s48->l.resize(48000);
    for (int i = 0; i < 48000; ++i) s48->l[static_cast<size_t>(i)] = 0.5f * static_cast<float>(std::sin(kTwoPi * 440.0 * i / 48000.0));
    s48->r = s48->l;
    Bank b = oneSlice(s48, 69);
    ChopEngine e = mkEngine(b, M_KEY); e.setParam(P_PITCHLOCK, 1.0f);
    e.noteOn(69, 1.0f); Buf o; render(e, o, 20000);
    CHECK(std::fabs(cents(peakFreq(o.l, 4000, 18000, 400, 480), 440.0)) < 5.0, "sample de 48 kHz en host de 44,1 kHz");
  }
}

int main() {
  testAutoChop();
  testPitchAndNames();
  testPitch();
  testPlayback();
  testSequencer();
  testPresetsAndRobustness();
  std::printf("dsp_tests: %d chequeos, %d fallas\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
