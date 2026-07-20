#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../ContentBase.h"

namespace Moonbase
{
namespace JUCEClient
{

//==============================================================================

    class OfflineActivationPageBase : public ContentBase
    {
    public:
        OfflineActivationPageBase (ContentHolder& parent, API& api_);
        ~OfflineActivationPageBase () override;

    private:

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OfflineActivationPageBase)
    };

    class OfflineActivationPage1 : public OfflineActivationPageBase
    {
    public:
        OfflineActivationPage1 (ContentHolder& parent, API& api_);
        ~OfflineActivationPage1 () override;

    private:
        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

         juce::ToggleButton machineFileButton { "Generate Machine File" };
        void handleMachineFileButtonClicked ();

         juce::Rectangle<float> txtArea;

        void paint  (juce::Graphics& g) override;
        void resized () override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OfflineActivationPage1)
    };

    class OfflineActivationPage2 : public OfflineActivationPageBase
    {
    public:
        OfflineActivationPage2 (ContentHolder& parent, API& api_);
        ~OfflineActivationPage2 () override;

    private:
        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

         juce::Rectangle<int> txtArea;
         juce::ToggleButton linkButton;
        void handleLinkButtonClicked ();

         juce::ToggleButton nextButton { "Next" };
        void handleNextButtonClicked ();

        void paint  (juce::Graphics& g) override;
        void resized () override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OfflineActivationPage2)
    };


    class LicenseFileDragAndDropTarget : public juce::FileDragAndDropTarget,
                                         public juce::Component
    {
    public:
        LicenseFileDragAndDropTarget (API& api_);

        int getHeightForWidth (int width) ;

    private:
        API& api;
        std::unique_ptr <juce::Drawable> targetIcon [3];

        bool isDraggingFile = false;
        bool isInterestedInCurrentFile = false;

        void paint  (juce::Graphics& g) override;

        void filesDropped (const juce::StringArray& files, int x, int y) override;
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray& files, int x, int y) override;
        void fileDragExit (const juce::StringArray& files) override;

        bool validateFile (const juce::File& f);

        static inline const juce::String offlineFileExtension { ".mb" };

        void mouseDoubleClick (const juce::MouseEvent& e) override;
        std::unique_ptr<juce::FileChooser> fileChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseFileDragAndDropTarget)
    };

    class OfflineActivationPage3 : public OfflineActivationPageBase
    {
    public:
        OfflineActivationPage3 (ContentHolder& parent, API& api_);
        ~OfflineActivationPage3 () override;

    private:
        LicenseFileDragAndDropTarget licenseFileDragAndDropTarget;

        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

        juce::Rectangle<float> txtArea;

        void paint  (juce::Graphics& g) override;
        void resized () override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OfflineActivationPage3)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
