#pragma once
#if INCLUDE_MOONBASE_UI
#include "../../../../../Assets/MoonbaseBinary.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/*
*/

namespace Moonbase
{

class WaitingOverlay  : public  juce::Component, public juce::Timer
{
public:
    WaitingOverlay();
    ~WaitingOverlay() override;

    void show (const juce::String& msg, bool darkenBackground = true);
    void hide ();
    void setBgColours (const  juce::Colour& layer1, const  juce::Colour& layer2);

    juce::String message;

    void setLogo (std::unique_ptr <juce::Drawable> logo);
    void setLogoScale (const float scaleNormalized);

private:
    std::unique_ptr <juce::Drawable> logo;
    std::unique_ptr <juce::Drawable> rings [3];
    bool darkenBackground = true;

    void timerCallback () override;
    void paint (juce::Graphics&) override;
    void resized() override;

    float rotations [3] { 0.f, 0.f, 0.f };
    juce::Range<float> incRanges [3]
    {
        { 0.075f, 0.09f },
        { 0.055f, 0.065f },
        { 0.03f, 0.045f }
    };
    float currentInc [3] { 0.075f, 0.055f, 0.03f };
    const float incInc = 0.075f;
    float logoScale = 0.3f;

    static inline const int fps { 30 };
     juce::Colour bgColours [2] {
        juce::Colours::white.withAlpha(0.1f),
        juce::Colours::black.withAlpha(0.65f)
    };

    JUCE_DECLARE_WEAK_REFERENCEABLE (WaitingOverlay)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaitingOverlay)
};

} // namespace Moonbase

#endif
