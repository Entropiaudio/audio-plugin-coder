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
            // unique per-instance folder — shared folder crashes multi-instance (kit gotcha)
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("Entropan-" + juce::Uuid().toString())))
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
            })
        .withNativeFunction ("setEditorSize",
            // AU hosts (Logic/Ableton) give no window-edge resize — the UI's
            // corner grip drives the editor size through this call (kit gotcha).
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 2)
                {
                    const int w = juce::jlimit (720, 1620, (int) args[0]);
                    const int h = juce::jlimit (448, 1008, (int) args[1]);
                    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::AudioProcessorEditor> (this), w, h]
                    {
                        if (safe != nullptr)
                            safe->setSize (w, h);
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

    fifoDrain.resize (kFftSize);
    fftAccum.resize (kFftSize, 0.0f);
    fftWork.resize ((size_t) kFftSize * 2, 0.0f);
    startTimerHz (30);
}

EntropanAudioProcessorEditor::~EntropanAudioProcessorEditor()
{
    stopTimer();
    setConstrainer (nullptr);
}

//==============================================================================
void EntropanAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr)
        return;

    // ── per-band mod values (post-slew × depth) + last MIDI note ──
    {
        juce::Array<juce::var> pans;
        for (int i = 0; i < EntropanAudioProcessor::kNumBands; ++i)
            pans.add ((double) audioProcessor.modOutDepth[(size_t) i].load (std::memory_order_relaxed));

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("pans", pans);
        obj->setProperty ("midiNote", audioProcessor.lastMidiNote.load());
        webView->emitEventIfBrowserIsVisible ("modvals", juce::var (obj));
    }

    // ── high-rate scope samples (all bands, ~750 Hz ring) ──
    {
        const int w = audioProcessor.scopeWrite.load (std::memory_order_acquire);
        int n = w - scopeReadPos;
        if (n > 0)
        {
            n = juce::jmin (n, EntropanAudioProcessor::kScopeRingSize / 2);
            const int startIdx = w - n;
            juce::Array<juce::var> bandsArr;
            for (int i = 0; i < EntropanAudioProcessor::kNumBands; ++i)
            {
                juce::Array<juce::var> vals;
                vals.ensureStorageAllocated (n);
                for (int k = 0; k < n; ++k)
                    vals.add ((double) audioProcessor.scopeRing[(size_t) i]
                        [(size_t) ((startIdx + k) & (EntropanAudioProcessor::kScopeRingSize - 1))]);
                bandsArr.add (juce::var (vals));
            }
            scopeReadPos = w;
            auto* so = new juce::DynamicObject();
            so->setProperty ("bands", bandsArr);
            webView->emitEventIfBrowserIsVisible ("scopevals", juce::var (so));
        }
        else if (n < 0)
        {
            scopeReadPos = w;
        }
    }

    // ── spectrum: drain FIFO into a sliding FFT frame ──
    const int got = audioProcessor.popAnalyzer (fifoDrain.data(), kFftSize);
    if (got > 0)
    {
        if (got >= kFftSize)
        {
            std::memcpy (fftAccum.data(), fifoDrain.data() + (got - kFftSize), sizeof (float) * kFftSize);
            accumFill = kFftSize;
        }
        else
        {
            const int keep = kFftSize - got;
            std::memmove (fftAccum.data(), fftAccum.data() + got, sizeof (float) * (size_t) keep);
            std::memcpy (fftAccum.data() + keep, fifoDrain.data(), sizeof (float) * (size_t) got);
            accumFill = juce::jmin (kFftSize, accumFill + got);
        }
    }

    if (accumFill >= kFftSize)
    {
        std::memcpy (fftWork.data(), fftAccum.data(), sizeof (float) * kFftSize);
        window.multiplyWithWindowingTable (fftWork.data(), kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftWork.data());

        // log-resample 20 Hz … 20 kHz into kSpectrumBins dB values
        const double sr = audioProcessor.getSampleRate() > 0 ? audioProcessor.getSampleRate() : 48000.0;
        const double binHz = sr / (double) kFftSize;
        juce::Array<juce::var> mags;
        mags.ensureStorageAllocated (kSpectrumBins);
        for (int b = 0; b < kSpectrumBins; ++b)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) b / kSpectrumBins);
            const double f1 = 20.0 * std::pow (1000.0, (double) (b + 1) / kSpectrumBins);
            int i0 = juce::jlimit (1, kFftSize / 2 - 1, (int) (f0 / binHz));
            int i1 = juce::jlimit (i0 + 1, kFftSize / 2, (int) std::ceil (f1 / binHz));
            float peak = 0.0f;
            for (int k = i0; k < i1; ++k)
                peak = juce::jmax (peak, fftWork[(size_t) k]);
            const double db = juce::Decibels::gainToDecibels ((double) peak / (double) (kFftSize / 4), -80.0);
            mags.add (juce::jlimit (-60.0, 0.0, db));
        }
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("mags", mags);
        webView->emitEventIfBrowserIsVisible ("spectrum", juce::var (obj));
    }
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
