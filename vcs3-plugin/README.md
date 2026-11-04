# VCS3 Pin Matrix — plugin (VST3 / AU / Standalone)

The Advanced edition as an instrument plugin, built with JUCE 8.

- **Panel**: `../vcs3-advanced/index.html`, embedded and shown in a web view. When the page finds
  JUCE's native bridge (`window.__JUCE__.backend`) it sends every change to the plugin instead of
  starting its own AudioWorklet, and draws the lamps, pin glow and scope from the plugin's data.
- **Sound**: `Source/Engine.h`, a line-for-line C++ port of the page's AudioWorklet. When you change
  the sound in the page, make the same change there.
- **State**: the host saves the panel's snapshot (patch, all knob values, pins, DK switches, voices,
  stick). It plays without the editor open. `Resources/default_snapshot.json` is the state before
  anything is saved and gives the knob names; regenerate it with `node tools/gen_default_snapshot.js`
  after changing knobs or the first preset.
- **MIDI / MPE**: notes, per-channel pitch bend (±48 semitones on channels 2-16, ±2 on channel 1),
  channel pressure and poly aftertouch → row 17, CC74 → row 18.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release            # fetches JUCE 8.0.4
cmake --build build --config Release
```

- macOS universal: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
- Windows: install the `Microsoft.Web.WebView2` NuGet package first (see the workflow).
- Linux: needs `libwebkit2gtk-4.1-dev`, `libgtk-3-dev`, `libasound2-dev` and the usual X11/freetype headers.

`.github/workflows/vcs3-build.yml` builds macOS (VST3, AU, Standalone; ad-hoc signed, AU checked with
`auval`) and Windows (VST3, Standalone) together with the desktop app, and puts them in one folder.
`INSTALL.txt` goes into each plugin zip.

## Licensing

JUCE 8 is dual-licensed (AGPLv3 or a commercial JUCE licence). Distributing this plugin as a closed,
paid product needs a JUCE licence that covers your revenue; check https://juce.com/get-juce/ before
selling it. The plugin is not code-signed or notarised.
