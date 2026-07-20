#if INCLUDE_MOONBASE_UI

#include "OfflineActivationPage.h"
#include "../ContentHolder.h"
#include "../../../API/APIImpl.h"
#include "../../../StaticMethods.h"
#include "../../Fonts.h"
//==============================================================================

OfflineActivationPageBase::OfflineActivationPageBase (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_)
{

}

OfflineActivationPageBase::~OfflineActivationPageBase ()
{

}

//==============================================================================

OfflineActivationPage1::OfflineActivationPage1 (ContentHolder& parent, API& api_)
:
OfflineActivationPageBase (parent, api_)
{
    addAndMakeVisible (machineFileButton);
    machineFileButton.onClick = std::bind (&OfflineActivationPage1::handleMachineFileButtonClicked, this);
    machineFileButton.setLookAndFeel (&wideTextButtonLAF);
}

OfflineActivationPage1::~OfflineActivationPage1 ()
{
    machineFileButton.setLookAndFeel (nullptr);
}

ContentBase::BackButtonConfig OfflineActivationPage1::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::ActivationMethod, false, true);
    }};
}

float OfflineActivationPage1::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair OfflineActivationPage1::getTitleAndSubtitle ()
{
    return { "Offline Activation", "Step 1/3" };
}

void OfflineActivationPage1::handleMachineFileButtonClicked ()
{
    api.generateMachineFile ([&](bool success, const juce::File& machineFile){
        juce::ignoreUnused (machineFile);
        if (success) contentHolder.setNextPage (ContentHolder::Offline2);
    });
}

void OfflineActivationPage1::paint  (juce::Graphics& g)
{
    OfflineActivationPageBase::paint (g);
    g.setColour ( juce::Colour (0xFF747474));

     juce::Font f = Roboto::Regular ();
    g.setFont (f.withHeight (static_cast<float> (getHeight ()) * (0.035f)));
    g.drawText ("This creates your personal machine file", txtArea,  juce::Justification::centred);

}

void OfflineActivationPage1::resized ()
{
    OfflineActivationPageBase::resized ();
    const auto area = getLocalBounds ();

    const auto buttonWidth = (int)(static_cast<float> (getWidth()) * (0.45f));
    const auto buttonHeight = (int)(static_cast<float> (buttonWidth) * wideTextButtonLAF.getRatioHoW ());
     juce::Rectangle<int> machineFileButtonArea (buttonWidth, buttonHeight);
    machineFileButtonArea.setCentre (area.getCentre ());
    machineFileButton.setBounds (machineFileButtonArea);

    txtArea = machineFileButtonArea.toFloat ().translated (0, static_cast<float> (getHeight ()) * (0.12f));
}

//==============================================================================

OfflineActivationPage2::OfflineActivationPage2 (ContentHolder& parent, API& api_)
:
OfflineActivationPageBase (parent, api_)
{
    addAndMakeVisible (linkButton);
    linkButton.onClick = std::bind (&OfflineActivationPage2::handleLinkButtonClicked, this);
    linkButton.setLookAndFeel (&textLinkLAF);
    linkButton.setName (api.getOfflineUrl ().toString (false).fromFirstOccurrenceOf ("https://", false, false));
    textLinkLAF.setUnderlined (true);
    textLinkLAF.setColours ( juce::Colour (0xFFD0D0D0),  juce::Colour (0xFFD0D0D0).brighter ());

    addAndMakeVisible (nextButton);
    nextButton.onClick = std::bind (&OfflineActivationPage2::handleNextButtonClicked, this);
    nextButton.setLookAndFeel (&narrowTextButtonLAF);
}

OfflineActivationPage2::~OfflineActivationPage2 ()
{
    linkButton.setLookAndFeel (nullptr);
    nextButton.setLookAndFeel (nullptr);
}

ContentBase::BackButtonConfig OfflineActivationPage2::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::Offline1, false, true);
    }};
}

