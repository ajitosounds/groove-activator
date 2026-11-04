#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <map>
#include "Engine.h"

// Settings from the panel, kept on the message thread and copied into the engine at the start of a block.
struct HostState {
    std::map<std::string, double> params;
    std::vector<int> ps, pd; std::vector<double> pg;
    bool in1Sig = false, in2Key = false, highPriority = true;
    int voices = 1;
    double stickX = 0, stickY = 0;
};

// Note and trigger events from the panel, passed to the audio thread through a lock-free FIFO.
struct Cmd {
    enum Type { NoteOn, NoteOff, Trig, Hold, Bend, Press, NotePress, Slide, Panic } type;
    int64_t id = 0; int note = 0, ch = 0; float v = 0;
};

// What the panel shows: voice lamps, pin glow, and the scope.
struct ViewData {
    std::array<float, vcs3::MAXV> env{}, act{};
    std::vector<float> pins;
    std::vector<float> out, meter;
};

class VCS3Processor : public juce::AudioProcessor {
public:
    VCS3Processor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VCS3 Pin Matrix"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // message thread
    void handlePanelMessage(const juce::var& msg);
    juce::var currentSnapshot() const;
    bool readView(ViewData& out);
    std::function<void()> onRestore; // set by the editor: resend the saved state to the panel

private:
    void applySnapshot(const juce::var& snap);
    void push(const Cmd& c);
    void drainCommands();
    void syncHostState();
    void handleMidi(const juce::MidiMessage& m);

    vcs3::Engine engine;
    std::vector<std::string> paramNames;

    juce::SpinLock stateLock; HostState hostState;
    std::atomic<bool> paramsDirty { true }, pinsDirty { true }, dkDirty { true }, voicesDirty { true }, stickDirty { true };

    juce::AbstractFifo fifo { 1024 }; std::array<Cmd, 1024> cmds;

    juce::SpinLock viewLock; ViewData view; std::vector<float> outRing, meterRing; int ringPos = 0;
    std::vector<float> meterBuf;

    mutable juce::CriticalSection snapLock; juce::String snapshotJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCS3Processor)
};
