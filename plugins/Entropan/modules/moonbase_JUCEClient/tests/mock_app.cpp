#include <juce_audio_processors/juce_audio_processors.h>
#include <moonbase_JUCEClient/moonbase_JUCEClient.h>
#include "BinaryData.h"

class MockApp : public juce::JUCEApplication
{
public:
    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
        // on CI, we start this as a background process
        juce::Process::makeForegroundProcess();
        mainWindow->toFront(true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    const juce::String getApplicationName() override { return "mock"; }
    const juce::String getApplicationVersion() override { return "v1"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name) : DocumentWindow (name,
            juce::Colours::lightgrey,
            DocumentWindow::allButtons)
        {
            centreWithSize (300, 200);
            setVisible (true);
            MOONBASE_SHOW_ACTIVATION_UI
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
        MOONBASE_DECLARE_LICENSING("Demo", "Mock App", "Mock App")
        MOONBASE_DECLARE_AND_INIT_ACTIVATION_UI_SAME_PARENT
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (MockApp)
