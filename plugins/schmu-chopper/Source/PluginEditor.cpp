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

void SliceKnob::init(juce::Component& parent, const juce::String& text, double lo, double hi, double def, std::function<juce::String(double)> fmt, double skewMid) {
  slider.setRange(lo, hi, 0.0);
  if (skewMid > 0.0) slider.setSkewFactorFromMidPoint(skewMid);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 14);
  slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  slider.setDoubleClickReturnValue(true, def);
  slider.textFromValueFunction = fmt;
  slider.setValue(def, juce::dontSendNotification);
  slider.updateText();
  label.setText(text, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(slider); addAndMakeVisible(label);
  parent.addAndMakeVisible(*this);
}
void SliceKnob::resized() {
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

// ---- forma de onda
void WaveView::setBank(const Bank& b, const juce::String& nm, const juce::String& note, int sel) {
  const bool sameData = bank.data == b.data;
  bank = b; name = nm; notice = note; selected = sel;
  if (!sameData) rebuildPeaks();
  repaint();
}
void WaveView::rebuildPeaks() {
  peaks.clear();
  if (!bank.data || bank.data->empty() || getWidth() < 8) return;
  const int bins = std::max(32, getWidth() - 4), len = bank.data->length();
  peaks.resize(static_cast<size_t>(bins));
  for (int i = 0; i < bins; ++i) {
    int a = static_cast<int>(static_cast<long long>(len) * i / bins), z = std::max(a + 1, static_cast<int>(static_cast<long long>(len) * (i + 1) / bins));
    float m = 0;
    for (int k = a; k < z && k < len; ++k) m = std::max(m, std::max(std::fabs(bank.data->l[static_cast<size_t>(k)]), std::fabs(bank.data->r[static_cast<size_t>(k)])));
    peaks[static_cast<size_t>(i)] = m;
  }
}
float WaveView::sampleToX(int s) const { return 2.0f + (static_cast<float>(getWidth()) - 4.0f) * static_cast<float>(s) / static_cast<float>(std::max(1, bank.data ? bank.data->length() : 1)); }
int WaveView::xToSample(float x) const {
  const int len = bank.data ? bank.data->length() : 0;
  return juce::jlimit(0, std::max(0, len - 1), static_cast<int>((x - 2.0f) / (static_cast<float>(getWidth()) - 4.0f) * static_cast<float>(len)));
}
int WaveView::markerNear(float x) const {
  for (size_t i = 1; i < bank.slices.size(); ++i) if (std::fabs(sampleToX(bank.slices[i].start) - x) <= 6.0f) return static_cast<int>(i);
  return -1;
}
void WaveView::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  g.setColour(kBg); g.fillRoundedRectangle(r, 8.0f);
  g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
  if (peaks.empty()) {
    g.setColour(kSoft); g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText(juce::String::fromUTF8("Arrastrá un WAV o la carpeta SCHMU_VC1"), getLocalBounds(), juce::Justification::centred);
    if (notice.isNotEmpty()) {
      g.setColour(kAcid); g.setFont(juce::Font(juce::FontOptions(12.0f)));
      g.drawFittedText(notice, getLocalBounds().reduced(20, 10).removeFromBottom(40), juce::Justification::centred, 2);
    }
    return;
  }
  const float cy = r.getCentreY(), hh = r.getHeight() * 0.42f, sx = (r.getWidth() - 4.0f) / static_cast<float>(peaks.size());
  float mx = 0.001f; for (float p : peaks) mx = std::max(mx, p);
  // zona del slice elegido
  if (selected >= 0 && selected < static_cast<int>(bank.slices.size())) {
    const auto& s = bank.slices[static_cast<size_t>(selected)];
    g.setColour(kAcid.withAlpha(0.10f));
    g.fillRect(juce::Rectangle<float>(sampleToX(s.start), r.getY() + 1.0f, sampleToX(s.end) - sampleToX(s.start), r.getHeight() - 2.0f));
  }
  for (size_t i = 0; i < peaks.size(); ++i) {
    const float a = peaks[i] / mx * hh;
    const int sampleAt = static_cast<int>(static_cast<long long>(bank.data->length()) * static_cast<long long>(i) / static_cast<long long>(peaks.size()));
    const int si = sliceAt(bank.slices, sampleAt);
    g.setColour(si < 0 ? kLine.brighter(0.2f) : (si == selected ? kAcid : kViolet));
    g.fillRect(r.getX() + 2.0f + static_cast<float>(i) * sx, cy - a, std::max(1.0f, sx - 0.4f), std::max(1.0f, a * 2.0f));
  }
  g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
  for (size_t i = 0; i < bank.slices.size(); ++i) {
    const float x = sampleToX(bank.slices[i].start);
    g.setColour(static_cast<int>(i) == selected ? kAcid : kText.withAlpha(0.7f));
    if (i > 0) g.drawLine(x, r.getY() + 14.0f, x, r.getBottom() - 2.0f, 1.0f);
    g.drawText(juce::String(i + 1), static_cast<int>(x) + 3, static_cast<int>(r.getY()) + 2, 22, 12, juce::Justification::centredLeft);
  }
  g.setColour(kSoft); g.setFont(juce::Font(juce::FontOptions(11.0f)));
  g.drawText(name, getLocalBounds().reduced(8, 4), juce::Justification::bottomRight, true);
}
void WaveView::mouseDown(const juce::MouseEvent& e) {
  if (!bank.data) return;
  const int m = markerNear(static_cast<float>(e.x));
  if (m > 0) {
    if (e.mods.isPopupMenu() || e.mods.isAltDown()) { if (onEdit) onEdit([m](Bank& b) { removeMarker(b.slices, m); }); return; }
    dragging = m; return;
  }
  const int pos = xToSample(static_cast<float>(e.x));
  if (e.mods.isShiftDown()) { if (onEdit) onEdit([pos](Bank& b) { if (b.data) insertMarker(b.slices, b.data->length(), pos); }); return; }
  const int si = sliceAt(bank.slices, pos);
  if (si >= 0) { auditioned = si; if (onSelect) onSelect(si); if (onAudition) onAudition(si, true); }
}
void WaveView::mouseDrag(const juce::MouseEvent& e) {
  if (dragging > 0 && onEdit) { const int pos = xToSample(static_cast<float>(e.x)), d = dragging; onEdit([pos, d](Bank& b) { moveMarker(b.slices, d, pos); }); }
}
void WaveView::mouseUp(const juce::MouseEvent&) {
  dragging = -1;
  if (auditioned >= 0 && onAudition) onAudition(auditioned, false);
  auditioned = -1;
}
void WaveView::mouseDoubleClick(const juce::MouseEvent& e) {
  if (!bank.data || markerNear(static_cast<float>(e.x)) > 0) return;
  const int pos = xToSample(static_cast<float>(e.x));
  if (onEdit) onEdit([pos](Bank& b) { if (b.data) insertMarker(b.slices, b.data->length(), pos); });
}

