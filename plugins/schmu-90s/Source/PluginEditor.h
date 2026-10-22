// SCHMU 90s · interfaz (estética SCHMULENSON: nocturna de club, verde ácido, violeta de apoyo).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

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

struct Knob : public juce::Component {
  juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
  void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text);
  void setText(const juce::String& t) { label.setText(t, juce::dontSendNotification); }
  void resized() override;
};

struct Choice : public juce::Component {
  juce::ComboBox box;
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
  void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text);
  void resized() override;
};

struct WaveView : public juce::Component {
  std::vector<float> peaks;
  juce::String name;
  void setSample(const std::shared_ptr<const schmu::SampleData>& s, const juce::String& nm);
  void paint(juce::Graphics&) override;
};
}  // namespace schmu_ui

class SchmuEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit SchmuEditor(SchmuProcessor&);
  ~SchmuEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;
  void updateEngineUI(int engine);
  void selectPreset(int index);
  void rebuildPresetMenu();
  void chooseChop();

  SchmuProcessor& proc_;
  schmu_ui::Look look_;

  // cabecera
  juce::ComboBox presetBox_;
  juce::TextButton prev_{"<"}, next_{">"};
  // motores
  std::array<juce::TextButton, schmu::E_COUNT> engineBtn_;
  juce::ParameterAttachment engineAtt_;
  int engine_ = -1;
  // perillas
  std::vector<std::unique_ptr<schmu_ui::Knob>> knobs_;
  std::vector<std::unique_ptr<schmu_ui::Choice>> choices_;
  schmu_ui::Knob* macro_[4] = {};
  std::vector<schmu_ui::Knob*> voiceKnobs_, filterKnobs_, drumKnobs_, chopKnobs_, fxKnobs_;
  std::vector<schmu_ui::Choice*> drumChoices_, chopChoices_, fxChoices_;
  schmu_ui::Choice* chordChoice_ = nullptr;
  schmu_ui::WaveView wave_;
  juce::TextButton loadBtn_{juce::String::fromUTF8("Cargar chop…")}, defaultBtn_{juce::String::fromUTF8("Chop de fábrica")};
  juce::MidiKeyboardComponent keyboard_;
  std::unique_ptr<juce::FileChooser> chooser_;
  juce::Rectangle<int> voiceR_, filterR_, engineR_, fxR_;
  int lastProgram_ = -1;

  schmu_ui::Knob* addKnob(int param, const juce::String& text);
  schmu_ui::Choice* addChoice(int param, const juce::String& text);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchmuEditor)
};
