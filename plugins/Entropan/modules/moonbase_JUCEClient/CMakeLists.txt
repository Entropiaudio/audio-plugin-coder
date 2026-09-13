cmake_minimum_required(VERSION 3.20)

project(moonbase_JUCEClient VERSION 1.0.0 LANGUAGES C CXX
    DESCRIPTION "Proprietary JUCE module to connect Moonbase for licensing"
    HOMEPAGE_URL "https://github.com/moonbase-sh/moonbase_JUCEClient")


include(cmake/CPM.cmake)

option(MOONBASE_JUCECLIENT_RUN_PREBUILD
    "Run moonbase_JUCEClient prebuild scripts during the build."
    ON)

set(MOONBASE_JUCECLIENT_CONFIG_JSON ""
    CACHE FILEPATH "Path to moonbase_api_config.json used by moonbase_JUCEClient prebuild")

if(MOONBASE_JUCECLIENT_RUN_PREBUILD AND MOONBASE_JUCECLIENT_CONFIG_JSON STREQUAL "")
    if(moonbase_JUCEClient_IS_TOP_LEVEL)
        set(_moonbase_default_config_json "${CMAKE_CURRENT_LIST_DIR}/tests/moonbase_api_config.json")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Resources/moonbase_api_config.json")
        set(_moonbase_default_config_json "${CMAKE_SOURCE_DIR}/Resources/moonbase_api_config.json")
    endif()

    if(DEFINED _moonbase_default_config_json AND EXISTS "${_moonbase_default_config_json}")
        set(MOONBASE_JUCECLIENT_CONFIG_JSON "${_moonbase_default_config_json}"
            CACHE FILEPATH "Path to moonbase_api_config.json used by moonbase_JUCEClient prebuild" FORCE)
    endif()

    unset(_moonbase_default_config_json)
endif()

