/***********************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:               moonbase_JUCEClient
  vendor:           Moonbase
  version:          1.0.0
  name:             Moonbase Licensing JUCE Client
  description:      This is a plug-and-play JUCE module for Moonbase Licensing.
  website:          https://moonbase.sh
  license:          

  dependencies:     juce_core juce_data_structures juce_events juce_audio_basics juce_product_unlocking
  OSXFrameworks:
  iOSFrameworks:
  linuxLibs:
  mingwLibs:

 END_JUCE_MODULE_DECLARATION
***********************************************************************************/

#pragma once

/** Config: INCLUDE_MOONBASE_UI
            Includes the Moonbase default ActivationUI. This requires juce_graphics, juce_gui_basics, juce_gui_extra and juce_audio_processors
*/
#ifndef INCLUDE_MOONBASE_UI
#define INCLUDE_MOONBASE_UI 1
#endif

/** Config: MOONBASE_APP_MODE
            Enables App Mode. Regular mode is for plugins, App Mode is for standalone apps. This only affects some UI texts.
*/
#ifndef MOONBASE_APP_MODE
#define MOONBASE_APP_MODE 0
#endif

/** Config: ENABLE_WEBUI_HELPERS
            Enables WebUI Helpers for the Moonbase JUCE Client. This requires JUCE 8 and WebView 2.
*/
#ifndef ENABLE_WEBUI_HELPERS
#define ENABLE_WEBUI_HELPERS 0
#endif

#if INCLUDE_MOONBASE_UI
    #include "Source/JUCEClientUI.h"
#endif

#include "Source/JUCEClientAPI.h"

