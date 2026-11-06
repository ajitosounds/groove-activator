cmake_minimum_required(VERSION 3.22)
# must be set before project(); otherwise the build targets the runner's own macOS
# (a binary built on macOS 26 then fails to load on older systems)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(VCS3PinMatrix VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8 (set FETCHCONTENT_SOURCE_DIR_JUCE to use a local checkout)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(VCS3_FORMATS VST3 Standalone)
set(FX_FORMATS VST3)
if(APPLE)
  list(APPEND VCS3_FORMATS AU)
  list(APPEND FX_FORMATS AU)
endif()

# AAX (Pro Tools): off unless -DJOY16_WITH_AAX=ON. JUCE 8 ships the AAX SDK (JOY16_AAX_SDK picks another).
# Retail Pro Tools loads it only after PACE signing (wraptool + iLok) on Gota's Mac, so it stays off for now.
option(JOY16_WITH_AAX "Build the AAX plug-ins" OFF)
set(JOY16_AAX_SDK "" CACHE PATH "AAX SDK folder; empty = the one bundled with JUCE")
if(JOY16_WITH_AAX)
  if(JOY16_AAX_SDK)
    juce_set_aax_sdk_path("${JOY16_AAX_SDK}")
  endif()
  list(APPEND VCS3_FORMATS AAX)
  list(APPEND FX_FORMATS AAX)
endif()

# two editions from one source: AJT JOY16 (instrument) and AJT JOY16 FX (Audio FX: the track's audio feeds the INPUT rows)
juce_add_binary_data(VCS3PinMatrixData SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../vcs3-advanced/index.html
  ${CMAKE_CURRENT_SOURCE_DIR}/Resources/default_snapshot.json
  ${CMAKE_CURRENT_SOURCE_DIR}/Resources/default_snapshot_fx.json)

function(joy16_edition target)
  cmake_parse_arguments(E "" "NAME;BUNDLE;CODE;SYNTH;AUTYPE;FX;AAXCAT" "FORMATS;VST3CAT" ${ARGN})
  juce_add_plugin(${target}
    PRODUCT_NAME "${E_NAME}"
    COMPANY_NAME "AJITO SOUNDS"
    BUNDLE_ID ${E_BUNDLE}
    PLUGIN_MANUFACTURER_CODE Ajts
    PLUGIN_CODE ${E_CODE}
    FORMATS ${E_FORMATS}
    IS_SYNTH ${E_SYNTH}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES ${E_VST3CAT}
    AU_MAIN_TYPE ${E_AUTYPE}
    AAX_CATEGORY ${E_AAXCAT}
    COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(${target} PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(${target} PUBLIC
    JOY16_FX=${E_FX}
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
  target_link_libraries(${target}
    PRIVATE VCS3PinMatrixData juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

joy16_edition(VCS3PinMatrix NAME "AJT JOY16" BUNDLE com.ajitosounds.joy16 CODE Jy16 SYNTH TRUE
  AUTYPE kAudioUnitType_MusicDevice FX 0 AAXCAT AAX_ePlugInCategory_SWGenerators FORMATS ${VCS3_FORMATS} VST3CAT Instrument Synth)
# kAudioUnitType_MusicEffect: an Audio FX that also takes MIDI (Logic: Audio FX slot, MIDI via the plug-in's side chain menu)
joy16_edition(JOY16FX NAME "AJT JOY16 FX" BUNDLE com.ajitosounds.joy16fx CODE Jy1F SYNTH FALSE
  AUTYPE kAudioUnitType_MusicEffect FX 1 AAXCAT AAX_ePlugInCategory_Modulation FORMATS ${FX_FORMATS} VST3CAT Fx)
