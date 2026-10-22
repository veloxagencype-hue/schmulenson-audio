// SCHMU Chopper · presets de fábrica (propios). Cada preset fija parámetros globales, los parámetros de TODOS los
// slices y, si corresponde, un patrón de 16 pasos. No incluyen ningún sample: funcionan con los chops que cargues.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "Chopper.h"

namespace schmu {

struct Preset {
  std::string name, category;
  std::vector<std::pair<int, float>> globals;   // (ParamId, valor); lo que no figure vuelve al valor por defecto
  SliceParams slice;                            // se aplica a los 16 slices
  std::array<int, 16> pattern{};                // slice por paso (-1 vacío); todo -1 = no toca el patrón
  std::array<float, 16> prob{};                 // probabilidad por paso (0 → 1.0)
  bool hasPattern = false;
};

inline std::array<float, P_COUNT> presetValues(const Preset& p) {
  std::array<float, P_COUNT> v{};
  for (int i = 0; i < P_COUNT; ++i) v[static_cast<size_t>(i)] = paramTable()[static_cast<size_t>(i)].def;
  for (const auto& kv : p.globals) v[static_cast<size_t>(kv.first)] = kv.second;
  return v;
}

inline std::array<Step, 16> presetSteps(const Preset& p) {
  std::array<Step, 16> s{};
  for (int i = 0; i < 16; ++i) {
    s[static_cast<size_t>(i)].slice = p.hasPattern ? p.pattern[static_cast<size_t>(i)] : -1;
    s[static_cast<size_t>(i)].prob = p.prob[static_cast<size_t>(i)] > 0 ? p.prob[static_cast<size_t>(i)] : 1.0f;
  }
  return s;
}

inline const std::vector<Preset>& presetList() {
  static const std::vector<Preset> L = [] {
    std::vector<Preset> v;
    auto sp = [](int mode, float a, float d, float s, float r, int filt = FT_OFF, float cut = 9000.0f) {
      SliceParams x; x.mode = mode; x.attack = a; x.decay = d; x.sustain = s; x.release = r; x.filter = filt; x.cutoff = cut; return x;
    };
    auto add = [&](const char* name, const char* cat, std::vector<std::pair<int, float>> g, SliceParams s, const std::array<int, 16>* pat = nullptr, const std::array<float, 16>* pr = nullptr) {
      Preset p; p.name = name; p.category = cat; p.globals = std::move(g); p.slice = s;
      p.pattern.fill(-1);
      if (pat) { p.pattern = *pat; p.hasPattern = true; }
      if (pr) p.prob = *pr;
      v.push_back(p);
    };
    const std::array<int, 16> four = {0, -1, 1, -1, 2, -1, 1, -1, 0, -1, 3, -1, 2, -1, 1, 4};
    const std::array<int, 16> stutter = {0, 0, -1, 0, 1, -1, 1, 1, 2, 2, -1, 2, 3, -1, 3, 3};
    const std::array<int, 16> garage = {0, -1, -1, 2, -1, 1, -1, -1, 3, -1, 0, -1, -1, 4, -1, 1};
    const std::array<int, 16> pulse = {0, -1, -1, -1, 1, -1, -1, 0, -1, -1, 2, -1, -1, -1, 3, -1};
    const std::array<float, 16> sparse = {1, 0, 0, 0, 0.8f, 0, 0, 0.6f, 0, 0, 0.7f, 0, 0, 0, 0.5f, 0};
    const std::array<int, 16> roll = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    // ---- Pads
    add("Init Chop", "Pads", {}, sp(PM_ONESHOT, 0.002f, 0.2f, 1.0f, 0.08f));
    add("Tight One-Shots", "Pads", {{P_VOLUME, 0.8f}}, sp(PM_ONESHOT, 0.001f, 0.1f, 1.0f, 0.03f));
    add("Gated Vox", "Pads", {{P_VOLUME, 0.8f}}, sp(PM_GATE, 0.002f, 0.2f, 1.0f, 0.06f));
    add("Lo-Fi Tape Vox", "Pads", {{P_TAPE, 0.55f}, {P_CRUSH, 0.12f}, {P_REVERBMIX, 0.12f}}, sp(PM_ONESHOT, 0.002f, 0.2f, 1.0f, 0.1f, FT_LP, 6500.0f));
    add("Crushed Choir", "Pads", {{P_CRUSH, 0.45f}, {P_REVERBMIX, 0.25f}, {P_REVERBSIZE, 0.7f}}, sp(PM_LOOP, 0.08f, 0.3f, 0.9f, 0.5f, FT_LP, 5200.0f));
    add("Reverse Tail Verb", "Pads", {{P_REVERBMIX, 0.35f}, {P_REVERBSIZE, 0.8f}}, [&] { auto x = sp(PM_ONESHOT, 0.004f, 0.3f, 1.0f, 0.25f); x.reverse = true; return x; }());
    add("Dub Echoes", "Pads", {{P_DELAYMIX, 0.4f}, {P_DELAYFB, 0.55f}, {P_DELAYDIV, 2}, {P_REVERBMIX, 0.15f}}, sp(PM_ONESHOT, 0.002f, 0.2f, 1.0f, 0.15f, FT_HP, 220.0f));
    // ---- Key
    add("Key Lead Vox", "Key", {{P_MODE, M_KEY}, {P_PITCHLOCK, 1}, {P_REVERBMIX, 0.12f}}, sp(PM_GATE, 0.01f, 0.2f, 1.0f, 0.2f));
    add("Formant Pad", "Key", {{P_MODE, M_KEY}, {P_PITCHLOCK, 1}, {P_REVERBMIX, 0.3f}, {P_REVERBSIZE, 0.75f}, {P_TAPE, 0.2f}}, sp(PM_LOOP, 0.25f, 0.5f, 0.85f, 0.9f, FT_LP, 4800.0f));
    add("Octave Down Drone", "Key", {{P_MODE, M_KEY}, {P_PITCHLOCK, 1}, {P_ROOTOFS, -12}, {P_REVERBMIX, 0.25f}}, sp(PM_LOOP, 0.15f, 0.4f, 0.9f, 0.7f, FT_LP, 3000.0f));
    add("Chipmunk Pitch Up", "Key", {{P_MODE, M_KEY}, {P_PITCHLOCK, 0}, {P_ROOTOFS, 7}}, sp(PM_ONESHOT, 0.001f, 0.2f, 1.0f, 0.08f));
    // ---- Sequence
    add("Minimal Pulse", "Sequence", {{P_MODE, M_SEQ}, {P_SEQLEN, 16}, {P_DELAYMIX, 0.22f}, {P_DELAYDIV, 3}, {P_HUMANIZE, 0.2f}}, sp(PM_ONESHOT, 0.001f, 0.15f, 1.0f, 0.05f), &pulse, &sparse);
    add("Garage Skip", "Sequence", {{P_MODE, M_SEQ}, {P_SWING, 0.62f}, {P_REVERBMIX, 0.12f}, {P_TAPE, 0.2f}}, sp(PM_ONESHOT, 0.001f, 0.15f, 1.0f, 0.06f), &garage);
    add("Gated Stutter", "Sequence", {{P_MODE, M_SEQ}, {P_SEQRATE, 2}, {P_TAPE, 0.15f}}, sp(PM_GATE, 0.001f, 0.1f, 1.0f, 0.02f), &stutter);
    add("Four Chops", "Sequence", {{P_MODE, M_SEQ}, {P_HUMANIZE, 0.15f}, {P_REVERBMIX, 0.1f}}, sp(PM_ONESHOT, 0.001f, 0.15f, 1.0f, 0.05f), &four);
    add("Machine Roll", "Sequence", {{P_MODE, M_SEQ}, {P_SEQRATE, 2}, {P_CRUSH, 0.2f}}, sp(PM_ONESHOT, 0.001f, 0.08f, 1.0f, 0.02f), &roll);
    return v;
  }();
  return L;
}

}  // namespace schmu
