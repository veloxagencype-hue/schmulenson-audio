// SCHMU Chopper · prueba de humo del plugin real: procesador + editor + presets + carga de archivos/carpetas + estado,
// con audio sintético (no hay voces reales en el repo). Dibuja el editor a PNG si SCHMU_SNAPSHOT_DIR está definida.
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++fails; std::printf("FALLA [%d] %s -> ", __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace {
struct Host : juce::AudioPlayHead {
  bool playing = false; double ppq = 0, bpm = 120;
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo p; p.setBpm(bpm); p.setPpqPosition(ppq); p.setIsPlaying(playing);
    return p;
  }
};

bool writeWav(const juce::File& f, const std::vector<float>& x, double sr = 44100.0) {
  f.deleteFile();
  juce::WavAudioFormat wav;
  std::unique_ptr<juce::FileOutputStream> os(f.createOutputStream());
  if (!os) return false;
  std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr, 1, 16, {}, 0));
  if (!w) return false;
  os.release();
  juce::AudioBuffer<float> b(1, static_cast<int>(x.size()));
  for (size_t i = 0; i < x.size(); ++i) b.setSample(0, static_cast<int>(i), x[i]);
  w->writeFromAudioSampleBuffer(b, 0, static_cast<int>(x.size()));
  return true;
}

// n ráfagas de seno (f0 + 20 Hz·i) separadas por 0,3 s
std::vector<float> bursts(int n, double f0, double sr = 44100.0) {
  std::vector<float> x(static_cast<size_t>(sr * 0.3 * n + sr * 0.2), 0.0f);
  for (int k = 0; k < n; ++k) {
    const int at = static_cast<int>(sr * (0.1 + 0.3 * k));
    for (int i = 0; i < static_cast<int>(sr * 0.25); ++i) {
      double t = i / sr;
      x[static_cast<size_t>(at + i)] += static_cast<float>(0.7 * std::min(1.0, t * 800.0) * std::exp(-9.0 * t) * std::sin(2.0 * 3.14159265358979 * (f0 + 20.0 * k) * t));
    }
  }
  return x;
}