float OfflineActivationPage2::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair OfflineActivationPage2::getTitleAndSubtitle ()
{
    return { "Offline Activation", "Step 2/3" };
}

void OfflineActivationPage2::handleLinkButtonClicked ()
{
    api.getOfflineUrl ().launchInDefaultBrowser ();
}

void OfflineActivationPage2::handleNextButtonClicked ()
{
    contentHolder.setNextPage (ContentHolder::Offline3);
}

void OfflineActivationPage2::paint  (juce::Graphics& g)
{
    OfflineActivationPageBase::paint (g);
    g.setColour ( juce::Colour (0xFFD0D0D0));

     juce::Font f = Roboto::Regular ();
    g.setFont (f.withHeight ((float)linkButton.getHeight ()));
    g.drawFittedText ("Take your machine file to a computer, that's connected to the internet and visit",
                      txtArea,  juce::Justification::centred, 2, 1.0f);

    g.drawFittedText ("Follow the instructions on the website and download your license file. Once you have it, click on 'Next'.",
                      txtArea.translated (0, (int)(static_cast<float> (getHeight ()) * (0.3f))),  juce::Justification::centred, 2, 1.0f);

}

void OfflineActivationPage2::resized ()
{
    OfflineActivationPageBase::resized ();
    const auto area = getLocalBounds ();

    const auto linkButtonHeight = static_cast<float> (getHeight ()) * (0.043f);
    const auto textLinkFont = Roboto::Regular ().withHeight (linkButtonHeight);
    
    #if JUCE_VERSION >= 0x080002
        const int linkButtonWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (textLinkFont, linkButton.getName()));
    #else
        const int linkButtonWidth = juce::roundToInt (textLinkFont.getStringWidth (linkButton.getName()));
    #endif
    
    juce::Rectangle<int> linkButtonArea (linkButtonWidth, (int)linkButtonHeight);
    linkButtonArea.setCentre (area.getCentre ());
    linkButton.setBounds (linkButtonArea);

    const auto nextButtonWidth = (int)(static_cast<float> (getWidth()) * (0.27f));
    const auto nextButtonHeight = (int)(narrowTextButtonLAF.getRatioHoW () * static_cast<float> (nextButtonWidth));
    juce::Rectangle<int> nextButtonArea ((int)(static_cast<float> (getWidth()) * (0.695f)),
                                          (int)(static_cast<float> (getHeight ()) * (0.82f)),
                                          nextButtonWidth, nextButtonHeight);
    nextButton.setBounds (nextButtonArea);

    const auto textWidth = static_cast<float> (getWidth ()) * (0.75f);
    const auto textHeight = linkButtonHeight * 2;
    const auto textX = (static_cast<float> (getWidth()) - textWidth) / 2.f;
    const auto textY = static_cast<float> (getHeight ()) * (0.32f);

    txtArea = juce::Rectangle<int> ((int)textX, (int)textY, (int)textWidth, (int)textHeight);
}



//==============================================================================
//==============================================================================
LicenseFileDragAndDropTarget::LicenseFileDragAndDropTarget (API& api_)
:
api (api_)
{
    targetIcon[0] = MB_LOAD_SVG (MoonbaseBinary::DropFile_Normal_svg);
    targetIcon[1]= MB_LOAD_SVG (MoonbaseBinary::DropFile_Interested_svg);
    targetIcon[2] = MB_LOAD_SVG (MoonbaseBinary::DropFile_NotInterested_svg);
}

void LicenseFileDragAndDropTarget::paint  (juce::Graphics& g)
{
    const auto area = getLocalBounds ().toFloat ();
    if (const auto& icon = targetIcon[isDraggingFile ? isInterestedInCurrentFile ? 1 : 2 : 0])
        icon->drawWithin (g, area, juce::RectanglePlacement::centred, 1.0f);
    else jassertfalse;
}

void LicenseFileDragAndDropTarget::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    isDraggingFile = true;
    isInterestedInCurrentFile = validateFile (files[0]);
    repaint ();
}