// ---- pads (4x4; el pad 1 está abajo a la izquierda, como en un MPC)
juce::Rectangle<float> PadGrid::padRect(int i) const {
  const float w = static_cast<float>(getWidth()) / 4.0f, h = static_cast<float>(getHeight()) / 4.0f;
  const int col = i % 4, row = 3 - i / 4;
  return juce::Rectangle<float>(w * static_cast<float>(col), h * static_cast<float>(row), w, h).reduced(3.0f);
}
int PadGrid::padAt(juce::Point<int> p) const {
  for (int i = 0; i < 16; ++i) if (padRect(i).contains(p.toFloat())) return i;
  return -1;
}
void PadGrid::paint(juce::Graphics& g) {
  for (int i = 0; i < 16; ++i) {
    auto r = padRect(i);
    const bool has = i < static_cast<int>(bank.slices.size());
    const bool down = proc && proc->keyboardState.isNoteOn(1, kPadBase + i);
    g.setColour(down ? kAcid : (has ? (i == selected ? kPanel2.brighter(0.25f) : kPanel2) : kPanel.darker(0.4f)));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(i == selected && has ? kAcid : kLine); g.drawRoundedRectangle(r, 9.0f, i == selected && has ? 1.6f : 1.0f);
    g.setColour(down ? kBg : (has ? kText : kSoft.withAlpha(0.4f)));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawText(juce::String(i + 1), r.toNearestInt().removeFromTop(static_cast<int>(r.getHeight() * 0.6f)), juce::Justification::centred);
    if (has) {
      const auto& p = bank.slices[static_cast<size_t>(i)].p;
      g.setFont(juce::Font(juce::FontOptions(10.0f)));
      g.setColour(down ? kBg : kSoft);
      juce::String tag = p.mode == PM_LOOP ? "LOOP" : (p.mode == PM_GATE ? "GATE" : "ONE");
      if (p.reverse) tag += " REV";
      g.drawText(tag, r.toNearestInt().removeFromBottom(static_cast<int>(r.getHeight() * 0.4f)), juce::Justification::centredTop);
    }
  }
}
void PadGrid::mouseDown(const juce::MouseEvent& e) {
  int i = padAt(e.getPosition());
  if (i < 0 || i >= static_cast<int>(bank.slices.size())) return;
  held = i;
  if (onSelect) onSelect(i);
  if (proc) proc->keyboardState.noteOn(1, kPadBase + i, 0.9f);
  repaint();
}
void PadGrid::mouseUp(const juce::MouseEvent&) {
  if (held >= 0 && proc) proc->keyboardState.noteOff(1, kPadBase + held, 0.0f);
  held = -1;
  repaint();
}

