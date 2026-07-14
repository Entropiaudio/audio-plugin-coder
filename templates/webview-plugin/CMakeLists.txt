# __PLUGIN__ — WebView plugin (Entropia Audio scaffold)
# Cross-platform: Windows (VST3), macOS (VST3/AU), Linux (VST3/LV2)
#
# Drop this folder in as plugins/__PLUGIN__/ inside the APC monorepo (the root
# CMakeLists auto-discovers any plugins/*/CMakeLists.txt), or add_subdirectory
# it from your own JUCE project. Then run rename.sh to set the real name/codes.

if(WIN32)
    set(PLUGIN_FORMATS VST3)
    set(NEEDS_WEBVIEW2 TRUE)
    set(NEEDS_WEB_BROWSER FALSE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(LV2_URI "https://entropiaaudio.com/plugins/__PLUGIN__")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ── Web UI resources (index.html, JUCE interop lib, fonts) ──
juce_add_binary_data(__PLUGIN___WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/fonts/Fraunces-Variable.woff2
        Source/ui/public/fonts/Montserrat-Variable.woff2
        Source/ui/public/fonts/JetBrainsMono-Variable.woff2
)

juce_add_plugin(__PLUGIN__
    COMPANY_NAME "Entropia Audio"
    PLUGIN_MANUFACTURER_CODE Etpa
    PLUGIN_CODE __CODE__
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "__PLUGIN__"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

target_sources(__PLUGIN__
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)
target_include_directories(__PLUGIN__ PRIVATE Source)

target_link_libraries(__PLUGIN__
    PRIVATE
        __PLUGIN___WebUI
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

target_compile_definitions(__PLUGIN__
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(__PLUGIN__ PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()
if(APPLE)
    target_compile_definitions(__PLUGIN__ PUBLIC JUCE_USE_CURL=0)
endif()
if(UNIX AND NOT APPLE)
    target_compile_definitions(__PLUGIN__ PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()
