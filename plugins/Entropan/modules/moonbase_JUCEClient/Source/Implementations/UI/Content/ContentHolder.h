#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../JuceIncludes.h"

#include "ContentBase.h"

namespace Moonbase
{
namespace JUCEClient
{

 //==============================================================================
    class ContentHolder : public  juce::Component
    {
    public:
        ContentHolder (API& api_);

        enum Page
        {
            Welcome,
            ActivationMethod,

            AutoActivation,

            Offline1,
            Offline2,
            Offline3,

            Activated,

            NUMPAGES
        };

        void setNextPage (const Page& page, const bool fromRight = true, const bool fromLeft = false, const bool force = false );
        Page getCurrentPage () const { return currentPage; }

        ContentBase& getPage (const Page& page);

        void update ();
        void setSpinnerLogo (std::unique_ptr<juce::Drawable> logo);
        void setSpinnerLogoScale (const float scaleNormalized);

    private:
        friend class ActivationComponent;
        API& api;
        juce::OwnedArray<ContentBase> content;

        juce::ComponentAnimator animator;

        Page currentPage = Welcome;

        void resized () override;
        bool resizedOnce = false;

        JUCE_DECLARE_WEAK_REFERENCEABLE (ContentHolder)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentHolder)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
