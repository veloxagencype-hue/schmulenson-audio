// SCHMU Chopper · interfaz (estética SCHMULENSON: nocturna de club, verde ácido, violeta de apoyo; igual a SCHMU 90s).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

namespace schmu_ui {
const juce::Colour kBg(0xff07070a), kPanel(0xff111118), kPanel2(0xff181822), kLine(0xff262633), kText(0xffecebf2),
    kSoft(0xff9a99ab), kAcid(0xffc8ff3d), kViolet(0xff8a5cff);

class Look : public juce::LookAndFeel_V4 {
 public:
  Look();
  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
  void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
  juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(13.0f, juce::Font::bold)); }
  juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13.0f)); }
  juce::Font getLabelFont(juce::Label&) override { return juce::Font(juce::FontOptions(12.0f)); }
};

// Perilla atada a un parámetro del plugin.
struct Knob : public juce::Component {
  juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
  void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text);
  void resized() override;
};

// Perilla de un parámetro del slice seleccionado (no es parámetro del host: se guarda en el estado del plugin).
struct SliceKnob : public juce::Component {
  juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
  juce::Label label;
  void init(juce::Component& parent, const juce::String& text, double lo, double hi, double def, std::function<juce::String(double)> fmt, double skewMid = -1.0);
  void resized() override;
};

struct Choice : public juce::Component {
  juce::ComboBox box;
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
  void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text);
  void resized() override;
};

// Forma de onda con marcadores de slice: clic = elegir/escuchar, doble clic o Shift+clic = nuevo corte,
// arrastrar un corte lo mueve, Alt+clic o clic derecho lo borra.
struct WaveView : public juce::Component {
  std::vector<float> peaks;
  schmu::Bank bank;
  juce::String name, notice;
  int selected = 0, dragging = -1;
  std::function<void(int)> onSelect;                 // slice elegido (para escucharlo)
  std::function<void(int, bool)> onAudition;         // (slice, down)
  std::function<void(const std::function<void(schmu::Bank&)>&)> onEdit;
  void setBank(const schmu::Bank& b, const juce::String& nm, const juce::String& note, int sel);
  void paint(juce::Graphics&) override;
  void resized() override { rebuildPeaks(); }
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
 private:
  void rebuildPeaks();
  int xToSample(float x) const;
  float sampleToX(int s) const;
  int markerNear(float x) const;
  int auditioned = -1;
};

struct PadGrid : public juce::Component {
  ChopperProcessor* proc = nullptr;
  schmu::Bank bank;
  juce::StringArray names;
  int selected = 0;
  std::function<void(int)> onSelect;
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
 private:
  int padAt(juce::Point<int>) const;
  juce::Rectangle<float> padRect(int i) const;
  int held = -1;
};

struct StepRow : public juce::Component {
  schmu::Bank bank;
  int length = 16, selectedSlice = 0, playStep = -1;
  std::function<void(const std::function<void(schmu::Bank&)>&)> onEdit;
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
 private:
  int cellAt(juce::Point<int>) const;
  juce::Rectangle<float> cellRect(int i) const;
};
}  // namespace schmu_ui

class ChopperEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
 public:
  explicit ChopperEditor(ChopperProcessor&);
  ~ChopperEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;
  bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
  void filesDropped(const juce::StringArray& files, int, int) override;

 private:
  void timerCallback() override;
  void refreshFromBank();
  void selectSlice(int i);
  void loadSelectedIntoControls();
  void updateModeUI(int mode);
  void selectPreset(int index);
  void rebuildPresetMenu();
  void chooseFile();
  void chooseFolder();
  void loadPath(const juce::File& f);
  void editSelected(const std::function<void(schmu::SliceParams&)>& fn);

  ChopperProcessor& proc_;
  schmu_ui::Look look_;

  juce::ComboBox presetBox_;
  juce::TextButton prev_{"<"}, next_{">"};
  std::array<juce::TextButton, schmu::M_COUNT> modeBtn_;
  juce::ParameterAttachment modeAtt_;
  int mode_ = -1;

  schmu_ui::WaveView wave_;
  schmu_ui::PadGrid pads_;
  schmu_ui::StepRow steps_;
  juce::TextButton fileBtn_{juce::String::fromUTF8("Cargar WAV…")}, folderBtn_{juce::String::fromUTF8("Cargar carpeta…")},
      chopBtn_{"Auto-chop"}, evenBtn_{juce::String::fromUTF8("16 parejos")}, clearBtn_{"Vaciar"}, revBtn_{"Reverse"};

  // slice seleccionado
  schmu_ui::SliceKnob gain_, pan_, pitch_, atk_, dec_, sus_, rel_, cut_;
  juce::ComboBox playMode_, filterBox_, chokeBox_;
  juce::Label playModeL_, filterL_, chokeL_;

  std::vector<std::unique_ptr<schmu_ui::Knob>> knobs_;
  std::vector<std::unique_ptr<schmu_ui::Choice>> choices_;
  schmu_ui::Knob *kVol_ = nullptr, *kSens_ = nullptr, *kTrans_ = nullptr, *kHum_ = nullptr, *kReso_ = nullptr, *kTape_ = nullptr, *kCrush_ = nullptr,
                 *kDelay_ = nullptr, *kFb_ = nullptr, *kRev_ = nullptr, *kSize_ = nullptr, *kSwing_ = nullptr, *kTempo_ = nullptr, *kSteps_ = nullptr;
  schmu_ui::Choice *cLock_ = nullptr, *cDelayT_ = nullptr, *cRate_ = nullptr;

  juce::MidiKeyboardComponent keyboard_;
  std::unique_ptr<juce::FileChooser> chooser_;
  juce::Rectangle<int> waveR_, padsR_, sliceR_, seqR_, fxR_;
  schmu::Bank bank_;
  uint32_t lastVersion_ = 0xffffffffu;
  int lastProgram_ = -1, selected_ = 0;
  bool loadingControls_ = false;

  schmu_ui::Knob* addKnob(int param, const juce::String& text);
  schmu_ui::Choice* addChoice(int param, const juce::String& text);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChopperEditor)
};