// ---- secuenciador
juce::Rectangle<float> StepRow::cellRect(int i) const {
  const float w = static_cast<float>(getWidth()) / 16.0f;
  return juce::Rectangle<float>(w * static_cast<float>(i), 0.0f, w, static_cast<float>(getHeight())).reduced(2.0f);
}
int StepRow::cellAt(juce::Point<int> p) const {
  for (int i = 0; i < 16; ++i) if (cellRect(i).contains(p.toFloat())) return i;
  return -1;
}
void StepRow::paint(juce::Graphics& g) {
  for (int i = 0; i < 16; ++i) {
    auto r = cellRect(i);
    const auto& st = bank.steps[static_cast<size_t>(i)];
    const bool on = st.slice >= 0, inLen = i < length;
    g.setColour(on ? kViolet.withAlpha(inLen ? 0.9f : 0.3f) : (i % 4 == 0 ? kPanel2.brighter(0.1f) : kPanel2));
    if (!inLen) g.setColour(kPanel.darker(0.3f));
    g.fillRoundedRectangle(r, 6.0f);
    if (on && inLen) {   // barra de probabilidad
      g.setColour(kAcid);
      g.fillRoundedRectangle(r.getX() + 4.0f, r.getBottom() - 9.0f, (r.getWidth() - 8.0f) * st.prob, 5.0f, 2.5f);   // barra = probabilidad
    }
    g.setColour(i == playStep ? kAcid : kLine); g.drawRoundedRectangle(r, 6.0f, i == playStep ? 2.0f : 1.0f);
    g.setColour(on ? kText : kSoft.withAlpha(0.5f)); g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(on ? juce::String(st.slice + 1) : juce::String(i + 1), r.toNearestInt().removeFromTop(static_cast<int>(r.getHeight() * 0.5f)), juce::Justification::centred);
  }
}
void StepRow::mouseDown(const juce::MouseEvent& e) {
  int i = cellAt(e.getPosition());
  if (i < 0 || !onEdit) return;
  const int sl = selectedSlice;
  if (e.mods.isPopupMenu() || e.mods.isAltDown()) { onEdit([i](Bank& b) { b.steps[static_cast<size_t>(i)].slice = -1; }); return; }
  onEdit([i, sl](Bank& b) {
    auto& st = b.steps[static_cast<size_t>(i)];
    if (st.slice == sl) st.slice = -1; else { st.slice = std::min(sl, std::max(0, static_cast<int>(b.slices.size()) - 1)); if (b.slices.empty()) st.slice = -1; }
  });
}
void StepRow::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  int i = cellAt(e.getPosition());
  if (i < 0 || !onEdit) return;
  const float d = w.deltaY > 0 ? 0.1f : -0.1f;
  onEdit([i, d](Bank& b) { b.steps[static_cast<size_t>(i)].prob = juce::jlimit(0.1f, 1.0f, b.steps[static_cast<size_t>(i)].prob + d); });
}

