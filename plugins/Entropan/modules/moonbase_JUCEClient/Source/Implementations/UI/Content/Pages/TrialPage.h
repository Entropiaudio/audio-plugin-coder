#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"

#include "../ContentBase.h"
namespace Moonbase
{
namespace JUCEClient
{

    //==============================================================================

    class TrialPage : public ContentBase
    {
    public:
        TrialPage (ContentHolder& parent, API& api_);
        ~TrialPage () override;

    private:
        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrialPage)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
