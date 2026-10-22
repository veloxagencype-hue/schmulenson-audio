// SCHMU 90s · tabla única de parámetros. La usan el motor, los presets, los tests y (para crear el
// AudioProcessorValueTreeState) el plugin: un solo lugar donde se define qué existe, su rango y su valor por defecto.
#pragma once
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace schmu {

enum ParamId {
  P_ENGINE, P_VOLUME,
  P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE,
  P_CUTOFF, P_RESO, P_ENVAMT, P_FDECAY,
  P_M1, P_M2, P_M3, P_M4,
  P_GLIDE, P_TUNE, P_SUB, P_CHORD,
  P_KIT, P_KICKTUNE, P_KICKDECAY, P_KICKPUNCH, P_SNARETONE, P_HATDECAY,
  P_CHOPMODE, P_CHOPSLICES, P_CHOPSTART, P_CHOPREV, P_CHOPLOOP,
  P_TAPE, P_CHORUS, P_CHORUSDEPTH,
  P_DELAYMIX, P_DELAYDIV, P_DELAYFB,
  P_REVERBMIX, P_REVERBSIZE,
  P_SWING,
  P_COUNT
};

enum Engine { E_ORGAN, E_PIANO, E_STAB, E_BASS, E_DRUMS, E_CHOPS, E_COUNT };

struct ParamDef {
  const char* id;       // identificador estable (presets y estado guardado del DAW dependen de él)
  const char* name;     // nombre visible
  float min, max, def;
  float skew;           // 1 = lineal; <1 da más resolución en los valores bajos (tiempos, frecuencias)
  const char* choices;  // opciones separadas por '|' si es un parámetro de lista; nullptr si es continuo
  const char* unit;
};

inline const std::array<ParamDef, P_COUNT>& paramTable() {
  static const std::array<ParamDef, P_COUNT> t = {{
    {"engine", "Engine", 0, 5, 0, 1, "Organ|Piano|Stab|Bass|Drums|Chops", ""},
    {"volume", "Volume", 0, 1, 0.8f, 1, nullptr, ""},
    {"attack", "Attack", 0.001f, 2.0f, 0.003f, 0.3f, nullptr, "s"},
    {"decay", "Decay", 0.005f, 4.0f, 0.4f, 0.35f, nullptr, "s"},
    {"sustain", "Sustain", 0, 1, 0.8f, 1, nullptr, ""},
    {"release", "Release", 0.005f, 4.0f, 0.15f, 0.35f, nullptr, "s"},
    {"cutoff", "Cutoff", 20.0f, 20000.0f, 18000.0f, 0.25f, nullptr, "Hz"},
    {"reso", "Resonance", 0, 1, 0.1f, 1, nullptr, ""},
    {"envamt", "Env Amount", -1, 1, 0, 1, nullptr, ""},
    {"fdecay", "Filter Decay", 0.01f, 3.0f, 0.3f, 0.35f, nullptr, "s"},
    {"m1", "Macro 1", 0, 1, 0.5f, 1, nullptr, ""},
    {"m2", "Macro 2", 0, 1, 0.3f, 1, nullptr, ""},
    {"m3", "Macro 3", 0, 1, 0.3f, 1, nullptr, ""},
    {"m4", "Macro 4", 0, 1, 0.0f, 1, nullptr, ""},
    {"glide", "Glide", 0, 0.5f, 0, 0.5f, nullptr, "s"},
    {"tune", "Tune", -12, 12, 0, 1, nullptr, "st"},
    {"sub", "Sub", 0, 1, 0, 1, nullptr, ""},
    {"chord", "Chord Memory", 0, 10, 0, 1, "Off|Min7|Maj7|Dom7|Min9|Dom9|Maj9|Min11|Sus4|m9 Open|Power", ""},
    {"kit", "Kit", 0, 1, 0, 1, "909|808", ""},
    {"kicktune", "Kick Tune", 35, 90, 52, 1, nullptr, "Hz"},
    {"kickdecay", "Kick Decay", 0.08f, 1.4f, 0.45f, 0.6f, nullptr, "s"},
    {"kickpunch", "Kick Punch", 0, 1, 0.5f, 1, nullptr, ""},
    {"snaretone", "Snare/Clap Tone", 0, 1, 0.5f, 1, nullptr, ""},
    {"hatdecay", "Hat Decay", 0.02f, 0.8f, 0.07f, 0.5f, nullptr, "s"},
    {"chopmode", "Chop Mode", 0, 1, 0, 1, "Keys|Slices", ""},
    {"chopslices", "Slices", 0, 2, 1, 1, "4|8|16", ""},
    {"chopstart", "Chop Start", 0, 1, 0, 1, nullptr, ""},
    {"choprev", "Reverse", 0, 1, 0, 1, "Off|On", ""},
    {"choploop", "Loop", 0, 1, 0, 1, "Off|On", ""},
    {"tape", "Tape", 0, 1, 0, 1, nullptr, ""},
    {"chorus", "Chorus", 0, 1, 0, 1, nullptr, ""},
    {"chorusdepth", "Chorus Depth", 0, 1, 0.5f, 1, nullptr, ""},
    {"delaymix", "Delay", 0, 1, 0, 1, nullptr, ""},
    {"delaydiv", "Delay Time", 0, 5, 2, 1, "1/16|1/8|1/8.|1/4|1/4.|1/2", ""},
    {"delayfb", "Delay Feedback", 0, 0.95f, 0.35f, 1, nullptr, ""},
    {"reverbmix", "Reverb", 0, 1, 0, 1, nullptr, ""},
    {"reverbsize", "Reverb Size", 0, 1, 0.5f, 1, nullptr, ""},
    {"swing", "Swing", 0.5f, 0.75f, 0.5f, 1, nullptr, ""},
  }};
  return t;
}

inline int paramIndexById(const char* id) {
  const auto& t = paramTable();
  for (int i = 0; i < P_COUNT; ++i) if (std::strcmp(t[i].id, id) == 0) return i;
  return -1;
}

inline std::vector<std::string> choiceList(int param) {
  std::vector<std::string> out;
  const char* c = paramTable()[param].choices;
  if (!c) return out;
  std::string cur;
  for (const char* p = c; ; ++p) {
    if (*p == '|' || *p == 0) { out.push_back(cur); cur.clear(); if (*p == 0) break; }
    else cur.push_back(*p);
  }
  return out;
}

// Chord memory: intervalos en semitonos desde la nota tocada (la fundamental).
struct ChordDef { const char* name; int n; std::array<int, 6> semis; };
inline const std::array<ChordDef, 11>& chordTable() {
  static const std::array<ChordDef, 11> c = {{
    {"Off",     1, {0, 0, 0, 0, 0, 0}},
    {"Min7",    4, {0, 3, 7, 10, 0, 0}},
    {"Maj7",    4, {0, 4, 7, 11, 0, 0}},
    {"Dom7",    4, {0, 4, 7, 10, 0, 0}},
    {"Min9",    5, {0, 3, 7, 10, 14, 0}},
    {"Dom9",    5, {0, 4, 7, 10, 14, 0}},
    {"Maj9",    5, {0, 4, 7, 11, 14, 0}},
    {"Min11",   6, {0, 3, 7, 10, 14, 17}},
    {"Sus4",    4, {0, 5, 7, 10, 0, 0}},
    {"m9 Open", 5, {0, 7, 10, 14, 15, 0}},
    {"Power",   3, {0, 7, 12, 0, 0, 0}},
  }};
  return c;
}

// Nombres de las 4 perillas "macro" según el motor (la UI los muestra; los tests los exigen completos).
inline const char* macroLabel(int engine, int macro) {
  static const char* L[E_COUNT][4] = {
    {"Draw", "Click", "Perc", "Vibrato"},
    {"Bright", "Tine", "Body", "Shimmer"},
    {"Wave", "Pulse Width", "Unison", "Air"},
    {"Wave", "Pitch Drop", "Drop Time", "Reese"},
    {"", "", "", ""},
    {"", "", "", ""},
  };
  return L[engine][macro];
}

}  // namespace schmu
