#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

//==============================================================================
/**
 * SNIP Bridge - Real-Time Mix Analyzer & Web Bridge
 *
 * Pure pass-through analyzer. Audio thread copies input to output unchanged.
 * Analysis runs in parallel: LUFS, Spectral, Stereo measurements.
 * Results sent to WebView UI at 30Hz via atomic data bridge.
 */
class SNIPBridgeAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SNIPBridgeAudioProcessor();
    ~SNIPBridgeAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter Value Tree State
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    // ANALYSIS DATA BRIDGE (atomic — audio thread writes, timer thread reads)
    //==============================================================================

    // Dynamics
    std::atomic<float> lufsShort { -60.0f };
    std::atomic<float> lufsIntegrated { -60.0f };
    std::atomic<float> rmsLevel { -60.0f };
    std::atomic<float> truePeak { -144.0f };  // dBTP true peak

    // Spectral (6 bands: Sub, Low, LMid, Mid, HMid, High)
    std::array<std::atomic<float>, 6> spectralBands {};

    // Stereo
    std::atomic<float> stereoCorrelation { 1.0f };
    std::atomic<float> stereoWidth { 0.0f };
    std::atomic<float> stereoBalance { 0.0f };  // -1.0 full L, 0.0 center, +1.0 full R

    //==============================================================================
    // Called from timer thread to compute 300-bin log-spaced spectrum
    // Returns true if new spectrum data was available
    bool computeSpectrum (float* output, int numBins);

    static constexpr int kSpectrumBins = 300;

    //==============================================================================
    // GENRE PROFILES & SCORING
    //==============================================================================
    struct GenreProfile
    {
        float lufsLo, lufsHi;
        float rmsLo, rmsHi;
        float widthLo, widthHi;
        float corrMin;

        // 6-band spectral targets (from snip_reference_profiles.json)
        // Bands: Sub(<80Hz), Low(80-250), LMid(250-1k), Mid(1-4k), HMid(4-8k), High(>8k)
        float bandMeans[6];
        float bandStdDevs[6];
    };

    struct FeedbackScores
    {
        float dynamics    = 0.0f;
        float tonality    = 0.0f;
        float width       = 0.0f;
        float correlation = 0.0f;

        // Direction: -1 = below target, 0 = on target, +1 = above target
        int dynamicsDir    = 0;   // -1 too quiet, +1 too loud
        int tonalityDir    = 0;   // -1 too dark/bass-heavy, +1 too bright/thin
        int widthDir       = 0;   // -1 too narrow, +1 too wide
        int correlationDir = 0;   // -1 phase issues

        int tonalityWorstBand = -1;  // 0-5: which band deviates most
    };

    static constexpr int kNumGenres = 12;
    static const GenreProfile genreProfiles[kNumGenres];

    // Compute feedback scores from current analysis values (timer thread)
    FeedbackScores computeFeedback (int genreIndex, float currentLufs, float currentRms,
                                    float currentWidth, float currentCorr,
                                    const float* spectrum, int numBins) const;

    // Build JSON snapshot of all analysis data for API submission
    juce::String buildAnalysisSnapshot (int genreIndex, const FeedbackScores& fb,
                                        const float* spectrum, int numBins) const;

    // Reset integrated LUFS measurement
    void resetIntegratedLufs();

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;

    //==============================================================================
    // K-WEIGHTING BIQUAD FILTER (ITU-R BS.1770-4)
    //==============================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1L = 0, z2L = 0;  // Left channel state
        double z1R = 0, z2R = 0;  // Right channel state

        float processL (float in)
        {
            double out = b0 * in + z1L;
            z1L = b1 * in - a1 * out + z2L;
            z2L = b2 * in - a2 * out;
            return (float)out;
        }

        float processR (float in)
        {
            double out = b0 * in + z1R;
            z1R = b1 * in - a1 * out + z2R;
            z2R = b2 * in - a2 * out;
            return (float)out;
        }

        void reset()
        {
            z1L = z2L = z1R = z2R = 0;
        }
    };

    void computeKWeightingCoeffs (double sampleRate);

    Biquad kPreFilter;   // Stage 1: High-shelf (+4 dB at HF)
    Biquad kRLBFilter;   // Stage 2: High-pass (38 Hz)

    //==============================================================================
    // LUFS ACCUMULATOR
    //==============================================================================
    double kWeightedSumL = 0;
    double kWeightedSumR = 0;
    int blockSampleCount = 0;
    int samplesPerLufsBlock = 4800;  // 100ms at 48kHz

    static constexpr int kShortTermBlocks = 30;  // 30 * 100ms = 3 seconds
    std::array<double, kShortTermBlocks> blockEnergies {};
    int blockWriteIdx = 0;
    int blocksWritten = 0;

    double integratedEnergySum = 0;
    int integratedBlockCount = 0;

    void finalizeLufsBlock();

    //==============================================================================
    // RMS ACCUMULATOR
    //==============================================================================
    double rawSquareSum = 0;
    int rmsSampleCount = 0;

    //==============================================================================
    // STEREO ACCUMULATOR (short-term: 100ms blocks for responsive display)
    //==============================================================================
    double corrSumLR = 0;
    double corrSumL2 = 0;
    double corrSumR2 = 0;
    int corrSampleCount = 0;

    // Long-term stereo accumulator (never resets — matches reference analyzer)
    double ltCorrSumLR = 0;
    double ltCorrSumL2 = 0;
    double ltCorrSumR2 = 0;

    //==============================================================================
    // FFT ENGINE
    //==============================================================================
    static constexpr int kFFTOrder = 12;
    static constexpr int kFFTSize = 1 << kFFTOrder;  // 4096

    juce::dsp::FFT fft { kFFTOrder };
    std::array<float, kFFTSize> fftWindow {};

    // Ring buffer: audio thread writes mono (L+R)/2, timer thread reads
    static constexpr int kRingSize = kFFTSize * 4;  // 16384
    std::array<float, kRingSize> ringBuffer {};
    std::atomic<int> ringWritePos { 0 };

    // Reusable FFT work buffer (avoids 32KB stack alloc per computeSpectrum call)
    std::array<float, kFFTSize * 2> fftWorkBuffer {};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SNIPBridgeAudioProcessor)
};
