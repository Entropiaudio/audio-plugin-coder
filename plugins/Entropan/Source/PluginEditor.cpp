#include "PluginEditor.h"
#include "BinaryData.h"

#if ENTROPAN_MOONBASE
 #include <moonbase_JUCEClient/moonbase_JUCEClient.h>   // ActivationUI + MOONBASE_* macros
#endif

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
juce::StringArray EntropanAudioProcessorEditor::sliderParamIds()
{
    juce::StringArray ids { "seed", "amount", "output", "wow", "flutter", "env_atk", "env_rel", "env_scf", "env_gain" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i) + "_";
        for (auto* s : { "freq", "width", "lift", "depth", "gain", "rate", "inertia", "phase", "bias", "stepsmooth", "slope" })
            ids.add (p + s);
    }
    return ids;   // 9 + 6×11 = 75
}

juce::StringArray EntropanAudioProcessorEditor::comboParamIds()
{
    juce::StringArray ids { "speed", "routing" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i) + "_";
        for (auto* s : { "mode", "ratemode", "div" })
            ids.add (p + s);
    }
    return ids;   // 2 + 6×3 = 20
}

juce::StringArray EntropanAudioProcessorEditor::toggleParamIds()
{
    juce::StringArray ids { "bypass", "env_rms" };
    for (int i = 1; i <= EntropanAudioProcessor::kNumBands; ++i)
    {
        ids.add ("b" + juce::String (i) + "_on");
        ids.add ("b" + juce::String (i) + "_uni");
        ids.add ("b" + juce::String (i) + "_freeze");
        ids.add ("b" + juce::String (i) + "_override");
    }
    return ids;   // 2 + 6×4 = 26
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
            })
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
        .withNativeFunction ("setLocks",
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 1)
                    audioProcessor.setLocksJson (args[0].toString());
                complete (true);
            })
        .withNativeFunction ("getLocks",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                complete (audioProcessor.getLocksJson());
            })
        .withNativeFunction ("setPresets",
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 1)
                    audioProcessor.setStepPresetsJson (args[0].toString());
                complete (true);
            })
        .withNativeFunction ("getPresets",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                complete (audioProcessor.getStepPresetsJson());
            })
        .withNativeFunction ("setRoutes",
            [this] (const juce::Array<juce::var>& args, auto complete)
            {
                if (args.size() >= 1)
                    audioProcessor.setRoutesJson (args[0].toString());
                complete (true);
            })
        .withNativeFunction ("getRoutes",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                complete (audioProcessor.getRoutesJson());
            });

    // ── plugin presets (Chaosverb pattern: PropertiesFile stores) ──
    // Two separate stores: "Entropan.presets" holds named snapshots under
    // preset_<name> keys; "Entropan.defaults" holds the values a fresh instance
    // starts from. Unlike Chaosverb, a preset also captures the non-APVTS state
    // (per-band step patterns + mod routes) — params alone would silently drop
    // half the sound.
    {
        auto presetProps = [] (const char* suffix)
        {
            juce::PropertiesFile::Options opts;
            opts.applicationName     = "Entropan";
            opts.filenameSuffix      = suffix;
            opts.osxLibrarySubFolder = "Application Support";
            return opts;
        };

        auto snapshotToVar = [this]() -> juce::var
        {
            auto* o = new juce::DynamicObject();
            juce::StringArray pairs;
            for (auto* param : audioProcessor.apvts.processor.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    pairs.add (rp->getParameterID() + "=" + juce::String (rp->getValue(), 6));
            o->setProperty ("params", pairs.joinIntoString ("|"));
            juce::Array<juce::var> steps;
            for (int i = 0; i < EntropanAudioProcessor::kNumBands; ++i)
                steps.add (audioProcessor.getStepsJson (i));
            o->setProperty ("steps", steps);
            o->setProperty ("routes", audioProcessor.getRoutesJson());
            return juce::var (o);
        };

        auto applySnapshot = [this] (const juce::var& v)
        {
            juce::StringArray pairs;
            pairs.addTokens (v.getProperty ("params", "").toString(), "|", "");
            for (const auto& pair : pairs)
            {
                const auto eq = pair.indexOfChar ('=');
                if (eq < 0) continue;
                if (auto* p = audioProcessor.apvts.getParameter (pair.substring (0, eq)))
                    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f,
                        pair.substring (eq + 1).getFloatValue()));
            }
            if (auto* steps = v.getProperty ("steps", juce::var()).getArray())
                for (int i = 0; i < juce::jmin (steps->size(), (int) EntropanAudioProcessor::kNumBands); ++i)
                    audioProcessor.setStepsJson (i, (*steps)[i].toString());
            const auto routes = v.getProperty ("routes", "").toString();
            if (routes.isNotEmpty())
                audioProcessor.setRoutesJson (routes);
        };

        options = options
            .withNativeFunction ("listPluginPresets",
                [presetProps] (const juce::Array<juce::var>&, auto complete)
                {
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".presets"));
                    const auto& all = props.getUserSettings()->getAllProperties();
                    juce::Array<juce::var> names;
                    for (const auto& key : all.getAllKeys())
                        if (key.startsWith ("preset_"))
                            names.add (key.substring (7));
                    complete (juce::var (names));
                })
            .withNativeFunction ("savePluginPreset",
                [presetProps, snapshotToVar] (const juce::Array<juce::var>& args, auto complete)
                {
                    const auto name = args.size() >= 1 ? args[0].toString().trim() : juce::String();
                    if (name.isEmpty()) { complete (juce::var (false)); return; }
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".presets"));
                    auto* user = props.getUserSettings();
                    user->setValue ("preset_" + name, juce::JSON::toString (snapshotToVar(), true));
                    user->setNeedsToBeSaved (true);
                    complete (juce::var (user->save()));   // force flush, don't rely on saveIfNeeded
                })
            .withNativeFunction ("loadPluginPreset",
                [this, presetProps, applySnapshot] (const juce::Array<juce::var>& args, auto complete)
                {
                    const auto name = args.size() >= 1 ? args[0].toString().trim() : juce::String();
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".presets"));
                    const auto data = props.getUserSettings()->getValue ("preset_" + name, "");
                    if (data.isEmpty()) { complete (juce::var (false)); return; }
                    applySnapshot (juce::JSON::parse (data));
                    audioProcessor.commitUndoIfChanged();   // preset load is one undo step
                    if (webView != nullptr)
                        webView->emitEventIfBrowserIsVisible ("stateReloaded", juce::var());
                    complete (juce::var (true));
                })
            .withNativeFunction ("deletePluginPreset",
                [presetProps] (const juce::Array<juce::var>& args, auto complete)
                {
                    const auto name = args.size() >= 1 ? args[0].toString().trim() : juce::String();
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".presets"));
                    auto* user = props.getUserSettings();
                    user->removeValue ("preset_" + name);
                    user->setNeedsToBeSaved (true);
                    complete (juce::var (user->save()));
                })
            .withNativeFunction ("saveCurrentAsDefaults",
                [presetProps, snapshotToVar] (const juce::Array<juce::var>&, auto complete)
                {
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".defaults"));
                    auto* user = props.getUserSettings();
                    user->clear();   // defaults store is a single snapshot — replace wholesale
                    user->setValue ("snapshot", juce::JSON::toString (snapshotToVar(), true));
                    user->setNeedsToBeSaved (true);
                    complete (juce::var (user->save()));
                })
            .withNativeFunction ("resetToDefaults",
                [this, presetProps, applySnapshot] (const juce::Array<juce::var>&, auto complete)
                {
                    juce::ApplicationProperties props;
                    props.setStorageParameters (presetProps (".defaults"));
                    const auto data = props.getUserSettings()->getValue ("snapshot", "");
                    if (data.isNotEmpty())
                        applySnapshot (juce::JSON::parse (data));
                    else
                        for (auto* param : audioProcessor.apvts.processor.getParameters())
                            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                                rp->setValueNotifyingHost (rp->getDefaultValue());
                    audioProcessor.commitUndoIfChanged();
                    if (webView != nullptr)
                        webView->emitEventIfBrowserIsVisible ("stateReloaded", juce::var());
                    complete (juce::var (true));
                });
    }

