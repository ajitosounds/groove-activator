#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
constexpr int SCOPE = 1024;
int64_t panelNoteId(const juce::String& s) { return (int64_t) (std::hash<std::string>{}(s.toStdString()) & 0x3fffffffffffLL) + 1; }
int64_t midiNoteId(int ch, int note) { return -(int64_t) (ch * 128 + note) - 1; }
}

juce::AudioProcessor::BusesProperties VCS3Processor::buses() {
    auto b = BusesProperties();
   #if JOY16_FX
    b = b.withInput("Input", juce::AudioChannelSet::stereo(), true);
   #endif
    return b.withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

VCS3Processor::VCS3Processor()
    : AudioProcessor(buses())
{
   #if JOY16_FX
    const juce::String def(BinaryData::default_snapshot_fx_json, BinaryData::default_snapshot_fx_jsonSize);
   #else
    const juce::String def(BinaryData::default_snapshot_json, BinaryData::default_snapshot_jsonSize);
   #endif
    const auto snap = juce::JSON::parse(def);
    if (auto* p = snap["p"].getDynamicObject())
        for (auto& prop : p->getProperties()) paramNames.push_back(prop.name.toString().toStdString());
    applySnapshot(snap);
    outRing.assign(SCOPE, 0.f); meterRing.assign(SCOPE, 0.f);
}

bool VCS3Processor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
   #if JOY16_FX
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
   #else
    return true;
   #endif
}

void VCS3Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, paramNames);
    meterBuf.assign((size_t) juce::jmax(samplesPerBlock, 1), 0.f);
    paramsDirty = pinsDirty = dkDirty = voicesDirty = stickDirty = true;
    syncHostState();
}

// ---- state from the panel (message thread) ----
void VCS3Processor::applySnapshot(const juce::var& snap) {
    {
        const juce::SpinLock::ScopedLockType sl(stateLock);
        if (auto* p = snap["p"].getDynamicObject())
            for (auto& prop : p->getProperties()) hostState.params[prop.name.toString().toStdString()] = (double) prop.value;
        const auto pins = snap["pins"];
        hostState.ps.clear(); hostState.pd.clear(); hostState.pg.clear();
        if (auto* s = pins["s"].getArray()) for (auto& v : *s) hostState.ps.push_back((int) v);
        if (auto* d = pins["d"].getArray()) for (auto& v : *d) hostState.pd.push_back((int) v);
        if (auto* g = pins["g"].getArray()) for (auto& v : *g) hostState.pg.push_back((double) v);
        const auto n = juce::jmin(hostState.ps.size(), hostState.pd.size(), hostState.pg.size());
        hostState.ps.resize(n); hostState.pd.resize(n); hostState.pg.resize(n);
        const auto dk = snap["dk"];
        hostState.in1Sig = (bool) dk["in1Sig"]; hostState.in2Key = (bool) dk["in2Key"];
        hostState.in1Ext = JOY16_FX && (bool) dk["in1Ext"]; hostState.in2Ext = JOY16_FX && (bool) dk["in2Ext"];
        hostState.highPriority = dk["priority"].toString() != "last";
        hostState.voices = juce::jlimit(1, vcs3::MAXV, (int) snap.getProperty("voices", 1));
        if (auto* st = snap["stick"].getArray(); st && st->size() == 2) { hostState.stickX = (*st)[0]; hostState.stickY = (*st)[1]; }
    }
    paramsDirty = pinsDirty = dkDirty = voicesDirty = stickDirty = true;
    const juce::ScopedLock sl(snapLock);
    snapshotJson = juce::JSON::toString(snap, true);
}

juce::var VCS3Processor::currentSnapshot() const {
    juce::var v;
    { const juce::ScopedLock sl(snapLock); v = juce::JSON::parse(snapshotJson); }
    if (auto* o = v.getDynamicObject()) o->setProperty("fx", (bool) JOY16_FX); // tells the panel which edition it is in
    return v;
}

void VCS3Processor::push(const Cmd& c) {
    const auto scope = fifo.write(1);
    if (scope.blockSize1 > 0) cmds[(size_t) scope.startIndex1] = c;
}

