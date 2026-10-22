// SCHMU 90s · datos de un sample para el slot de chops, y un chop vocal sintético de arranque.
#pragma once
#include <memory>
#include <vector>

#include "Common.h"

namespace schmu {

struct SampleData {
  std::vector<float> l, r;   // r puede ser igual a l (mono)
  double sampleRate = 44100;
  int length() const { return static_cast<int>(l.size()); }
  bool empty() const { return l.empty(); }
};

// Chop de arranque "ah/oh" generado por código (sierra glotal filtrada por tres formantes), para que el slot
// suene desde el primer momento aunque todavía no se haya cargado ninguno de los Vocal Chops de Schmulenson.
inline std::shared_ptr<SampleData> makeDefaultChop(double sr = 44100.0) {
  auto d = std::make_shared<SampleData>();
  d->sampleRate = sr;
  int n = static_cast<int>(sr * 1.0);
  d->l.assign(n, 0.0f);
  Rng rng; rng.seed(77);
  Svf f1, f2, f3;
  // 4 sílabas cortas: "ah", "oh", "eh", "ah" (formantes aproximados de voz cantada)
  static const float F[4][3] = {{800, 1150, 2900}, {500, 900, 2800}, {600, 1700, 2600}, {750, 1100, 2800}};
  double ph = 0;
  for (int syl = 0; syl < 4; ++syl) {
    int s0 = static_cast<int>(syl * sr * 0.25), s1 = static_cast<int>((syl + 1) * sr * 0.25);
    f1.set(F[syl][0], 0.8f, sr); f2.set(F[syl][1], 0.8f, sr); f3.set(F[syl][2], 0.7f, sr);
    float pitch = 196.0f * (syl == 1 ? 1.122f : (syl == 2 ? 1.26f : 1.0f));
    for (int i = s0; i < s1 && i < n; ++i) {
      ph += pitch / sr; if (ph >= 1.0) ph -= 1.0;
      float src = static_cast<float>(2.0 * ph - 1.0) + 0.03f * rng.next();
      float y = f1.bandpass(src) * 1.0f + f2.bandpass(src) * 0.7f + f3.bandpass(src) * 0.4f;
      float t = static_cast<float>(i - s0) / static_cast<float>(s1 - s0);
      float env = std::min(1.0f, t * 30.0f) * std::pow(1.0f - t, 0.8f);
      d->l[i] = y * env * 0.5f;
    }
  }
  float pk = 0; for (float x : d->l) pk = std::max(pk, std::fabs(x));
  if (pk > 0) for (float& x : d->l) x *= 0.8f / pk;   // normalizado a -2 dBFS
  d->r = d->l;
  return d;
}

}  // namespace schmu
