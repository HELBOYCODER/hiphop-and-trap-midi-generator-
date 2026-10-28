juce_add_plugin(HipHopMidiGenerator
  COMPANY_NAME "HHMG"
  PLUGIN_MANUFACTURER_CODE Hhmg
  PLUGIN_CODE Hmg1
  FORMATS VST3
  PRODUCT_NAME "HipHop MIDI Generator"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument Synth
  NEEDS_CURL TRUE   # Linux only: libcurl for the AI bridge (macOS/Windows use the OS HTTP stack)
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(HHMGData SOURCES ../data/patterns.pop909.json ../data/patterns.cymatics.json)

target_sources(HipHopMidiGenerator PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/AiBridge.cpp
  Source/PresetManager.cpp)

target_compile_definitions(HipHopMidiGenerator PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(HipHopMidiGenerator
  PRIVATE
    hhmg_core
    HHMGData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Headless processor test (transport playback, scale lock, state, import/export, editor snapshot).
if(HHMG_BUILD_TESTS)
  juce_add_console_app(HipHopProcessorTest PRODUCT_NAME "HipHopProcessorTest" NEEDS_CURL TRUE)
  target_sources(HipHopProcessorTest PRIVATE
    Tests/ProcessorTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/AiBridge.cpp Source/PresetManager.cpp)
  target_compile_definitions(HipHopProcessorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
  target_link_libraries(HipHopProcessorTest PRIVATE hhmg_core HHMGData
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_recommended_config_flags)
endif()