// ---------------------------------------------------------------- editor
Knob* ChopperEditor::addKnob(int param, const juce::String& text) {
  knobs_.push_back(std::make_unique<Knob>());
  knobs_.back()->init(*this, proc_.apvts, param, text);
  return knobs_.back().get();
}
Choice* ChopperEditor::addChoice(int param, const juce::String& text) {
  choices_.push_back(std::make_unique<Choice>());
  choices_.back()->init(*this, proc_.apvts, param, text);
  return choices_.back().get();
}

ChopperEditor::ChopperEditor(ChopperProcessor& p)
    : AudioProcessorEditor(&p), proc_(p),
      modeAtt_(*p.apvts.getParameter("mode"), [this](float v) { updateModeUI(static_cast<int>(v + 0.5f)); }, nullptr),
      keyboard_(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
  setLookAndFeel(&look_);

  rebuildPresetMenu();
  addAndMakeVisible(presetBox_);
  presetBox_.onChange = [this] { int id = presetBox_.getSelectedId(); if (id > 0) selectPreset(id - 1); };
  addAndMakeVisible(prev_); addAndMakeVisible(next_);
  prev_.onClick = [this] { int n = proc_.getNumPrograms(); selectPreset((proc_.getCurrentProgram() + n - 1) % n); };
  next_.onClick = [this] { selectPreset((proc_.getCurrentProgram() + 1) % proc_.getNumPrograms()); };

  static const char* modeNames[M_COUNT] = {"CHOP", "KEY", "SEQUENCE"};
  for (int i = 0; i < M_COUNT; ++i) {
    auto& b = modeBtn_[static_cast<size_t>(i)];
    b.setButtonText(modeNames[i]);
    b.setClickingTogglesState(false);
    b.onClick = [this, i] { modeAtt_.setValueAsCompleteGesture(static_cast<float>(i)); };
    addAndMakeVisible(b);
  }

  addAndMakeVisible(wave_); addAndMakeVisible(pads_); addAndMakeVisible(steps_);
  pads_.proc = &proc_;
  wave_.onSelect = [this](int i) { selectSlice(i); };
  wave_.onAudition = [this](int i, bool down) { if (down) proc_.keyboardState.noteOn(1, kPadBase + i, 0.9f); else proc_.keyboardState.noteOff(1, kPadBase + i, 0.0f); };
  wave_.onEdit = [this](const std::function<void(Bank&)>& fn) { proc_.editBank(fn); };
  pads_.onSelect = [this](int i) { selectSlice(i); };
  steps_.onEdit = [this](const std::function<void(Bank&)>& fn) { proc_.editBank(fn); };

  for (auto* b : {&fileBtn_, &folderBtn_, &chopBtn_, &evenBtn_, &clearBtn_}) addAndMakeVisible(*b);
  fileBtn_.onClick = [this] { chooseFile(); };
  folderBtn_.onClick = [this] { chooseFolder(); };
  chopBtn_.onClick = [this] { proc_.autoChop(); };
  evenBtn_.onClick = [this] { proc_.evenChop(16); };
  clearBtn_.onClick = [this] { proc_.clearChops(); };

  // slice seleccionado
  auto pct = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
  auto ms = [](double v) { return v < 1.0 ? juce::String(juce::roundToInt(v * 1000.0)) + " ms" : juce::String(v, 2) + " s"; };
  gain_.init(*this, "Gain", -24, 12, 0, [](double v) { return juce::String(v, 1) + " dB"; });
  pan_.init(*this, "Pan", -1, 1, 0, [](double v) { return v < -0.02 ? "L " + juce::String(juce::roundToInt(-v * 100.0)) : (v > 0.02 ? "R " + juce::String(juce::roundToInt(v * 100.0)) : juce::String("C")); });
  pitch_.init(*this, "Pitch", -12, 12, 0, [](double v) { return juce::String(v, 1) + " st"; });
  atk_.init(*this, "Attack", 0.001, 2.0, 0.002, ms, 0.05);
  dec_.init(*this, "Decay", 0.005, 4.0, 0.2, ms, 0.3);
  sus_.init(*this, "Sustain", 0, 1, 1, pct);
  rel_.init(*this, "Release", 0.005, 4.0, 0.08, ms, 0.2);
  cut_.init(*this, "Cutoff", 40, 18000, 8000, [](double v) { return v >= 1000 ? juce::String(v / 1000.0, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }, 1200.0);
  gain_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.gainDb = static_cast<float>(gain_.slider.getValue()); }); };
  pan_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.pan = static_cast<float>(pan_.slider.getValue()); }); };
  pitch_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.pitch = static_cast<float>(pitch_.slider.getValue()); }); };
  atk_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.attack = static_cast<float>(atk_.slider.getValue()); }); };
  dec_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.decay = static_cast<float>(dec_.slider.getValue()); }); };
  sus_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.sustain = static_cast<float>(sus_.slider.getValue()); }); };
  rel_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.release = static_cast<float>(rel_.slider.getValue()); }); };
  cut_.slider.onValueChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.cutoff = static_cast<float>(cut_.slider.getValue()); }); };
  for (auto* c : {&playMode_, &filterBox_, &chokeBox_}) addAndMakeVisible(*c);
  playMode_.addItemList({"One-shot", "Gate", "Loop"}, 1);
  filterBox_.addItemList({"Filter Off", "Low-pass", "High-pass"}, 1);
  chokeBox_.addItem(juce::String::fromUTF8("Choke: —"), 1);
  for (int i = 1; i <= 8; ++i) chokeBox_.addItem("Choke " + juce::String(i), i + 1);
  playMode_.onChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.mode = playMode_.getSelectedId() - 1; }); };
  filterBox_.onChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.filter = filterBox_.getSelectedId() - 1; }); };
  chokeBox_.onChange = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.choke = chokeBox_.getSelectedId() - 1; }); };
  addAndMakeVisible(revBtn_);
  revBtn_.setClickingTogglesState(true);
  revBtn_.onClick = [this] { if (!loadingControls_) editSelected([&](SliceParams& s) { s.reverse = revBtn_.getToggleState(); }); };

  // globales y FX
  kVol_ = addKnob(P_VOLUME, "Volume");
  kSens_ = addKnob(P_SENS, "Sensitivity");
  cLock_ = addChoice(P_PITCHLOCK, "Keep Length");
  kTrans_ = addKnob(P_ROOTOFS, "Transpose");
  kHum_ = addKnob(P_HUMANIZE, "Humanize");
  kReso_ = addKnob(P_RESO, "Filter Reso");
  kTape_ = addKnob(P_TAPE, "Tape");
  kCrush_ = addKnob(P_CRUSH, "Crush");
  kDelay_ = addKnob(P_DELAYMIX, "Delay");
  cDelayT_ = addChoice(P_DELAYDIV, "Delay Time");
  kFb_ = addKnob(P_DELAYFB, "Feedback");
  kRev_ = addKnob(P_REVERBMIX, "Reverb");
  kSize_ = addKnob(P_REVERBSIZE, "Size");
  kSwing_ = addKnob(P_SWING, "Swing");
  kSteps_ = addKnob(P_SEQLEN, "Steps");
  cRate_ = addChoice(P_SEQRATE, "Step Rate");
  kTempo_ = addKnob(P_TEMPO, "Tempo (sin host)");

  addAndMakeVisible(keyboard_);
  keyboard_.setKeyWidth(22.0f);
  keyboard_.setAvailableRange(24, 96);
  keyboard_.setLowestVisibleKey(36);

  setSize(980, 760);   // al final: resized() necesita que ya existan todos los controles
  modeAtt_.sendInitialUpdate();
  startTimerHz(15);
  timerCallback();
}

