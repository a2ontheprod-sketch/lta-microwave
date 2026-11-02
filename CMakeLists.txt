cmake_minimum_required(VERSION 3.22)
project(LTA_Microwave VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.6.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LTA_Microwave
    COMPANY_NAME "LTA Audio"
    BUNDLE_ID "com.listentoaarxn.ltamicrowave"
    PLUGIN_MANUFACTURER_CODE Ltaa
    PLUGIN_CODE Lm01
    FORMATS VST3
    PRODUCT_NAME "LTA MICROWAVE"
    DESCRIPTION "Underground multi-FX microwave"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Required by Source/*.h, avoids the JuceHeader.h error from the first plugin.
juce_generate_juce_header(LTA_Microwave)

juce_add_binary_data(LTA_Microwave_Artwork
    SOURCES Assets/microwave_bg.png
)

target_sources(LTA_Microwave PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(LTA_Microwave PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LTA_Microwave
    PRIVATE
        LTA_Microwave_Artwork
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
