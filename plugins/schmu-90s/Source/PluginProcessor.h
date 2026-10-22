// SCHMU 90s · procesador del plugin (JUCE): parámetros, MIDI (con swing), presets y estado.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <memory>
#include <vector>

#include "dsp/Engine.h"
#include "dsp/Presets.h"

class SchmuProcessor : public juce::AudioProcessor {
 public:
  SchmuProcessor();
  ~SchmuProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using juce::AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "SCHMU 90s"; }
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

  // Slot de chops (hilo de mensajes). Devuelve false y deja `error` si no se pudo leer el archivo.
  bool loadChop(const juce::File& file, juce::String& error);
  void resetChop();
  juce::String chopName() const { return chopName_; }
  std::shared_ptr<const schmu::SampleData> shownChop() const { return shownChop_; }

  juce::AudioProcessorValueTreeState apvts;
  juce::MidiKeyboardState keyboardState;

 private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  schmu::SchmuEngine engine_;
  schmu::SwingScheduler swing_;
  std::array<std::atomic<float>*, schmu::P_COUNT> raw_{};
  double sampleRate_ = 44100.0;
  long long sampleCounter_ = 0;
  int currentProgram_ = 0;

  juce::String chopName_, chopPath_;
  std::shared_ptr<const schmu::SampleData> shownChop_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchmuProcessor)
};