ChopperEditor::~ChopperEditor() { setLookAndFeel(nullptr); }

void ChopperEditor::rebuildPresetMenu() {
  presetBox_.clear(juce::dontSendNotification);
  std::string cat;
  const auto& L = presetList();
  for (size_t i = 0; i < L.size(); ++i) {
    if (L[i].category != cat) { cat = L[i].category; presetBox_.addSectionHeading(cat); }
    presetBox_.addItem(L[i].name, static_cast<int>(i) + 1);
  }
}

void ChopperEditor::selectPreset(int index) {
  proc_.setCurrentProgram(index);
  presetBox_.setSelectedId(index + 1, juce::dontSendNotification);
  lastProgram_ = index;
}

void ChopperEditor::editSelected(const std::function<void(SliceParams&)>& fn) {
  const int sel = selected_;
  proc_.editBank([&](Bank& b) { if (sel >= 0 && sel < static_cast<int>(b.slices.size())) fn(b.slices[static_cast<size_t>(sel)].p); });
}

void ChopperEditor::selectSlice(int i) {
  selected_ = i;
  proc_.setSelectedSlice(i);
  refreshFromBank();
  loadSelectedIntoControls();
}

void ChopperEditor::loadSelectedIntoControls() {
  loadingControls_ = true;
  const bool has = selected_ >= 0 && selected_ < static_cast<int>(bank_.slices.size());
  const SliceParams p = has ? bank_.slices[static_cast<size_t>(selected_)].p : SliceParams();
  gain_.slider.setValue(p.gainDb, juce::dontSendNotification);
  pan_.slider.setValue(p.pan, juce::dontSendNotification);
  pitch_.slider.setValue(p.pitch, juce::dontSendNotification);
  atk_.slider.setValue(p.attack, juce::dontSendNotification);
  dec_.slider.setValue(p.decay, juce::dontSendNotification);
  sus_.slider.setValue(p.sustain, juce::dontSendNotification);
  rel_.slider.setValue(p.release, juce::dontSendNotification);
  cut_.slider.setValue(p.cutoff, juce::dontSendNotification);
  playMode_.setSelectedId(p.mode + 1, juce::dontSendNotification);
  filterBox_.setSelectedId(p.filter + 1, juce::dontSendNotification);
  chokeBox_.setSelectedId(p.choke + 1, juce::dontSendNotification);
  revBtn_.setToggleState(p.reverse, juce::dontSendNotification);
  for (auto* c : std::initializer_list<juce::Component*>{&gain_, &pan_, &pitch_, &atk_, &dec_, &sus_, &rel_, &cut_, &playMode_, &filterBox_, &chokeBox_, &revBtn_}) c->setEnabled(has);
  loadingControls_ = false;
}

