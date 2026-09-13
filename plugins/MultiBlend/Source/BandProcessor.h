#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginSlot.h"
#include "MSProcessor.h"
#include <array>
#include <atomic>

//==============================================================================
// Processes a single frequency band: gain staging, 5 serial plugin slots,
// dry/wet blend. One BandProcessor per band (low, mid, high).
//==============================================================================
class BandProcessor
{
public:
    BandProcessor() = default;

    static constexpr int maxAlignmentDelaySamples = 8192;   // ~170 ms @ 48k

    void prepare (double sampleRate, int blockSize)
    {
        inputGain.reset (sampleRate, 0.02);
        outputGain.reset (sampleRate, 0.02);
        bandDryWet.reset (sampleRate, 0.02);
        bandDryBuffer.setSize (2, blockSize);
        msProcessor.prepare (blockSize);

        // Alignment delay line — pre-allocate max so audio-thread set never reallocates
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = 2;
        alignDelay.prepare (spec);
        alignDelay.setMaximumDelayInSamples (maxAlignmentDelaySamples);
        alignDelay.setDelay ((float) currentDelaySamples.load());

        for (auto& slot : slots)
            slot.prepare (sampleRate, blockSize);
    }

    void releaseResources()
    {
        for (auto& slot : slots)
            slot.releaseResources();
    }

    //==========================================================================
    // Process the band buffer in-place: input gain → slots → output gain → dry/wet
    //==========================================================================
    void processBlock (juce::AudioBuffer<float>& bandBuffer, juce::MidiBuffer& midi)
    {
        const int numSamples = bandBuffer.getNumSamples();
        const int numChannels = bandBuffer.getNumChannels();

        // Bypass = skip gain / M-S / plugin chain / dry-wet, but STILL apply the
        // alignment delay below so this clean band stays phase-aligned with
        // latency-compensated active bands at the crossover.
        const bool bypassed = (bypassParam != nullptr && bypassParam->load() > 0.5f);
        if (! bypassed)
        {

        // Capture dry signal before any processing
        for (int ch = 0; ch < numChannels; ++ch)
            bandDryBuffer.copyFrom (ch, 0, bandBuffer, ch, 0, numSamples);

        // Apply input gain
        if (inputGainParam != nullptr)
        {
            float gainDb = inputGainParam->load();
            inputGain.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = bandBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] *= inputGain.getNextValue();
        }

        // M/S encode before plugin chain (based on morph param)
        float morph = (msMorphParam != nullptr) ? msMorphParam->load() : 0.5f;
        msProcessor.encodeForMorph (bandBuffer, morph);

        // Process through 5 serial plugin slots
        for (auto& slot : slots)
            slot.processBlock (bandBuffer, midi);

        // M/S decode after plugin chain
        msProcessor.decodeFromMorph (bandBuffer);

        // Apply output gain
        if (outputGainParam != nullptr)
        {
            float gainDb = outputGainParam->load();
            outputGain.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = bandBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] *= outputGain.getNextValue();
        }

        // Apply band dry/wet
        if (dryWetParam != nullptr)
        {
            float dwPct = dryWetParam->load();
            bandDryWet.setTargetValue (dwPct / 100.0f);
        }

        if (bandDryWet.isSmoothing() || bandDryWet.getTargetValue() < 0.999f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* wet = bandBuffer.getWritePointer (ch);
                const auto* dry = bandDryBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float w = bandDryWet.getNextValue();
                    wet[i] = dry[i] * (1.0f - w) + wet[i] * w;
                }
            }
        }

        }   // end if (! bypassed)

        // ===== Alignment delay (parallel-band latency compensation) =====
        // Bands with less plugin latency are delayed so all bands re-emerge in phase.
        // Pulled from atomic so message-thread updateLatency() can change safely.
        int targetDelay = currentDelaySamples.load (std::memory_order_relaxed);
        if (targetDelay != lastAppliedDelay)
        {
            // Clamp to allocated capacity
            targetDelay = juce::jlimit (0, maxAlignmentDelaySamples, targetDelay);
            alignDelay.setDelay ((float) targetDelay);
            lastAppliedDelay = targetDelay;
        }

        if (lastAppliedDelay > 0)
        {
            juce::dsp::AudioBlock<float> block (bandBuffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            alignDelay.process (ctx);
        }
    }

    // ===== Latency helpers (call from message thread) =====

    // Sum non-bypassed slot latencies. Safe to call from message thread.
    int computeLatencySamples() const
    {
        if (bypassParam != nullptr && bypassParam->load() > 0.5f)
            return 0;
        int total = 0;
        for (auto& slot : slots)
            total += slot.getLatencySamples();
        return total;
    }

    // Band "active" = not bypassed AND has at least one loaded plugin.
    // Drives Dynamic-Phase mode (only active bands get crossover-filtered).
    // Audio-thread safe: isLoaded() reads an atomic pointer.
    bool hasActivePlugin() const
    {
        if (bypassParam != nullptr && bypassParam->load() > 0.5f)
            return false;
        for (auto& slot : slots)
            if (slot.isLoaded())
                return true;
        return false;
    }

    // Set alignment delay (in samples). Audio thread picks up on next block.
    void setAlignmentDelay (int samples)
    {
        currentDelaySamples.store (juce::jlimit (0, maxAlignmentDelaySamples, samples),
                                   std::memory_order_relaxed);
    }

    void resetAlignmentDelay()
    {
        alignDelay.reset();
        lastAppliedDelay = 0;
    }

    // APVTS raw parameter pointers (set by processor constructor)
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;
    std::atomic<float>* soloParam = nullptr;
    std::atomic<float>* muteParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* msMorphParam = nullptr;

    // 5 serial plugin slots
    std::array<PluginSlot, 5> slots;

private:
    juce::SmoothedValue<float> inputGain { 1.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    juce::SmoothedValue<float> bandDryWet { 1.0f };

    juce::AudioBuffer<float> bandDryBuffer;
    MSProcessor msProcessor;

    // Linear interpolation delay line — fine for integer-sample alignment delays
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> alignDelay;
    std::atomic<int> currentDelaySamples { 0 };
    int lastAppliedDelay = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandProcessor)
};
