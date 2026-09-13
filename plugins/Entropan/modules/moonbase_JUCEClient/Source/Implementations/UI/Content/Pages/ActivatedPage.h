#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../ContentBase.h"
#include "../WaitingOverlay/WaitingOverlay.h"

namespace Moonbase
{
namespace JUCEClient
{

    class ActivatedPage : public ContentBase
    {
    public:
        ActivatedPage (ContentHolder& parent, API& api_);
        ~ActivatedPage () override;

    private:

        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

        void pageBecomingVisible () override;

        juce::TextEditor text;
        void resized () override;

        juce::ToggleButton deactivateButton { "Deactivate" };
        void handleDeactivateButtonClicked ();

        JUCE_DECLARE_WEAK_REFERENCEABLE (ActivatedPage)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivatedPage)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
