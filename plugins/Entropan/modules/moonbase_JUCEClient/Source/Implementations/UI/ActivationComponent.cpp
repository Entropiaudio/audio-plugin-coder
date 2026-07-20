#if INCLUDE_MOONBASE_UI

#include "ActivationComponent.h"
#include "../../SupressMSVCWarnings.h"


//==============================================================================
//==============================================================================
ActivationComponent::ActivationComponent (API& api_, UI_Impl& ui_)
:
uiImpl (ui_),
contentHolder (api_)
{
    bg = MB_LOAD_SVG (MoonbaseBinary::BG_svg);

    addAndMakeVisible (contentHolder);
    // addAndMakeVisible (emailEditor);
    // addAndMakeVisible (pwEditor);
    // addAndMakeVisible (activateButton);

    // activateButton.setButtonText ("Activate");
    // activateButton.onClick = [&] ()
    // {
    //     api.requestOnlineActivation (emailEditor.getText (), pwEditor.getText ());
    // };
}

void ActivationComponent::update ()
{
    contentHolder.update ();
    resized ();
}

int ActivationComponent::getHeightForWidth (const int width)
{
    return (int) (static_cast<float> (width) * (static_cast<float> (bg->getHeight ()) / static_cast<float> (bg->getWidth ())));
}

int ActivationComponent::getWidthForHeight (const int height)
{
    return (int) (static_cast<float> (height) * (static_cast<float> (bg->getWidth ()) / static_cast<float> (bg->getHeight ())));
}

void ActivationComponent::setSpinnerLogo (std::unique_ptr<juce::Drawable> logo)
{
    contentHolder.setSpinnerLogo (std::move (logo));
}

void ActivationComponent::setSpinnerLogoScale (const float scaleNormalized)
{
    contentHolder.setSpinnerLogoScale (scaleNormalized);
}

void ActivationComponent::paint  (juce::Graphics& g)
{
    const auto area = getLocalBounds ().toFloat ();
    if (bg != nullptr)
        bg->drawWithin (g, area, juce::RectanglePlacement::centred, 1.0f);
}

void ActivationComponent::resized ()
{
    auto area = getLocalBounds ();
    contentHolder.setBounds (area);
    contentHolder.resized ();
}

#endif
