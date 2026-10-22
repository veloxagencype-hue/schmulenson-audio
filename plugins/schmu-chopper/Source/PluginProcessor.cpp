#include "PluginProcessor.h"

#include "PluginEditor.h"

using namespace schmu;

namespace {
juce::String formatValue(const ParamDef& d, float v) {
  juce::String u(d.unit);
  if (u == "s") return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms" : juce::String(v, 2) + " s";
  if (u == "st") return juce::String(v, 1) + " st";
  if (d.min == 0.5f && d.max == 0.75f) return juce::String(juce::roundToInt(v * 100.0f)) + " %";   // swing
  if (juce::String(d.id) == "tempo") return juce::String(juce::roundToInt(v)) + " BPM";
  if (juce::String(d.id) == "seqlen") return juce::String(juce::roundToInt(v));
  return juce::String(juce::roundToInt(v * 100.0f)) + " %";
}
const char* kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg";
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ChopperProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  for (int i = 0; i < P_COUNT; ++i) {
    const ParamDef& d = paramTable()[static_cast<size_t>(i)];
    juce::ParameterID pid{d.id, 1};
    if (d.choices) {
      juce::StringArray names;
      for (const auto& c : choiceList(i)) names.add(c);
      layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, names, static_cast<int>(d.def)));
    } else if (juce::String(d.id) == "seqlen") {
      layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, 1, 16, 16));
    } else {
      juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
      auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction([d](float v, int) { return formatValue(d, v); });
      layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, range, d.def, attrs));
    }
  }
  return layout;
}

ChopperProcessor::ChopperProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SCHMUChopper", createLayout()) {
  for (int i = 0; i < P_COUNT; ++i) raw_[static_cast<size_t>(i)] = apvts.getRawParameterValue(paramTable()[static_cast<size_t>(i)].id);
  chopName_ = {};
  uiBank_.slices.reserve(kMaxSlices);
  setCurrentProgram(0);
}

bool ChopperProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
  return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
}

void ChopperProcessor::prepareToPlay(double sr, int) {
  sampleRate_ = sr;
  engine_.prepare(sr);
  const juce::SpinLock::ScopedLockType l(lock_);
  dirty_ = true;
}

void ChopperProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  buffer.clear();
  if (buffer.getNumChannels() < 2) return;

  keyboardState.processNextMidiBuffer(midi, 0, n, true);   // pads y teclas de la pantalla

  for (int i = 0; i < P_COUNT; ++i) engine_.setParam(i, raw_[static_cast<size_t>(i)]->load());
  {
    const juce::SpinLock::ScopedTryLockType l(lock_);       // si la UI está escribiendo justo ahora, queda para el próximo bloque
    if (l.isLocked() && dirty_) { engine_.setBank(uiBank_); dirty_ = false; }
  }

  bool hasHost = false, playing = false;
  double bpm = 120.0, ppq0 = 0.0;
  if (auto* ph = getPlayHead()) {
    if (auto pos = ph->getPosition()) {
      if (auto b = pos->getBpm()) bpm = *b;
      playing = pos->getIsPlaying();
      if (auto p = pos->getPpqPosition()) { ppq0 = *p; hasHost = true; }
    }
  }
  const double ppqPerSample = bpm / 60.0 / sampleRate_;

  float* L = buffer.getWritePointer(0);
  float* R = buffer.getWritePointer(1);
  int done = 0;
  auto renderTo = [&](int upTo) {
    while (done < upTo) {
      const int len = upTo - done;
      engine_.setHost(hasHost, playing, ppq0 + done * ppqPerSample, bpm);
      engine_.process(L + done, R + done, len);
      done += len;
    }
  };
  for (const auto meta : midi) {
    const auto m = meta.getMessage();
    renderTo(std::min(n, meta.samplePosition));
    if (m.isNoteOn()) { engine_.setHost(hasHost, playing, ppq0 + done * ppqPerSample, bpm); engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity()); }
    else if (m.isNoteOff()) engine_.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine_.allNotesOff();
  }
  renderTo(n);
  midi.clear();
}

