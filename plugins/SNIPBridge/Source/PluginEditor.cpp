#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
SNIPBridgeAudioProcessorEditor::SNIPBridgeAudioProcessorEditor (SNIPBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    DBG("SNIPBridge: Editor constructor started");

    //==========================================================================
    // CRITICAL: CREATION ORDER (Proven pattern from CloudWash)
    // 1. Relays already created (member initialization)
    // 2. Create attachments FIRST (before WebView)
    // 3. Create WebBrowserComponent with proper JUCE 8 API
    // 4. addAndMakeVisible LAST
    //==========================================================================

    // Create parameter attachments BEFORE WebView
    DBG("SNIPBridge: Creating parameter attachments");
    genreAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("target_genre"), genreRelay);

    windowAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("analysis_window"), windowRelay);

    // Create WebBrowserComponent with JUCE 8 API
    DBG("SNIPBridge: Creating WebView");
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory))
            )
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) { return getResource(url); })
            .withOptionsFrom(genreRelay)
            .withOptionsFrom(windowRelay)
            .withEventListener("sendToSnip", [this](const juce::var& /*event*/) {
                DBG("SNIPBridge: sendToSnip event received");
                sendAnalysisToSnip();
            })
            .withEventListener("resetLufs", [this](const juce::var& /*event*/) {
                DBG("SNIPBridge: resetLufs event received");
                audioProcessor.resetIntegratedLufs();
            })
    );

    // addAndMakeVisible AFTER attachments are created
    DBG("SNIPBridge: Adding WebView to component");
    addAndMakeVisible(*webView);

    // Load web content via resource provider
    DBG("SNIPBridge: Loading web content");
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    // Set editor size to match WebView UI design (1000x500)
    setSize(1000, 500);

    // Start timer for analysis updates (30 Hz)
    startTimerHz(30);

#if JUCE_DEBUG
    DBG("Resource provider root: " + juce::WebBrowserComponent::getResourceProviderRoot());
#endif

    DBG("SNIPBridge: Editor constructor completed");
}

SNIPBridgeAudioProcessorEditor::~SNIPBridgeAudioProcessorEditor()
{
    stopTimer();
    // Destruction happens in reverse order of declaration:
    // 1. Attachments destroyed first
    // 2. WebView destroyed next
    // 3. Relays destroyed last
}

//==============================================================================
void SNIPBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void SNIPBridgeAudioProcessorEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}

//==============================================================================
void SNIPBridgeAudioProcessorEditor::timerCallback()
{
    if (!webView || !webView->isVisible())
        return;

    // Read analysis data from processor (thread-safe atomics)
    float lufsS = audioProcessor.lufsShort.load();
    float lufsI = audioProcessor.lufsIntegrated.load();
    float rms   = audioProcessor.rmsLevel.load();
    float corr  = audioProcessor.stereoCorrelation.load();
    float width = audioProcessor.stereoWidth.load();

    // Read spectral bands
    float bands[6];
    for (int i = 0; i < 6; ++i)
        bands[i] = audioProcessor.spectralBands[i].load();

    // Compute spectrum (300 bins, log-spaced)
    float spectrumData[SNIPBridgeAudioProcessor::kSpectrumBins];
    bool hasSpectrum = audioProcessor.computeSpectrum (spectrumData, SNIPBridgeAudioProcessor::kSpectrumBins);

    // Build JSON payload
    auto* obj = new juce::DynamicObject();
    obj->setProperty("lufsShort", lufsS);
    obj->setProperty("lufsInt", lufsI);
    obj->setProperty("rms", rms);
    obj->setProperty("correlation", corr);
    obj->setProperty("width", width);

    // Spectral bands array (6 bands)
    juce::Array<juce::var> bandsArray;
    for (int i = 0; i < 6; ++i)
        bandsArray.add(bands[i]);
    obj->setProperty("bands", bandsArray);

    // Full spectrum array (300 bins) — only if new data available
    if (hasSpectrum)
    {
        juce::Array<juce::var> specArray;
        for (int i = 0; i < SNIPBridgeAudioProcessor::kSpectrumBins; ++i)
            specArray.add(spectrumData[i]);
        obj->setProperty("spectrum", specArray);
    }

    // Genre feedback scoring
    int genreIdx = (int)audioProcessor.apvts.getRawParameterValue("target_genre")->load();
    auto fb = audioProcessor.computeFeedback (genreIdx, lufsS, rms, width, corr,
                                              hasSpectrum ? spectrumData : nullptr,
                                              hasSpectrum ? SNIPBridgeAudioProcessor::kSpectrumBins : 0);

    auto* fbObj = new juce::DynamicObject();
    fbObj->setProperty("dynamics", fb.dynamics);
    fbObj->setProperty("tonality", fb.tonality);
    fbObj->setProperty("width", fb.width);
    fbObj->setProperty("correlation", fb.correlation);
    obj->setProperty("feedback", juce::var(fbObj));

    // Silence state
    obj->setProperty("isSilent", audioProcessor.isSilent.load());

    // Send to WebView via JUCE event system
    webView->emitEventIfBrowserIsVisible("analysisUpdate", juce::var(obj));
}

