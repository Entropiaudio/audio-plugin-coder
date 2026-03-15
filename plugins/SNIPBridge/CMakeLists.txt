# SNIP Bridge - Real-Time Mix Analyzer & Web Bridge
# WebView-based UI with analysis visualization
# Cross-platform: Windows (VST3), macOS (VST3/AU), Linux (VST3/LV2)

# ============================================
# PLATFORM-SPECIFIC CONFIGURATION
# ============================================
if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/SNIPBridge")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "SNIPBridge: Building for ${PLUGIN_FORMATS}")
message(STATUS "SNIPBridge: WebView backend: ${WEBVIEW_BACKEND}")

# ============================================
# BINARY DATA (Web UI Resources)
# ============================================
juce_add_binary_data(SNIPBridge_WebUI
    SOURCES
        Source/ui/public/index.html
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(SNIPBridge
    COMPANY_NAME "Noizefield"
    PLUGIN_MANUFACTURER_CODE Nfld
    PLUGIN_CODE SnBr
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "SNIP Bridge"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Analyzer Tools
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(SNIPBridge
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# ============================================
# INCLUDE DIRECTORIES
# ============================================
target_include_directories(SNIPBridge
    PRIVATE
        Source
)

# ============================================
# LINK LIBRARIES
# ============================================
target_link_libraries(SNIPBridge
    PRIVATE
        SNIPBridge_WebUI
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
# COMPILE DEFINITIONS (Platform-Specific)
# ============================================

# Base definitions for all platforms
target_compile_definitions(SNIPBridge
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(SNIPBridge
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

# macOS-specific definitions
if(APPLE)
    target_compile_definitions(SNIPBridge
        PUBLIC
            JUCE_USE_CURL=0
    )
endif()

# Linux-specific definitions
if(UNIX AND NOT APPLE)
    target_compile_definitions(SNIPBridge
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
    )
endif()

# ============================================
# MACOS UNIVERSAL BINARY SUPPORT
# ============================================
# Handled via CMAKE_OSX_ARCHITECTURES at configure time
