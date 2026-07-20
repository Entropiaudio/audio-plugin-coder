#pragma once

#if JUCE_WINDOWS

/**
 *  Specify the /EHsc compiler flag to enable standard C++ exception handling 
 *  with asynchronous (structured) exception handling (SEH) disabled.
 *  This is important for proper stack unwinding during exceptions in MSVC builds, 
 *  which is required by the OBFY library, that implements try/catch blocks through 
 *  their OBF_BEGIN/OBF_END macros.
 * 
 *   On local MSVC setups, this is normally enabled by default.
 * 
 *   On Cmake builds you'll have to add it manually, e.g. like this:
 *    if(MSVC)
 *      add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
 *    endif()
 */ 
#ifndef _CPPUNWIND
#error "The moonbase JUCE module code requires /EHsc (C++ exceptions enabled). See moonbase_JUCEClient/Source/SupressMSVCWarnings.h for details."
#endif

/**
 *  Suppresses warnings about unused function parameters in MSVC builds
 * 
 *  Rationale:
 *  Without this, the following definition will throw warnings for all parameters:
 *  void drawAdditionalStuff  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float>area) {}
 * 
 *  Changing it to 
 *  void drawAdditionalStuff  (juce::Graphics&, juce::ToggleButton&, bool, bool,  juce::Rectangle<float>) {}
 *  suppresses the warnings, but makes the code a lot less readable.
 * 
 * */
#pragma warning(disable: 4100)

/**
 *  Suppresses warnings about unreachable code in MSVC builds
 * 
 *  Rationale:
 *  Without this, the following definition will throw warnings:
 *  const juce::String APIImpl::getCurrentReleaseVersion () const
 *  {
 *  OBF_BEGIN
 *      return GetCurrentReleaseVersionFromLicense (*this);
 *  OBF_END
 *      return {};  // Warning: unreachable code
 *  }
 * 
 *  The OBF_END macro contains control flow statements that make subsequent code unreachable,
 *  but the fallback return statement is intentionally kept for code clarity and completeness.
 * 
 * */
#pragma warning(disable: 4702)

#endif