float run(ChopperProcessor& p, int blocks, int bs, juce::MidiBuffer first, bool& finite, std::vector<float>* keep = nullptr, Host* host = nullptr) {
  float peak = 0;
  for (int b = 0; b < blocks; ++b) {
    juce::AudioBuffer<float> buf(2, bs);
    juce::MidiBuffer midi;
    if (b == 0) midi = first;
    if (host && host->playing) host->ppq = host->ppq;   // el llamador avanza el transporte entre bloques
    p.processBlock(buf, midi);
    for (int i = 0; i < bs; ++i) {
      float v = buf.getSample(0, i);
      if (!std::isfinite(v) || !std::isfinite(buf.getSample(1, i))) finite = false;
      peak = std::max(peak, std::fabs(v));
      if (keep) keep->push_back(v);
    }
    if (host && host->playing) host->ppq += bs * host->bpm / 60.0 / 44100.0;
  }
  return peak;
}
}  // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI gui;
  const juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("schmu_chopper_smoke");
  tmp.deleteRecursively();
  tmp.createDirectory();

  ChopperProcessor p;
  p.setPlayConfigDetails(0, 2, 44100.0, 512);
  p.prepareToPlay(44100.0, 512);
  CHECK(p.getNumPrograms() >= 10, "programas: %d", p.getNumPrograms());
  CHECK(p.chopName().isEmpty() && p.bankCopy().slices.empty(), "arranca vacío");

  // ---- sin chops: no suena y no cae
  {
    juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 36, 0.9f), 10);
    bool finite = true; float pk = run(p, 4, 512, m, finite);
    CHECK(finite && pk == 0.0f, "sin chops, silencio (%g)", pk);
  }

  // ---- cargar un WAV: auto-chop en 8 slices
  const juce::File wav = tmp.getChildFile("SCHMU_VC1_SYL_01_A_126.wav");
  CHECK(writeWav(wav, bursts(8, 220.0)), "escribir wav");
  juce::String err;
  CHECK(p.loadChops(wav, err), "loadChops: %s", err.toRawUTF8());
  {
    auto b = p.bankCopy();
    CHECK(b.slices.size() == 8, "8 ráfagas → %zu slices", b.slices.size());
    CHECK(p.chopName() == "SCHMU_VC1_SYL_01_A_126.wav", "nombre");
    bool rootsA = !b.slices.empty();
    for (const auto& s : b.slices) rootsA &= (s.root % 12) == 9;
    CHECK(rootsA, "la tonalidad del nombre (A) pasa a las notas de origen");
  }
  juce::String bad;
  CHECK(!p.loadChops(juce::File("/no/existe.wav"), bad) && bad.isNotEmpty(), "archivo inválido");

  // ---- tocar un pad: suena el slice (sample-accurate: nada antes de la posición del evento)
  {
    p.setCurrentProgram(0);
    juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 36 + 2, 1.0f), 100);
    bool finite = true; std::vector<float> out;
    run(p, 6, 512, m, finite, &out);
    int first = -1; for (size_t i = 0; i < out.size(); ++i) if (std::fabs(out[i]) > 1e-4f) { first = static_cast<int>(i); break; }
    CHECK(finite && first >= 100 && first < 100 + 200, "arranca en el offset del evento (%d)", first);
    // el slice 3 (índice 2) es de 260 Hz
    double best = 0, bf = 0;
    for (double f = 230; f < 300; f += 0.5) {
      double w = 2 * 3.14159265 * f / 44100.0, c = 2 * std::cos(w), s1 = 0, s2 = 0;
      for (int i = 200; i < 2500 && i < static_cast<int>(out.size()); ++i) { double s0 = out[static_cast<size_t>(i)] + c * s1 - s2; s2 = s1; s1 = s0; }
      double g = s1 * s1 + s2 * s2 - c * s1 * s2;
      if (g > best) { best = g; bf = f; }
    }
    CHECK(std::fabs(bf - 260.0) < 3.0, "pad 3 suena a %.1f Hz (esperado 260)", bf);
    juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    run(p, 1, 512, off, finite);
  }

  // ---- todos los presets, en su modo, con el transporte andando
  for (int i = 0; i < p.getNumPrograms(); ++i) {
    p.setCurrentProgram(i);
    auto vals = schmu::presetValues(schmu::presetList()[static_cast<size_t>(i)]);
    int mode = static_cast<int>(p.apvts.getRawParameterValue("mode")->load() + 0.5f);
    CHECK(mode == static_cast<int>(vals[schmu::P_MODE] + 0.5f), "preset %d: modo %d", i, mode);
    Host host; host.playing = true; p.setPlayHead(&host);
    juce::MidiBuffer m;
    if (mode != schmu::M_SEQ) m.addEvent(juce::MidiMessage::noteOn(1, mode == schmu::M_KEY ? 60 : 38, 0.9f), 10);
    bool finite = true;
    float pk = run(p, 40, 512, m, finite, nullptr, &host);
    CHECK(finite && pk > 0.01f && pk <= 1.0f, "preset %d (%s): pico %.3f finito=%d", i, p.getProgramName(i).toRawUTF8(), pk, finite);
    p.setPlayHead(nullptr);
    juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    run(p, 1, 512, off, finite);
  }

  // ---- los presets conservan lo que cargaste (slices y nombre)
  CHECK(p.bankCopy().slices.size() == 8 && p.chopName().isNotEmpty(), "los presets no tocan los chops");

  // ---- editar: auto-chop con otra sensibilidad, 16 parejos, marcadores manuales
  p.apvts.getParameter("sens")->setValueNotifyingHost(0.0f);
  p.autoChop();
  CHECK(p.bankCopy().slices.size() >= 1, "auto-chop con sens 0");
  p.evenChop(16);
  CHECK(p.bankCopy().slices.size() == 16, "16 parejos");
  p.editBank([](schmu::Bank& b) { b.slices[3].p.pitch = 5.0f; b.steps[2].slice = 3; b.steps[2].prob = 0.5f; b.keySlice = 3; });

  // ---- estado: chop chico viaja dentro del proyecto (aunque borres el archivo)
  {
    juce::MemoryBlock mb; p.getStateInformation(mb);
    wav.deleteFile();
    ChopperProcessor q; q.setPlayConfigDetails(0, 2, 44100.0, 512); q.prepareToPlay(44100.0, 512);
    q.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    auto b = q.bankCopy();
    CHECK(b.data && b.data->length() > 40000 && b.slices.size() == 16, "el chop se restaura embebido (%zu slices)", b.slices.size());
    CHECK(std::fabs(b.slices[3].p.pitch - 5.0f) < 1e-4f && b.steps[2].slice == 3 && std::fabs(b.steps[2].prob - 0.5f) < 1e-4f && b.keySlice == 3, "slices, pasos y keySlice restaurados");
    CHECK(q.notice().isEmpty() && q.chopName() == p.chopName(), "sin aviso");
    bool finite = true; juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 40, 1.0f), 0);
    CHECK(run(q, 16, 512, m, finite) > 0.01f, "suena tras restaurar");
  }

  // ---- carpeta SCHMU_VC1: cada archivo es un slice, con su tono
  {
    const juce::File pack = tmp.getChildFile("SCHMU_VC1_Vocal-Chops-Vol1");
    const juce::File cat = pack.getChildFile("01_Syllables");
    cat.createDirectory();
    const char* keys[6] = {"A", "C", "Ds", "Fs", "NOKEY", "G"};
    const double fr[6] = {220.0, 261.63, 311.13, 369.99, 200.0, 392.0};
    for (int i = 0; i < 6; ++i) {
      std::vector<float> x(static_cast<size_t>(44100 * 8));   // 6 × 8 s: el pack pasa los 2 MB y el estado lo guarda por ruta
      for (size_t k = 0; k < x.size(); ++k) x[k] = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * fr[i] * k / 44100.0) * std::exp(-0.4 * k / 44100.0));
      CHECK(writeWav(cat.getChildFile(juce::String::formatted("SCHMU_VC1_SYL_%02d_%s_126.wav", i + 1, keys[i])), x), "wav %d", i);
    }
    ChopperProcessor f; f.setPlayConfigDetails(0, 2, 44100.0, 512); f.prepareToPlay(44100.0, 512);
    CHECK(f.loadChops(pack, err), "loadChops carpeta: %s", err.toRawUTF8());
    auto b = f.bankCopy();
    CHECK(b.slices.size() == 6, "6 archivos → %zu slices", b.slices.size());
    if (b.slices.size() == 6) {
      CHECK(b.slices[0].root == 57 && b.slices[1].root == 60 && b.slices[2].root % 12 == 3 && b.slices[3].root % 12 == 6 && b.slices[5].root % 12 == 7,
            "tonos por archivo: %d %d %d %d %d", b.slices[0].root, b.slices[1].root, b.slices[2].root, b.slices[3].root, b.slices[5].root);
      CHECK(f.sliceNames()[0] == "SCHMU_VC1_SYL_01_A_126" && f.sliceNames().size() == 6, "nombres de los pads");
    }
    // modo Key: tecla = raíz del slice → mismo tono; +12 → octava
    f.apvts.getParameter("mode")->setValueNotifyingHost(f.apvts.getParameter("mode")->convertTo0to1(1.0f));
    f.editBank([](schmu::Bank& bb) { bb.keySlice = 1; });
    juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    bool finite = true; std::vector<float> out; run(f, 8, 512, m, finite, &out);
    double best = 0, bf = 0;
    for (double fq = 240; fq < 290; fq += 0.25) {
      double w = 2 * 3.14159265 * fq / 44100.0, c = 2 * std::cos(w), s1 = 0, s2 = 0;
      for (int i = 500; i < 3500; ++i) { double s0 = out[static_cast<size_t>(i)] + c * s1 - s2; s2 = s1; s1 = s0; }
      double g = s1 * s1 + s2 * s2 - c * s1 * s2;
      if (g > best) { best = g; bf = fq; }
    }
    CHECK(std::fabs(1200.0 * std::log2(bf / 261.63)) < 8.0, "modo Key toca la raíz del slice (%.2f Hz)", bf);

    // estado de un pack grande: por ruta, y si movés la carpeta la encuentra por la biblioteca
    juce::MemoryBlock mb; f.getStateInformation(mb);
    ChopperProcessor g; g.setPlayConfigDetails(0, 2, 44100.0, 512); g.prepareToPlay(44100.0, 512);
    g.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK(g.bankCopy().data && g.bankCopy().slices.size() == 6, "restaurado el pack por ruta");
    CHECK(mb.getSize() < 200000, "el estado de un pack grande no lleva el audio adentro (%zu bytes)", mb.getSize());
    // rutas relativas: se mueve el pack y se apunta la biblioteca a la nueva ubicación
    const juce::File moved = tmp.getChildFile("otra_carpeta");
    moved.createDirectory();
    pack.copyDirectoryTo(moved.getChildFile("SCHMU_VC1_Vocal-Chops-Vol1"));
    auto resolved = ChopperProcessor::resolvePath(tmp.getChildFile("no_existe").getFullPathName(), "SCHMU_VC1_Vocal-Chops-Vol1/01_Syllables");
    CHECK(resolved == juce::File() || resolved.exists(), "resolvePath no revienta");
    ChopperProcessor::libraryFile().getParentDirectory().createDirectory();
    ChopperProcessor::libraryFile().replaceWithText(moved.getFullPathName());
    auto r2 = ChopperProcessor::resolvePath(tmp.getChildFile("no_existe").getFullPathName(), "SCHMU_VC1_Vocal-Chops-Vol1/01_Syllables");
    CHECK(r2.isDirectory(), "ruta relativa resuelta con la biblioteca");
    // falta el archivo: aviso, sin caerse
    pack.deleteRecursively();
    moved.deleteRecursively();
    ChopperProcessor h; h.setPlayConfigDetails(0, 2, 44100.0, 512); h.prepareToPlay(44100.0, 512);
    h.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK(h.bankCopy().data == nullptr && h.notice().isNotEmpty(), "pack faltante: aviso (%s)", h.notice().toRawUTF8());
    bool fin = true; juce::MidiBuffer mm; mm.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    CHECK(run(h, 3, 512, mm, fin) == 0.0f && fin, "pack faltante: silencio");
  }

  // ---- el editor se construye y se dibuja en cada modo
  {
    p.loadChops(tmp.getChildFile("no_existe.wav"), err);
    const juce::File w2 = tmp.getChildFile("SCHMU_VC1_SYL_02_A_126.wav");
    writeWav(w2, bursts(8, 220.0));
    p.loadChops(w2, err);
    p.setCurrentProgram(12);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    CHECK(ed != nullptr, "editor");
    if (ed) {
      for (int mode : {0, 1, 2}) {
        p.apvts.getParameter("mode")->setValueNotifyingHost(p.apvts.getParameter("mode")->convertTo0to1(static_cast<float>(mode)));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(img.getWidth() == 980 && img.getHeight() == 760, "snapshot %dx%d", img.getWidth(), img.getHeight());
        const char* dir = std::getenv("SCHMU_SNAPSHOT_DIR");
        if (dir) {
          juce::File out = juce::File(dir).getChildFile("chopper_mode" + juce::String(mode) + ".png");
          out.deleteFile();
          juce::PNGImageFormat png; juce::FileOutputStream fo(out);
          if (fo.openedOk()) png.writeImageToStream(img, fo);
        }
      }
      p.clearChops();
      juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
      const char* dir = std::getenv("SCHMU_SNAPSHOT_DIR");
      if (dir) {
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File out = juce::File(dir).getChildFile("chopper_vacio.png");
        out.deleteFile();
        juce::PNGImageFormat png; juce::FileOutputStream fo(out);
        if (fo.openedOk()) png.writeImageToStream(img, fo);
      }
    }
  }
  tmp.deleteRecursively();
  std::printf("plugin_smoke: %d chequeos, %d fallas\n", checks, fails);
  return fails ? 1 : 0;
}
