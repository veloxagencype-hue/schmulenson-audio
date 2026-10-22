// SCHMU 90s · prueba de humo del plugin real: instancia el procesador, recorre TODOS los presets por la ruta de
// programas del host, manda MIDI por processBlock, guarda/restaura el estado y dibuja el editor a un PNG.
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FALLA [%d] %s -> ", __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static float runBlocks(SchmuProcessor& p, int blocks, int bs, juce::MidiBuffer first, bool& finite) {
  float peak = 0;
  for (int b = 0; b < blocks; ++b) {
    juce::AudioBuffer<float> buf(2, bs);
    juce::MidiBuffer midi;
    if (b == 0) midi = first;
    p.processBlock(buf, midi);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { float v = buf.getSample(c, i); if (!std::isfinite(v)) finite = false; peak = std::max(peak, std::fabs(v)); }
  }
  return peak;
}

int main() {
  juce::ScopedJuceInitialiser_GUI gui;
  SchmuProcessor p;
  p.setPlayConfigDetails(0, 2, 44100.0, 512);
  p.prepareToPlay(44100.0, 512);
  CHECK(p.getNumPrograms() >= 40, "programas: %d", p.getNumPrograms());
  CHECK(p.apvts.state.getNumChildren() >= 0, "apvts");

  // todos los presets por la ruta de programas del host
  for (int i = 0; i < p.getNumPrograms(); ++i) {
    p.setCurrentProgram(i);
    auto vals = schmu::presetValues(schmu::presetList()[static_cast<size_t>(i)]);
    int eng = static_cast<int>(vals[schmu::P_ENGINE] + 0.5f);
    float got = p.apvts.getRawParameterValue("engine")->load();
    CHECK(static_cast<int>(got + 0.5f) == eng, "preset %d: engine %g vs %d", i, got, eng);
    int note = eng == schmu::E_DRUMS ? 36 : (eng == schmu::E_BASS ? 36 : (eng == schmu::E_CHOPS && vals[schmu::P_CHOPMODE] > 0.5f ? 36 : 60));
    juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, note, 0.9f), 10);
    bool finite = true;
    float pk = runBlocks(p, 8, 512, m, finite);
    CHECK(finite && pk > 0.01f && pk <= 1.0f, "preset %d (%s): pico %.3f finito=%d", i, p.getProgramName(i).toRawUTF8(), pk, finite);
    juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    runBlocks(p, 1, 512, off, finite);
  }

  // estado: guardar y restaurar conserva parámetros y programa
  p.setCurrentProgram(12);
  p.apvts.getParameter("cutoff")->setValueNotifyingHost(p.apvts.getParameter("cutoff")->convertTo0to1(1234.0f));
  juce::MemoryBlock mb; p.getStateInformation(mb);
  SchmuProcessor q; q.setPlayConfigDetails(0, 2, 44100.0, 512); q.prepareToPlay(44100.0, 512);
  q.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
  CHECK(q.getCurrentProgram() == 12, "programa restaurado: %d", q.getCurrentProgram());
  CHECK(std::fabs(q.apvts.getRawParameterValue("cutoff")->load() - 1234.0f) < 2.0f, "cutoff restaurado: %g", q.apvts.getRawParameterValue("cutoff")->load());

  // carga de un chop real (WAV generado al vuelo) y de un archivo inválido
  {
    juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("schmu_smoke.wav");
    tmp.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(tmp.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), 44100.0, 1, 16, {}, 0));
    if (w) {
      os.release();
      juce::AudioBuffer<float> b(1, 22050);
      for (int i = 0; i < 22050; ++i) b.setSample(0, i, 0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / 44100.0f));
      w->writeFromAudioSampleBuffer(b, 0, 22050); w.reset();
      juce::String err;
      CHECK(p.loadChop(tmp, err), "loadChop: %s", err.toRawUTF8());
      CHECK(p.chopName() == "schmu_smoke.wav", "nombre del chop: %s", p.chopName().toRawUTF8());
      juce::MemoryBlock mb2; p.getStateInformation(mb2);
      SchmuProcessor r; r.setPlayConfigDetails(0, 2, 44100.0, 512); r.prepareToPlay(44100.0, 512);
      r.setStateInformation(mb2.getData(), static_cast<int>(mb2.getSize()));
      CHECK(r.chopName() == "schmu_smoke.wav", "el chop se restaura con el estado");
      tmp.deleteFile();
    } else { std::printf("(no se pudo crear el WAV de prueba)\n"); ++fails; }
    juce::String err2;
    CHECK(!p.loadChop(juce::File("/no/existe.wav"), err2) && err2.isNotEmpty(), "archivo inválido");
  }

  // el editor se construye y se dibuja con cada motor
  {
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    CHECK(ed != nullptr, "editor");
    if (ed) {
      for (int eng : {0, 2, 4, 5}) {
        p.apvts.getParameter("engine")->setValueNotifyingHost(p.apvts.getParameter("engine")->convertTo0to1(static_cast<float>(eng)));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(img.getWidth() == 940 && img.getHeight() == 640, "snapshot");
        const char* dir = std::getenv("SCHMU_SNAPSHOT_DIR");
        if (dir) {
          juce::File out = juce::File(dir).getChildFile("editor_engine" + juce::String(eng) + ".png");
          out.deleteFile();
          juce::PNGImageFormat png; juce::FileOutputStream fo(out);
          if (fo.openedOk()) png.writeImageToStream(img, fo);
        }
      }
    }
  }
  std::printf("plugin_smoke: %d fallas\n", fails);
  return fails ? 1 : 0;
}
