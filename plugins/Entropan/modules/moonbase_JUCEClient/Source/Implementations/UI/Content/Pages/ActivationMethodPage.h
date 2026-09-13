#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../ContentBase.h"

namespace Moonbase
{
namespace JUCEClient
{

 //==============================================================================

    enum ActivationMethod {
        AutoActivation,
        Offline
    };


    class ActivationMethodPage : public ContentBase
    {
    public:
        ActivationMethodPage (ContentHolder& parent, API& api_);
        ~ActivationMethodPage () override;

    private:
        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

        juce::ToggleButton autoActivationButton { "Let's go!" };
        juce::ToggleButton offlineButton        { "Offline Activation" };
        juce::Array<juce::ToggleButton*> buttons      { &autoActivationButton, &offlineButton };

        void paint  (juce::Graphics& g) override;
        void resized () override;

        void handleAutoActivateClicked ();
        void handleOfflineClicked ();

        juce::Rectangle<float> txt1Area, txt2Area;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationMethodPage)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
