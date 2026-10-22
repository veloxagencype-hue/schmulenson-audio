#include "PluginEditor.h"

using namespace schmu;
using namespace schmu_ui;

// ---------------------------------------------------------------- look & feel
Look::Look() {
  setColour(juce::ResizableWindow::backgroundColourId, kBg);
  setColour(juce::Slider::textBoxTextColourId, kText);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
  setColour(juce::Label::textColourId, kSoft);
  setColour(juce::ComboBox::backgroundColourId, kPanel2);
  setColour(juce::ComboBox::textColourId, kText);
  setColour(juce::ComboBox::outlineColourId, kLine);
  setColour(juce::ComboBox::arrowColourId, kAcid);
  setColour(juce::PopupMenu::backgroundColourId, kPanel);
  setColour(juce::PopupMenu::textColourId, kText);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, kAcid);
  setColour(juce::PopupMenu::highlightedTextColourId, kBg);
  setColour(juce::PopupMenu::headerTextColourId, kViolet);
  setColour(juce::TextButton::buttonColourId, kPanel2);
  setColour(juce::TextButton::buttonOnColourId, kAcid);
  setColour(juce::TextButton::textColourOffId, kText);
  setColour(juce::TextButton::textColourOnId, kBg);
  setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe9e8f0));
  setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff15151d));
  setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, kLine);
  setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, kAcid.withAlpha(0.8f));
  setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kViolet.withAlpha(0.4f));
}

void Look::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
  auto b = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(4.0f);
  float d = std::min(b.getWidth(), b.getHeight());
  auto c = b.withSizeKeepingCentre(d, d);
  float ang = start + pos * (end - start);
  float r = d * 0.5f;
  g.setColour(kPanel2);
  g.fillEllipse(c.reduced(3.0f));
  juce::Path track; track.addCentredArc(c.getCentreX(), c.getCentreY(), r - 2.0f, r - 2.0f, 0.0f, start, end, true);
  g.setColour(kLine); g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  juce::Path val; val.addCentredArc(c.getCentreX(), c.getCentreY(), r - 2.0f, r - 2.0f, 0.0f, start, ang, true);
  g.setColour(s.isEnabled() ? kAcid : kSoft); g.strokePath(val, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  juce::Path tick; tick.addRoundedRectangle(-1.5f, -(r - 8.0f), 3.0f, r * 0.42f, 1.5f);
  g.setColour(kText); g.fillPath(tick, juce::AffineTransform::rotation(ang).translated(c.getCentreX(), c.getCentreY()));
}

void Look::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) {
  auto r = b.getLocalBounds().toFloat().reduced(0.5f);
  bool on = b.getToggleState();
  auto fill = on ? kAcid : (down ? kViolet.withAlpha(0.6f) : (hover ? kPanel2.brighter(0.15f) : kPanel2));
  g.setColour(fill); g.fillRoundedRectangle(r, 8.0f);
  g.setColour(on ? kAcid : kLine); g.drawRoundedRectangle(r, 8.0f, 1.0f);
}

void Look::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
  auto r = juce::Rectangle<float>(0, 0, static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
  g.setColour(kPanel2); g.fillRoundedRectangle(r, 8.0f);
  g.setColour(kLine); g.drawRoundedRectangle(r, 8.0f, 1.0f);
  juce::Path arrow; float cx = static_cast<float>(w) - 14.0f, cy = static_cast<float>(h) * 0.5f;
  arrow.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
  g.setColour(box.findColour(juce::ComboBox::arrowColourId)); g.fillPath(arrow);
}

// ---------------------------------------------------------------- widgets
void Knob::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text) {
  const auto& d = paramTable()[static_cast<size_t>(param)];
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 14);
  slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  slider.setDoubleClickReturnValue(true, d.def);
  slider.setPopupDisplayEnabled(false, false, nullptr);
  label.setText(text, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(slider); addAndMakeVisible(label);
  att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, d.id, slider);
  parent.addAndMakeVisible(*this);
}
void Knob::resized() {
  auto r = getLocalBounds();
  label.setBounds(r.removeFromTop(16));
  slider.setBounds(r);
}

