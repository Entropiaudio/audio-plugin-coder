#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../ContentBase.h"
namespace Moonbase
{
namespace JUCEClient
{

//==============================================================================

    class AutoActivationPage : public ContentBase
    {
    public:
        AutoActivationPage (ContentHolder& parent, API& api_);
        ~AutoActivationPage () override;


        void startFlow ();

    private:

        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;


        void pageBecomingVisible () override;
        void update () override;
        void resized () override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoActivationPage)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
