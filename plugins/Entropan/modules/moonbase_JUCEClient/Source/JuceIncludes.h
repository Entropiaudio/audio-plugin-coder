#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_audio_processors/juce_audio_processors.h>

#if INCLUDE_MOONBASE_UI
    #include <juce_graphics/juce_graphics.h>
    #include <juce_gui_basics/juce_gui_basics.h>
    #include <juce_gui_extra/juce_gui_extra.h>
#endif

#include <juce_product_unlocking/juce_product_unlocking.h>

#include "JUCEClientAPI.h"

#if INCLUDE_MOONBASE_UI
    #include "JUCEClientUI.h"
#endif

namespace Moonbase
{
namespace JUCEClient
{
using StringPair = std::pair<juce::String, juce::String>;
} // namespace JUCEClient
} // namespace Moonbase
using namespace Moonbase;
using namespace Moonbase::JUCEClient;

#ifdef WHEN
  #undef WHEN //this is clashing (warn-level) between obfy and the catch2 testing framework, but can be safely undef'd here
#endif
#include "../obfy/instr.h"

#include "SupressMSVCWarnings.h"
