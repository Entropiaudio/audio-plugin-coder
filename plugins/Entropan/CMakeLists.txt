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
# MOONBASE LICENSING (vendored JUCE module under modules/)
# ============================================
# The module's prebuild scrambles moonbase_api_config.json into obfuscated C++
# headers at build time. Point it at THIS plugin's config explicitly: the
# module's default search path is the top-level source dir, which is wrong in a
# multi-plugin monorepo. Must run before juce_add_plugin (module docs).
set(MOONBASE_JUCECLIENT_CONFIG_JSON
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/moonbase_api_config.json"
    CACHE FILEPATH "Path to Entropan's moonbase_api_config.json" FORCE)
add_subdirectory(modules/moonbase_JUCEClient)

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
        # Barlow Condensed is the UI face (SIL OFL). Static cuts — there is no
        # variable version — so one file per weight. getResource() matches on
        # basename, so these need no other plumbing.
        Source/ui/public/fonts/BarlowCondensed-300.woff2
        Source/ui/public/fonts/BarlowCondensed-400.woff2
        Source/ui/public/fonts/BarlowCondensed-500.woff2
        Source/ui/public/fonts/BarlowCondensed-600.woff2
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(Entropan
    # Explicit: without it the plugin inherits the monorepo project's version
    # and reported "1.0.0" to hosts + Moonbase analytics while the UI said
    # V0.1.0. This is the single source for JucePlugin_VersionString.
    VERSION 0.7.0
    COMPANY_NAME "Entropia Audio"
    # Explicit: JUCE would otherwise derive "com.Entropia Audio.Entropan" from
    # COMPANY_NAME — a space in the bundle ID breaks notarisation/Gatekeeper.
    BUNDLE_ID "com.entropiaudio.Entropan"
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
        moonbase_JUCEClient           # Moonbase licensing (vendored module)
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
        # --- Moonbase licensing module flags ---
        INCLUDE_MOONBASE_UI=1     # use the module's built-in Activate screen
        MOONBASE_APP_MODE=0       # plugin wording, not standalone-app wording
        ENABLE_WEBUI_HELPERS=0    # native overlay; we bridge status by hand
)

# The Moonbase prebuild generates obfuscated config headers that Entropan's own
# translation units include — it must finish before they compile.
add_dependencies(Entropan moonbase_JUCEClient_prebuild)

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
            # Licensing compiled OUT of the harness. It shares PluginProcessor.cpp
            # with the product, and on an unactivated machine the lapsed-trial gate
            # periodically silences the output — which would corrupt every null test
            # with failures that look like DSP bugs.
            ENTROPAN_MOONBASE=0
    )
endif()
