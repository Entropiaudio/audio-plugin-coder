#if INCLUDE_MOONBASE_UI

#include "ActivationMethodPage.h"

#include "../ContentHolder.h"
#include "../../Fonts.h"
//==============================================================================
//==============================================================================
ActivationMethodPage::ActivationMethodPage (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_)
{
    for (auto& b : buttons)
    {
        addAndMakeVisible (b);
    }

    autoActivationButton.setLookAndFeel (&narrowTextButtonLAF);
    offlineButton.setLookAndFeel (&textLinkLAF);
    textLinkLAF.setColours ( juce::Colour (0xFF747474),  juce::Colour (0xFFD0D0D0));

    autoActivationButton.onClick = std::bind (&ActivationMethodPage::handleAutoActivateClicked, this);
    offlineButton.onClick = std::bind (&ActivationMethodPage::handleOfflineClicked, this);

    offlineButton.setVisible (api.getOfflineUrl().isWellFormed());
}

ActivationMethodPage::~ActivationMethodPage ()
{
    for (auto& b : buttons)
        b->setLookAndFeel (nullptr);
}

void ActivationMethodPage::handleAutoActivateClicked ()
{
    contentHolder.setNextPage (ContentHolder::Page::AutoActivation);
}

void ActivationMethodPage::handleOfflineClicked ()
{
    contentHolder.setNextPage (ContentHolder::Page::Offline1);
}

float ActivationMethodPage::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

ContentBase::BackButtonConfig ActivationMethodPage::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::Welcome, false, true);
    }};
}

StringPair ActivationMethodPage::getTitleAndSubtitle ()
{
    return { "Activation", "" };
}

void ActivationMethodPage::paint  (juce::Graphics& g)
{
   ContentBase::paint (g);

    juce::Font f = Roboto::Regular ();
    g.setFont (f.withHeight (txt1Area.getHeight ()));
    g.setColour ( juce::Colour(0xFFD0D0D0));
    g.drawText ("Opens your browser for", txt1Area,  juce::Justification::centred);

    g.setFont (f.withHeight (txt2Area.getHeight ()));
    g.setColour ( juce::Colour(0xFFD0D0D0));
    g.drawText ("Trial & Activation", txt2Area,  juce::Justification::centred);
}

void ActivationMethodPage::resized ()
{
    ContentBase::resized ();
    const auto area = getLocalBounds ();

    const auto autoActivateButtonHeight = (int) (static_cast<float> (getHeight ()) * (0.14f));
    const auto autoActivateButtonWidth = (int) (static_cast<float> (getWidth ()) * (0.3f));
    const auto autoActivateButtonCentreY = (int) (static_cast<float> (getHeight ()) * (0.5f));
    juce::Rectangle<int> autoActivateButtonArea (autoActivateButtonWidth, autoActivateButtonHeight);
    autoActivateButtonArea.setCentre (area.getCentreX (), autoActivateButtonCentreY);
    autoActivationButton.setBounds (autoActivateButtonArea);

    const auto offlineButtonHeight = (int) (static_cast<float> (getHeight ()) * (0.045f));
    const auto offlineButtonWidth = (int) (static_cast<float> (getWidth ()) * (0.28f));
    const auto offlineButtonCentreY = (int) (static_cast<float> (getHeight ()) * (0.86f));
    juce::Rectangle<int> offlineButtonArea (offlineButtonWidth, offlineButtonHeight);
    offlineButtonArea.setCentre (area.getCentreX (), offlineButtonCentreY);
    offlineButton.setBounds (offlineButtonArea);

    txt1Area = juce::Rectangle<float> (0, static_cast<float> (getHeight ()) * (0.6f),
                                (float)getWidth (), static_cast<float> (getHeight ()) * (0.035f));

    txt2Area = juce::Rectangle<float> (0, static_cast<float> (getHeight ()) * (0.65f),
                                (float)getWidth (), static_cast<float> (getHeight ()) * (0.045f));
}


#endif