void ChopperEditor::refreshFromBank() {
  bank_ = proc_.bankCopy();
  lastVersion_ = proc_.bankVersion();
  selected_ = juce::jlimit(0, std::max(0, static_cast<int>(bank_.slices.size()) - 1), selected_);
  wave_.setBank(bank_, proc_.chopName(), proc_.notice(), selected_);
  pads_.bank = bank_; pads_.names = proc_.sliceNames(); pads_.selected = selected_; pads_.repaint();
  steps_.bank = bank_; steps_.selectedSlice = selected_;
  steps_.length = juce::jlimit(1, 16, static_cast<int>(proc_.apvts.getRawParameterValue("seqlen")->load() + 0.5f));
  steps_.repaint();
}

void ChopperEditor::timerCallback() {
  int cur = proc_.getCurrentProgram();
  if (cur != lastProgram_) { presetBox_.setSelectedId(cur + 1, juce::dontSendNotification); lastProgram_ = cur; }
  if (proc_.bankVersion() != lastVersion_) {
    const int ps = proc_.selectedSlice();
    if (ps != selected_ && ps >= 0) selected_ = ps;
    refreshFromBank();
    loadSelectedIntoControls();
  }
  const int len = juce::jlimit(1, 16, static_cast<int>(proc_.apvts.getRawParameterValue("seqlen")->load() + 0.5f));
  if (len != steps_.length) { steps_.length = len; steps_.repaint(); }
  pads_.repaint();
}