juce::AudioProcessorEditor* ChopperProcessor::createEditor() { return new ChopperEditor(*this); }

const juce::String ChopperProcessor::getProgramName(int index) {
  const auto& L = presetList();
  if (index < 0 || index >= static_cast<int>(L.size())) return {};
  return L[static_cast<size_t>(index)].name;
}

void ChopperProcessor::applyPreset(const Preset& p) {
  const auto vals = presetValues(p);
  for (int i = 0; i < P_COUNT; ++i) {
    if (i == P_SENS) continue;   // la sensibilidad es del chopeo, no del preset
    if (auto* prm = apvts.getParameter(paramTable()[static_cast<size_t>(i)].id)) prm->setValueNotifyingHost(prm->convertTo0to1(vals[static_cast<size_t>(i)]));
  }
  {
    const juce::SpinLock::ScopedLockType l(lock_);
    template_ = p.slice;
  }
  editBank([&](Bank& b) {
    for (auto& s : b.slices) { const float pan = s.p.pan; s.p = p.slice; s.p.pan = pan; }
    if (p.hasPattern) b.steps = presetSteps(p);
  });
}

void ChopperProcessor::setCurrentProgram(int index) {
  const auto& L = presetList();
  if (index < 0 || index >= static_cast<int>(L.size())) return;
  currentProgram_ = index;
  applyPreset(L[static_cast<size_t>(index)]);
  engine_.allNotesOff();
}

// ------------------------------------------------------------------ banco
void ChopperProcessor::publishLocked() { dirty_ = true; ++version_; }

int ChopperProcessor::pathKey() const {   // clase de tono que dice el nombre del archivo cargado (con lock_ tomado)
  const std::string p = chopPath_.toStdString();
  const size_t cut = p.find_last_of("/\\");
  return keyFromFilename(cut == std::string::npos ? p : p.substr(cut + 1));
}

schmu::Bank ChopperProcessor::bankCopy() const { const juce::SpinLock::ScopedLockType l(lock_); return uiBank_; }

void ChopperProcessor::editBank(const std::function<void(Bank&)>& fn) {
  const juce::SpinLock::ScopedLockType l(lock_);
  fn(uiBank_);
  publishLocked();
}

void ChopperProcessor::setSelectedSlice(int i) {
  selected_.store(i);
  editBank([i](Bank& b) { b.keySlice = i; });
}

bool ChopperProcessor::readFile(const juce::File& f, SampleData& out, juce::String& error) const {
  juce::AudioFormatManager fm;
  fm.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
  if (!reader) { error = juce::String::fromUTF8("No se pudo leer el archivo (usá WAV, AIFF, FLAC u OGG)."); return false; }
  const juce::int64 maxLen = static_cast<juce::int64>(reader->sampleRate * 60.0);
  const int len = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, maxLen));
  if (len < 256) { error = "El archivo es demasiado corto."; return false; }
  juce::AudioBuffer<float> buf(static_cast<int>(std::max<unsigned>(2u, reader->numChannels)), len);
  buf.clear();
  reader->read(&buf, 0, len, 0, true, true);
  out.sampleRate = reader->sampleRate;
  out.l.assign(buf.getReadPointer(0), buf.getReadPointer(0) + len);
  const int rc = reader->numChannels > 1 ? 1 : 0;
  out.r.assign(buf.getReadPointer(rc), buf.getReadPointer(rc) + len);
  return true;
}

void ChopperProcessor::setBankLocked(Bank b, const juce::String& name, const juce::StringArray& names) {
  if (uiBank_.data) retired_.push_back(uiBank_.data);
  retired_.erase(std::remove_if(retired_.begin(), retired_.end(), [](const auto& p) { return p.use_count() <= 1; }), retired_.end());
  const auto keepSteps = uiBank_.steps;
  const auto keepParams = uiBank_.slices;
  b.steps = keepSteps;
  for (size_t i = 0; i < b.slices.size(); ++i) b.slices[i].p = i < keepParams.size() ? keepParams[i].p : template_;   // conserva el sonido de cada pad
  uiBank_ = std::move(b);
  uiBank_.keySlice = std::min(uiBank_.keySlice, std::max(0, static_cast<int>(uiBank_.slices.size()) - 1));
  chopName_ = name; sliceNames_ = names; notice_.clear();
  publishLocked();
}

