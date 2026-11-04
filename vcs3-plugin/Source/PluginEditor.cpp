#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
juce::WebBrowserComponent::Options webOptions(VCS3Processor& p, std::function<std::optional<juce::WebBrowserComponent::Resource>(const juce::String&)> serve) {
    auto o = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider(std::move(serve))
        .withEventListener("msg", [&p](const juce::var& v) { p.handlePanelMessage(v); });
   #if JUCE_WINDOWS
    o = o.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
         .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
             .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("VCS3PinMatrix")));
   #endif
    return o;
}
juce::var toVar(const float* d, size_t n) { juce::Array<juce::var> a; a.ensureStorageAllocated((int) n); for (size_t i = 0; i < n; ++i) a.add(d[i]); return a; }
}

VCS3Editor::VCS3Editor(VCS3Processor& p)
    : AudioProcessorEditor(&p), proc(p), web(webOptions(p, serve))
{
    addAndMakeVisible(web);
    proc.onRestore = [this] { web.emitEventIfBrowserIsVisible("restore", proc.currentSnapshot()); };
    web.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable(true, true);
    setResizeLimits(480, 520, 2400, 1800);
    setSize(1360, 960);
    startTimerHz(30);
}

VCS3Editor::~VCS3Editor() { proc.onRestore = nullptr; }

void VCS3Editor::resized() { web.setBounds(getLocalBounds()); }

std::optional<juce::WebBrowserComponent::Resource> VCS3Editor::serve(const juce::String& url) {
    if (url == "/" || url.startsWith("/index.html")) {
        // a UTF-8 byte-order mark makes every web view read the page as UTF-8, whatever the content type says
        std::vector<std::byte> data((size_t) BinaryData::index_htmlSize + 3);
        data[0] = std::byte { 0xEF }; data[1] = std::byte { 0xBB }; data[2] = std::byte { 0xBF };
        std::memcpy(data.data() + 3, BinaryData::index_html, (size_t) BinaryData::index_htmlSize);
        return juce::WebBrowserComponent::Resource { std::move(data), "text/html; charset=utf-8" };
    }
    return std::nullopt;
}

void VCS3Editor::timerCallback() {
    if (!proc.readView(view)) return;
    auto* o = new juce::DynamicObject();
    o->setProperty("env", toVar(view.env.data(), view.env.size()));
    o->setProperty("act", toVar(view.act.data(), view.act.size()));
    o->setProperty("pins", toVar(view.pins.data(), view.pins.size()));
    o->setProperty("out", toVar(view.out.data(), view.out.size()));
    o->setProperty("meter", toVar(view.meter.data(), view.meter.size()));
    web.emitEventIfBrowserIsVisible("engine", juce::var(o));
}
