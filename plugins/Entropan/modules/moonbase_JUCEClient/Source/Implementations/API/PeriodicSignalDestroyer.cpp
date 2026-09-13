#include "PeriodicSignalDestroyer.h"


//---------------------------------------------------------------------
PeriodicSignalDestroyer::PeriodicSignalDestroyer (APIImpl& api_)
:
Thread ("audio-worker-" + juce::Uuid ().toString ()),
api (api_)
{
    startThread (juce::Thread::Priority::low);
}

PeriodicSignalDestroyer::~PeriodicSignalDestroyer ()
{
    signalThreadShouldExit ();
    notify();
    stopThread (currentNoisePeriod.get () * 2000);
}

void PeriodicSignalDestroyer::setNoiseMode (bool state)
{
    isNoiseMode.set (state);
}

void PeriodicSignalDestroyer::setNoiseParameters (int period, int duration)
{
    currentNoisePeriod = period;
    currentNoiseDuration = duration;

    numSamplesUntilNoise.set ((juce::int64)currentSr * period);
    numSamplesNoise.set ((juce::int64)currentSr * duration);
}

void PeriodicSignalDestroyer::prepareToPlay (double sr, int /*bufSize*/) noexcept
{
    currentSr = sr;
    setNoiseParameters (currentNoisePeriod.get (), currentNoiseDuration);
}

void PeriodicSignalDestroyer::run ()
{
    while (! threadShouldExit())
    {
        isUnlocked.set (MB_IS_UNLOCKED_OBFUSCATED(api).first);

        // Once shutdown has been signalled, do not sleep or start another
        // validation pass. This keeps the join inside stopThread() bounded to
        // at most one in-flight pass so JUCE never has to fall back to
        // TerminateThread() (which would corrupt the heap if the thread were
        // killed while holding the CRT heap lock).
        if (threadShouldExit())
            break;

        // wait (1000);//checking every second for testing
        wait (currentNoisePeriod.get () * 1000);
    }
}

void PeriodicSignalDestroyer::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (isUnlocked.get ()) return;

    const auto numCh { buffer.getNumChannels () };
    const auto numSamps { buffer.getNumSamples () };

    sampleCounter += numSamps;
    if (sampleCounter >= numSamplesUntilNoise.get ())
    {
        const auto playNoise { isNoiseMode.get() };

        if (playNoise)
        {
            for (int ch { 0 }; ch < numCh; ch++)
            {
                auto writePointer { buffer.getWritePointer (ch) };
                for (int s { 0 }; s < numSamps; s++)
                    writePointer[s] += juce::jmap (rand.nextFloat (), 0.f, 1.f, -0.1f, 0.1f);
            }
        }
        else
        {
            for (int ch { 0 }; ch < numCh; ch++)
            {
                auto writePointer { buffer.getWritePointer (ch) };
                for (int s { 0 }; s < numSamps; s++)
                    writePointer[s] = 0.0f;
            }
        }

        noiseCounter += numSamps;
        if (noiseCounter >= numSamplesNoise.get ())
        {
            noiseCounter = 0;
            sampleCounter = 0;
        }
    }

}