void Choice::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, int param, const juce::String& text) {
  const auto& d = paramTable()[static_cast<size_t>(param)];
  int id = 1;
  for (const auto& c : choiceList(param)) box.addItem(c, id++);
  label.setText(text, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(box); addAndMakeVisible(label);
  att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, d.id, box);
  parent.addAndMakeVisible(*this);
}
void Choice::resized() {
  auto r = getLocalBounds();
  label.setBounds(r.removeFromTop(16));
  box.setBounds(r.removeFromTop(26));
}

void WaveView::setSample(const std::shared_ptr<const SampleData>& s, const juce::String& nm) {
  name = nm; peaks.clear();
  if (!s || s->empty()) { repaint(); return; }
  const int bins = 360, len = s->length();
  peaks.resize(bins);
  for (int b = 0; b < bins; ++b) {
    int a = static_cast<int>(static_cast<long long>(len) * b / bins), z = std::max(a + 1, static_cast<int>(static_cast<long long>(len) * (b + 1) / bins));
    float m = 0; for (int i = a; i < z && i < len; ++i) m = std::max(m, std::fabs(s->l[static_cast<size_t>(i)]));
    peaks[static_cast<size_t>(b)] = m;
  }
  repaint();
}
void WaveView::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  g.setColour(kBg); g.fillRoundedRectangle(r, 8.0f);
  g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
  if (!peaks.empty()) {
    float mx = 0.001f; for (float p : peaks) mx = std::max(mx, p);
    float cy = r.getCentreY(), hh = r.getHeight() * 0.42f;
    float sx = r.getWidth() / static_cast<float>(peaks.size());
    g.setColour(kViolet);
    for (size_t i = 0; i < peaks.size(); ++i) {
      float a = peaks[i] / mx * hh;
      g.fillRect(r.getX() + static_cast<float>(i) * sx, cy - a, std::max(1.0f, sx - 0.5f), a * 2.0f);
    }
  }
  g.setColour(kSoft); g.setFont(juce::Font(juce::FontOptions(11.0f)));
  g.drawText(name, getLocalBounds().reduced(8, 4), juce::Justification::topLeft, true);
}

// ---------------------------------------------------------------- editor
Knob* SchmuEditor::addKnob(int param, const juce::String& text) {
  knobs_.push_back(std::make_unique<Knob>());
  knobs_.back()->init(*this, proc_.apvts, param, text);
  return knobs_.back().get();
}
Choice* SchmuEditor::addChoice(int param, const juce::String& text) {
  choices_.push_back(std::make_unique<Choice>());
  choices_.back()->init(*this, proc_.apvts, param, text);
  return choices_.back().get();
}