function(moonbase_juceclient_enable_prebuild target_name)
    if(NOT MOONBASE_JUCECLIENT_RUN_PREBUILD)
        return()
    endif()

    if(MOONBASE_JUCECLIENT_CONFIG_JSON STREQUAL "")
        message(WARNING
            "Skipping moonbase_JUCEClient prebuild because MOONBASE_JUCECLIENT_CONFIG_JSON is not set.")
        return()
    endif()

    if(NOT EXISTS "${MOONBASE_JUCECLIENT_CONFIG_JSON}")
        message(FATAL_ERROR
            "MOONBASE_JUCECLIENT_CONFIG_JSON does not exist: ${MOONBASE_JUCECLIENT_CONFIG_JSON}")
    endif()

    if(TARGET moonbase_JUCEClient_prebuild)
        add_dependencies(${target_name} moonbase_JUCEClient_prebuild)
        return()
    endif()

    file(GLOB_RECURSE _moonbase_asset_source_files
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/Assets/Source/*")

    set(_moonbase_prebuild_outputs
        "${CMAKE_CURRENT_LIST_DIR}/Assets/BinaryIncludes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Assets/MoonbaseBinary.h"
        "${CMAKE_CURRENT_LIST_DIR}/Assets/MoonbaseBinary1.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Source/ConfigDetails.generated.h"
        "${CMAKE_CURRENT_LIST_DIR}/Source/Implementations/IntegrityCheck.generated.h")

    if(WIN32)
        find_program(MOONBASE_JUCECLIENT_POWERSHELL_EXECUTABLE NAMES powershell.exe powershell pwsh.exe pwsh REQUIRED)

        set(_moonbase_prebuild_command
            "${MOONBASE_JUCECLIENT_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -NonInteractive
            -ExecutionPolicy
            Bypass
            -File
            "${CMAKE_CURRENT_LIST_DIR}/PreBuild.ps1"
            "${MOONBASE_JUCECLIENT_CONFIG_JSON}")

        set(_moonbase_prebuild_script_deps
            "${CMAKE_CURRENT_LIST_DIR}/PreBuild.ps1"
            "${CMAKE_CURRENT_LIST_DIR}/Scripts/PreBuildCacheHelpers.ps1"
            "${CMAKE_CURRENT_LIST_DIR}/Assets/Build.ps1"
            "${CMAKE_CURRENT_LIST_DIR}/KeyIntegrity/IntegrityCheck.ps1"
            "${CMAKE_CURRENT_LIST_DIR}/GenerateConfigDetails/GenerateConfigDetails.ps1"
            "${CMAKE_CURRENT_LIST_DIR}/GenerateConfigDetails/GenerateConfigDetails_windows.exe")
    else()
        find_program(MOONBASE_JUCECLIENT_BASH_EXECUTABLE NAMES bash REQUIRED)

        set(_moonbase_prebuild_command
            "${MOONBASE_JUCECLIENT_BASH_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/PreBuild.sh"
            "${MOONBASE_JUCECLIENT_CONFIG_JSON}")

        set(_moonbase_prebuild_script_deps
            "${CMAKE_CURRENT_LIST_DIR}/PreBuild.sh"
            "${CMAKE_CURRENT_LIST_DIR}/Scripts/WindowsPowerShellSibling.sh"
            "${CMAKE_CURRENT_LIST_DIR}/Scripts/PreBuildCacheHelpers.sh"
            "${CMAKE_CURRENT_LIST_DIR}/Assets/Build.sh"
            "${CMAKE_CURRENT_LIST_DIR}/KeyIntegrity/IntegrityCheck.sh"
            "${CMAKE_CURRENT_LIST_DIR}/GenerateConfigDetails/GenerateConfigDetails.sh"
            "${CMAKE_CURRENT_LIST_DIR}/GenerateConfigDetails/GenerateConfigDetails_macOS"
            "${CMAKE_CURRENT_LIST_DIR}/GenerateConfigDetails/GenerateConfigDetails_linux")
    endif()

    add_custom_target(moonbase_JUCEClient_prebuild
        COMMAND "${CMAKE_COMMAND}" -E env
                "BUILD_CONFIG=$<CONFIG>"
                "CONFIGURATION=$<CONFIG>"
                "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
                ${_moonbase_prebuild_command}
        BYPRODUCTS ${_moonbase_prebuild_outputs}
        DEPENDS
            ${_moonbase_prebuild_script_deps}
            "${MOONBASE_JUCECLIENT_CONFIG_JSON}"
            ${_moonbase_asset_source_files}
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        COMMENT "Running moonbase_JUCEClient prebuild"
        VERBATIM)

    add_dependencies(${target_name} moonbase_JUCEClient_prebuild)
endfunction()

# This config is for running tests standalone
# not relevant when run from a plugin as a module
if (moonbase_JUCEClient_IS_TOP_LEVEL)

    # Set /EHsc compiler flags for exception unwinding on Windows
    if(MSVC)
      add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
    endif()


    option(CPP20 "Compile with C++20" OFF)
    if (CPP20)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_CXX_STANDARD_REQUIRED YES)
    else ()
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED YES)
    endif ()

    option(JUCE7 "Run tests on JUCE 7" OFF)
    if (JUCE7)
        CPMAddPackage("gh:juce-framework/juce#7.0.12")
    else ()
        CPMAddPackage("gh:juce-framework/juce#8.0.4")
    endif ()

    # Use Catch2 v3 for testing
    CPMAddPackage("gh:catchorg/Catch2@3.7.1")

    # Add a test gui app
    juce_add_gui_app("mock_app"
        VERSION 1.0.0
        NEEDS_CURL TRUE
        NEEDS_WEB_BROWSER TRUE
    )
    target_sources("mock_app" PRIVATE "tests/mock_app.cpp")

    target_compile_definitions("mock_app" PUBLIC
        JUCE_USE_CURL=1
        JUCE_WEB_BROWSER=1
    )

    if ((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (CMAKE_SYSTEM_NAME MATCHES ".*BSD"))
        find_package(PkgConfig REQUIRED)

        pkg_check_modules(GTK_X11 QUIET IMPORTED_TARGET gtk+-x11-3.0)
        if (GTK_X11_FOUND)
            target_link_libraries("mock_app" PRIVATE PkgConfig::GTK_X11)
        else ()
            pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
            target_link_libraries("mock_app" PRIVATE PkgConfig::GTK3)
        endif ()

        pkg_check_modules(WEBKIT40 QUIET IMPORTED_TARGET webkit2gtk-4.0)
        if (WEBKIT40_FOUND)
            target_link_libraries("mock_app" PRIVATE PkgConfig::WEBKIT40)
        else ()
            pkg_check_modules(WEBKIT41 REQUIRED IMPORTED_TARGET webkit2gtk-4.1)
            target_link_libraries("mock_app" PRIVATE PkgConfig::WEBKIT41)
        endif ()
    endif ()

    # Add mock json
    juce_add_binary_data(mock_binary SOURCES tests/moonbase_api_config.json)
    set_target_properties(mock_binary PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    # Ideally this could be true, but lots of warnings in obfy
    # set_target_properties(mock_app PROPERTIES COMPILE_WARNING_AS_ERROR ON)

    # Add all test files to a console app
    juce_add_console_app(tests)
    juce_generate_juce_header(tests)
    set_target_properties(tests PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    file(GLOB_RECURSE TestFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.tests.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.tests.h")
    target_sources(tests PRIVATE ${TestFiles})
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
    target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(tests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_UNIT_TESTS=1
        JUCE_LOG_ASSERTIONS=1
    )
    target_compile_definitions(tests PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)
    include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(tests ${DISCOVERY_MODE} "PRE_TEST")

    # Add the test executable
    enable_testing()

endif ()

if (NOT COMMAND juce_add_module)
    message(FATAL_ERROR "JUCE must be added to your project before moonbase!")
endif ()

# Note: CLion + Ninja + JUCE has a problem when JUCE modules are in the base dir
# Avoid ninja when building Moonbase's tests standalone
# See: https://forum.juce.com/t/infinite-loop-calling-juce-add-module-from-a-cmakelists-txt-in-the-modules-own-directory/
juce_add_module("${CMAKE_CURRENT_LIST_DIR}")
moonbase_juceclient_enable_prebuild(moonbase_JUCEClient)

if (moonbase_JUCEClient_IS_TOP_LEVEL)
    target_link_libraries(mock_app PRIVATE
        moonbase_JUCEClient
        mock_binary
        juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
        
    target_link_libraries(tests PRIVATE
        moonbase_JUCEClient
        mock_binary
        juce_audio_processors
        juce_product_unlocking
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif ()