void VCS3Processor::handlePanelMessage(const juce::var& m) {
    const auto t = m["t"].toString();
    if (t == "hello") { if (onRestore) onRestore(); return; }
    if (t == "snapshot") { const juce::ScopedLock sl(snapLock); snapshotJson = juce::JSON::toString(m["data"], true); return; }
    if (t == "p") {
        if (auto* o = m["v"].getDynamicObject()) {
            const juce::SpinLock::ScopedLockType sl(stateLock);
            for (auto& prop : o->getProperties()) hostState.params[prop.name.toString().toStdString()] = (double) prop.value;
        }
        paramsDirty = true; return;
    }
    if (t == "pins") {
        const juce::SpinLock::ScopedLockType sl(stateLock);
        hostState.ps.clear(); hostState.pd.clear(); hostState.pg.clear();
        if (auto* s = m["s"].getArray()) for (auto& v : *s) hostState.ps.push_back((int) v);
        if (auto* d = m["d"].getArray()) for (auto& v : *d) hostState.pd.push_back((int) v);
        if (auto* g = m["g"].getArray()) for (auto& v : *g) hostState.pg.push_back((double) v);
        pinsDirty = true; return;
    }
    if (t == "dk") {
        const juce::SpinLock::ScopedLockType sl(stateLock);
        hostState.in1Sig = (bool) m["in1Sig"]; hostState.in2Key = (bool) m["in2Key"]; hostState.highPriority = m["priority"].toString() != "last";
        hostState.in1Ext = JOY16_FX && (bool) m["in1Ext"]; hostState.in2Ext = JOY16_FX && (bool) m["in2Ext"];
        dkDirty = true; return;
    }
    if (t == "voices") { { const juce::SpinLock::ScopedLockType sl(stateLock); hostState.voices = juce::jlimit(1, vcs3::MAXV, (int) m["v"]); } voicesDirty = true; return; }
    if (t == "stick") { { const juce::SpinLock::ScopedLockType sl(stateLock); hostState.stickX = m["x"]; hostState.stickY = m["y"]; } stickDirty = true; return; }
    Cmd c {};
    if (t == "on") { c.type = Cmd::NoteOn; c.id = panelNoteId(m["id"].toString()); c.note = (int) m["note"]; c.v = (float) (double) m["vel"]; c.ch = (int) m["ch"]; }
    else if (t == "off") { c.type = Cmd::NoteOff; c.id = panelNoteId(m["id"].toString()); }
    else if (t == "trig") c.type = Cmd::Trig;
    else if (t == "hold") { c.type = Cmd::Hold; c.v = (bool) m["v"] ? 1.f : 0.f; }
    else if (t == "bend") { c.type = Cmd::Bend; c.ch = (int) m["ch"]; c.v = (float) (double) m["v"]; }
    else if (t == "press") {
        if (m.hasProperty("id")) { c.type = Cmd::NotePress; c.id = panelNoteId(m["id"].toString()); }
        else { c.type = Cmd::Press; c.ch = (int) m["ch"]; }
        c.v = (float) (double) m["v"];
    }
    else if (t == "slide") { c.type = Cmd::Slide; c.ch = (int) m["ch"]; c.v = (float) (double) m["v"]; }
    else if (t == "panic") c.type = Cmd::Panic;
    else return;
    push(c);
}

// ---- audio thread ----
void VCS3Processor::syncHostState() {
    if (!(paramsDirty || pinsDirty || dkDirty || voicesDirty || stickDirty)) return;
    const juce::SpinLock::ScopedTryLockType sl(stateLock);
    if (!sl.isLocked()) return; // try again next block
    if (paramsDirty.exchange(false)) { for (auto& [k, v] : hostState.params) engine.setParam(k, v); engine.paramsDone(); }
    if (pinsDirty.exchange(false)) engine.setPins(hostState.ps, hostState.pd, hostState.pg);
    if (dkDirty.exchange(false)) engine.setDk(hostState.in1Sig, hostState.in2Key, hostState.highPriority, hostState.in1Ext, hostState.in2Ext);
    if (voicesDirty.exchange(false)) { if (hostState.voices != engine.nv) engine.setVoices(hostState.voices); }
    if (stickDirty.exchange(false)) engine.setStick(hostState.stickX, hostState.stickY);
}

