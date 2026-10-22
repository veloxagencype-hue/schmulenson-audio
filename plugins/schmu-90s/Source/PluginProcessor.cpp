#include "PluginProcessor.h"

#include "PluginEditor.h"

using namespace schmu;

namespace {
juce::String formatValue(const ParamDef& d, float v) {
  juce::String u(d.unit);
  if (u == "s") return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms" : juce::String(v, 2) + " s";
  if (u == "Hz") return v >= 1000.0f ? juce::String(v / 1000.0f, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
  if (u == "st") return juce::String(v, 2) + " st";
  if (d.min == 0.5f && d.max == 0.75f) return juce::String(juce::roundToInt(v * 100.0f)) + " %";  // swing
  if (d.min == -1.0f) return juce::String(v, 2);
  return juce::String(juce::roundToInt(v * 100.0f)) + " %";
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SchmuProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  for (int i = 0; i < P_COUNT; ++i) {
    const ParamDef& d = paramTable()[i];
    juce::ParameterID pid{d.id, 1};
    if (d.choices) {
      juce::StringArray names;
      for (const auto& c : choiceList(i)) names.add(c);
      layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, names, static_cast<int>(d.def)));
    } else {
      juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
      auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction([d](float v, int) { return formatValue(d, v); });
      layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, range, d.def, attrs));
    }
  }
  return layout;
}

SchmuProcessor::SchmuProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SCHMU90s", createLayout()) {
  for (int i = 0; i < P_COUNT; ++i) raw_[i] = apvts.getRawParameterValue(paramTable()[i].id);
  chopName_ = juce::String::fromUTF8("Chop de fábrica (formantes)");
  shownChop_ = engine_.sample();
  setCurrentProgram(0);
}

bool SchmuProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
  return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
}

void SchmuProcessor::prepareToPlay(double sr, int) {
  sampleRate_ = sr;
  engine_.prepare(sr);
  swing_.reset();
  sampleCounter_ = 0;
}

void SchmuProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  buffer.clear();
  if (buffer.getNumChannels() < 2) return;

  keyboardState.processNextMidiBuffer(midi, 0, n, true);   // notas del teclado en pantalla

  for (int i = 0; i < P_COUNT; ++i) engine_.setParam(i, raw_[i]->load());

  double bpm = 120.0, ppq0 = -1.0;
  bool playing = false;
  if (auto* ph = getPlayHead()) {
    if (auto pos = ph->getPosition()) {
      if (auto b = pos->getBpm()) bpm = *b;
      playing = pos->getIsPlaying();
      if (playing) if (auto p = pos->getPpqPosition()) ppq0 = *p;
    }
  }
  engine_.setTempo(bpm);
  const double swingAmt = raw_[P_SWING]->load();

  for (const auto meta : midi) {
    const auto m = meta.getMessage();
    const int pos = meta.samplePosition;
    const long long at = sampleCounter_ + pos;
    const double ppq = (playing && ppq0 >= 0.0) ? ppq0 + (static_cast<double>(pos) / sampleRate_) * bpm / 60.0 : -1.0;
    if (m.isNoteOn()) swing_.add(true, m.getNoteNumber(), m.getFloatVelocity(), at, ppq, bpm, sampleRate_, swingAmt);
    else if (m.isNoteOff()) swing_.add(false, m.getNoteNumber(), 0.0f, at, ppq, bpm, sampleRate_, swingAmt);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine_.allNotesOff(); swing_.reset(); }
  }

  float* L = buffer.getWritePointer(0);
  float* R = buffer.getWritePointer(1);
  int done = 0;
  while (done < n) {
    const long long now = sampleCounter_ + done;
    SwingScheduler::Event ev;
    while (swing_.pop(now + 1, ev)) {
      if (ev.on) engine_.noteOn(ev.note, ev.vel); else engine_.noteOff(ev.note);
    }
    int len = n - done;
    long long nextDue = 0;
    if (swing_.peek(nextDue)) len = static_cast<int>(std::max<long long>(1, std::min<long long>(len, nextDue - now)));
    engine_.process(L + done, R + done, len);
    done += len;
  }
  sampleCounter_ += n;
  midi.clear();
}

juce::AudioProcessorEditor* SchmuProcessor::createEditor() { return new SchmuEditor(*this); }

const juce::String SchmuProcessor::getProgramName(int index) {
  const auto& L = presetList();
  if (index < 0 || index >= static_cast<int>(L.size())) return {};
  return L[static_cast<size_t>(index)].name;
}

void SchmuProcessor::setCurrentProgram(int index) {
  const auto& L = presetList();
  if (index < 0 || index >= static_cast<int>(L.size())) return;
  currentProgram_ = index;
  const auto vals = presetValues(L[static_cast<size_t>(index)]);
  for (int i = 0; i < P_COUNT; ++i) {
    if (auto* p = apvts.getParameter(paramTable()[i].id)) p->setValueNotifyingHost(p->convertTo0to1(vals[static_cast<size_t>(i)]));
  }
  engine_.allNotesOff();
}

void SchmuProcessor::getStateInformation(juce::MemoryBlock& dest) {
  auto state = apvts.copyState();
  state.setProperty("program", currentProgram_, nullptr);
  state.setProperty("chopPath", chopPath_, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void SchmuProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size)) {
    if (xml->hasTagName(apvts.state.getType())) {
      auto tree = juce::ValueTree::fromXml(*xml);
      currentProgram_ = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty("program", 0)));
      const juce::String path = tree.getProperty("chopPath", "").toString();
      apvts.replaceState(tree);
      if (path.isNotEmpty()) {
        juce::File f(path);
        juce::String err;
        if (f.existsAsFile()) loadChop(f, err);   // si el archivo se movió, queda el chop de fábrica
      }
    }
  }
}

bool SchmuProcessor::loadChop(const juce::File& file, juce::String& error) {
  juce::AudioFormatManager fm;
  fm.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
  if (!reader) { error = juce::String::fromUTF8("No se pudo leer el archivo (usá WAV, AIFF, FLAC u OGG)."); return false; }
  const juce::int64 maxLen = static_cast<juce::int64>(reader->sampleRate * 30.0);
  const int len = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, maxLen));
  if (len < 64) { error = "El archivo es demasiado corto."; return false; }
  juce::AudioBuffer<float> buf(static_cast<int>(std::max<unsigned>(2u, reader->numChannels)), len);
  buf.clear();
  reader->read(&buf, 0, len, 0, true, true);
  auto data = std::make_shared<SampleData>();
  data->sampleRate = reader->sampleRate;
  data->l.assign(buf.getReadPointer(0), buf.getReadPointer(0) + len);
  data->r.assign(buf.getReadPointer(reader->numChannels > 1 ? 1 : 0), buf.getReadPointer(reader->numChannels > 1 ? 1 : 0) + len);
  shownChop_ = data;
  engine_.setSample(data);
  chopName_ = file.getFileName();
  chopPath_ = file.getFullPathName();
  return true;
}

void SchmuProcessor::resetChop() {
  auto d = makeDefaultChop(44100.0);
  shownChop_ = d;
  engine_.setSample(d);
  chopName_ = juce::String::fromUTF8("Chop de fábrica (formantes)");
  chopPath_.clear();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SchmuProcessor(); }
