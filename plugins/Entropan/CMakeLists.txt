# Entropan — band-targeted spectral panner (Entropia Audio)
# WebView UI (Chaosverb-clone design system)
# Cross-platform: Windows (VST3), macOS (VST3/AU), Linux (VST3/LV2)

# ============================================
# PLATFORM-SPECIFIC CONFIGURATION
# ============================================
if(WIN32)
    set(PLUGIN_FORMATS VST3)
    set(NEEDS_WEBVIEW2 TRUE)
    set(NEEDS_WEB_BROWSER FALSE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(AU_MAIN_TYPE kAudioUnitType_MusicEffect)  # aumf: DAWs route MIDI to it
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(LV2_URI "https://entropiaaudio.com/plugins/Entropan")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ============================================
# BINARY DATA (Web UI resources)
# ============================================
juce_add_binary_data(Entropan_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/fonts/Fraunces-Variable.woff2
        Source/ui/public/fonts/Montserrat-Variable.woff2
        Source/ui/public/fonts/JetBrainsMono-Variable.woff2
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(Entropan
    COMPANY_NAME "Entropia Audio"
    PLUGIN_MANUFACTURER_CODE Etpa
    PLUGIN_CODE Etpn
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Entropan"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    NEEDS_MIDI_INPUT TRUE
    VST3_CATEGORIES Fx Spatial Modulation
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(Entropan
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(Entropan
    PRIVATE
        Source
)

# ============================================
# LINK LIBRARIES
# ============================================
target_link_libraries(Entropan
    PRIVATE
        Entropan_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================
# COMPILE DEFINITIONS
# ============================================
target_compile_definitions(Entropan
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(Entropan
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

if(APPLE)
    target_compile_definitions(Entropan
        PUBLIC
            JUCE_USE_CURL=0
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(Entropan
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
    )
endif()

# ============================================
# NULL-TEST HARNESS (console, dev only)
# ============================================
option(ENTROPAN_BUILD_TESTS "Build Entropan DSP null-test console app" ON)
if(ENTROPAN_BUILD_TESTS)
    juce_add_console_app(Entropan_NullTest PRODUCT_NAME "EntropanNullTest")
    target_sources(Entropan_NullTest
        PRIVATE
            Source/tests/NullTest.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
    )
    target_include_directories(Entropan_NullTest PRIVATE Source)
    target_link_libraries(Entropan_NullTest
        PRIVATE
            Entropan_WebUI
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_data_structures
            juce::juce_dsp
            juce::juce_events
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
    )
    target_compile_definitions(Entropan_NullTest
        PRIVATE
            JUCE_WEB_BROWSER=1
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
    )
endif()
