#if INCLUDE_MOONBASE_UI
#include "AutoActivationPage.h"
#include "../ContentHolder.h"

//==============================================================================
//==============================================================================

AutoActivationPage::AutoActivationPage (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_)
{
    waitingOverlay.show ("Continue in browser...");
}

AutoActivationPage::~AutoActivationPage ()
{

}

void AutoActivationPage::pageBecomingVisible ()
{
    ContentBase::pageBecomingVisible ();
    startFlow ();
}

void AutoActivationPage::update ()
{
    ContentBase::update ();
    waitingOverlay.show ("Continue in browser...");
}

void AutoActivationPage::startFlow ()
{
    waitingOverlay.show ("Continue in browser...");
    api.requestAutoActivation ([&](const AutoActivationConfig& config)
    {
        waitingOverlay.show ("Continue in browser...");
        api.startPollingAutoActivationResult ([&](const bool finished, bool success, const juce::String& licenseContent)
        {
            juce::ignoreUnused (success, licenseContent);
            if (finished)
            {
                waitingOverlay.hide ();
                if (MB_IS_UNLOCKED_OBFUSCATED(api).first && ! api.isTrial ())
                    contentHolder.setNextPage (ContentHolder::Activated, true, false);
                else
                    contentHolder.setNextPage (ContentHolder::Welcome, false, true);
            }
        });

        config.browserUrl.launchInDefaultBrowser ();
    });
}

ContentBase::BackButtonConfig AutoActivationPage::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::ActivationMethod, false, true);
    }};
}

float AutoActivationPage::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair AutoActivationPage::getTitleAndSubtitle ()
{
    return { "Auto Activation", "" };
}

void AutoActivationPage::resized ()
{
    ContentBase::resized ();

    const auto area = getLocalBounds ();
    const auto waitingOverlaySize = (int) (static_cast<float> (getHeight ()) * (0.5f));
    const auto waitingOverlayY = (int) (static_cast<float> (getHeight ()) *  (0.5f));

    juce::Rectangle<int> waitingOverlayArea (waitingOverlaySize, waitingOverlaySize);
    waitingOverlayArea.setCentre (area.getCentreX (), waitingOverlayY);
    waitingOverlay.setBounds (waitingOverlayArea);

}
#endif
