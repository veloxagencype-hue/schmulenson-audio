// SCHMU Chopper · procesador del plugin (JUCE): parámetros, MIDI sample-accurate, banco de chops y estado.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>
#include <vector>

#include "dsp/Chopper.h"
#include "dsp/Presets.h"

class ChopperProcessor : public juce::AudioProcessor {
 public:
  ChopperProcessor();
  ~ChopperProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using juce::AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "SCHMU Chopper"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 6.0; }

  int getNumPrograms() override { return static_cast<int>(schmu::presetList().size()); }
  int getCurrentProgram() override { return currentProgram_; }
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // ---- banco de chops (hilo de mensajes) ----
  // Carga un WAV/AIFF/FLAC/OGG o una carpeta (SCHMU_VC1…): en una carpeta, cada archivo es un slice.
  bool loadChops(const juce::File& fileOrFolder, juce::String& error);
  void clearChops();
  std::vector<schmu::Slice> autoChop_(const schmu::SampleData& d) const;
  void autoChop();                       // vuelve a cortar por transientes con la sensibilidad actual
  void evenChop(int n);
  schmu::Bank bankCopy() const;
  void editBank(const std::function<void(schmu::Bank&)>& fn);   // aplica un cambio y lo publica al audio
  juce::String chopName() const { const juce::SpinLock::ScopedLockType l(lock_); return chopName_; }
  juce::StringArray sliceNames() const { const juce::SpinLock::ScopedLockType l(lock_); return sliceNames_; }
  juce::String notice() const { const juce::SpinLock::ScopedLockType l(lock_); return notice_; }
  uint32_t bankVersion() const { return version_.load(); }
  int selectedSlice() const { return selected_.load(); }
  void setSelectedSlice(int i);
  std::atomic<int> padLight{0};           // bitmask de pads sonando (para la UI)

  static juce::File libraryFile();
  static juce::File resolvePath(const juce::String& absolute, const juce::String& relative);

  juce::AudioProcessorValueTreeState apvts;
  juce::MidiKeyboardState keyboardState;

 private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  void publishLocked();
  int pathKey() const;                   // marca el banco como "para copiar" (con lock_ tomado)
  void applyPreset(const schmu::Preset& p);
  bool readFile(const juce::File& f, schmu::SampleData& out, juce::String& error) const;
  juce::String encodeSample(const schmu::SampleData& d) const;
  bool decodeSample(const juce::String& b64, bool mono, double sr, schmu::SampleData& out) const;
  void setBankLocked(schmu::Bank b, const juce::String& name, const juce::StringArray& names);

  schmu::ChopEngine engine_;
  std::array<std::atomic<float>*, schmu::P_COUNT> raw_{};
  double sampleRate_ = 44100.0;
  int currentProgram_ = 0;

  mutable juce::SpinLock lock_;
  schmu::Bank uiBank_;                    // copia maestra (la edita la UI)
  bool dirty_ = true;
  juce::String chopName_, chopPath_, chopRel_, notice_;
  juce::StringArray sliceNames_;
  std::vector<std::shared_ptr<const schmu::SampleData>> retired_;   // se liberan en el hilo de mensajes, nunca en el de audio
  std::atomic<int> selected_{0};
  std::atomic<uint32_t> version_{0};
  schmu::SliceParams template_;           // sonido por defecto de los slices nuevos (el del último preset)

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChopperProcessor)
};
