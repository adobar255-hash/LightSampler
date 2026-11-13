cmake_minimum_required(VERSION 3.22)
project(LightSampler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LightSampler
    COMPANY_NAME "LightSampler"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Lsmp
    PLUGIN_CODE Lsm1
    FORMATS VST3 Standalone
    VST3_CATEGORIES Instrument Sampler
    PRODUCT_NAME "LightSampler"
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(LightSampler)

target_sources(LightSampler PRIVATE
    Source/PluginProcessor.cpp)

target_compile_definitions(LightSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LightSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