#if ENTROPAN_MOONBASE
    // ── licensing bridge: web settings panel ↔ Moonbase ──
    options = options
        .withNativeFunction ("getLicenseStatus",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                auto* obj = new juce::DynamicObject();
                if (audioProcessor.moonbaseClient == nullptr)
                {
                    obj->setProperty ("state",  "unavailable");
                    obj->setProperty ("active", false);
                    complete (juce::var (obj));
                    return;
                }

                auto& mb = *audioProcessor.moonbaseClient;
                // Go through the obfuscation macro rather than calling
                // isUnlocked() directly — module guidance, makes the check
                // harder to patch out of the shipped binary.
                const auto unlockedPair = MB_IS_UNLOCKED_OBFUSCATED (mb);
                const bool unlocked = (bool) unlockedPair.first;
                const bool trial    = (bool) mb.isTrial();
                const bool offline  = (bool) mb.isOfflineActivated();

                // Surface the client's own error text. Without this the only
                // report is whatever the Activate screen renders, which is too
                // vague to act on ("Could not parse server response").
                juce::String err = unlockedPair.second;
                if (err.isEmpty()) err = mb.getLastError();
                obj->setProperty ("error", err);

                obj->setProperty ("state", ! unlocked ? "unlicensed"
                                         : trial      ? "trial_active"
                                         : offline    ? "offline_activated"
                                                      : "licensed");
                obj->setProperty ("active",  unlocked);
                obj->setProperty ("isTrial", trial);
                obj->setProperty ("email",   mb.getUserId());
                // offline reaches the UI folded into "state"; nothing reads a
                // separate isOffline/userName, so none are sent

                const auto exp = mb.getLicenseExpiration();
                obj->setProperty ("expirationMs", (juce::int64) exp.toMilliseconds());
                complete (juce::var (obj));
            })
        .withNativeFunction ("deactivateLicense",
            [this] (const juce::Array<juce::var>&, auto complete)
            {
                if (audioProcessor.moonbaseClient == nullptr)
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty ("success", false);
                    obj->setProperty ("message", "Licensing unavailable.");
                    complete (juce::var (obj));
                    return;
                }

                audioProcessor.moonbaseClient->deactivateLicense (
                    [this, complete] (bool ok)
                    {
                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("success", ok);
                        obj->setProperty ("message", ok
                            ? juce::String ("License deactivated.")
                            : juce::String ("Deactivation failed — check your internet connection."));
                        complete (juce::var (obj));

                        // Hand the screen back to the Activate overlay. The
                        // timerCallback swap would get there on its own, but
                        // only after the module notices; do it immediately so
                        // the user never sees a live UI they no longer own.
                        if (ok)
                            juce::MessageManager::callAsync ([this]
                            {
                                if (activationUI != nullptr) activationUI->update();
                                if (webView != nullptr)      webView->setVisible (false);
                            });
                    });
            });
