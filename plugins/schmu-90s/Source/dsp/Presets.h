// SCHMU 90s · presets de fábrica (propios). Cada preset parte de los valores por defecto y pisa solo lo que lista.
#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "Params.h"

namespace schmu {

struct Preset {
  std::string name, category;
  std::vector<std::pair<int, float>> values;
};

// Índices de la lista de chord memory (ver chordTable()).
enum { CH_OFF, CH_MIN7, CH_MAJ7, CH_DOM7, CH_MIN9, CH_DOM9, CH_MAJ9, CH_MIN11, CH_SUS4, CH_M9OPEN, CH_POWER };
// Índices de delaydiv.
enum { DV_16, DV_8, DV_8D, DV_4, DV_4D, DV_2 };

inline const std::vector<Preset>& presetList() {
  static const std::vector<Preset> L = {
    // ============ ORGAN (estilo organ de house de los 90: aditivo + click + percusión) ============
    {"Classic House Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .5f}, {P_M2, .35f}, {P_M3, .6f}, {P_M4, .15f}, {P_ATTACK, .003f}, {P_DECAY, .3f}, {P_SUSTAIN, .85f}, {P_RELEASE, .12f}, {P_CHORUS, .35f}, {P_REVERBMIX, .15f}}},
    {"Chicago Pluck Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .55f}, {P_M2, .5f}, {P_M3, 1.f}, {P_M4, 0.f}, {P_DECAY, .25f}, {P_SUSTAIN, .35f}, {P_RELEASE, .08f}, {P_DELAYMIX, .12f}, {P_DELAYDIV, DV_8D}, {P_REVERBMIX, .1f}}},
    {"Soft Gospel Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .15f}, {P_M2, .1f}, {P_M3, .2f}, {P_M4, .5f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .3f}, {P_CHORUS, .5f}, {P_REVERBMIX, .3f}}},
    {"Bright Reed Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .9f}, {P_M2, .2f}, {P_M3, .3f}, {P_M4, .1f}, {P_SUSTAIN, 1.f}, {P_CHORUS, .3f}, {P_CUTOFF, 9000.f}}},
    {"Organ Bass", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .1f}, {P_M2, .15f}, {P_M3, .3f}, {P_M4, 0.f}, {P_ATTACK, .002f}, {P_DECAY, .2f}, {P_SUSTAIN, .6f}, {P_RELEASE, .08f}, {P_CUTOFF, 1800.f}, {P_RESO, .15f}, {P_SUB, .4f}}},
    {"Deep Organ Bass", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, 0.f}, {P_M2, .1f}, {P_M3, .5f}, {P_ATTACK, .002f}, {P_DECAY, .35f}, {P_SUSTAIN, .5f}, {P_RELEASE, .1f}, {P_CUTOFF, 900.f}, {P_ENVAMT, .4f}, {P_FDECAY, .2f}, {P_SUB, .6f}, {P_TAPE, .2f}}},
    {"Dub Chord Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_CHORD, CH_MIN7}, {P_M1, .4f}, {P_M2, .2f}, {P_M3, .3f}, {P_DELAYMIX, .45f}, {P_DELAYDIV, DV_8D}, {P_DELAYFB, .5f}, {P_REVERBMIX, .3f}}},
    {"Piano-Organ Stab", "Organ", {{P_ENGINE, E_ORGAN}, {P_CHORD, CH_MIN9}, {P_M1, .6f}, {P_M2, .6f}, {P_M3, .8f}, {P_DECAY, .2f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .1f}, {P_CHORUS, .3f}}},
    {"Late Night Organ", "Organ", {{P_ENGINE, E_ORGAN}, {P_M1, .3f}, {P_M2, .15f}, {P_M3, .25f}, {P_M4, .35f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .5f}, {P_TAPE, .3f}, {P_CHORUS, .5f}, {P_REVERBMIX, .35f}}},

    // ============ PIANO (FM brillante estilo piano de house, con chorus) ============
    {"Classic House Piano", "Piano", {{P_ENGINE, E_PIANO}, {P_M1, .6f}, {P_M2, .35f}, {P_M3, .45f}, {P_M4, .4f}, {P_ATTACK, .002f}, {P_DECAY, 1.2f}, {P_SUSTAIN, .2f}, {P_RELEASE, .35f}, {P_CHORUS, .45f}, {P_REVERBMIX, .2f}}},
    {"Classic Rave Piano", "Piano", {{P_ENGINE, E_PIANO}, {P_M1, .8f}, {P_M2, .5f}, {P_M3, .35f}, {P_M4, .5f}, {P_DECAY, .8f}, {P_SUSTAIN, .1f}, {P_CHORUS, .3f}, {P_DELAYMIX, .15f}, {P_DELAYDIV, DV_8D}}},
    {"Soft Keys", "Piano", {{P_ENGINE, E_PIANO}, {P_M1, .25f}, {P_M2, .15f}, {P_M3, .6f}, {P_M4, .5f}, {P_DECAY, 2.f}, {P_SUSTAIN, .35f}, {P_RELEASE, .5f}, {P_TAPE, .15f}, {P_REVERBMIX, .3f}}},
    {"Tine Keys", "Piano", {{P_ENGINE, E_PIANO}, {P_M1, .4f}, {P_M2, .8f}, {P_M3, .5f}, {P_M4, .6f}, {P_DECAY, 1.4f}, {P_SUSTAIN, .25f}, {P_CHORUS, .6f}, {P_CHORUSDEPTH, .6f}, {P_REVERBMIX, .2f}}},
    {"Stab Piano m7", "Piano", {{P_ENGINE, E_PIANO}, {P_CHORD, CH_MIN7}, {P_M1, .7f}, {P_M2, .4f}, {P_M3, .3f}, {P_DECAY, .35f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .2f}, {P_CHORUS, .3f}}},
    {"Dream Piano 9th", "Piano", {{P_ENGINE, E_PIANO}, {P_CHORD, CH_MIN9}, {P_M1, .35f}, {P_M2, .2f}, {P_M3, .7f}, {P_RELEASE, .8f}, {P_SUSTAIN, .3f}, {P_DECAY, 2.f}, {P_REVERBMIX, .5f}, {P_DELAYMIX, .3f}, {P_DELAYDIV, DV_4D}}},
    {"Hard Chord Piano", "Piano", {{P_ENGINE, E_PIANO}, {P_CHORD, CH_DOM7}, {P_M1, 1.f}, {P_M2, .6f}, {P_M3, .3f}, {P_DECAY, .5f}, {P_SUSTAIN, .05f}, {P_TAPE, .25f}}},
    {"Lo-fi Piano", "Piano", {{P_ENGINE, E_PIANO}, {P_M1, .3f}, {P_M2, .2f}, {P_M3, .5f}, {P_M4, .3f}, {P_TAPE, .7f}, {P_CUTOFF, 4000.f}, {P_CHORUS, .5f}, {P_REVERBMIX, .2f}}},

    // ============ STAB (saw/pulse poly + filtro resonante + chord memory) ============
    {"Garage Stab m9", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_MIN9}, {P_M1, .3f}, {P_M2, .5f}, {P_M3, .5f}, {P_M4, 0.f}, {P_ATTACK, .003f}, {P_DECAY, .35f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .2f}, {P_CUTOFF, 1800.f}, {P_RESO, .25f}, {P_ENVAMT, .6f}, {P_FDECAY, .25f}, {P_DELAYMIX, .25f}, {P_DELAYDIV, DV_8}, {P_DELAYFB, .35f}, {P_REVERBMIX, .2f}, {P_CHORUS, .3f}}},
    {"Classic House Stab", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_MIN7}, {P_M1, 0.f}, {P_M2, .5f}, {P_M3, .4f}, {P_DECAY, .2f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .12f}, {P_CUTOFF, 2500.f}, {P_RESO, .3f}, {P_ENVAMT, .7f}, {P_FDECAY, .18f}, {P_CHORUS, .4f}, {P_REVERBMIX, .15f}}},
    {"Rave Stab Power", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_POWER}, {P_M1, .6f}, {P_M2, .4f}, {P_M3, .8f}, {P_DECAY, .3f}, {P_SUSTAIN, .15f}, {P_CUTOFF, 3500.f}, {P_RESO, .35f}, {P_ENVAMT, .8f}, {P_FDECAY, .22f}, {P_TAPE, .3f}, {P_DELAYMIX, .3f}, {P_DELAYDIV, DV_8D}}},
    {"Sus4 Pad Stab", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_SUS4}, {P_M1, .2f}, {P_M3, .6f}, {P_ATTACK, .02f}, {P_DECAY, .6f}, {P_SUSTAIN, .5f}, {P_RELEASE, .5f}, {P_CUTOFF, 1500.f}, {P_RESO, .2f}, {P_ENVAMT, .3f}, {P_CHORUS, .5f}, {P_REVERBMIX, .4f}}},
    {"2-Step Pluck", "Stab", {{P_ENGINE, E_STAB}, {P_M1, .5f}, {P_M2, .6f}, {P_M3, .4f}, {P_DECAY, .15f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .1f}, {P_CUTOFF, 1200.f}, {P_RESO, .4f}, {P_ENVAMT, .8f}, {P_FDECAY, .12f}, {P_DELAYMIX, .35f}, {P_DELAYDIV, DV_8D}, {P_REVERBMIX, .25f}}},
    {"m9 Open Voicing", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_M9OPEN}, {P_M1, .2f}, {P_M3, .5f}, {P_DECAY, .45f}, {P_SUSTAIN, .1f}, {P_RELEASE, .3f}, {P_CUTOFF, 2200.f}, {P_ENVAMT, .5f}, {P_FDECAY, .3f}, {P_REVERBMIX, .3f}, {P_DELAYMIX, .2f}, {P_DELAYDIV, DV_8D}}},
    {"Maj9 Sunrise", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_MAJ9}, {P_M1, .1f}, {P_M3, .5f}, {P_ATTACK, .01f}, {P_DECAY, .8f}, {P_SUSTAIN, .4f}, {P_RELEASE, .6f}, {P_CUTOFF, 3000.f}, {P_ENVAMT, .3f}, {P_CHORUS, .6f}, {P_REVERBMIX, .45f}}},
    {"Dirty Garage Chord", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_DOM9}, {P_M1, .4f}, {P_M3, .9f}, {P_DECAY, .25f}, {P_SUSTAIN, 0.f}, {P_CUTOFF, 1400.f}, {P_RESO, .5f}, {P_ENVAMT, .9f}, {P_FDECAY, .2f}, {P_TAPE, .6f}, {P_SUB, .3f}}},
    {"Brass-ish Stab", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_DOM7}, {P_M1, 0.f}, {P_M3, .5f}, {P_ATTACK, .02f}, {P_DECAY, .3f}, {P_SUSTAIN, .5f}, {P_RELEASE, .15f}, {P_CUTOFF, 800.f}, {P_RESO, .2f}, {P_ENVAMT, 1.f}, {P_FDECAY, .2f}, {P_CHORUS, .3f}}},
    {"Minor 11 Stack", "Stab", {{P_ENGINE, E_STAB}, {P_CHORD, CH_MIN11}, {P_M1, .15f}, {P_M3, .55f}, {P_DECAY, .5f}, {P_SUSTAIN, .2f}, {P_RELEASE, .4f}, {P_CUTOFF, 2000.f}, {P_ENVAMT, .45f}, {P_REVERBMIX, .4f}, {P_CHORUS, .4f}}},

    // ============ BASS (2-step / garage: sub, reese, pitch drop, glide) ============
    {"2-Step Sub", "Bass", {{P_ENGINE, E_BASS}, {P_M1, 0.f}, {P_M2, .35f}, {P_M3, .3f}, {P_M4, 0.f}, {P_ATTACK, .003f}, {P_DECAY, .6f}, {P_SUSTAIN, .7f}, {P_RELEASE, .12f}, {P_CUTOFF, 6000.f}, {P_TAPE, .15f}}},
    {"Garage Wobble Reese", "Bass", {{P_ENGINE, E_BASS}, {P_M1, 1.f}, {P_M2, 0.f}, {P_M4, .6f}, {P_SUSTAIN, .8f}, {P_CUTOFF, 900.f}, {P_RESO, .3f}, {P_ENVAMT, .5f}, {P_FDECAY, .35f}, {P_TAPE, .3f}, {P_CHORUS, .2f}}},
    {"808 Sub Sine", "Bass", {{P_ENGINE, E_BASS}, {P_M1, 0.f}, {P_M2, .5f}, {P_M3, .5f}, {P_ATTACK, .002f}, {P_DECAY, 1.2f}, {P_SUSTAIN, .3f}, {P_RELEASE, .3f}, {P_CUTOFF, 5000.f}, {P_TAPE, .2f}}},
    {"Glide Bass", "Bass", {{P_ENGINE, E_BASS}, {P_GLIDE, .09f}, {P_M1, .3f}, {P_M2, 0.f}, {P_SUSTAIN, .85f}, {P_CUTOFF, 3000.f}, {P_RESO, .2f}}},
    {"Bounce Bass", "Bass", {{P_ENGINE, E_BASS}, {P_M1, .5f}, {P_M2, .2f}, {P_M3, .15f}, {P_DECAY, .25f}, {P_SUSTAIN, .2f}, {P_CUTOFF, 1400.f}, {P_ENVAMT, .5f}, {P_FDECAY, .2f}, {P_RESO, .35f}}},
    {"Reese Dub", "Bass", {{P_ENGINE, E_BASS}, {P_M1, 1.f}, {P_M2, 0.f}, {P_M4, 1.f}, {P_SUSTAIN, .9f}, {P_CUTOFF, 700.f}, {P_RESO, .45f}, {P_CHORUS, .3f}, {P_TAPE, .4f}, {P_REVERBMIX, .08f}}},
    {"Pluck Sub", "Bass", {{P_ENGINE, E_BASS}, {P_M1, .2f}, {P_M2, .1f}, {P_ATTACK, .002f}, {P_DECAY, .28f}, {P_SUSTAIN, 0.f}, {P_RELEASE, .1f}, {P_CUTOFF, 4000.f}}},
    {"Fat Saw Bass", "Bass", {{P_ENGINE, E_BASS}, {P_M1, .8f}, {P_M4, .3f}, {P_SUSTAIN, .6f}, {P_CUTOFF, 1800.f}, {P_ENVAMT, .6f}, {P_FDECAY, .25f}, {P_TAPE, .25f}}},

    // ============ DRUMS (síntesis modelada estilo 909 / 808) ============
    {"909 House Kit", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 0.f}, {P_KICKTUNE, 52.f}, {P_KICKDECAY, .45f}, {P_KICKPUNCH, .6f}, {P_SNARETONE, .55f}, {P_HATDECAY, .07f}, {P_TAPE, .2f}}},
    {"909 Tight Garage", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 0.f}, {P_KICKTUNE, 58.f}, {P_KICKDECAY, .3f}, {P_KICKPUNCH, .8f}, {P_SNARETONE, .7f}, {P_HATDECAY, .05f}, {P_REVERBMIX, .1f}, {P_SWING, .62f}}},
    {"808 Deep Kit", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 1.f}, {P_KICKTUNE, 45.f}, {P_KICKDECAY, 1.f}, {P_KICKPUNCH, .3f}, {P_SNARETONE, .4f}, {P_HATDECAY, .1f}}},
    {"808 2-Step Kit", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 1.f}, {P_KICKTUNE, 50.f}, {P_KICKDECAY, .6f}, {P_KICKPUNCH, .45f}, {P_SNARETONE, .6f}, {P_HATDECAY, .06f}, {P_SWING, .6f}, {P_DELAYMIX, .1f}}},
    {"Dusty Break Kit", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 0.f}, {P_KICKTUNE, 60.f}, {P_KICKDECAY, .35f}, {P_KICKPUNCH, .5f}, {P_SNARETONE, .8f}, {P_HATDECAY, .09f}, {P_TAPE, .7f}, {P_SWING, .58f}}},
    {"Rave 909 Hard", "Drums", {{P_ENGINE, E_DRUMS}, {P_KIT, 0.f}, {P_KICKTUNE, 48.f}, {P_KICKDECAY, .6f}, {P_KICKPUNCH, 1.f}, {P_SNARETONE, .5f}, {P_HATDECAY, .12f}, {P_TAPE, .4f}, {P_REVERBMIX, .08f}}},

    // ============ CHOPS (slot de samples: Vocal Chops de Schmulenson) ============
    {"Vocal Chop Keys", "Chops", {{P_ENGINE, E_CHOPS}, {P_CHOPMODE, 0.f}, {P_ATTACK, .002f}, {P_DECAY, 2.f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .15f}, {P_CHORUS, .2f}, {P_REVERBMIX, .25f}}},
    {"Chop Slices x8", "Chops", {{P_ENGINE, E_CHOPS}, {P_CHOPMODE, 1.f}, {P_CHOPSLICES, 1.f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .1f}, {P_REVERBMIX, .15f}, {P_DELAYMIX, .15f}, {P_DELAYDIV, DV_8D}}},
    {"Reverse Chop Wash", "Chops", {{P_ENGINE, E_CHOPS}, {P_CHOPMODE, 0.f}, {P_CHOPREV, 1.f}, {P_ATTACK, .005f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .5f}, {P_REVERBMIX, .5f}, {P_DELAYMIX, .3f}, {P_DELAYDIV, DV_4}}},
    {"Chop Sustain Pad", "Chops", {{P_ENGINE, E_CHOPS}, {P_CHOPMODE, 0.f}, {P_CHOPLOOP, 1.f}, {P_ATTACK, .1f}, {P_SUSTAIN, 1.f}, {P_RELEASE, .6f}, {P_CUTOFF, 3000.f}, {P_CHORUS, .5f}, {P_REVERBMIX, .5f}}},
  };
  return L;
}

// Valores completos (por defecto + lo que pisa el preset), en el orden de ParamId.
inline std::array<float, P_COUNT> presetValues(const Preset& p) {
  std::array<float, P_COUNT> v{};
  for (int i = 0; i < P_COUNT; ++i) v[i] = paramTable()[i].def;
  for (const auto& kv : p.values) v[kv.first] = kv.second;
  return v;
}

}  // namespace schmu