juce::File ChopperProcessor::libraryFile() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Schmulenson").getChildFile("SCHMU Chopper").getChildFile("libreria.txt");
}

juce::File ChopperProcessor::resolvePath(const juce::String& absolute, const juce::String& relative) {
  if (absolute.isNotEmpty()) { juce::File f(absolute); if (f.exists()) return f; }
  if (relative.isNotEmpty()) {
    auto lib = libraryFile();
    if (lib.existsAsFile()) {
      juce::File dir(lib.loadFileAsString().trim());
      juce::File f = dir.getChildFile(relative);
      if (f.exists()) return f;
    }
  }
  return {};
}

bool ChopperProcessor::loadChops(const juce::File& fileOrFolder, juce::String& error) {
  SampleData data;
  std::vector<Slice> slices;
  juce::StringArray names;
  juce::String name;
  if (fileOrFolder.isDirectory()) {
    juce::Array<juce::File> files = fileOrFolder.findChildFiles(juce::File::findFiles, true, kAudioWildcard);
    struct Cmp { static int compareElements(const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural(b.getFullPathName()); } } cmp;
    files.sort(cmp);
    if (files.isEmpty()) { error = juce::String::fromUTF8("La carpeta no tiene archivos de audio."); return false; }
    if (files.size() > kMaxSlices) files.removeRange(kMaxSlices, files.size() - kMaxSlices);
    const int gap = 441;
    for (const auto& f : files) {
      SampleData one;
      if (!readFile(f, one, error)) return false;
      if (data.empty()) data.sampleRate = one.sampleRate;
      if (std::fabs(one.sampleRate - data.sampleRate) > 1.0) {   // pasa todo a la frecuencia del primero (interpolación lineal)
        const double ratio = one.sampleRate / data.sampleRate;
        const int nn = static_cast<int>(one.length() / ratio);
        std::vector<float> a(static_cast<size_t>(nn)), b(static_cast<size_t>(nn));
        for (int i = 0; i < nn; ++i) {
          const double p = i * ratio; const int i0 = static_cast<int>(p); const int i1 = std::min(one.length() - 1, i0 + 1); const float fr = static_cast<float>(p - i0);
          a[static_cast<size_t>(i)] = one.l[static_cast<size_t>(i0)] + (one.l[static_cast<size_t>(i1)] - one.l[static_cast<size_t>(i0)]) * fr;
          b[static_cast<size_t>(i)] = one.r[static_cast<size_t>(i0)] + (one.r[static_cast<size_t>(i1)] - one.r[static_cast<size_t>(i0)]) * fr;
        }
        one.l = std::move(a); one.r = std::move(b); one.sampleRate = data.sampleRate;
      }
      Slice s; s.start = data.length(); s.end = s.start + one.length();
      data.l.insert(data.l.end(), one.l.begin(), one.l.end()); data.r.insert(data.r.end(), one.r.begin(), one.r.end());
      data.l.insert(data.l.end(), static_cast<size_t>(gap), 0.0f); data.r.insert(data.r.end(), static_cast<size_t>(gap), 0.0f);
      s.root = rootNoteFor(data, s.start, s.end, keyFromFilename(f.getFileName().toStdString()));
      slices.push_back(s);
      names.add(f.getFileNameWithoutExtension());
    }
    name = fileOrFolder.getFileName() + " (" + juce::String(files.size()) + ")";
  } else {
    if (!readFile(fileOrFolder, data, error)) return false;
    slices = autoChop_(data);
    const int pc = keyFromFilename(fileOrFolder.getFileName().toStdString());
    for (auto& s : slices) s.root = rootNoteFor(data, s.start, s.end, pc);
    for (size_t i = 0; i < slices.size(); ++i) names.add(juce::String(i + 1));
    name = fileOrFolder.getFileName();
  }
  Bank b;
  b.data = std::make_shared<SampleData>(std::move(data));
  b.slices = std::move(slices);
  {   // ruta recordada (absoluta y relativa a la carpeta del pack) + biblioteca
    juce::File p = fileOrFolder;
    juce::String rel;
    for (juce::File q = p; q.getParentDirectory() != q; q = q.getParentDirectory()) {
      if (q.getFileName().startsWith("SCHMU_VC1")) {
        rel = p.getRelativePathFrom(q.getParentDirectory()).replaceCharacter('\\', '/');
        auto lib = libraryFile();
        lib.getParentDirectory().createDirectory();
        lib.replaceWithText(q.getParentDirectory().getFullPathName());
        break;
      }
    }
    const juce::SpinLock::ScopedLockType l(lock_);
    chopPath_ = p.getFullPathName(); chopRel_ = rel;
    setBankLocked(std::move(b), name, names);
    selected_.store(0);
  }
  return true;
}

