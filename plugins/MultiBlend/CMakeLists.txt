# MultiBlend - Multiband VST3 Host Plugin
# WebView-based UI with spectrum visualization
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
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/MultiBlend")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "MultiBlend: Building for ${PLUGIN_FORMATS}")
message(STATUS "MultiBlend: WebView backend: ${WEBVIEW_BACKEND}")

# ============================================
# BINARY DATA (Web UI Resources)
# ============================================
juce_add_binary_data(MultiBlend_WebUI
    SOURCES
        Source/ui/public/design-tokens.css
        Source/ui/public/entropia-header.css
        Source/ui/public/entropia-buttons.css
        Source/ui/public/knob.css
        Source/ui/public/knob-renderer.js
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/fonts/Inter-Variable.woff2
        Source/ui/public/fonts/Fraunces-Variable.woff2
        Source/ui/public/fonts/Montserrat-Variable.woff2
        Source/ui/public/fonts/JetBrainsMono-Variable.woff2
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(MultiBlend
    COMPANY_NAME "Entropia Audio"
    PLUGIN_MANUFACTURER_CODE EnAu
    PLUGIN_CODE MlBl
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "MultiBlend"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(MultiBlend
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# ============================================
# INCLUDE DIRECTORIES
# ============================================
target_include_directories(MultiBlend
    PRIVATE
        Source
)

# ============================================
# LINK LIBRARIES
# ============================================
target_link_libraries(MultiBlend
    PRIVATE
        MultiBlend_WebUI
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
target_compile_definitions(MultiBlend
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
)

if(WIN32)
    target_compile_definitions(MultiBlend
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

if(APPLE)
    target_compile_definitions(MultiBlend
        PUBLIC
            JUCE_USE_CURL=0
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(MultiBlend
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
    )
endif()

# ============================================
# MBESCANNER — out-of-process plugin scanner helper
# ============================================
add_executable(MultiBlendScanner Source/Scanner/scanner_main.cpp)
target_link_libraries(MultiBlendScanner
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
target_compile_definitions(MultiBlendScanner
    PUBLIC
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
        JUCE_USE_CURL=0
)

# Copy MultiBlendScanner into each plugin format's Resources directory after build
set(MBESCANNER_BIN $<TARGET_FILE:MultiBlendScanner>)

foreach(FMT VST3 AU Standalone)
    if(TARGET MultiBlend_${FMT})
        add_custom_command(TARGET MultiBlend_${FMT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:MultiBlend_${FMT}>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy
                ${MBESCANNER_BIN}
                "$<TARGET_BUNDLE_DIR:MultiBlend_${FMT}>/Contents/Resources/MultiBlendScanner"
            COMMENT "Copying MultiBlendScanner into ${FMT} bundle Resources"
        )
        add_dependencies(MultiBlend_${FMT} MultiBlendScanner)
    endif()
endforeach()