#endif

    for (auto& r : sliderRelays)  options = options.withOptionsFrom (*r);
    for (auto& r : comboRelays)   options = options.withOptionsFrom (*r);
    for (auto& r : toggleRelays)  options = options.withOptionsFrom (*r);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

#if ENTROPAN_MOONBASE
    // Moonbase Activate screen: created AFTER the WebView so it is added on top.
    // On first launch (no license) the module shows it automatically; once
    // activated it hides itself. setSize() later triggers resized(), which gives
    // it full-editor bounds.
    MOONBASE_INIT_ACTIVATION_UI (audioProcessor);
    if (activationUI != nullptr)
        activationUI->setWelcomePageText ("Entropan", "Entropia Audio");
#endif

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

    fifoDrain.resize (EntropanAnalyzer::kBigSize);
    fftAccum.resize (EntropanAnalyzer::kBigSize, 0.0f);          // sliding 65536: big FFT reads all, small the tail
    workSmall.resize ((size_t) EntropanAnalyzer::kSmallSize * 2, 0.0f);
    workBig.resize ((size_t) EntropanAnalyzer::kBigSize * 2, 0.0f);
    bigMags.resize ((size_t) EntropanAnalyzer::kBigSize, 0.0f);
    startTimerHz (60);   // smooth analyzer/scope motion
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

