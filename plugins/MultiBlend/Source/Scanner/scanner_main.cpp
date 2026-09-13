// MultiBlendScanner — out-of-process plugin scanner helper.
// Spawned by MultiBlend via juce::ChildProcess. Scans one plugin file.
// If plugin crashes, only this process dies; parent host stays alive.
//
// Usage: MultiBlendScanner <plugin-path> <output-xml-path>
// Exit 0 = success (XML written), non-zero = scan failed.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <iostream>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: MultiBlendScanner <plugin-path> <output-xml-path>\n";
        return 2;
    }

    juce::String pluginPath = argv[1];
    juce::String outPath    = argv[2];

    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::AudioPluginFormatManager formatManager;
    juce::addDefaultFormatsToManager (formatManager);

    juce::KnownPluginList list;
    juce::XmlElement root ("SCANRESULT");

    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat (i);
        juce::OwnedArray<juce::PluginDescription> descs;
        list.scanAndAddFile (pluginPath, true, descs, *format);

        for (auto* d : descs)
            if (auto xml = d->createXml())
                root.addChildElement (xml.release());

        if (root.getNumChildElements() > 0)
            break;
    }

    if (root.getNumChildElements() == 0)
        return 1;

    juce::File (outPath).replaceWithText (root.toString());
    return 0;
}
