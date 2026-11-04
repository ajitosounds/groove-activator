#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The panel is the Advanced page itself, shown in a web view; it talks to the processor through JUCE's native bridge.
class VCS3Editor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VCS3Editor(VCS3Processor&);
    ~VCS3Editor() override;
    void resized() override;
private:
    void timerCallback() override;
    static std::optional<juce::WebBrowserComponent::Resource> serve(const juce::String& url);
    VCS3Processor& proc;
    juce::WebBrowserComponent web;
    ViewData view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCS3Editor)
};