std::vector<Slice> ChopperProcessor::autoChop_(const SampleData& d) const {
  return schmu::autoChop(d, raw_[P_SENS]->load());
}

void ChopperProcessor::autoChop() {
  editBank([this](Bank& b) {
    if (!b.data) return;
    auto s = autoChop_(*b.data);
    const int pc = pathKey();
    for (size_t i = 0; i < s.size(); ++i) { s[i].root = rootNoteFor(*b.data, s[i].start, s[i].end, pc); s[i].p = i < b.slices.size() ? b.slices[i].p : template_; }
    b.slices = s;
    sliceNames_.clear();
    for (size_t i = 0; i < s.size(); ++i) sliceNames_.add(juce::String(i + 1));
    b.keySlice = std::min(b.keySlice, std::max(0, static_cast<int>(s.size()) - 1));
  });
}

void ChopperProcessor::evenChop(int n) {
  editBank([this, n](Bank& b) {
    if (!b.data) return;
    auto s = schmu::evenChop(*b.data, n);
    const int pc = pathKey();
    for (size_t i = 0; i < s.size(); ++i) { s[i].root = rootNoteFor(*b.data, s[i].start, s[i].end, pc); s[i].p = i < b.slices.size() ? b.slices[i].p : template_; }
    b.slices = s;
    sliceNames_.clear();
    for (size_t i = 0; i < s.size(); ++i) sliceNames_.add(juce::String(i + 1));
    b.keySlice = std::min(b.keySlice, std::max(0, static_cast<int>(s.size()) - 1));
  });
}

void ChopperProcessor::clearChops() {
  const juce::SpinLock::ScopedLockType l(lock_);
  if (uiBank_.data) retired_.push_back(uiBank_.data);
  uiBank_.data.reset(); uiBank_.slices.clear();
  chopName_ = {}; chopPath_ = {}; chopRel_ = {}; sliceNames_.clear(); notice_.clear();
  publishLocked();
}

// ------------------------------------------------------------------ estado
juce::String ChopperProcessor::encodeSample(const SampleData& d) const {
  bool mono = d.l == d.r;
  juce::MemoryBlock mb;
  const int n = d.length();
  mb.setSize(static_cast<size_t>(n) * (mono ? 1 : 2) * 2);
  auto* p = static_cast<juce::int16*>(mb.getData());
  for (int i = 0; i < n; ++i) {
    p[mono ? i : 2 * i] = static_cast<juce::int16>(juce::roundToInt(juce::jlimit(-1.0f, 1.0f, d.l[static_cast<size_t>(i)]) * 32767.0f));
    if (!mono) p[2 * i + 1] = static_cast<juce::int16>(juce::roundToInt(juce::jlimit(-1.0f, 1.0f, d.r[static_cast<size_t>(i)]) * 32767.0f));
  }
  return mb.toBase64Encoding();
}

bool ChopperProcessor::decodeSample(const juce::String& b64, bool mono, double sr, SampleData& out) const {
  juce::MemoryBlock mb;
  if (!mb.fromBase64Encoding(b64)) return false;
  const int n = static_cast<int>(mb.getSize() / 2 / (mono ? 1 : 2));
  if (n < 64) return false;
  const auto* p = static_cast<const juce::int16*>(mb.getData());
  out.sampleRate = sr; out.l.resize(static_cast<size_t>(n)); out.r.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    out.l[static_cast<size_t>(i)] = p[mono ? i : 2 * i] / 32767.0f;
    out.r[static_cast<size_t>(i)] = mono ? out.l[static_cast<size_t>(i)] : p[2 * i + 1] / 32767.0f;
  }
  return true;
}

