#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
juce::StringArray EntropanAudioProcessorEditor::sliderParamIds()
{
    juce::StringArray ids { "seed", "amount", "output" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i) + "_";
        for (auto* s : { "freq", "width", "lift", "depth", "gain", "rate", "inertia", "phase" })
            ids.add (p + s);
    }
    return ids;   // 3 + 6×8 = 51
}

juce::StringArray EntropanAudioProcessorEditor::comboParamIds()
{
    juce::StringArray ids { "speed" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i) + "_";
        for (auto* s : { "mode", "ratemode", "div" })
            ids.add (p + s);
    }
    return ids;   // 1 + 6×3 = 19
}

juce::StringArray EntropanAudioProcessorEditor::toggleParamIds()
{
    juce::StringArray ids { "bypass" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
        ids.add ("b" + juce::String (i) + "_on");
    return ids;   // 1 + 6 = 7
}

//==============================================================================
EntropanAudioProcessorEditor::EntropanAudioProcessorEditor (EntropanAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // ── 1. Relays first ──
    for (const auto& id : sliderParamIds())
        sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (id));
    for (const auto& id : comboParamIds())
        comboRelays.push_back (std::make_unique<juce::WebComboBoxRelay> (id));
    for (const auto& id : toggleParamIds())
        toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (id));

    // ── 2. WebView ──
    auto options = juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); })
        .withNativeFunction ("reroll",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                audioProcessor.requestReroll();
                complete (true);
            })
        .withNativeFunction ("setSteps",
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 2)
                    audioProcessor.setStepsJson ((int) args[0], args[1].toString());
                complete (true);
            })
        .withNativeFunction ("getSteps",
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                const int band = args.size() > 0 ? (int) args[0] : 0;
                complete (audioProcessor.getStepsJson (band));
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
                sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (
                    *param, *sliderRelays[(size_t) idx], nullptr));
            ++idx;
        }
        idx = 0;
        for (const auto& id : comboParamIds())
        {
            if (auto* param = audioProcessor.apvts.getParameter (id))
                comboAttachments.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (
                    *param, *comboRelays[(size_t) idx], nullptr));
            ++idx;
        }
        idx = 0;
        for (const auto& id : toggleParamIds())
        {
            if (auto* param = audioProcessor.apvts.getParameter (id))
                toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (
                    *param, *toggleRelays[(size_t) idx], nullptr));
            ++idx;
        }
    }

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Saved size (Chaosverb pattern) — read BEFORE setResizeLimits clamps.
    const int savedW = audioProcessor.apvts.state.getProperty ("editorWidth",  900);
    const int savedH = audioProcessor.apvts.state.getProperty ("editorHeight", 560);
    setResizable (true, true);   // host edge-drag + corner resizer
    setResizeLimits (720, 448, 1620, 1008);
    constrainer.setFixedAspectRatio (900.0 / 560.0);
    setConstrainer (&constrainer);
    setSize (savedW, savedH);
}

EntropanAudioProcessorEditor::~EntropanAudioProcessorEditor()
{
    setConstrainer (nullptr);
}

//==============================================================================
void EntropanAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14110f));   // --bg-shell behind the WebView
}

void EntropanAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    audioProcessor.apvts.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    audioProcessor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
const char* EntropanAudioProcessorEditor::mimeForExtension (const juce::String& ext)
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
EntropanAudioProcessorEditor::getResource (const juce::String& url)
{
    // Root → index.html; otherwise match by original filename (all basenames
    // in the embedded set are unique: index.html, index.js,
    // check_native_interop.js, 3× .woff2).
    const auto requested = url == "/" ? juce::String ("index.html")
                                      : url.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const auto original = juce::String (BinaryData::originalFilenames[i]);
        if (original == requested)
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
