#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow (juce::AudioPluginInstance* plugin, int bandIdx, int slotIdx)
        : DocumentWindow (juce::String (bandIdx) + ":" + juce::String (slotIdx + 1) + " - " + plugin->getName(),
                          juce::Colours::darkgrey,
                          DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        ownedPlugin = plugin;
        if (auto* editor = plugin->createEditorIfNeeded())
        {
            setContentOwned (editor, true);
            setResizable (editor->isResizable(), false);
            centreWithSize (editor->getWidth(), editor->getHeight());
            setVisible (true);
            setAlwaysOnTop (true);
        }
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }

    juce::AudioPluginInstance* getOwnedPlugin() const noexcept { return ownedPlugin; }

private:
    juce::AudioPluginInstance* ownedPlugin = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
};
