cmake_minimum_required(VERSION 3.22)
project(VCS3PinMatrix VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# JUCE 8 (set FETCHCONTENT_SOURCE_DIR_JUCE to use a local checkout)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(VCS3_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND VCS3_FORMATS AU)
endif()

juce_add_plugin(VCS3PinMatrix
  PRODUCT_NAME "VCS3 Pin Matrix"
  COMPANY_NAME "AJITO SOUNDS"
  BUNDLE_ID com.ajitosounds.vcs3pinmatrix
  PLUGIN_MANUFACTURER_CODE Ajts
  PLUGIN_CODE V3pm
  FORMATS ${VCS3_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_WEB_BROWSER TRUE
  NEEDS_WEBVIEW2 TRUE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VCS3PinMatrix PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

# the panel is the Advanced page itself; default_snapshot.json comes from tools/gen_default_snapshot.js
juce_add_binary_data(VCS3PinMatrixData SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../vcs3-advanced/index.html
  ${CMAKE_CURRENT_SOURCE_DIR}/Resources/default_snapshot.json)

target_compile_definitions(VCS3PinMatrix PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

target_link_libraries(VCS3PinMatrix
  PRIVATE VCS3PinMatrixData juce::juce_audio_utils juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