SchmuEditor::SchmuEditor(SchmuProcessor& p)
    : AudioProcessorEditor(&p), proc_(p),
      engineAtt_(*p.apvts.getParameter("engine"), [this](float v) { updateEngineUI(static_cast<int>(v + 0.5f)); }, nullptr),
      keyboard_(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
  setLookAndFeel(&look_);

  rebuildPresetMenu();
  addAndMakeVisible(presetBox_);
  presetBox_.onChange = [this] { int id = presetBox_.getSelectedId(); if (id > 0) selectPreset(id - 1); };
  addAndMakeVisible(prev_); addAndMakeVisible(next_);
  prev_.onClick = [this] { int n = proc_.getNumPrograms(); selectPreset((proc_.getCurrentProgram() + n - 1) % n); };
  next_.onClick = [this] { selectPreset((proc_.getCurrentProgram() + 1) % proc_.getNumPrograms()); };

  static const char* engineNames[E_COUNT] = {"ORGAN", "PIANO", "STAB", "BASS", "DRUMS", "CHOPS"};
  for (int i = 0; i < E_COUNT; ++i) {
    auto& b = engineBtn_[static_cast<size_t>(i)];
    b.setButtonText(engineNames[i]);
    b.setClickingTogglesState(false);
    b.onClick = [this, i] { engineAtt_.setValueAsCompleteGesture(static_cast<float>(i)); };
    addAndMakeVisible(b);
  }

  // VOICE
  for (int id : {P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_SUB, P_GLIDE, P_TUNE, P_VOLUME})
    voiceKnobs_.push_back(addKnob(id, paramTable()[static_cast<size_t>(id)].name));
  // FILTER
  filterKnobs_.push_back(addKnob(P_CUTOFF, "Cutoff"));
  filterKnobs_.push_back(addKnob(P_RESO, "Reso"));
  filterKnobs_.push_back(addKnob(P_ENVAMT, "Env Amt"));
  filterKnobs_.push_back(addKnob(P_FDECAY, "Env Decay"));
  // ENGINE (macros + chord)
  for (int i = 0; i < 4; ++i) macro_[i] = addKnob(P_M1 + i, "");
  chordChoice_ = addChoice(P_CHORD, "Chord Memory");
  // DRUMS
  drumChoices_.push_back(addChoice(P_KIT, "Kit"));
  for (int id : {P_KICKTUNE, P_KICKDECAY, P_KICKPUNCH, P_SNARETONE, P_HATDECAY}) drumKnobs_.push_back(addKnob(id, paramTable()[static_cast<size_t>(id)].name));
  drumKnobs_[3]->setText("Snare/Clap"); drumKnobs_[4]->setText("Hat Decay");
  // CHOPS
  chopChoices_.push_back(addChoice(P_CHOPMODE, "Mode"));
  chopChoices_.push_back(addChoice(P_CHOPSLICES, "Slices"));
  chopChoices_.push_back(addChoice(P_CHOPREV, "Reverse"));
  chopChoices_.push_back(addChoice(P_CHOPLOOP, "Loop"));
  chopKnobs_.push_back(addKnob(P_CHOPSTART, "Start"));
  addAndMakeVisible(wave_); addAndMakeVisible(loadBtn_); addAndMakeVisible(defaultBtn_);
  wave_.setSample(proc_.shownChop(), proc_.chopName());
  loadBtn_.onClick = [this] { chooseChop(); };
  defaultBtn_.onClick = [this] { proc_.resetChop(); wave_.setSample(proc_.shownChop(), proc_.chopName()); };
  // FX
  fxKnobs_.push_back(addKnob(P_TAPE, "Tape"));
  fxKnobs_.push_back(addKnob(P_CHORUS, "Chorus"));
  fxKnobs_.push_back(addKnob(P_CHORUSDEPTH, "Depth"));
  fxKnobs_.push_back(addKnob(P_DELAYMIX, "Delay"));
  fxChoices_.push_back(addChoice(P_DELAYDIV, "Delay Time"));
  fxKnobs_.push_back(addKnob(P_DELAYFB, "Feedback"));
  fxKnobs_.push_back(addKnob(P_REVERBMIX, "Reverb"));
  fxKnobs_.push_back(addKnob(P_REVERBSIZE, "Size"));
  fxKnobs_.push_back(addKnob(P_SWING, "Swing"));

  addAndMakeVisible(keyboard_);
  keyboard_.setKeyWidth(26.0f);
  keyboard_.setAvailableRange(24, 96);
  keyboard_.setLowestVisibleKey(36);

  setSize(940, 640);   // al final: resized() necesita que ya existan todos los controles
  engineAtt_.sendInitialUpdate();
  startTimerHz(8);
  timerCallback();
}

SchmuEditor::~SchmuEditor() { setLookAndFeel(nullptr); }

void SchmuEditor::rebuildPresetMenu() {
  presetBox_.clear(juce::dontSendNotification);
  std::string cat;
  const auto& L = presetList();
  for (size_t i = 0; i < L.size(); ++i) {
    if (L[i].category != cat) { cat = L[i].category; presetBox_.addSectionHeading(cat); }
    presetBox_.addItem(L[i].name, static_cast<int>(i) + 1);
  }
}

void SchmuEditor::selectPreset(int index) {
  proc_.setCurrentProgram(index);
  presetBox_.setSelectedId(index + 1, juce::dontSendNotification);
  lastProgram_ = index;
}

