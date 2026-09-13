#pragma once
#include "Includes.h"

namespace Moonbase
{
namespace JUCEClient
{
    struct APIImpl;
    class PeriodicSignalDestroyer : private juce::Thread
    {
    public:
        PeriodicSignalDestroyer (APIImpl& api);
        ~PeriodicSignalDestroyer () override;

        void prepareToPlay (double sr, int bufSize) noexcept;
        void process (juce::AudioBuffer<float>& buffer) noexcept;
        void setNoiseParameters (int period, int duration);

        void setNoiseMode (bool state);

    private:
        APIImpl& api;
        juce::Atomic<bool> isNoiseMode { false };

        double currentSr { 44100 };
        juce::Random rand;

        juce::int64 sampleCounter { 0 };
        juce::Atomic<juce::int64> numSamplesUntilNoise { 100000 };

        juce::int64 noiseCounter { 0 };
        juce::Atomic<juce::int64> numSamplesNoise { 100000 };

        juce::Atomic<int> currentNoisePeriod { 10 }; //s
        int currentNoiseDuration { 3 }; //s

        void run () override;
        juce::Atomic<bool> isUnlocked = true;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeriodicSignalDestroyer)
    };
} // namespace JUCEClient
} // namespace Moonbase
