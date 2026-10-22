// SCHMU Chopper · render de demostración a WAV (sin JUCE): arma un "pack" sintético, lo chopea solo y toca un patrón.
//   schmu_render salida.wav [bpm]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../Source/dsp/Chopper.h"
#include "../Source/dsp/Presets.h"

using namespace schmu;

static void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
  FILE* f = std::fopen(path, "wb");
  if (!f) { std::perror("wav"); std::exit(1); }
  auto u32 = [&](unsigned v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](unsigned short v) { std::fwrite(&v, 2, 1, f); };
  unsigned n = static_cast<unsigned>(l.size());
  std::fwrite("RIFF", 1, 4, f); u32(36 + n * 4); std::fwrite("WAVEfmt ", 1, 8, f); u32(16); u16(1); u16(2); u32(static_cast<unsigned>(sr)); u32(static_cast<unsigned>(sr) * 4); u16(4); u16(16);
  std::fwrite("data", 1, 4, f); u32(n * 4);
  for (unsigned i = 0; i < n; ++i) {
    short a = static_cast<short>(std::lround(std::max(-1.0f, std::min(1.0f, l[i])) * 32767.0f));
    short b = static_cast<short>(std::lround(std::max(-1.0f, std::min(1.0f, r[i])) * 32767.0f));
    std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
  }
  std::fclose(f);
}

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "chopper_demo.wav";
  double bpm = argc > 2 ? std::atof(argv[2]) : 126.0;
  const double SR = 44100.0;
  auto d = std::make_shared<SampleData>();
  d->sampleRate = SR; d->l.assign(static_cast<size_t>(4 * SR), 0.0f);
  for (int k = 0; k < 8; ++k) {   // 8 "sílabas" sintéticas (seno con vibrato y envolvente), solo para el demo
    int at = static_cast<int>((0.1 + 0.48 * k) * SR);
    double f = 220.0 * std::pow(2.0, (k * 3 % 12) / 12.0);
    for (int i = 0; i < static_cast<int>(0.4 * SR); ++i) {
      double t = i / SR;
      double env = std::min(1.0, t * 500.0) * std::exp(-5.0 * t);
      d->l[static_cast<size_t>(at + i)] += static_cast<float>(0.6 * env * (std::sin(kTwoPi * f * t + 0.3 * std::sin(kTwoPi * 5 * t)) + 0.3 * std::sin(kTwoPi * 2 * f * t)));
    }
  }
  d->r = d->l;
  Bank b; b.data = d; b.slices = autoChop(*d, 0.5f);
  std::printf("slices: %zu\n", b.slices.size());
  const Preset* pre = nullptr;
  for (const auto& p : presetList()) if (p.name == "Garage Skip") pre = &p;
  for (auto& s : b.slices) { s.p = pre->slice; s.root = 57; }
  b.steps = presetSteps(*pre);
  ChopEngine e; e.prepare(SR); e.setBank(b);
  auto vals = presetValues(*pre);
  for (int i = 0; i < P_COUNT; ++i) e.setParam(i, vals[static_cast<size_t>(i)]);
  std::vector<float> L, R; double ppq = 0;
  for (int blk = 0; blk < static_cast<int>(8.0 * SR / 512); ++blk) {
    std::vector<float> l(512), r(512);
    e.setHost(true, true, ppq, bpm); e.process(l.data(), r.data(), 512);
    L.insert(L.end(), l.begin(), l.end()); R.insert(R.end(), r.begin(), r.end());
    ppq += 512 * bpm / 60.0 / SR;
  }
  writeWav(out, L, R, static_cast<int>(SR));
  std::printf("escrito %s (%zu muestras)\n", out, L.size());
  return 0;
}