//==============================================================================
// ASYNC HTTP POST TO MEETSNIP.COM
//==============================================================================
void SNIPBridgeAudioProcessorEditor::sendAnalysisToSnip()
{
    if (isSending.load())
        return;

    isSending.store (true);

    // Gather analysis data on message thread
    int genreIdx = (int) audioProcessor.apvts.getRawParameterValue ("target_genre")->load();

    float spectrumData[SNIPBridgeAudioProcessor::kSpectrumBins];
    bool hasSpectrum = audioProcessor.computeSpectrum (spectrumData, SNIPBridgeAudioProcessor::kSpectrumBins);

    float lufsS = audioProcessor.lufsShort.load();
    float rms   = audioProcessor.rmsLevel.load();
    float width = audioProcessor.stereoWidth.load();
    float corr  = audioProcessor.stereoCorrelation.load();

    auto fb = audioProcessor.computeFeedback (genreIdx, lufsS, rms, width, corr,
                                              hasSpectrum ? spectrumData : nullptr,
                                              hasSpectrum ? SNIPBridgeAudioProcessor::kSpectrumBins : 0);

    juce::String jsonPayload = audioProcessor.buildAnalysisSnapshot (
        genreIdx, fb,
        hasSpectrum ? spectrumData : nullptr,
        hasSpectrum ? SNIPBridgeAudioProcessor::kSpectrumBins : 0);

    // Post on background thread
    juce::Thread::launch ([this, jsonPayload]()
    {
        bool success = false;
        juce::String errorMsg;

        juce::URL url ("https://meetsnip.com/api/analysis");
        auto request = url.withPOSTData (jsonPayload);

        int statusCode = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withConnectionTimeoutMs (5000)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withStatusCode (&statusCode);

        auto stream = request.createInputStream (options);

        if (stream != nullptr)
        {
            success = (statusCode >= 200 && statusCode < 300);
            if (! success)
                errorMsg = "HTTP " + juce::String (statusCode);
        }
        else
        {
            errorMsg = "Connection failed";
        }

        // Report result back on message thread
        juce::MessageManager::callAsync ([this, success, errorMsg]()
        {
            isSending.store (false);

            if (webView != nullptr)
            {
                auto* resultObj = new juce::DynamicObject();
                resultObj->setProperty ("success", success);
                if (! success)
                    resultObj->setProperty ("error", errorMsg);
                webView->emitEventIfBrowserIsVisible ("sendResult", juce::var (resultObj));
            }
        });
    });
}

//==============================================================================
// RESOURCE PROVIDER IMPLEMENTATION
//==============================================================================

const char* SNIPBridgeAudioProcessorEditor::getMimeForExtension(const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html" },
        { "css",  "text/css" },
        { "js",   "text/javascript" },
        { "mjs",  "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },
        { "jpg",  "image/jpeg" },
        { "svg",  "image/svg+xml" }
    };

    auto it = mimeMap.find(extension.toLowerCase());
    if (it != mimeMap.end())
        return it->second;

    return "text/plain";
}

juce::String SNIPBridgeAudioProcessorEditor::getExtension(juce::String filename)
{
    return filename.fromLastOccurrenceOf(".", false, false);
}

std::optional<juce::WebBrowserComponent::Resource> SNIPBridgeAudioProcessorEditor::getResource(const juce::String& url)
{
    auto resourcePath = url.fromFirstOccurrenceOf(
        juce::WebBrowserComponent::getResourceProviderRoot(), false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

#if JUCE_DEBUG
    DBG("Resource requested: " + url);
    DBG("Resource path: " + resourcePath);
#endif

    const char* resourceData = nullptr;
    int resourceSize = 0;
    juce::String mimeType;

    auto path = resourcePath.substring(1);  // Remove leading slash

    // All JS is inlined in index.html — only one resource needed
    if (path == "index.html")
    {
        resourceData = BinaryData::index_html;
        resourceSize = BinaryData::index_htmlSize;
        mimeType = "text/html";
    }

#if JUCE_DEBUG
    if (resourceData != nullptr)
        DBG("Resource FOUND: " + path + " (" + juce::String(resourceSize) + " bytes)");
    else
        DBG("Resource NOT FOUND: " + path);
#endif

    if (resourceData != nullptr && resourceSize > 0)
    {
        std::vector<std::byte> data(resourceSize);
        std::memcpy(data.data(), resourceData, resourceSize);

        return juce::WebBrowserComponent::Resource {
            std::move(data),
            mimeType
        };
    }

    // Resource not found
    juce::String fallbackHtml = R"(<!DOCTYPE html>
<html><head><title>SNIP Bridge - Not Found</title>
<style>body{background:#050505;color:#E8E8F0;font-family:sans-serif;padding:40px;}
h1{color:#E91E8C;}code{background:#1C1C22;padding:2px 6px;border-radius:3px;}</style>
</head><body><h1>SNIP Bridge - Resource Not Found</h1>
<p>Could not load: <code>)" + path + R"(</code></p></body></html>)";

    std::vector<std::byte> fallbackData((size_t)fallbackHtml.length());
    std::memcpy(fallbackData.data(), fallbackHtml.toRawUTF8(), (size_t)fallbackHtml.length());

    return juce::WebBrowserComponent::Resource { std::move(fallbackData), "text/html" };
}
