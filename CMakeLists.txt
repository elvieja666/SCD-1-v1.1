cmake_minimum_required(VERSION 3.22)
project(PhantomAsym VERSION 1.10.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(PhantomAsym
    COMPANY_NAME "MiEstudio"
    PLUGIN_MANUFACTURER_CODE Mest
    PLUGIN_CODE Pasm
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "SCD-1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(PhantomAsymFonts SOURCES
    Resources/Inter-Regular.ttf
    Resources/Inter-Medium.ttf)

target_sources(PhantomAsym PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(PhantomAsym PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PhantomAsym
    PRIVATE PhantomAsymFonts
            juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