void LicenseFileDragAndDropTarget::fileDragExit (const juce::StringArray& files)
{
    juce::ignoreUnused (files);
    isDraggingFile = false;
    isInterestedInCurrentFile = false;
    repaint ();
}

bool LicenseFileDragAndDropTarget::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (files.size () != 1)
        return false;

    const auto& file = files[0];

    return file.endsWithIgnoreCase (offlineFileExtension);
}

bool LicenseFileDragAndDropTarget::validateFile (const juce::File& f)
{
    const auto validationResult = ValidateMoonbaseOfflineLicenseFile (*static_cast<APIImpl*>(api.impl), f);

    if (!validationResult.first)
        api.setError (validationResult.second);

    return f.getFileExtension () == offlineFileExtension && static_cast<bool>( validationResult.first );
}

void LicenseFileDragAndDropTarget::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    const auto result = files[0];

    if (validateFile (result))
        api.loadOfflineLicenseFile (result);
    else jassertfalse; //how did we get here?

    isDraggingFile = false;
    isInterestedInCurrentFile = false;
    repaint ();
}

int LicenseFileDragAndDropTarget::getHeightForWidth (int width)
{
    return (int)(static_cast<float> (width) * (static_cast<float> (targetIcon[0]->getHeight ()) / static_cast<float> (targetIcon[0]->getWidth ())));
}

void LicenseFileDragAndDropTarget::mouseDoubleClick (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    static const juce::String wildcard = "*"  + offlineFileExtension;

    fileChooser = std::make_unique<juce::FileChooser> ("Select " + offlineFileExtension + " file",
                                                juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
                                                wildcard);
    fileChooser->launchAsync (juce::FileBrowserComponent::FileChooserFlags::openMode | juce::FileBrowserComponent::FileChooserFlags::canSelectFiles,
    [&](const juce::FileChooser& fc) {
        const auto result = fc.getResult ();
        if (validateFile (result))
            api.loadOfflineLicenseFile (result);
    });
}

//==============================================================================
OfflineActivationPage3::OfflineActivationPage3 (ContentHolder& parent, API& api_)
:
OfflineActivationPageBase (parent, api_),
licenseFileDragAndDropTarget (api_)
{
    addAndMakeVisible (licenseFileDragAndDropTarget);
}

OfflineActivationPage3::~OfflineActivationPage3 ()
{

}

ContentBase::BackButtonConfig OfflineActivationPage3::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::Offline2, false, true);
    }};
}

float OfflineActivationPage3::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair OfflineActivationPage3::getTitleAndSubtitle ()
{
    return { "Offline Activation", "Last step" };
}

void OfflineActivationPage3::paint  (juce::Graphics& g)
{
    OfflineActivationPageBase::paint (g);
     juce::Font f = Roboto::Regular ();
    g.setFont (f.withHeight (txtArea.getHeight ()));
    g.setColour ( juce::Colour (0xFFD0D0D0));
    g.drawText ("Drop your license file below", txtArea,  juce::Justification::centred, true);
}

void OfflineActivationPage3::resized ()
{
    OfflineActivationPageBase::resized ();

    const auto area = getLocalBounds ();

    const auto dndWidth = (int)(static_cast<float> (getWidth ()) *  (0.7f));
    const auto dndHeight = licenseFileDragAndDropTarget.getHeightForWidth(dndWidth);

    juce::Rectangle<int> dndArea (dndWidth, dndHeight);
    const auto dndCentre = area.getCentre ().translated (0, (int)(static_cast<float> (getHeight ()) * (0.15f)));
    dndArea.setCentre (dndCentre);

    licenseFileDragAndDropTarget.setBounds (dndArea);

    txtArea = juce::Rectangle<float> (0.f, static_cast<float> (getHeight ()) * (0.33f),
                              (float)getWidth (), static_cast<float> (getHeight ()) * (0.047f));
}


#endif