void ChopperProcessor::getStateInformation(juce::MemoryBlock& dest) {
  auto state = apvts.copyState();
  state.setProperty("program", currentProgram_, nullptr);
  juce::ValueTree bank("BANK");
  {
    const juce::SpinLock::ScopedLockType l(lock_);
    bank.setProperty("name", chopName_, nullptr);
    bank.setProperty("path", chopPath_, nullptr);
    bank.setProperty("rel", chopRel_, nullptr);
    bank.setProperty("keySlice", uiBank_.keySlice, nullptr);
    bank.setProperty("names", sliceNames_.joinIntoString("|"), nullptr);
    if (uiBank_.data) {
      bank.setProperty("sr", uiBank_.data->sampleRate, nullptr);
      bank.setProperty("length", uiBank_.data->length(), nullptr);
      // los chops chicos (< 2 MB) viajan dentro del proyecto; los grandes se vuelven a leer por ruta (relativa al pack si hace falta)
      const size_t bytes = static_cast<size_t>(uiBank_.data->length()) * 2 * (uiBank_.data->l == uiBank_.data->r ? 1 : 2);
      if (bytes <= 2u * 1024u * 1024u) {
        bank.setProperty("mono", uiBank_.data->l == uiBank_.data->r, nullptr);
        bank.setProperty("data", encodeSample(*uiBank_.data), nullptr);
      }
    }
    for (const auto& s : uiBank_.slices) {
      juce::ValueTree v("SLICE");
      v.setProperty("start", s.start, nullptr); v.setProperty("end", s.end, nullptr); v.setProperty("root", s.root, nullptr);
      v.setProperty("gain", s.p.gainDb, nullptr); v.setProperty("pan", s.p.pan, nullptr); v.setProperty("pitch", s.p.pitch, nullptr);
      v.setProperty("rev", s.p.reverse, nullptr); v.setProperty("mode", s.p.mode, nullptr);
      v.setProperty("a", s.p.attack, nullptr); v.setProperty("d", s.p.decay, nullptr); v.setProperty("s", s.p.sustain, nullptr); v.setProperty("r", s.p.release, nullptr);
      v.setProperty("filt", s.p.filter, nullptr); v.setProperty("cut", s.p.cutoff, nullptr); v.setProperty("choke", s.p.choke, nullptr);
      bank.appendChild(v, nullptr);
    }
    for (size_t i = 0; i < uiBank_.steps.size(); ++i) {
      juce::ValueTree v("STEP");
      v.setProperty("slice", uiBank_.steps[i].slice, nullptr); v.setProperty("prob", uiBank_.steps[i].prob, nullptr); v.setProperty("pitch", uiBank_.steps[i].pitch, nullptr);
      bank.appendChild(v, nullptr);
    }
  }
  state.removeChild(state.getChildWithName("BANK"), nullptr);
  state.appendChild(bank, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void ChopperProcessor::setStateInformation(const void* data, int size) {
  auto xml = getXmlFromBinary(data, size);
  if (!xml || !xml->hasTagName(apvts.state.getType())) return;
  auto tree = juce::ValueTree::fromXml(*xml);
  currentProgram_ = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty("program", 0)));
  auto bank = tree.getChildWithName("BANK");
  tree.removeChild(bank, nullptr);
  apvts.replaceState(tree);
  if (!bank.isValid()) return;

  Bank b;
  juce::StringArray names;
  names.addTokens(bank.getProperty("names").toString(), "|", "");
  juce::String name = bank.getProperty("name").toString(), path = bank.getProperty("path").toString(), rel = bank.getProperty("rel").toString(), notice;
  SampleData sd;
  bool have = false;
  if (bank.hasProperty("data")) have = decodeSample(bank.getProperty("data").toString(), static_cast<bool>(bank.getProperty("mono", true)), static_cast<double>(bank.getProperty("sr", 44100.0)), sd);
  if (!have && (path.isNotEmpty() || rel.isNotEmpty())) {
    juce::File f = resolvePath(path, rel);
    juce::String err;
    if (f != juce::File() && f.isDirectory()) {
      // carpeta: se vuelve a armar igual que la primera vez (mismo orden y mismo tope)
      juce::Array<juce::File> files = f.findChildFiles(juce::File::findFiles, true, kAudioWildcard);
      struct Cmp { static int compareElements(const juce::File& a, const juce::File& c) { return a.getFullPathName().compareNatural(c.getFullPathName()); } } cmp;
      files.sort(cmp);
      if (files.size() > kMaxSlices) files.removeRange(kMaxSlices, files.size() - kMaxSlices);
      for (const auto& fl : files) {
        SampleData one;
        if (!readFile(fl, one, err)) { sd = {}; break; }
        if (sd.empty()) sd.sampleRate = one.sampleRate;
        sd.l.insert(sd.l.end(), one.l.begin(), one.l.end()); sd.r.insert(sd.r.end(), one.r.begin(), one.r.end());
        sd.l.insert(sd.l.end(), 441, 0.0f); sd.r.insert(sd.r.end(), 441, 0.0f);
      }
      have = !sd.empty() && sd.length() == static_cast<int>(bank.getProperty("length", 0));
    } else if (f != juce::File()) have = readFile(f, sd, err);
    if (!have) notice = juce::String::fromUTF8("No se encontró ") + (name.isNotEmpty() ? name : path) + juce::String::fromUTF8(". Arrastrá de nuevo el WAV o la carpeta SCHMU_VC1.");
  }
  if (have) b.data = std::make_shared<SampleData>(std::move(sd));
  for (int i = 0; i < bank.getNumChildren(); ++i) {
    auto v = bank.getChild(i);
    if (v.hasType("SLICE") && b.slices.size() < static_cast<size_t>(kMaxSlices)) {
      Slice s;
      s.start = v.getProperty("start"); s.end = v.getProperty("end"); s.root = v.getProperty("root", -1);
      s.p.gainDb = v.getProperty("gain", 0.0f); s.p.pan = v.getProperty("pan", 0.0f); s.p.pitch = v.getProperty("pitch", 0.0f);
      s.p.reverse = static_cast<bool>(v.getProperty("rev", false)); s.p.mode = v.getProperty("mode", 0);
      s.p.attack = v.getProperty("a", 0.002f); s.p.decay = v.getProperty("d", 0.2f); s.p.sustain = v.getProperty("s", 1.0f); s.p.release = v.getProperty("r", 0.08f);
      s.p.filter = v.getProperty("filt", 0); s.p.cutoff = v.getProperty("cut", 8000.0f); s.p.choke = v.getProperty("choke", 0);
      if (b.data && (s.start < 0 || s.end > b.data->length() || s.end - s.start < 16)) continue;   // estado dañado o sample distinto
      b.slices.push_back(s);
    }
  }
  int si = 0;
  for (int i = 0; i < bank.getNumChildren() && si < 16; ++i) {
    auto v = bank.getChild(i);
    if (v.hasType("STEP")) { b.steps[static_cast<size_t>(si)].slice = v.getProperty("slice", -1); b.steps[static_cast<size_t>(si)].prob = v.getProperty("prob", 1.0f); b.steps[static_cast<size_t>(si)].pitch = v.getProperty("pitch", 0); ++si; }
  }
  b.keySlice = bank.getProperty("keySlice", 0);
  const juce::SpinLock::ScopedLockType l(lock_);
  if (uiBank_.data) retired_.push_back(uiBank_.data);
  uiBank_ = std::move(b);
  chopName_ = name; chopPath_ = path; chopRel_ = rel; sliceNames_ = names; notice_ = notice;
  selected_.store(juce::jlimit(0, std::max(0, static_cast<int>(uiBank_.slices.size()) - 1), uiBank_.keySlice));
  publishLocked();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChopperProcessor(); }
