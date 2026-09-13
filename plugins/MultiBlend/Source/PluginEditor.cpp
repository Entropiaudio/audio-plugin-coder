#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
MultiBlendAudioProcessorEditor::MultiBlendAudioProcessorEditor (MultiBlendAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    //==========================================================================
    // 1. Create relays from shared parameter ID list
    //==========================================================================
    auto paramInfos = ParamIDs::getAllParams();

    for (auto& info : paramInfos)
    {
        if (info.isToggle)
            toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (info.id));
        else
            sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (info.id));
    }

    //==========================================================================
    // 2. Create attachments BEFORE WebView
    //==========================================================================
    for (size_t i = 0; i < sliderRelays.size(); ++i)
    {
        // Find matching parameter by iterating param infos (slider-only)
        auto& relay = sliderRelays[i];
        // Build ID from the relay — we track IDs via the paramInfos order
        juce::String paramId;
        int sliderIdx = 0;
        for (auto& info : paramInfos)
        {
            if (! info.isToggle)
            {
                if (sliderIdx == (int) i)
                {
                    paramId = info.id;
                    break;
                }
                ++sliderIdx;
            }
        }

        if (paramId.isNotEmpty())
        {
            auto* param = audioProcessor.apvts.getParameter (paramId);
            if (param != nullptr)
                sliderAttachments.push_back (
                    std::make_unique<juce::WebSliderParameterAttachment> (*param, *relay));
        }
    }

    for (size_t i = 0; i < toggleRelays.size(); ++i)
    {
        juce::String paramId;
        int ti = 0;
        for (auto& info : paramInfos)
        {
            if (info.isToggle)
            {
                if (ti == (int) i)
                {
                    paramId = info.id;
                    break;
                }
                ++ti;
            }
        }

        if (paramId.isNotEmpty())
        {
            auto* param = audioProcessor.apvts.getParameter (paramId);
            if (param != nullptr)
                toggleAttachments.push_back (
                    std::make_unique<juce::WebToggleButtonParameterAttachment> (*param, *toggleRelays[i]));
        }
    }

    //==========================================================================
    // 3. Build WebBrowserComponent with all relays + native functions
    //==========================================================================
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (
                    juce::File::SpecialLocationType::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); });

    // Chain all slider relays
    for (auto& relay : sliderRelays)
        options = options.withOptionsFrom (*relay);

    // Chain all toggle relays
    for (auto& relay : toggleRelays)
        options = options.withOptionsFrom (*relay);

    // Native functions for plugin hosting
    options = options
        .withNativeFunction ("getPluginList", [this] (const auto& args, auto complete)
        {
            juce::Array<juce::var> pluginArray;
            auto plugins = audioProcessor.hostManager.getAvailablePlugins();
            for (int i = 0; i < plugins.size(); ++i)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty ("index", i);
                obj->setProperty ("name", plugins[i].name);
                obj->setProperty ("manufacturer", plugins[i].manufacturerName);
                obj->setProperty ("category", plugins[i].category);
                obj->setProperty ("format", plugins[i].pluginFormatName);
                pluginArray.add (juce::var (obj));
            }
            complete (pluginArray);
        })
        .withNativeFunction ("loadPlugin", [this] (const auto& args, auto complete)
        {
            if (args.size() < 3) { complete (juce::var ("error: need band, slot, pluginIndex")); return; }

            int band = (int) args[0];
            int slot = (int) args[1];
            int pluginIdx = (int) args[2];

            auto plugins = audioProcessor.hostManager.getAvailablePlugins();
            if (pluginIdx < 0 || pluginIdx >= plugins.size()) { complete (juce::var ("error: invalid index")); return; }
            if (band < 0 || band >= 3 || slot < 0 || slot >= 5) { complete (juce::var ("error: invalid band/slot")); return; }

            // Undoable: before = whatever's there now, after = new plugin (default state)
            auto before = audioProcessor.captureSlot (band, slot);
            MultiBlendAudioProcessor::SlotPluginState after;
            after.loaded = true;
            after.desc   = plugins[pluginIdx];
            juce::MessageManager::callAsync ([this, band, slot, before, after]()
            {
                editorWindows.erase ({ band, slot });
                audioProcessor.pushSlotPluginAction (band, slot, before, after, "Load Plugin");
            });
            complete (juce::var ("loading"));
        })
        .withNativeFunction ("unloadPlugin", [this] (const auto& args, auto complete)
        {
            if (args.size() < 2) { complete (juce::var ("error")); return; }
            int band = (int) args[0];
            int slot = (int) args[1];
            if (band < 0 || band >= 3 || slot < 0 || slot >= 5) { complete (juce::var ("error")); return; }

            auto before = audioProcessor.captureSlot (band, slot);   // desc + state
            MultiBlendAudioProcessor::SlotPluginState after;          // empty = unloaded
            juce::MessageManager::callAsync ([this, band, slot, before, after]()
            {
                editorWindows.erase ({ band, slot });
                audioProcessor.pushSlotPluginAction (band, slot, before, after, "Remove Plugin");
            });
            complete (juce::var ("unloaded"));
        })
        .withNativeFunction ("openPluginEditor", [this] (const auto& args, auto complete)
        {
            if (args.size() < 2) { complete (juce::var ("error")); return; }
            int band = (int) args[0];
            int slot = (int) args[1];
            if (band < 0 || band >= 3 || slot < 0 || slot >= 5) { complete (juce::var ("error")); return; }

            auto* plugin = audioProcessor.getSlot (band, slot).getPluginInstance();
            if (plugin == nullptr) { complete (juce::var ("no plugin loaded")); return; }

            auto key = std::make_pair (band, slot);
            // Validate: existing window's plugin must match current slot's plugin.
            // Stale windows (post-shuffle) get destroyed + recreated.
            auto it = editorWindows.find (key);
            if (it != editorWindows.end())
            {
                if (it->second->getOwnedPlugin() != plugin)
                    editorWindows.erase (it);   // stale — destroy + recreate
            }

            if (editorWindows.count (key) && editorWindows[key]->isVisible())
                editorWindows[key]->toFront (true);
            else
                editorWindows[key] = std::make_unique<PluginEditorWindow> (plugin, band, slot);

            complete (juce::var ("opened"));
        })
        .withNativeFunction ("scanPlugins", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // args[0] (optional) = array of format names (e.g. ["VST3","AudioUnit"]). Empty = all.
            std::set<juce::String> formats;
            if (args.size() >= 1 && args[0].isArray())
                for (auto& v : *args[0].getArray())
                    formats.insert (v.toString());

            audioProcessor.hostManager.startScan ([this]() {
                if (webView != nullptr)
                    webView->emitEventIfBrowserIsVisible ("scanComplete", juce::var (true));
            }, formats);
            complete (juce::var ("started"));
        })
        .withNativeFunction ("getScanProgress", [this] (const auto& args, auto complete)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("scanning", audioProcessor.hostManager.isScanning());
            obj->setProperty ("progress", audioProcessor.hostManager.getScanProgress());
            obj->setProperty ("current", audioProcessor.hostManager.getCurrentScanFile());
            obj->setProperty ("count", audioProcessor.hostManager.getNumPlugins());
            complete (juce::var (obj));
        })
        .withNativeFunction ("loadPluginByPath", [this] (const auto& args, auto complete)
        {
            if (args.size() < 3) { complete (juce::var ("error: need band, slot, path")); return; }
            int band = (int) args[0];
            int slot = (int) args[1];
            juce::String path = args[2].toString();
            if (band < 0 || band >= 3 || slot < 0 || slot >= 5) { complete (juce::var ("error: bad band/slot")); return; }

            // Scan the file first to get description
            for (int i = 0; i < audioProcessor.hostManager.formatManager.getNumFormats(); ++i)
            {
                auto* format = audioProcessor.hostManager.formatManager.getFormat (i);
                juce::OwnedArray<juce::PluginDescription> descs;
                audioProcessor.hostManager.knownPluginList.scanAndAddFile (path, true, descs, *format);
                if (descs.size() > 0)
                {
                    audioProcessor.hostManager.saveKnownPluginListToDisk();
                    audioProcessor.hostManager.createPluginAsync (*descs[0],
                        audioProcessor.getSampleRate(), audioProcessor.getBlockSize(),
                        [this, band, slot, complete] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& err)
                        {
                            if (instance != nullptr)
                            {
                                auto name = instance->getName();
                                audioProcessor.getSlot (band, slot).loadPlugin (std::move (instance));
                                complete (juce::var ("loaded: " + name));
                            }
                            else complete (juce::var ("error: " + err));
                        });
                    return;
                }
            }
            complete (juce::var ("error: file not recognized as plugin"));
        })
        .withNativeFunction ("clearBlacklist", [this] (const auto& args, auto complete)
        {
            audioProcessor.hostManager.knownPluginList.clearBlacklistedFiles();
            audioProcessor.hostManager.saveKnownPluginListToDisk();
            complete (juce::var (true));
        })
        .withNativeFunction ("moveSlot", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // args = [srcBand, srcSlot, dstBand, dstSlot]
            if (args.size() < 4) { complete (juce::var ("error")); return; }
            int sb = (int) args[0], ss = (int) args[1], db = (int) args[2], ds = (int) args[3];
            juce::MessageManager::callAsync ([this, sb, ss, db, ds]()
            {
                audioProcessor.closeChaosIfOpen();
                auto& src = audioProcessor.getSlot (sb, ss);
                auto& dst = audioProcessor.getSlot (db, ds);
                if (! src.isLoaded()) return;
                closeAllEditorWindows();   // dst.unloadPlugin frees old → kill its window first
                dst.unloadPlugin();
                auto* p = src.detachPlugin();
                auto d = src.detachDescription();
                dst.attachPlugin (p, d);
                audioProcessor.updateLatency();
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("duplicateSlot", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // args = [srcBand, srcSlot, dstBand, dstSlot]
            if (args.size() < 4) { complete (juce::var ("error")); return; }
            int sb = (int) args[0], ss = (int) args[1], db = (int) args[2], ds = (int) args[3];

            auto* srcPlugin = audioProcessor.getSlot (sb, ss).getPluginInstance();
            if (srcPlugin == nullptr) { complete (juce::var ("no source plugin")); return; }

            auto desc = srcPlugin->getPluginDescription();
            // Snapshot source state on message thread (safe)
            juce::MemoryBlock state;
            srcPlugin->getStateInformation (state);

            const double sr = audioProcessor.getSampleRate();
            const int    bs = audioProcessor.getBlockSize();

            audioProcessor.hostManager.createPluginAsync (desc, sr, bs,
                [this, db, ds, state] (std::unique_ptr<juce::AudioPluginInstance> inst,
                                       const juce::String& err)
                {
                    if (inst == nullptr) { DBG ("duplicateSlot create failed: " + err); return; }
                    audioProcessor.closeChaosIfOpen();
                    closeAllEditorWindows();   // dst loadPlugin frees old → kill its window first
                    inst->setStateInformation (state.getData(), (int) state.getSize());
                    audioProcessor.getSlot (db, ds).loadPlugin (std::move (inst));
                    audioProcessor.updateLatency();
                });
            complete (juce::var (true));
        })
        .withNativeFunction ("undo", [this] (const auto& args, auto complete)
        {
            juce::ignoreUnused (args);
            juce::MessageManager::callAsync ([this]()
            {
                closeAllEditorWindows();   // undo may unload/replace plugins → kill windows first
                audioProcessor.undoManager.beginNewTransaction();
                audioProcessor.undoManager.undo();
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("redo", [this] (const auto& args, auto complete)
        {
            juce::ignoreUnused (args);
            juce::MessageManager::callAsync ([this]()
            {
                closeAllEditorWindows();   // redo may unload/replace plugins → kill windows first
                audioProcessor.undoManager.beginNewTransaction();
                audioProcessor.undoManager.redo();
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("getUndoState", [this] (const auto& args, auto complete)
        {
            juce::ignoreUnused (args);
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("canUndo", audioProcessor.undoManager.canUndo());
            obj->setProperty ("canRedo", audioProcessor.undoManager.canRedo());
            complete (juce::var (obj));
        })
        .withNativeFunction ("beginUndoTransaction", [this] (const auto& args, auto complete)
        {
            juce::ignoreUnused (args);
            audioProcessor.undoManager.beginNewTransaction();
            complete (juce::var (true));
        })
        .withNativeFunction ("getCrossoverMode", [this] (const auto& args, auto complete)
        {
            // returns { dynamic: bool, linearPhase: bool }
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("dynamic", audioProcessor.apvts.getRawParameterValue ("dynamic_phase")->load() > 0.5f);
            obj->setProperty ("linearPhase", audioProcessor.apvts.getRawParameterValue ("linear_phase")->load() > 0.5f);
            complete (juce::var (obj));
        })
        .withNativeFunction ("setCrossoverMode", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // args[0] = "dynamic" | "zerolatency" | "linearphase"
            if (args.size() >= 1)
            {
                auto mode = args[0].toString();
                auto set = [this] (const char* id, bool on)
                {
                    if (auto* p = audioProcessor.apvts.getParameter (id))
                        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
                };
                if (mode == "dynamic")        { set ("dynamic_phase", true);  set ("linear_phase", false); }
                else if (mode == "zerolatency") { set ("dynamic_phase", false); set ("linear_phase", false); }
                else if (mode == "linearphase") { set ("dynamic_phase", false); set ("linear_phase", true); }
            }
            complete (juce::var (true));
        })
        .withNativeFunction ("getHostBpm", [this] (const auto& args, auto complete)
        {
            double bpm = 0.0;
            if (auto* ph = audioProcessor.getPlayHead())
            {
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm())
                        bpm = *b;
            }
            complete (juce::var (bpm));
        })
        .withNativeFunction ("triggerChaos", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // args[0] = locked slot coords (linear ints). args[1] = locked param IDs (strings).
            std::set<int> lockedSlots;
            std::set<juce::String> lockedParams;
            if (args.size() >= 1 && args[0].isArray())
                for (auto& v : *args[0].getArray()) lockedSlots.insert ((int) v);
            if (args.size() >= 2 && args[1].isArray())
                for (auto& v : *args[1].getArray()) lockedParams.insert (v.toString());

            juce::MessageManager::callAsync ([this, lockedSlots, lockedParams]()
            {
                closeAllEditorWindows();   // chaos relocates plugins; a later undo frees them → no dangling windows
                audioProcessor.triggerChaos (lockedSlots, lockedParams);
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("shufflePlugins", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            // arg[0] = array of locked linear coords (band * 5 + slot)
            std::set<int> locked;
            if (args.size() >= 1 && args[0].isArray())
            {
                for (auto& v : *args[0].getArray())
                    locked.insert ((int) v);
            }
            juce::MessageManager::callAsync ([this, locked]()
            {
                // Close windows first: shuffle relocates plugins and a subsequent
                // undo/replace frees them — a window owning that plugin's editor
                // would then dangle (UAD-style main-thread timer → use-after-free).
                closeAllEditorWindows();
                audioProcessor.shufflePlugins (locked);
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("setEditorSize", [this] (const juce::Array<juce::var>& args, auto complete)
        {
            if (args.size() >= 2) {
                const int w = (int) args[0];
                const int h = (int) args[1];
                juce::MessageManager::callAsync ([this, w, h]() { setSize (w, h); });
            }
            complete (juce::var (true));
        })
        .withNativeFunction ("listPresets", [this] (const auto& args, auto complete)
        {
            // Preset stub — returns empty list. Wire to real preset manager later.
            juce::Array<juce::var> names;
            complete (names);
        })
        .withNativeFunction ("savePreset", [this] (const auto& args, auto complete)
        {
            complete (juce::var (true));
        })
        .withNativeFunction ("loadPreset", [this] (const auto& args, auto complete)
        {
            complete (juce::var (true));
        })
        .withNativeFunction ("deletePreset", [this] (const auto& args, auto complete)
        {
            complete (juce::var (true));
        })
        .withNativeFunction ("resetToDefaults", [this] (const auto& args, auto complete)
        {
            // Reset all APVTS params to defaults
            juce::MessageManager::callAsync ([this]() {
                auto& apvts = audioProcessor.apvts;
                for (auto* p : audioProcessor.getParameters())
                    p->setValueNotifyingHost (p->getDefaultValue());
            });
            complete (juce::var (true));
        })
        .withNativeFunction ("saveCurrentAsDefaults", [this] (const auto& args, auto complete)
        {
            complete (juce::var (true));
        })
        .withNativeFunction ("openExternalURL", [this] (const auto& args, auto complete)
        {
            if (args.size() >= 1) {
                juce::URL (args[0].toString()).launchInDefaultBrowser();
            }
            complete (juce::var (true));
        })
        .withNativeFunction ("getSpectrum", [this] (const auto& args, auto complete)
        {
            // LOG-spaced bins (20 Hz .. min(20k, Nyquist)) → even detail across the
            // log frequency axis. Sparse low end interpolates between FFT bins;
            // dense high end takes the band peak.
            constexpr int numLin = MultiBlendAudioProcessor::fftSize / 2;
            constexpr int LB = 480;                       // log output bins
            double fs = audioProcessor.getSampleRate();
            if (fs < 1.0) fs = 48000.0;
            const double binHz = fs / (double) MultiBlendAudioProcessor::fftSize;
            const double fmin = 20.0;
            const double fmax = juce::jmin (20000.0, fs * 0.5);
            const double ratio = fmax / fmin;

            auto buildLayer = [&] (MultiBlendAudioProcessor::FFTChannel ch) {
                std::vector<float> mags ((size_t) numLin, 0.0f);
                audioProcessor.getNextFFTBlock (mags.data(), numLin, ch);
                juce::Array<juce::var> arr;
                arr.ensureStorageAllocated (LB);
                for (int j = 0; j < LB; ++j)
                {
                    const double fLo = fmin * std::pow (ratio, (double) j       / LB);
                    const double fHi = fmin * std::pow (ratio, (double)(j + 1)  / LB);
                    const double fC  = std::sqrt (fLo * fHi);
                    const double k0d = fLo / binHz;
                    const double k1d = fHi / binHz;
                    float v;
                    if (k1d - k0d <= 1.0)
                    {
                        // sparse (low freq): linear-interp FFT magnitude at band center
                        const double bp = fC / binHz;
                        int i0 = (int) std::floor (bp);
                        i0 = juce::jlimit (0, numLin - 1, i0);
                        int i1 = juce::jlimit (0, numLin - 1, i0 + 1);
                        const float frac = (float) (bp - std::floor (bp));
                        v = mags[(size_t) i0] * (1.0f - frac) + mags[(size_t) i1] * frac;
                    }
                    else
                    {
                        // dense (high freq): peak across the band
                        int k0 = juce::jlimit (0, numLin - 1, (int) std::floor (k0d));
                        int k1 = juce::jlimit (0, numLin - 1, (int) std::ceil  (k1d));
                        v = 0.0f;
                        for (int k = k0; k <= k1; ++k) v = juce::jmax (v, mags[(size_t) k]);
                    }
                    arr.add ((double) v);
                }
                return arr;
            };

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("input",  buildLayer (MultiBlendAudioProcessor::FFTChannel::Input));
            obj->setProperty ("output", buildLayer (MultiBlendAudioProcessor::FFTChannel::Output));
            obj->setProperty ("logBins", LB);
            obj->setProperty ("fmin", fmin);
            obj->setProperty ("fmax", fmax);
            complete (juce::var (obj));
        })
        .withNativeFunction ("getSlotStatus", [this] (const auto& args, auto complete)
        {
            juce::Array<juce::var> bandsArray;
            for (int b = 0; b < 3; ++b)
            {
                juce::Array<juce::var> slotsArray;
                for (int s = 0; s < 5; ++s)
                {
                    auto& slot = audioProcessor.getSlot (b, s);
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty ("loaded", slot.isLoaded());
                    obj->setProperty ("name", slot.getPluginName());
                    slotsArray.add (juce::var (obj));
                }
                bandsArray.add (slotsArray);
            }
            complete (bandsArray);
        });

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Read saved size before setResizeLimits — clamping triggers resized() which
    // would overwrite saved values.
    int savedW = audioProcessor.apvts.state.getProperty ("editorWidth2",  1050);
    int savedH = audioProcessor.apvts.state.getProperty ("editorHeight2", 600);

    setResizable (true, true);
    setResizeLimits (650, 380, 2400, 1400);
    // Lock aspect ratio so resize stays proportional — no empty bg ever
    getConstrainer()->setFixedAspectRatio (1050.0 / 600.0);
    setSize (savedW, savedH);

    startTimerHz (30);
}

MultiBlendAudioProcessorEditor::~MultiBlendAudioProcessorEditor()
{
    stopTimer();
    editorWindows.clear();
}

//==============================================================================
void MultiBlendAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Match WebView shell bg so corner cell blends in
    g.fillAll (juce::Colour (0xff14110f));
}

void MultiBlendAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    audioProcessor.apvts.state.setProperty ("editorWidth2",  getWidth(),  nullptr);
    audioProcessor.apvts.state.setProperty ("editorHeight2", getHeight(), nullptr);
}

//==============================================================================
void MultiBlendAudioProcessorEditor::timerCallback()
{
    // Latency refresh — every ~250 ms (every 8th tick at 30 Hz). Cheap query but
    // do not call every frame in case host's setLatencySamples triggers reload.
    if ((++latencyTickCounter % 8) == 0)
        audioProcessor.updateLatency();

    // Undo transaction coalescing: close transaction after ~500ms idle so
    // each drag/edit = one undo step. EXCEPTION: "Chaos" transaction stays
    // open indefinitely so multiple CHAOS clicks (manual or timer) collapse
    // into a single undo step. Closed only when user does non-chaos action
    // (which opens its own transaction via APVTS attachment).
    {
        const int cur = audioProcessor.undoManager.getNumActionsInCurrentTransaction();
        const bool isChaosTx = audioProcessor.undoManager.getCurrentTransactionName() == "Chaos";
        if (cur > 0 && ! isChaosTx)
        {
            if (cur != lastUndoActionCount) { undoIdleTicks = 0; lastUndoActionCount = cur; }
            else if (++undoIdleTicks > 15)
            {
                audioProcessor.undoManager.beginNewTransaction();
                undoIdleTicks = 0;
                lastUndoActionCount = 0;
            }
        }
        else
        {
            undoIdleTicks = 0;
            lastUndoActionCount = 0;
        }
    }

    if (webView == nullptr || ! webView->isVisible())
        return;

    // Send slot status to WebView
    auto* obj = new juce::DynamicObject();

    juce::Array<juce::var> bandsArray;
    for (int b = 0; b < 3; ++b)
    {
        juce::Array<juce::var> slotsArray;
        for (int s = 0; s < 5; ++s)
        {
            auto& slot = audioProcessor.getSlot (b, s);
            auto* slotObj = new juce::DynamicObject();
            slotObj->setProperty ("loaded", slot.isLoaded());
            slotObj->setProperty ("name", slot.getPluginName());
            slotsArray.add (juce::var (slotObj));
        }
        bandsArray.add (slotsArray);
    }
    obj->setProperty ("slots", bandsArray);

    webView->emitEventIfBrowserIsVisible ("slotStatusUpdate", juce::var (obj));
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
MultiBlendAudioProcessorEditor::getResource (const juce::String& url)
{
    auto urlToRetrieve = url == "/" ? juce::String ("/index.html") : url;

    // Map URL to binary resource name — try full path first, then filename only
    auto fullPath = urlToRetrieve.fromFirstOccurrenceOf ("/", false, false);
    auto resourceName = fullPath.replace ("/", "_").replace (".", "_").replace ("-", "");

    int dataSize = 0;
    auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize);

    // Fallback: JUCE strips directory prefixes from BinaryData names
    if (data == nullptr)
    {
        auto filenameOnly = fullPath.fromLastOccurrenceOf ("/", false, false)
                                .replace (".", "_").replace ("-", "");
        data = BinaryData::getNamedResource (filenameOnly.toRawUTF8(), dataSize);
    }

    if (data != nullptr)
    {
        auto extension = urlToRetrieve.fromLastOccurrenceOf (".", false, false);
        auto mime = juce::String (getMimeForExtension (extension));

        return juce::WebBrowserComponent::Resource {
            std::vector<std::byte> (reinterpret_cast<const std::byte*> (data),
                                    reinterpret_cast<const std::byte*> (data) + dataSize),
            mime
        };
    }
    return std::nullopt;
}

const char* MultiBlendAudioProcessorEditor::getMimeForExtension (const juce::String& ext)
{
    if (ext == "html") return "text/html";
    if (ext == "js")   return "application/javascript";
    if (ext == "css")  return "text/css";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "json") return "application/json";
    if (ext == "woff2") return "font/woff2";
    if (ext == "woff") return "font/woff";
    if (ext == "ttf") return "font/ttf";
    return "application/octet-stream";
}
