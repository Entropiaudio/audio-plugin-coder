
#if INCLUDE_MOONBASE_UI
#include "JUCEClientUI.h"

#include "Implementations/UI/Includes.h"
#include "Implementations/UI/Includes.cpp"
#include "WebUIHelpers.cpp"


//==============================================================================
//==============================================================================
ActivationUI::ActivationUI (API& api,  juce::Component& parent)
{
    const auto ctorRoutine = [&]() -> juce::var {
    OBF_BEGIN
        impl = new UI_Impl (api, parent);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    ctorRoutine();
}

ActivationUI::~ActivationUI ()
{
    const auto dtorRoutine = [&]() -> juce::var {
    OBF_BEGIN
        delete &GetUiImpl (*this);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    dtorRoutine();
}

API& ActivationUI::getAPI () const
{
    return GetUiImpl(*this).api;
}

void ActivationUI::show ()
{
    const auto showRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).show (true);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    showRoutine();
}

void ActivationUI::hide ()
{
    const auto hideRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).show (false);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    hideRoutine();
}

void ActivationUI::enableUpdateBadge (const UpdateBadge::Options& options)
{
    GetUiImpl(*this).enableUpdateBadge (options);
}


const ActivationUI::Visibility ActivationUI::getVisibility () const
{
    OBF_BEGIN
        return GetUiImpl(*this).getVisibility ();
    OBF_END
    return { true, true };
}

void ActivationUI::setBounds (const  juce::Rectangle<int>& bounds)
{
    const auto setBoundsRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setBoundsWrapping (bounds);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setBoundsRoutine();
}

void ActivationUI::update ()
{
    const auto updateRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).updateVisibility ();
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    updateRoutine();
}

void ActivationUI::setCompanyLogo (std::unique_ptr< juce::Component> logo)
{
    const auto setCompanyLogoRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setCompanyLogo (std::move (logo));
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setCompanyLogoRoutine();
}

 juce::Component* ActivationUI::getCompanyLogo ()
{
    OBF_BEGIN
        return GetUiImpl(*this).getCompanyLogo ();
    OBF_END
        return nullptr;
}


void ActivationUI::setCompanyLogoScale (const float scale)
{
    const auto setCompanyLogoScaleRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setCompanyLogoScale (scale);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setCompanyLogoScaleRoutine();
}

void ActivationUI::setWelcomePageText (const juce::String& line1, const juce::String& line2)
{
    const auto setWelcomePageTextRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setWelcomePageText (line1, line2);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setWelcomePageTextRoutine();
}

std::pair<juce::String, juce::String> ActivationUI::getWelcomePageText () const
{
OBF_BEGIN
    return GetUiImpl(*this).getWelcomePageText ();
OBF_END
    return {};
}

void ActivationUI::setWelcomeButtonTextScale (const float scaleNormalized)
{
    const auto setWelcomeButtonTextScaleRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setWelcomeButtonTextScale (scaleNormalized);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setWelcomeButtonTextScaleRoutine();
}

void ActivationUI::setSpinnerLogo (std::unique_ptr <juce::Drawable> logo)
{
    const auto setSpinnerLogoRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setSpinnerLogo (std::move (logo));
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setSpinnerLogoRoutine();
}

void ActivationUI::setSpinnerLogoScale (const float scaleNormalized)
{
    const auto setSpinnerLogoScaleRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setSpinnerLogoScale (scaleNormalized);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setSpinnerLogoScaleRoutine();
}

void ActivationUI::setMaxWidth (const float maxWidth)
{
    const auto setMaxWidthRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).setMaxWidth (maxWidth);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setMaxWidthRoutine();
}

void ActivationUI::addListener (Listener* listener)
{
    const auto addListenerRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).addListener (listener);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    addListenerRoutine();
}

void ActivationUI::removeListener (Listener* listener)
{
    const auto removeListenerRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetUiImpl(*this).removeListener (listener);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    removeListenerRoutine();
}

#endif