void ChopperEditor::updateModeUI(int m) {
  m = std::max(0, std::min(M_COUNT - 1, m));
  for (int i = 0; i < M_COUNT; ++i) modeBtn_[static_cast<size_t>(i)].setToggleState(i == m, juce::dontSendNotification);
  mode_ = m;
  steps_.setAlpha(m == M_SEQ ? 1.0f : 0.45f);
  kTrans_->setEnabled(m == M_KEY);
  cLock_->setEnabled(m == M_KEY || true);
  for (auto* k : {kSwing_, kSteps_, kTempo_}) k->setEnabled(m == M_SEQ);
  cRate_->setEnabled(m == M_SEQ);
  repaint();
}

void ChopperEditor::loadPath(const juce::File& f) {
  juce::String err;
  if (!proc_.loadChops(f, err)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "SCHMU Chopper", err);
  else selected_ = 0;
  refreshFromBank();
  loadSelectedIntoControls();
}

void ChopperEditor::filesDropped(const juce::StringArray& files, int, int) {
  if (files.isEmpty()) return;
  loadPath(juce::File(files[0]));
}

void ChopperEditor::chooseFile() {
  chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Elegí un WAV (AIFF, FLAC u OGG)"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
    auto f = fc.getResult();
    if (f != juce::File()) loadPath(f);
  });
}

void ChopperEditor::chooseFolder() {
  chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Elegí la carpeta de los chops (por ejemplo SCHMU_VC1…/01_Syllables)"), juce::File());
  chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
    auto f = fc.getResult();
    if (f != juce::File()) loadPath(f);
  });
}

void ChopperEditor::paint(juce::Graphics& g) {
  g.fillAll(kBg);
  g.setGradientFill(juce::ColourGradient(kViolet.withAlpha(0.22f), 0.0f, 0.0f, kBg, 0.0f, 260.0f, false));
  g.fillRect(0, 0, getWidth(), 260);
  g.setColour(kText); g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
  g.drawText("SCHMULENSON", 22, 12, 220, 28, juce::Justification::centredLeft);
  g.setColour(kAcid); g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
  g.drawText("S C H M U   C H O P P E R", 24, 38, 260, 16, juce::Justification::centredLeft);
  auto panel = [&](juce::Rectangle<int> r, const juce::String& title) {
    g.setColour(kPanel); g.fillRoundedRectangle(r.toFloat(), 14.0f);
    g.setColour(kLine); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 14.0f, 1.0f);
    g.setColour(kSoft); g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(title.toUpperCase(), r.getX() + 14, r.getY() + 8, 560, 14, juce::Justification::centredLeft);
  };
  panel(waveR_, juce::String::fromUTF8("Chops · clic = escuchar · doble clic = cortar · arrastrar = mover · Alt+clic = borrar"));
  panel(padsR_, juce::String::fromUTF8("Pads · C1 en adelante"));
  panel(sliceR_, juce::String::fromUTF8("Slice ") + juce::String(selected_ + 1));
  panel(seqR_, juce::String::fromUTF8("Sequence · clic = poner el slice elegido · rueda = probabilidad · clic derecho = borrar"));
  panel(fxR_, juce::String::fromUTF8("Global · FX: Tape → Crush → Delay → Reverb"));
}