void SchmuEditor::timerCallback() {
  int cur = proc_.getCurrentProgram();
  if (cur != lastProgram_) { presetBox_.setSelectedId(cur + 1, juce::dontSendNotification); lastProgram_ = cur; }
}

void SchmuEditor::updateEngineUI(int e) {
  e = std::max(0, std::min(E_COUNT - 1, e));
  for (int i = 0; i < E_COUNT; ++i) engineBtn_[static_cast<size_t>(i)].setToggleState(i == e, juce::dontSendNotification);
  engine_ = e;
  const bool pitched = e <= E_STAB;
  for (int i = 0; i < 4; ++i) { macro_[i]->setText(macroLabel(e, i)); macro_[i]->setVisible(e <= E_BASS); }
  chordChoice_->setVisible(pitched);
  for (auto* k : drumKnobs_) k->setVisible(e == E_DRUMS);
  for (auto* c : drumChoices_) c->setVisible(e == E_DRUMS);
  for (auto* k : chopKnobs_) k->setVisible(e == E_CHOPS);
  for (auto* c : chopChoices_) c->setVisible(e == E_CHOPS);
  wave_.setVisible(e == E_CHOPS); loadBtn_.setVisible(e == E_CHOPS); defaultBtn_.setVisible(e == E_CHOPS);
  // glide solo importa en el bajo; sub no aplica a bajo/chops
  voiceKnobs_[4]->setEnabled(e != E_BASS && e != E_CHOPS && e != E_DRUMS);
  voiceKnobs_[5]->setEnabled(e == E_BASS);
  voiceKnobs_[6]->setEnabled(e != E_DRUMS);
  for (auto* k : filterKnobs_) k->setEnabled(e != E_DRUMS);
  for (size_t i = 0; i < 4; ++i) voiceKnobs_[i]->setEnabled(e != E_DRUMS);
  repaint();
}

void SchmuEditor::chooseChop() {
  chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Elegí un chop (WAV, AIFF, FLAC u OGG)"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
    auto f = fc.getResult();
    if (f == juce::File()) return;
    juce::String err;
    if (!proc_.loadChop(f, err)) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "SCHMU 90s", err); return; }
    wave_.setSample(proc_.shownChop(), proc_.chopName());
  });
}

void SchmuEditor::paint(juce::Graphics& g) {
  g.fillAll(kBg);
  g.setGradientFill(juce::ColourGradient(kViolet.withAlpha(0.22f), 0.0f, 0.0f, kBg, 0.0f, 260.0f, false));
  g.fillRect(0, 0, getWidth(), 260);
  // marca
  g.setColour(kText); g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
  g.drawText("SCHMULENSON", 22, 12, 220, 28, juce::Justification::centredLeft);
  g.setColour(kAcid); g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
  g.drawText("S C H M U   9 0 s", 24, 38, 220, 16, juce::Justification::centredLeft);
  auto panel = [&](juce::Rectangle<int> r, const juce::String& title) {
    g.setColour(kPanel); g.fillRoundedRectangle(r.toFloat(), 14.0f);
    g.setColour(kLine); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 14.0f, 1.0f);
    g.setColour(kSoft); g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(title.toUpperCase(), r.getX() + 14, r.getY() + 8, 480, 14, juce::Justification::centredLeft);
  };
  panel(voiceR_, "Voice");
  panel(filterR_, "Filter");
  static const char* titles[E_COUNT] = {"Organ", "Piano", "Stab", "Bass", "Drum synth", "Chop slot"};
  panel(engineR_, titles[std::max(0, engine_)]);
  panel(fxR_, juce::String::fromUTF8("FX · Tape → Chorus → Delay → Reverb · Swing"));
}

