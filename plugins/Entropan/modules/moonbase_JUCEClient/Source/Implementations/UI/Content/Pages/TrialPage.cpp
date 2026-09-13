#if INCLUDE_MOONBASE_UI
#include "TrialPage.h"


//==============================================================================

TrialPage::TrialPage (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_)
{

}

TrialPage::~TrialPage ()
{

}

ContentBase::BackButtonConfig TrialPage::getBackButtonConfig () 
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::ActivationMethod, false, true);
    }};
}

float TrialPage::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair TrialPage::getTitleAndSubtitle ()
{
    return { "Trial", "Activation" };
}

#endif