void ChopperEditor::resized() {
  if (choices_.size() < 3 || knobs_.size() < 14) return;
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
  {
    int tw = 130;
    for (int i = 0; i < M_COUNT; ++i) modeBtn_[static_cast<size_t>(i)].setBounds(tabs.removeFromLeft(tw).reduced(3, 0));
    auto btns = tabs.removeFromRight(560);
    for (auto* b : {&clearBtn_, &evenBtn_, &chopBtn_, &folderBtn_, &fileBtn_}) b->setBounds(btns.removeFromRight(b == &folderBtn_ ? 140 : (b == &fileBtn_ ? 120 : 100)).reduced(3, 0));
  }
  r.removeFromTop(8);

  auto keyArea = r.removeFromBottom(58);
  r.removeFromBottom(8);
  fxR_ = r.removeFromBottom(160);
  r.removeFromBottom(8);
  seqR_ = r.removeFromBottom(78);
  r.removeFromBottom(8);
  keyboard_.setBounds(keyArea);

  auto top = r.removeFromTop(190);
  r.removeFromTop(8);
  sliceR_ = r;
  padsR_ = top.removeFromRight(300);
  top.removeFromRight(8);
  waveR_ = top;
  wave_.setBounds(waveR_.reduced(10, 26).withTrimmedBottom(2));
  pads_.setBounds(padsR_.reduced(8, 24));

  // slice: 8 perillas + 3 listas + reverse
  {
    auto a = sliceR_.reduced(10, 0); a.removeFromTop(22);
    auto combos = a.removeFromRight(190);
    SliceKnob* ks[8] = {&gain_, &pan_, &pitch_, &atk_, &dec_, &sus_, &rel_, &cut_};
    int kw = a.getWidth() / 8;
    for (auto* k : ks) k->setBounds(a.removeFromLeft(kw).reduced(2, 0));
    int h = combos.getHeight() / 4;
    playMode_.setBounds(combos.removeFromTop(h).reduced(4, 3));
    filterBox_.setBounds(combos.removeFromTop(h).reduced(4, 3));
    chokeBox_.setBounds(combos.removeFromTop(h).reduced(4, 3));
    revBtn_.setBounds(combos.removeFromTop(h).reduced(4, 3));
  }
  // secuencia
  {
    auto a = seqR_.reduced(10, 0); a.removeFromTop(22);
    steps_.setBounds(a.reduced(0, 2));
  }
  // globales: dos filas
  {
    auto a = fxR_.reduced(8, 0); a.removeFromTop(22);
    auto row1 = a.removeFromTop(a.getHeight() / 2), row2 = a;
    auto layout = [](juce::Rectangle<int> row, std::vector<juce::Component*> items) {
      const int cw = row.getWidth() / static_cast<int>(items.size());
      for (auto* c : items) {
        auto cell = row.removeFromLeft(cw).reduced(2);
        const bool isChoice = dynamic_cast<schmu_ui::Choice*>(c) != nullptr;
        c->setBounds(isChoice ? cell.reduced(4, 6) : cell);
      }
    };
    layout(row1, {kVol_, kSens_, cLock_, kTrans_, kHum_, kReso_, kTape_, kCrush_, kDelay_, cDelayT_});
    layout(row2, {kFb_, kRev_, kSize_, cRate_, kSteps_, kSwing_, kTempo_});
  }
}
