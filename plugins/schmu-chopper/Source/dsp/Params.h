// SCHMU Chopper · tabla única de parámetros globales (los de cada slice viven en Chopper.h → SliceParams).
// La usan el motor, los presets, los tests y (para crear el AudioProcessorValueTreeState) el plugin.
#pragma once
#include <array>
#include <string>
#include <vector>

namespace schmu {

enum ParamId {
  P_VOLUME, P_MODE, P_SENS, P_PITCHLOCK, P_ROOTOFS, P_HUMANIZE, P_RESO,
  P_TAPE, P_CRUSH,
  P_DELAYMIX, P_DELAYDIV, P_DELAYFB,
  P_REVERBMIX, P_REVERBSIZE,
  P_SWING, P_SEQLEN, P_SEQRATE, P_TEMPO,
  P_COUNT
};

enum Mode { M_CHOP, M_KEY, M_SEQ, M_COUNT };

struct ParamDef {
  const char* id;       // identificador estable (presets y estado guardado del DAW dependen de él)
  const char* name;
  float min, max, def;
  float skew;           // 1 = lineal; <1 da más resolución en los valores bajos
  const char* choices;  // opciones separadas por '|' si es de lista; nullptr si es continuo
  const char* unit;
};

inline const std::array<ParamDef, P_COUNT>& paramTable() {
  static const std::array<ParamDef, P_COUNT> t = {{
    {"volume", "Volume", 0, 1, 0.8f, 1, nullptr, ""},
    {"mode", "Mode", 0, 2, 0, 1, "Chop|Key|Sequence", ""},
    {"sens", "Sensitivity", 0, 1, 0.5f, 1, nullptr, ""},
    {"pitchlock", "Keep Length", 0, 1, 1, 1, "Off|On", ""},
    {"rootofs", "Transpose", -12, 12, 0, 1, nullptr, "st"},
    {"humanize", "Humanize", 0, 1, 0, 1, nullptr, ""},
    {"reso", "Filter Reso", 0, 1, 0.15f, 1, nullptr, ""},
    {"tape", "Tape", 0, 1, 0, 1, nullptr, ""},
    {"crush", "Crush", 0, 1, 0, 1, nullptr, ""},
    {"delaymix", "Delay", 0, 1, 0, 1, nullptr, ""},
    {"delaydiv", "Delay Time", 0, 5, 1, 1, "1/16|1/8|1/8.|1/4|1/4.|1/2", ""},
    {"delayfb", "Feedback", 0, 0.9f, 0.35f, 1, nullptr, ""},
    {"reverbmix", "Reverb", 0, 1, 0, 1, nullptr, ""},
    {"reverbsize", "Size", 0, 1, 0.5f, 1, nullptr, ""},
    {"swing", "Swing", 0.5f, 0.75f, 0.5f, 1, nullptr, ""},
    {"seqlen", "Steps", 1, 16, 16, 1, nullptr, ""},
    {"seqrate", "Step Rate", 0, 2, 1, 1, "1/8|1/16|1/32", ""},
    {"tempo", "Tempo (sin host)", 60, 200, 126, 1, nullptr, ""},
  }};
  return t;
}

inline std::vector<std::string> choiceList(int param) {
  std::vector<std::string> out;
  const char* c = paramTable()[static_cast<size_t>(param)].choices;
  if (!c) return out;
  std::string cur;
  for (const char* p = c; ; ++p) {
    if (*p == '|' || *p == 0) { out.push_back(cur); cur.clear(); if (*p == 0) break; }
    else cur.push_back(*p);
  }
  return out;
}

// Pasos por negra según P_SEQRATE (1/8, 1/16, 1/32).
inline double stepsPerBeat(int rate) {
  static const double s[3] = {2.0, 4.0, 8.0};
  return s[rate < 0 ? 0 : (rate > 2 ? 2 : rate)];
}

}  // namespace schmu
