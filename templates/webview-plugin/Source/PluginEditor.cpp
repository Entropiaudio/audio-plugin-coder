#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    juce::var makeUndoState (bool canUndo, bool canRedo)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("canUndo", canUndo);
        o->setProperty ("canRedo", canRedo);
        return juce::var (o);
    }
}

//==============================================================================
// One entry per parameter — add a param by adding its id here.
juce::StringArray __PLUGIN___AudioProcessorEditor::sliderParamIds()  { return { "gain", "cutoff" }; }
juce::StringArray __PLUGIN___AudioProcessorEditor::comboParamIds()   { return { "mode" }; }
juce::StringArray __PLUGIN___AudioProcessorEditor::toggleParamIds()  { return { "bypass" }; }

//==============================================================================
__PLUGIN___AudioProcessorEditor::__PLUGIN___AudioProcessorEditor (__PLUGIN___AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // ── 1. Relays first ──
    for (const auto& id : sliderParamIds())  sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (id));
    for (const auto& id : comboParamIds())   comboRelays.push_back  (std::make_unique<juce::WebComboBoxRelay> (id));
    for (const auto& id : toggleParamIds())  toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (id));

    // ── 2. WebView ──
    auto options = juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
            // unique per-instance folder — a shared folder crashes multi-instance on Windows
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("__PLUGIN__-" + juce::Uuid().toString())))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); })
        .withNativeFunction ("commitUndo",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                audioProcessor.commitUndoIfChanged();
                complete (makeUndoState (audioProcessor.canUndo(), audioProcessor.canRedo()));
            })
        .withNativeFunction ("undo",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                const bool ok = audioProcessor.undoState();
                if (ok && webView != nullptr)
                    webView->emitEventIfBrowserIsVisible ("stateReloaded", juce::var());
                complete (makeUndoState (audioProcessor.canUndo(), audioProcessor.canRedo()));
            })
        .withNativeFunction ("redo",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                const bool ok = audioProcessor.redoState();
                if (ok && webView != nullptr)
                    webView->emitEventIfBrowserIsVisible ("stateReloaded", juce::var());
                complete (makeUndoState (audioProcessor.canUndo(), audioProcessor.canRedo()));
            })
        .withNativeFunction ("setEditorSize",
            // AU hosts (Logic/Ableton) give no window-edge resize — the UI's
            // corner grip drives the editor size through this call.
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 2)
                {
                    const int w = juce::jlimit (480, 1600, (int) args[0]);
                    const int h = juce::jlimit (320, 1067, (int) args[1]);
                    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::AudioProcessorEditor> (this), w, h]
                    {
                        if (safe != nullptr) safe->setSize (w, h);
                    });
                }
                complete (true);
            });

    for (auto& r : sliderRelays)  options = options.withOptionsFrom (*r);
    for (auto& r : comboRelays)   options = options.withOptionsFrom (*r);
    for (auto& r : toggleRelays)  options = options.withOptionsFrom (*r);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // ── 3. Attachments last ──
    {
        int idx = 0;
        for (const auto& id : sliderParamIds())
        {
            if (auto* param = audioProcessor.apvts.getParameter (id))
                sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (*param, *sliderRelays[(size_t) idx], nullptr));
            ++idx;
        }
        idx = 0;
        for (const auto& id : comboParamIds())
        {
            if (auto* param = audioProcessor.apvts.getParameter (id))
                comboAttachments.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (*param, *comboRelays[(size_t) idx], nullptr));
            ++idx;
        }
        idx = 0;
        for (const auto& id : toggleParamIds())
        {
            if (auto* param = audioProcessor.apvts.getParameter (id))
                toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (*param, *toggleRelays[(size_t) idx], nullptr));
            ++idx;
        }
    }

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    const int savedW = audioProcessor.apvts.state.getProperty ("editorWidth",  720);
    const int savedH = audioProcessor.apvts.state.getProperty ("editorHeight", 480);
    setResizable (true, true);
    setResizeLimits (480, 320, 1600, 1067);
    constrainer.setFixedAspectRatio (720.0 / 480.0);
    setConstrainer (&constrainer);
    setSize (savedW, savedH);
}

__PLUGIN___AudioProcessorEditor::~__PLUGIN___AudioProcessorEditor()
{
    setConstrainer (nullptr);
}

//==============================================================================
void __PLUGIN___AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14110f));
}

void __PLUGIN___AudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
    audioProcessor.apvts.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    audioProcessor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
const char* __PLUGIN___AudioProcessorEditor::mimeForExtension (const juce::String& ext)
{
    if (ext == "html")  return "text/html";
    if (ext == "js")    return "text/javascript";
    if (ext == "css")   return "text/css";
    if (ext == "woff2") return "font/woff2";
    if (ext == "json")  return "application/json";
    if (ext == "svg")   return "image/svg+xml";
    if (ext == "png")   return "image/png";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource>
__PLUGIN___AudioProcessorEditor::getResource (const juce::String& url)
{
    auto clean = url.upToFirstOccurrenceOf ("?", false, false)
                    .upToFirstOccurrenceOf ("#", false, false);
    const auto requested = clean == "/" || clean.isEmpty()
                             ? juce::String ("index.html")
                             : clean.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (juce::String (BinaryData::originalFilenames[i]) == requested)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
            {
                std::vector<std::byte> bytes ((size_t) size);
                std::memcpy (bytes.data(), data, (size_t) size);
                return juce::WebBrowserComponent::Resource {
                    std::move (bytes),
                    juce::String (mimeForExtension (requested.fromLastOccurrenceOf (".", false, false)))
                };
            }
        }
    }
    return std::nullopt;
}