void VCS3Processor::drainCommands() {
    const auto scope = fifo.read(fifo.getNumReady());
    auto run = [this](const Cmd& c) {
        switch (c.type) {
            case Cmd::NoteOn: engine.noteOn(c.id, c.note, c.v, c.ch); break;
            case Cmd::NoteOff: engine.noteOff(c.id); break;
            case Cmd::Trig: engine.trigger(); break;
            case Cmd::Hold: engine.setHold(c.v > 0.5f); break;
            case Cmd::Bend: engine.bend(c.ch, c.v); break;
            case Cmd::Press: engine.pressure(c.ch, c.v); break;
            case Cmd::NotePress: engine.notePressure(c.id, c.v); break;
            case Cmd::Slide: engine.slide(c.ch, c.v); break;
            case Cmd::Panic: engine.allOff(); break;
        }
    };
    for (int i = 0; i < scope.blockSize1; ++i) run(cmds[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) run(cmds[(size_t) (scope.startIndex2 + i)]);
}

// MPE: per-note bend ±48 semitones on channels 2-16 (±2 on channel 1), channel pressure → row 17, CC74 → row 18.
void VCS3Processor::handleMidi(const juce::MidiMessage& m) {
    const int ch = m.getChannel();
    if (m.isNoteOn()) engine.noteOn(midiNoteId(ch, m.getNoteNumber()), m.getNoteNumber(), juce::jlimit(-1.f, 1.f, (m.getVelocity() - 64) / 63.f), ch);
    else if (m.isNoteOff()) engine.noteOff(midiNoteId(ch, m.getNoteNumber()));
    else if (m.isPitchWheel()) engine.bend(ch, (m.getPitchWheelValue() - 8192) / 8192.f * (ch == 1 ? 2.f : 48.f));
    else if (m.isChannelPressure()) engine.pressure(ch, m.getChannelPressureValue() / 127.f * 2.f);
    else if (m.isAftertouch()) engine.notePressure(midiNoteId(ch, m.getNoteNumber()), m.getAfterTouchValue() / 127.f * 2.f);
    else if (m.isController() && m.getControllerNumber() == 74) engine.slide(ch, (m.getControllerValue() - 64) / 63.f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
}

void VCS3Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    syncHostState();
    drainCommands();
    const int n = buffer.getNumSamples();
    if ((int) meterBuf.size() < n) meterBuf.resize((size_t) n); // only if the host exceeds the prepared size
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    // FX edition: the input shares the output buffer; render() reads each input sample before writing that output sample
    const int nIn = getTotalNumInputChannels();
    const float* inL = nIn > 0 ? L : nullptr;
    const float* inR = nIn > 1 ? R : inL;
    auto run = [&](int from, int len) { engine.render(L + from, R + from, meterBuf.data() + from, len, inL ? inL + from : nullptr, inR ? inR + from : nullptr); };
    int pos = 0;
    for (const auto meta : midi) {
        const int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > pos) { run(pos, at - pos); pos = at; }
        handleMidi(meta.getMessage());
    }
    if (pos < n) run(pos, n - pos);
    midi.clear();

    for (int i = 0; i < n; ++i) { outRing[(size_t) ringPos] = 0.5f * (L[i] + R[i]); meterRing[(size_t) ringPos] = meterBuf[(size_t) i]; ringPos = (ringPos + 1) % SCOPE; }
    if (engine.viewFresh) {
        const juce::SpinLock::ScopedTryLockType sl(viewLock);
        if (sl.isLocked()) {
            view.env = engine.envOut; view.act = engine.actOut; view.pins = engine.pinOut;
            view.out.resize(SCOPE); view.meter.resize(SCOPE);
            for (int i = 0; i < SCOPE; ++i) { const int k = (ringPos + i) % SCOPE; view.out[(size_t) i] = outRing[(size_t) k]; view.meter[(size_t) i] = meterRing[(size_t) k]; }
            engine.viewFresh = false;
        }
    }
}

bool VCS3Processor::readView(ViewData& out) {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    if (view.out.empty()) return false;
    out = view;
    return true;
}

// ---- host session state ----
void VCS3Processor::getStateInformation(juce::MemoryBlock& destData) {
    const juce::ScopedLock sl(snapLock);
    destData.replaceAll(snapshotJson.toRawUTF8(), snapshotJson.getNumBytesAsUTF8());
}

void VCS3Processor::setStateInformation(const void* data, int sizeInBytes) {
    const auto snap = juce::JSON::parse(juce::String::fromUTF8((const char*) data, sizeInBytes));
    if (!snap.isObject()) return;
    applySnapshot(snap);
    if (onRestore) juce::MessageManager::callAsync([this] { if (onRestore) onRestore(); });
}

juce::AudioProcessorEditor* VCS3Processor::createEditor() { return new VCS3Editor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VCS3Processor(); }
