#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "PluginHostManager.h"

class PluginSlot
{
public:
    PluginSlot() = default;

    ~PluginSlot()
    {
        auto* plugin = pluginInstance.exchange (nullptr, std::memory_order_release);
        if (plugin != nullptr)
        {
            plugin->releaseResources();
            delete plugin;
        }
    }

    void prepare (double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        smoothedDryWet.reset (sampleRate, 0.02);
        slotDryBuffer.setSize (2, blockSize);

        auto* plugin = pluginInstance.load (std::memory_order_acquire);
        if (plugin != nullptr)
        {
            plugin->setRateAndBufferSizeDetails (sampleRate, blockSize);
            plugin->prepareToPlay (sampleRate, blockSize);
        }
    }

    void releaseResources()
    {
        auto* plugin = pluginInstance.load (std::memory_order_acquire);
        if (plugin != nullptr)
            plugin->releaseResources();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        auto* plugin = pluginInstance.load (std::memory_order_acquire);
        if (plugin == nullptr)
            return;

        // Check bypass
        if (bypassParam != nullptr && bypassParam->load() > 0.5f)
            return;

        // Update smoothed dry/wet from APVTS
        if (dryWetParam != nullptr)
            smoothedDryWet.setTargetValue (dryWetParam->load() / 100.0f);

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // Capture dry signal for dry/wet blend
        for (int ch = 0; ch < numChannels; ++ch)
            slotDryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // Process through hosted plugin
        plugin->processBlock (buffer, midi);

        // Apply dry/wet if not fully wet
        if (smoothedDryWet.isSmoothing() || smoothedDryWet.getTargetValue() < 0.999f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* wet = buffer.getWritePointer (ch);
                const auto* dry = slotDryBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float w = smoothedDryWet.getNextValue();
                    wet[i] = dry[i] * (1.0f - w) + wet[i] * w;
                }
            }
        }
    }

    // ===== Plugin Lifecycle (message thread only) =====

    void loadPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin)
    {
        jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

        if (newPlugin != nullptr)
        {
            // ===== Bus configuration (critical for crash-free hosting) =====
            // Disable EVERY non-main bus (side-chain, aux outs). With them enabled,
            // plugin expects multi-bus buffer in processBlock; we only pass stereo →
            // null deref inside plugin. Comet, Pro-Q3, Saturn etc. crash on this.
            const int numInBuses  = newPlugin->getBusCount (true);
            const int numOutBuses = newPlugin->getBusCount (false);

            for (int i = 0; i < numInBuses; ++i)
            {
                if (auto* bus = newPlugin->getBus (true, i))
                    bus->enable (i == 0);   // only main input
            }
            for (int i = 0; i < numOutBuses; ++i)
            {
                if (auto* bus = newPlugin->getBus (false, i))
                    bus->enable (i == 0);   // only main output
            }

            // Force main buses to stereo where possible
            if (numInBuses > 0)
                if (auto* mainIn = newPlugin->getBus (true, 0))
                    mainIn->setCurrentLayout (juce::AudioChannelSet::stereo());
            if (numOutBuses > 0)
                if (auto* mainOut = newPlugin->getBus (false, 0))
                    mainOut->setCurrentLayout (juce::AudioChannelSet::stereo());

            // Channel counts must agree with bus state before prepareToPlay
            newPlugin->setPlayConfigDetails (newPlugin->getTotalNumInputChannels(),
                                              newPlugin->getTotalNumOutputChannels(),
                                              currentSampleRate, currentBlockSize);
            newPlugin->setNonRealtime (false);
            newPlugin->prepareToPlay (currentSampleRate, currentBlockSize);
            loadedDescription = newPlugin->getPluginDescription();
        }
        else
        {
            loadedDescription = {};
        }

        // Atomic swap — audio thread will see the new plugin on next block
        auto* oldRaw = pluginInstance.exchange (newPlugin.release(), std::memory_order_release);

        // Defer deletion via timer — callAsync runs on NEXT message-loop tick,
        // which is too soon (audio thread may still be mid-processBlock with
        // the old pointer it loaded just before our exchange). 2 s ≫ any plausible
        // audio block latency, so old plugin guaranteed unused by then.
        if (oldRaw != nullptr)
        {
            std::shared_ptr<juce::AudioPluginInstance> old (oldRaw);
            juce::Timer::callAfterDelay (2000, [old]()
            {
                old->releaseResources();
            });
        }
    }

    // ===== Plugin shuffle support (atomic ownership transfer) =====
    // detachPlugin: returns raw pointer + clears slot atomically. No delete.
    // detachDescription: returns desc + clears it.
    // attachPlugin: stores raw ptr + desc atomically. No prepareToPlay (already set up).
    // Call from MESSAGE THREAD only.
    juce::AudioPluginInstance* detachPlugin()
    {
        return pluginInstance.exchange (nullptr, std::memory_order_release);
    }
    juce::PluginDescription detachDescription()
    {
        auto d = loadedDescription;
        loadedDescription = {};
        return d;
    }
    void attachPlugin (juce::AudioPluginInstance* p, const juce::PluginDescription& d)
    {
        jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
        loadedDescription = d;
        // Old (should be nullptr after detach) discarded — caller's responsibility
        pluginInstance.store (p, std::memory_order_release);
    }

    void unloadPlugin()
    {
        loadPlugin (nullptr);
    }

    bool isLoaded() const
    {
        return pluginInstance.load (std::memory_order_acquire) != nullptr;
    }

    juce::String getPluginName() const
    {
        return loadedDescription.name;
    }

    const juce::PluginDescription& getDescription() const
    {
        return loadedDescription;
    }

    juce::AudioPluginInstance* getPluginInstance() const
    {
        return pluginInstance.load (std::memory_order_acquire);
    }

    // Reported latency in samples. Returns 0 if bypassed or no plugin loaded.
    int getLatencySamples() const
    {
        if (bypassParam != nullptr && bypassParam->load() > 0.5f)
            return 0;
        auto* plugin = pluginInstance.load (std::memory_order_acquire);
        return plugin != nullptr ? plugin->getLatencySamples() : 0;
    }

    // ===== State Persistence =====

    void saveState (juce::XmlElement& parent, int bandIndex, int slotIndex)
    {
        auto* plugin = pluginInstance.load (std::memory_order_acquire);
        if (plugin == nullptr)
            return;

        auto slotXml = std::make_unique<juce::XmlElement> ("Slot");
        slotXml->setAttribute ("band", bandIndex);
        slotXml->setAttribute ("slot", slotIndex);

        // Save plugin description
        auto descXml = loadedDescription.createXml();
        if (descXml != nullptr)
            slotXml->addChildElement (descXml.release());

        // Save plugin state as base64
        juce::MemoryBlock stateData;
        plugin->getStateInformation (stateData);
        slotXml->setAttribute ("state", stateData.toBase64Encoding());

        parent.addChildElement (slotXml.release());
    }

    void restoreState (const juce::XmlElement& parent, int bandIndex, int slotIndex,
                       PluginHostManager& hostManager)
    {
        for (auto* slotXml : parent.getChildIterator())
        {
            if (slotXml->getIntAttribute ("band") != bandIndex ||
                slotXml->getIntAttribute ("slot") != slotIndex)
                continue;

            // Restore plugin description
            auto* descXml = slotXml->getChildByName ("PLUGIN");
            if (descXml == nullptr)
                continue;

            juce::PluginDescription desc;
            desc.loadFromXml (*descXml);

            auto stateBase64 = slotXml->getStringAttribute ("state");
            juce::MemoryBlock stateData;
            stateData.fromBase64Encoding (stateBase64);

            // Load plugin async and restore state
            hostManager.createPluginAsync (desc, currentSampleRate, currentBlockSize,
                [this, stateData] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                   const juce::String& err)
                {
                    if (instance != nullptr)
                    {
                        instance->setStateInformation (stateData.getData(), (int) stateData.getSize());
                        loadPlugin (std::move (instance));
                    }
                    else
                    {
                        DBG ("Failed to restore plugin: " + err);
                    }
                });

            break;
        }
    }

    // APVTS raw parameter pointers (set by processor constructor)
    std::atomic<float>* dryWetParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

private:
    std::atomic<juce::AudioPluginInstance*> pluginInstance { nullptr };
    juce::SmoothedValue<float> smoothedDryWet { 1.0f };
    juce::AudioBuffer<float> slotDryBuffer;
    juce::PluginDescription loadedDescription;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginSlot)
};