void SchmuEditor::resized() {
  if (fxKnobs_.size() < 8 || fxChoices_.empty() || chopKnobs_.empty() || drumChoices_.empty()) return;
  auto r = getLocalBounds().reduced(16, 12);
  auto head = r.removeFromTop(48);
  auto presetArea = head.removeFromRight(420);
  next_.setBounds(presetArea.removeFromRight(36).reduced(0, 8));
  presetArea.removeFromRight(4);
  prev_.setBounds(presetArea.removeFromLeft(36).reduced(0, 8));
  presetArea.removeFromLeft(4);
  presetBox_.setBounds(presetArea.reduced(0, 8));
  r.removeFromTop(6);
  auto tabs = r.removeFromTop(34);
  int tw = tabs.getWidth() / E_COUNT;
  for (int i = 0; i < E_COUNT; ++i) engineBtn_[static_cast<size_t>(i)].setBounds(tabs.removeFromLeft(tw).reduced(3, 0));
  r.removeFromTop(8);

  auto keyArea = r.removeFromBottom(70);
  r.removeFromBottom(8);
  auto fxArea = r.removeFromBottom(128);
  r.removeFromBottom(8);
  keyboard_.setBounds(keyArea);
  fxR_ = fxArea;

  // fila principal: voice | filter | engine
  auto main = r;
  voiceR_ = main.removeFromLeft(340); main.removeFromLeft(8);
  filterR_ = main.removeFromLeft(200); main.removeFromLeft(8);
  engineR_ = main;

  auto grid = [](juce::Rectangle<int> area, std::vector<schmu_ui::Knob*>& ks, int cols) {
    area.removeFromTop(24);
    int rows = (static_cast<int>(ks.size()) + cols - 1) / cols;
    int cw = area.getWidth() / cols, ch = area.getHeight() / rows;
    for (size_t i = 0; i < ks.size(); ++i) ks[i]->setBounds(juce::Rectangle<int>(area.getX() + static_cast<int>(i % static_cast<size_t>(cols)) * cw, area.getY() + static_cast<int>(i / static_cast<size_t>(cols)) * ch, cw, ch).reduced(2));
  };
  grid(voiceR_.reduced(8, 0), voiceKnobs_, 4);
  grid(filterR_.reduced(8, 0), filterKnobs_, 2);

  // engine: macros en fila + chord memory abajo
  {
    auto a = engineR_.reduced(8, 0); a.removeFromTop(24);
    auto chordRow = a.removeFromBottom(54);
    chordChoice_->setBounds(chordRow.removeFromLeft(220).reduced(6, 4));
    int cw = a.getWidth() / 4;
    for (int i = 0; i < 4; ++i) macro_[i]->setBounds(a.removeFromLeft(cw).reduced(2));
  }
  // drums
  {
    auto a = engineR_.reduced(8, 0); a.removeFromTop(24);
    drumChoices_[0]->setBounds(a.removeFromTop(50).removeFromLeft(160).reduced(6, 2));
    int cw = a.getWidth() / 5;
    for (auto* k : drumKnobs_) k->setBounds(a.removeFromLeft(cw).reduced(2));
  }
  // chops
  {
    auto a = engineR_.reduced(10, 0); a.removeFromTop(24);
    auto top = a.removeFromTop(50);
    int cw = top.getWidth() / 5;
    for (auto* c : chopChoices_) c->setBounds(top.removeFromLeft(cw).reduced(4, 2));
    chopKnobs_[0]->setBounds(top.removeFromLeft(cw).reduced(2, 0));
    auto btns = a.removeFromBottom(30);
    loadBtn_.setBounds(btns.removeFromLeft(150).reduced(0, 2)); btns.removeFromLeft(8);
    defaultBtn_.setBounds(btns.removeFromLeft(150).reduced(0, 2));
    a.removeFromBottom(6);
    wave_.setBounds(a);
  }
  // FX (una fila de 9 elementos; el de "Delay Time" es una lista)
  {
    auto a = fxR_.reduced(8, 0); a.removeFromTop(22);
    int cw = a.getWidth() / 9;
    size_t ki = 0, ci = 0;
    const bool isChoice[9] = {false, false, false, false, true, false, false, false, false};
    for (int i = 0; i < 9; ++i) {
      auto cell = a.removeFromLeft(cw).reduced(2);
      if (isChoice[i]) fxChoices_[ci++]->setBounds(cell.reduced(0, 18)); else fxKnobs_[ki++]->setBounds(cell);
    }
  }
}
