cmake_minimum_required(VERSION 3.22)

project(YamahaArranger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE (baixado automaticamente pelo CMake — não precisa instalar nada)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.10
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# O plugin
# ---------------------------------------------------------------------------
juce_add_plugin(YamahaArranger
  VERSION                1.0.0
  COMPANY_NAME           "Yamaha Arranger"
  COMPANY_COPYRIGHT      "MIT"
  FORMATS                VST3 Standalone
  PRODUCT_NAME           "Yamaha Arranger"
  IS_SYNTH               TRUE
  NEEDS_MIDI_INPUT       TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_COPY_DIR          "$ENV{HOME}/.vst3")

juce_generate_juce_header(YamahaArranger)

target_sources(YamahaArranger
  PRIVATE
    engine/Chord.cpp
    engine/ChordDetector.cpp
    engine/ChordTransposer.cpp
    engine/StyParser.cpp
    engine/StylePlayer.cpp
    plugin/TsfSynth.cpp
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/PluginEntry.cpp)

target_compile_definitions(YamahaArranger
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(YamahaArranger
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Testes do motor (C++ puro, sem JUCE) — rode:  cmake --build build --target engine_test
# ---------------------------------------------------------------------------
add_executable(engine_test
  tests/engine_test.cpp
  engine/Chord.cpp
  engine/ChordDetector.cpp
  engine/ChordTransposer.cpp
  engine/StyParser.cpp
  engine/StylePlayer.cpp)