#if ENTROPAN_MOONBASE
    // The Activate overlay is a native JUCE panel, but WebBrowserComponent is
    // backed by a native platform view (WKWebView / WebView2) that always paints
    // ABOVE JUCE-drawn components — setAlwaysOnTop on the overlay is ignored.
    // So the two must be mutually exclusive: swap the web UI out whenever the
    // overlay needs the screen, and bring it back when it steps aside.
    // Cheap — setVisible only fires on an actual state change.
    if (activationUI != nullptr)
    {
        const bool overlayVisible = activationUI->getVisibility().isVisible;
        if (webView->isVisible() == overlayVisible)
            webView->setVisible (! overlayVisible);
    }
#endif

    // Everything below builds telemetry for the web UI. When the WebView is
    // hidden (Activate overlay up, or host hid us) JUCE drops the events at
    // emit time — but only AFTER we would have paid for the full 4096-pt FFT
    // and array building, 60×/s for nobody. Skip it. (The overlay swap above
    // must stay ahead of this gate or a licensed UI could never swap back in.)
    if (! webView->isVisible())
        return;

    // ── per-band mod values (post-slew × depth) + last MIDI note ──
    {
        juce::Array<juce::var> pans, phases, srcs;
        for (int i = 0; i < EntropanAudioProcessor::kNumBands; ++i)
        {
            pans.add ((double) audioProcessor.modOutDepth[(size_t) i].load (std::memory_order_relaxed));
            phases.add ((double) audioProcessor.modPhase[(size_t) i].load (std::memory_order_relaxed));
        }
        // srcs: band-major × waveform (6×6 = 36) — routes tap any waveform now
        srcs.ensureStorageAllocated (EntropanAudioProcessor::kNumBands * EntropanAudioProcessor::kNumWaves);
        for (int k = 0; k < EntropanAudioProcessor::kNumBands * EntropanAudioProcessor::kNumWaves; ++k)
            srcs.add ((double) audioProcessor.modSrcVal[(size_t) k].load (std::memory_order_relaxed));

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("pans", pans);
        obj->setProperty ("phases", phases);
        obj->setProperty ("srcs", srcs);   // raw modulator → live mod-matrix rings
        obj->setProperty ("midiNote", audioProcessor.lastMidiNote.load());
        webView->emitEventIfBrowserIsVisible ("modvals", juce::var (obj));
    }

    // ── high-rate scope samples (all bands, ~750 Hz ring) ──
    {
        const auto w = audioProcessor.scopeWrite.load (std::memory_order_acquire);
        int n = (int) (juce::uint32) (w - scopeReadPos);   // wrap-safe unsigned delta
        if (n > 0)
        {
            n = juce::jmin (n, EntropanAudioProcessor::kScopeRingSize / 2);
            const auto startIdx = w - (juce::uint32) n;
            auto readRing = [startIdx, n] (const auto& ring)
            {
                juce::Array<juce::var> vals;
                vals.ensureStorageAllocated (n);
                for (int k = 0; k < n; ++k)
                    vals.add ((double) ring[(size_t) ((startIdx + (juce::uint32) k)
                                                      & (EntropanAudioProcessor::kScopeRingSize - 1))]);
                return vals;
            };
            juce::Array<juce::var> bandsArr;
            for (int i = 0; i < EntropanAudioProcessor::kNumBands; ++i)
                bandsArr.add (juce::var (readRing (audioProcessor.scopeRing[(size_t) i])));
            auto envArr = readRing (audioProcessor.envScopeRing);
            scopeReadPos = w;
            auto* so = new juce::DynamicObject();
            so->setProperty ("bands", bandsArr);
            so->setProperty ("env", envArr);
            webView->emitEventIfBrowserIsVisible ("scopevals", juce::var (so));
        }
        else if (n < 0)   // shouldn't happen with unsigned deltas; resync anyway
        {
            scopeReadPos = w;
        }
    }

    // ── spectrum: drain FIFO into a sliding FFT frame ──
    // If the tap overflowed since last frame, the stream has a gap — flush the
    // fifo's pre-gap backlog and restart accumulation from post-gap audio, so a
    // spliced window (which renders as broadband LF garbage) is never analyzed.
    {
        const int drops = audioProcessor.analyzerDropped.load (std::memory_order_relaxed);
        if (drops != analyzerDropsSeen)
        {
            analyzerDropsSeen = drops;
            while (audioProcessor.popAnalyzer (fifoDrain.data(), (int) fifoDrain.size()) > 0) {}
            accumFill = 0; bigValid = false;
        }
    }
    using namespace EntropanAnalyzer;
    const int got = audioProcessor.popAnalyzer (fifoDrain.data(), (int) fifoDrain.size());
    if (got > 0)
    {
        if (got >= kBigSize)
        {
            std::memcpy (fftAccum.data(), fifoDrain.data() + (got - kBigSize), sizeof (float) * kBigSize);
            accumFill = kBigSize;
        }
        else
        {
            const int keep = kBigSize - got;
            std::memmove (fftAccum.data(), fftAccum.data() + got, sizeof (float) * (size_t) keep);
            std::memcpy (fftAccum.data() + keep, fifoDrain.data(), sizeof (float) * (size_t) got);
            accumFill = juce::jmin (kBigSize, accumFill + got);
        }
    }

    // Only rebuild + emit when samples actually arrived — after audio stops,
    // re-FFT-ing the same window 60×/s produced identical frames forever.
    if (accumFill >= kSmallSize && got > 0)
    {
        // small FFT: the newest 16384 samples (fast half of the display)
        std::memcpy (workSmall.data(), fftAccum.data() + (kBigSize - kSmallSize), sizeof (float) * kSmallSize);
        winSmall.multiplyWithWindowingTable (workSmall.data(), kSmallSize);
        fftSmall.performFrequencyOnlyForwardTransform (workSmall.data());

        // big FFT: every 2nd emit once 65536 contiguous samples exist —
        // the low columns update at ~30 Hz, plenty for slow-moving lows
        if (accumFill >= kBigSize && (bigCounter++ & 1) == 0)
        {
            std::memcpy (workBig.data(), fftAccum.data(), sizeof (float) * kBigSize);
            winBig.multiplyWithWindowingTable (workBig.data(), kBigSize);
            fftBig.performFrequencyOnlyForwardTransform (workBig.data());
            std::memcpy (bigMags.data(), workBig.data(), sizeof (float) * kBigSize);
            bigValid = true;
        }

        const double sr = audioProcessor.getSampleRate() > 0 ? audioProcessor.getSampleRate() : 48000.0;
        if (colGeomSr != sr)
        {
            colGeomSr = sr;
            colGeom.resize ((size_t) kBins);
            buildGeometry (sr, colGeom.data());
        }
        float cols[kBins];
        computeColumns (colGeom.data(), workSmall.data(), bigValid ? bigMags.data() : nullptr, cols);

        juce::Array<juce::var> mags;
        mags.ensureStorageAllocated (kBins);
        for (int b = 0; b < kBins; ++b)
            mags.add ((double) cols[b]);
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

#if ENTROPAN_MOONBASE
    // Keep the Activate overlay covering the whole editor at any size. Skipping
    // this is the #1 documented cause of "the activation screen never appears".
    MOONBASE_RESIZE_ACTIVATION_UI;
#endif

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
    auto clean = url.upToFirstOccurrenceOf ("?", false, false)
                    .upToFirstOccurrenceOf ("#", false, false);
    const auto requested = clean == "/" || clean.isEmpty()
                             ? juce::String ("index.html")
                             : clean.fromLastOccurrenceOf ("/", false, false);

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
