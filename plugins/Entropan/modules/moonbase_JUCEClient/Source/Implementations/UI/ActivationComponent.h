#if INCLUDE_MOONBASE_UI
#pragma once

#include "../../JuceIncludes.h"
#include "Content/ContentHolder.h"


namespace Moonbase
{
namespace JUCEClient
{

    //==============================================================================
    //==============================================================================
    struct UI_Impl;
    class ActivationComponent : public  juce::Component
    {
    public:
        ActivationComponent (API& api_, UI_Impl& ui_);
        UI_Impl& uiImpl;

        int getHeightForWidth (const int width);
        int getWidthForHeight (const int height);

        void update ();

        void setSpinnerLogo (std::unique_ptr<juce::Drawable> logo);
        void setSpinnerLogoScale (const float scaleNormalized);

    private:
        friend struct UI_Impl;
        std::unique_ptr <juce::Drawable> bg;

        ContentHolder contentHolder;

        void paint  (juce::Graphics& g) override;
        void resized () override;


        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationComponent)
    };

} // namespace JUCEClient
} // namespace Moonbase
#endif
