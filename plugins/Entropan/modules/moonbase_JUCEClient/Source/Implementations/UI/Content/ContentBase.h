#if INCLUDE_MOONBASE_UI

#pragma once
#include "../../../JuceIncludes.h"
#include "../LAFs.h"
#include "../ErrorDisplay.h"
#include "WaitingOverlay/WaitingOverlay.h"

namespace Moonbase
{
namespace JUCEClient
{
 //==============================================================================
    class ContentHolder;
    class ContentBase : public  juce::Component
    {
    public:
        ContentBase (ContentHolder& parent, API& api_);
        ~ContentBase () override;

        virtual void update ();
        virtual void pageBecomingVisible ();

        void setSpinnerLogo (std::unique_ptr <juce::Drawable> logo);
        void setSpinnerLogoScale (const float scaleNormalized);

    protected:
        struct BackButtonConfig {
            bool hasBackButton = false;
            std::function<void()> callback = nullptr;
        };
        virtual BackButtonConfig getBackButtonConfig ()  { return {}; }
        virtual float getErrorDisplayCentreYRelative () = 0;
        virtual StringPair getTitleAndSubtitle () { return { "", "" }; }

        API& api;
        ContentHolder& contentHolder;

         juce::ToggleButton closeButton { "Close Activation Window" };
         juce::ToggleButton backButton { "Go Back" };
        ErrorDisplay errorDisplay;

        CloseButtonLAF closeButtonLAF;
        BackButtonLAF backButtonLAF;
        WideTextButtonLAF wideTextButtonLAF;
        NarrowTextButtonLAF narrowTextButtonLAF;
        TextLinkLAF textLinkLAF;

        WaitingOverlay waitingOverlay;

         juce::Rectangle<float> titleArea;
         juce::Rectangle<float> subtitleArea;

         juce::Rectangle<int> bottomRightSmallButtonArea;
         juce::Rectangle<int> bottomCenterWideButtonArea;

        void paint  (juce::Graphics& g) override;
        void resized () override;

        juce::WeakReference< juce::Component> companyLogo;

    private:
        friend class ContentHolder;

        void updateCloseButtonVisibility ();
        void updateBackButton ();

        void mouseUp (const juce::MouseEvent& e) override;

        void updateCompanyLogo ();


        JUCE_DECLARE_WEAK_REFERENCEABLE (ContentBase)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentBase)
    };


} // namespace JUCEClient
} // namespace Moonbase

#endif
