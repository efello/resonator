cmake_minimum_required(VERSION 3.22)
project(Resonator VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # statically link the MSVC runtime so users do not need the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(APPLE)
    # Universal binary (Apple Silicon + Intel) and a sane deployment target
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

# JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it.
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(RES_FORMATS AU VST3 Standalone)
else()
    set(RES_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Resonator
    COMPANY_NAME "Flo"
    PLUGIN_MANUFACTURER_CODE FloX
    PLUGIN_CODE Rsn1
    FORMATS ${RES_FORMATS}
    PRODUCT_NAME "Resonator"
    BUNDLE_ID com.flo.resonator
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Filter
    COPY_PLUGIN_AFTER_BUILD $<BOOL:${APPLE}>)   # installs to ~/Library/Audio/Plug-Ins on macOS

target_sources(Resonator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Presets.h
    Source/ResonatorDSP.h)

target_compile_definitions(Resonator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Resonator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Plain-C++ regression test of the DSP core (no JUCE): cmake --build . --target dsp_test
add_executable(dsp_test Tests/dsp_test.cpp)

# Headless test that instantiates the real processor:  cmake --build . --target ResonatorTest
juce_add_console_app(ResonatorTest PRODUCT_NAME "ResonatorTest")
set_target_properties(ResonatorTest PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(ResonatorTest PRIVATE Tests/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(ResonatorTest PRIVATE JucePlugin_Name="Resonator" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(ResonatorTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
