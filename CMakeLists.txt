cmake_minimum_required(VERSION 3.22)
# ============================================================================
#  EDIT HERE: plugin name + version. Everything else follows these two lines.
# ============================================================================
set(PLUGIN_NAME "SonyTune")
set(PLUGIN_VERSION "1.0.0")

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.0.6 (same version as CRUSH and LOCK).
# Already have JUCE on disk? configure with:  -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(${PLUGIN_NAME}
    COMPANY_NAME "CrushAudio"
    PLUGIN_MANUFACTURER_CODE Crsh
    PLUGIN_CODE Stun
    FORMATS VST3
    PRODUCT_NAME "${PLUGIN_NAME}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(${PLUGIN_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(${PLUGIN_NAME} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
