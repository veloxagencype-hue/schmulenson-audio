// SCHMU 90s · render offline: un tema de garage de 8 compases armado SOLO con el propio synth
// (bombo+hats 909, bajo 2-step, stab m9, piano, organ). Sirve de demo, de referencia para los reels
// y de chequeo auditivo. Uso: schmu_render salida.wav [bpm]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../Source/dsp/Engine.h"
#include "../Source/dsp/Presets.h"

using namespace schmu;

static const Preset& byName(const char* n) {
  for (const auto& p : presetList()) if (p.name == n) return p;
  std::fprintf(stderr, "preset no encontrado: %s\n", n); std::exit(2);
}
static void load(SchmuEngine& e, const char* name, double sr, double bpm) {
  e.prepare(sr); e.setTempo(bpm);
  auto v = presetValues(byName(name));
  for (int i = 0; i < P_COUNT; ++i) e.setParam(i, v[i]);
}
static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::perror("wav"); std::exit(1); }
  uint32_t n = static_cast<uint32_t>(l.size()), dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, rate = sr, byteRate = sr * 4;
  uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
  std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
  std::fwrite(&fmtLen, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f);
  std::fwrite(&byteRate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
  std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f);
  for (uint32_t i = 0; i < n; ++i) {
    int16_t a = static_cast<int16_t>(std::lround(std::fmax(-1.0f, std::fmin(1.0f, l[i])) * 32767.0f));
    int16_t b = static_cast<int16_t>(std::lround(std::fmax(-1.0f, std::fmin(1.0f, r[i])) * 32767.0f));
    std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
  }
  std::fclose(f);
}

struct Ev { double step; int note; double len; float vel; };  // step en semicorcheas

int main(int argc, char** argv) {
  std::string out = argc > 1 ? argv[1] : "schmu90s_demo.wav";
  double bpm = argc > 2 ? std::atof(argv[2]) : 132.0;
  const double sr = 44100.0;
  const int bars = 8, steps = bars * 16;
  const double stepSec = 60.0 / bpm / 4.0;
  const int total = static_cast<int>((steps * stepSec + 4.0) * sr);

  struct Track { SchmuEngine e; std::vector<Ev> ev; float gain; };
  std::vector<Track> T(5);
  load(T[0].e, "909 Tight Garage", sr, bpm); T[0].gain = 0.95f;
  load(T[1].e, "2-Step Sub", sr, bpm);       T[1].gain = 0.9f;
  load(T[2].e, "Garage Stab m9", sr, bpm);   T[2].gain = 0.55f;
  load(T[3].e, "Classic House Piano", sr, bpm);   T[3].gain = 0.45f;
  load(T[4].e, "Classic House Organ", sr, bpm);   T[4].gain = 0.35f;

  // Progresión Cm9 - Ab maj7 - Bb - Gm7 (un compás cada una, 2 vueltas)
  const int roots[4] = {48, 44, 46, 43};
  for (int bar = 0; bar < bars; ++bar) {
    double b0 = bar * 16;
    int root = roots[bar % 4];
    // 2-step: bombo en 1 y en el "y" del 2 (semicorchea 6), caja en 4 y 12
    T[0].ev.push_back({b0 + 0, 36, 1, 1.0f}); T[0].ev.push_back({b0 + 6, 36, 1, 0.85f});
    T[0].ev.push_back({b0 + 10, 36, 1, 0.8f}); T[0].ev.push_back({b0 + 4, 38, 1, 0.9f}); T[0].ev.push_back({b0 + 12, 39, 1, 0.95f});
    for (int h = 0; h < 8; ++h) T[0].ev.push_back({b0 + h * 2 + 1.0 - (h % 2) * 1.0 + (h % 2 ? 0 : 0), 42, 1, h % 2 ? 0.7f : 0.45f});
    T[0].ev.push_back({b0 + 14, 46, 1, 0.6f});
    // bajo
    T[1].ev.push_back({b0 + 0, root - 12, 3, 1.0f}); T[1].ev.push_back({b0 + 6, root - 12, 2, 0.9f});
    T[1].ev.push_back({b0 + 10, root - 5, 2, 0.9f}); T[1].ev.push_back({b0 + 14, root - 12, 1.5, 0.8f});
    // stab m9 con el patrón "a contratiempo"
    T[2].ev.push_back({b0 + 3, root + 12, 1, 0.9f}); T[2].ev.push_back({b0 + 7, root + 12, 1, 0.8f}); T[2].ev.push_back({b0 + 11, root + 12, 1, 0.9f});
    // piano y organ: acordes largos a partir del 2º compás
    if (bar >= 2) { T[3].ev.push_back({b0 + 0, root + 12, 8, 0.7f}); T[3].ev.push_back({b0 + 3, root + 24, 1, 0.6f}); }
    if (bar >= 4) { T[4].ev.push_back({b0 + 0, root + 12, 6, 0.6f}); }
  }
  // chord memory del piano/organ para el demo
  T[3].e.setParam(P_CHORD, CH_MIN7); T[4].e.setParam(P_CHORD, CH_MIN9);
  T[0].e.setParam(P_SWING, 0.6f);  // (el swing real lo aplica el host; acá se simula en los eventos)

  std::vector<float> L(total, 0.0f), R(total, 0.0f);
  const double swing = 0.6;
  for (auto& t : T) {
    struct Item { long long at; bool on; int note; float vel; };
    std::vector<Item> items;
    for (const auto& ev : t.ev) {
      int si = static_cast<int>(ev.step);
      double delay16 = (si % 2 == 1) ? (swing - 0.5) * 2.0 : 0.0;
      long long on = static_cast<long long>((ev.step + delay16) * stepSec * sr);
      long long off = static_cast<long long>((ev.step + delay16 + ev.len * 0.9) * stepSec * sr);
      items.push_back({on, true, ev.note, ev.vel}); items.push_back({off, false, ev.note, 0});
    }
    std::vector<float> tl(total, 0.0f), tr(total, 0.0f);
    int pos = 0;
    std::vector<size_t> order(items.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return items[a].at < items[b].at; });
    size_t k = 0;
    while (pos < total) {
      long long next = total;
      if (k < order.size()) next = std::min<long long>(items[order[k]].at, total);
      if (next > pos) { t.e.process(&tl[pos], &tr[pos], static_cast<int>(next - pos)); pos = static_cast<int>(next); }
      while (k < order.size() && items[order[k]].at <= pos) { const auto& it = items[order[k]]; if (it.on) t.e.noteOn(it.note, it.vel); else t.e.noteOff(it.note); ++k; }
    }
    for (int i = 0; i < total; ++i) { L[i] += tl[i] * t.gain; R[i] += tr[i] * t.gain; }
  }
  float pk = 0; for (int i = 0; i < total; ++i) pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
  float g = pk > 0.9f ? 0.9f / pk : 1.0f;
  for (int i = 0; i < total; ++i) { L[i] *= g; R[i] *= g; }
  writeWav(out, L, R, static_cast<int>(sr));
  std::printf("listo: %s (%.1f s, %.0f bpm, pico %.2f)\n", out.c_str(), total / sr, bpm, pk * g);
  return 0;
}
