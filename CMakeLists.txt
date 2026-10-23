cmake_minimum_required(VERSION 3.22)

project(Vocoder VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(VOCODER_TESTS_ONLY "Build only the DSP tests (skips downloading JUCE)" OFF)

enable_testing()
add_executable(dsp_tests tests/DspTests.cpp)
add_test(NAME dsp_tests COMMAND dsp_tests)

if(VOCODER_TESTS_ONLY)
    return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(VOCODER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VOCODER_FORMATS AU)
endif()

juce_add_plugin(Vocoder
    COMPANY_NAME "Colin Blackshear"
    BUNDLE_ID "com.colinblackshear.vocoder"
    PLUGIN_MANUFACTURER_CODE Cblk
    PLUGIN_CODE Vcdr
    FORMATS ${VOCODER_FORMATS}
    PRODUCT_NAME "Vocoder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Vocoder uses your microphone as the voice it shapes the synth with."
    VST3_CATEGORIES Fx Vocoder
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Vocoder PRIVATE Source/PluginProcessor.cpp)

target_compile_definitions(Vocoder PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Vocoder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
