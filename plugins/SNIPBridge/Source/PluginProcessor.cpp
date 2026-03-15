#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SNIPBridgeAudioProcessor::SNIPBridgeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Initialize spectral bands to zero
    for (auto& band : spectralBands)
        band.store (0.0f);
}

SNIPBridgeAudioProcessor::~SNIPBridgeAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SNIPBridgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Target Genre: Choice 0-4
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "target_genre", 1 },
        "Target Genre",
        juce::StringArray { "Hip-Hop / Trap", "Pop", "EDM / Dance",
                            "Rock", "Lo-Fi" },
        0));

    // Analysis Window (Reaction Time): 0.5 - 10.0 seconds
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "analysis_window", 1 },
        "Reaction Time",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f),
        3.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String SNIPBridgeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SNIPBridgeAudioProcessor::acceptsMidi() const  { return false; }
bool SNIPBridgeAudioProcessor::producesMidi() const { return false; }
bool SNIPBridgeAudioProcessor::isMidiEffect() const { return false; }
double SNIPBridgeAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SNIPBridgeAudioProcessor::getNumPrograms()    { return 1; }
int SNIPBridgeAudioProcessor::getCurrentProgram() { return 0; }
void SNIPBridgeAudioProcessor::setCurrentProgram (int) {}
const juce::String SNIPBridgeAudioProcessor::getProgramName (int) { return {}; }
void SNIPBridgeAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void SNIPBridgeAudioProcessor::computeKWeightingCoeffs (double sampleRate)
{
    // ITU-R BS.1770-4 K-weighting filter
    // Stage 1: Pre-filter (high-shelf boosting HF by ~4 dB)
    {
        double db  =  4.0;
        double f0  =  1681.974450955533;
        double Q   =  0.7071752369554196;
        double K   =  std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
        double Vh  =  std::pow (10.0, db / 20.0);
        double Vb  =  std::pow (Vh, 0.4996667741545416);

        double a0  =  1.0 + K / Q + K * K;
        kPreFilter.b0 = (Vh + Vb * K / Q + K * K) / a0;
        kPreFilter.b1 = 2.0 * (K * K - Vh) / a0;
        kPreFilter.b2 = (Vh - Vb * K / Q + K * K) / a0;
        kPreFilter.a1 = 2.0 * (K * K - 1.0) / a0;
        kPreFilter.a2 = (1.0 - K / Q + K * K) / a0;
    }

    // Stage 2: Revised Low-frequency B-curve (high-pass at ~38 Hz)
    {
        double f0  =  38.13547087602444;
        double Q   =  0.5003270373238773;
        double K   =  std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);

        double a0  =  1.0 + K / Q + K * K;
        kRLBFilter.b0 =  1.0 / a0;
        kRLBFilter.b1 = -2.0 / a0;
        kRLBFilter.b2 =  1.0 / a0;
        kRLBFilter.a1 =  2.0 * (K * K - 1.0) / a0;
        kRLBFilter.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

//==============================================================================
void SNIPBridgeAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // K-weighting filters
    computeKWeightingCoeffs (sampleRate);
    kPreFilter.reset();
    kRLBFilter.reset();

    // LUFS accumulators
    samplesPerLufsBlock = (int)(sampleRate * 0.1);  // 100ms blocks
    kWeightedSumL = 0;
    kWeightedSumR = 0;
    blockSampleCount = 0;
    blockWriteIdx = 0;
    blocksWritten = 0;
    blockEnergies.fill (0.0);
    integratedEnergySum = 0;
    integratedBlockCount = 0;

    // RMS accumulator
    rawSquareSum = 0;
    rmsSampleCount = 0;

    // Stereo accumulator
    corrSumLR = 0;
    corrSumL2 = 0;
    corrSumR2 = 0;
    corrSampleCount = 0;

    // FFT window (Hann)
    for (int i = 0; i < kFFTSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (float)kFFTSize));

    // Ring buffer
    ringBuffer.fill (0.0f);
    ringWritePos.store (0);
    ringReadPos = 0;

    // Reset atomic outputs
    lufsShort.store (-60.0f);
    lufsIntegrated.store (-60.0f);
    rmsLevel.store (-60.0f);
    stereoCorrelation.store (1.0f);
    stereoWidth.store (0.0f);

    for (auto& band : spectralBands)
        band.store (0.0f);
}

void SNIPBridgeAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SNIPBridgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Support stereo only
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}
#endif

//==============================================================================
void SNIPBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // PURE PASS-THROUGH: Input is already in the buffer, audio passes unchanged.
    // Analysis runs in parallel — we read samples but never modify them.

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || totalNumInputChannels < 2)
        return;

    const float* dataL = buffer.getReadPointer (0);
    const float* dataR = buffer.getReadPointer (1);

    int ringPos = ringWritePos.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        float sL = dataL[i];
        float sR = dataR[i];

        //--------------------------------------------------------------
        // 1. K-WEIGHTING for LUFS
        //--------------------------------------------------------------
        float kL = kRLBFilter.processL (kPreFilter.processL (sL));
        float kR = kRLBFilter.processR (kPreFilter.processR (sR));

        kWeightedSumL += (double)(kL * kL);
        kWeightedSumR += (double)(kR * kR);

        //--------------------------------------------------------------
        // 2. RAW RMS
        //--------------------------------------------------------------
        rawSquareSum += (double)(sL * sL + sR * sR) * 0.5;

        //--------------------------------------------------------------
        // 3. STEREO CORRELATION & WIDTH
        //--------------------------------------------------------------
        corrSumLR += (double)(sL * sR);
        corrSumL2 += (double)(sL * sL);
        corrSumR2 += (double)(sR * sR);

        //--------------------------------------------------------------
        // 4. FFT RING BUFFER (mono mix)
        //--------------------------------------------------------------
        ringBuffer[ringPos] = (sL + sR) * 0.5f;
        ringPos = (ringPos + 1) & (kRingSize - 1);

        //--------------------------------------------------------------
        // 5. BLOCK BOUNDARY CHECK
        //--------------------------------------------------------------
        ++blockSampleCount;
        ++rmsSampleCount;
        ++corrSampleCount;

        if (blockSampleCount >= samplesPerLufsBlock)
        {
            finalizeLufsBlock();

            // RMS update (aligned with LUFS blocks = 100ms)
            if (rmsSampleCount > 0)
            {
                double rmsDb = 10.0 * std::log10 (rawSquareSum / (double)rmsSampleCount + 1e-30);
                rmsLevel.store ((float)juce::jlimit (-60.0, 0.0, rmsDb));
                rawSquareSum = 0;
                rmsSampleCount = 0;
            }

            // Stereo update
            if (corrSampleCount > 0)
            {
                double denom = std::sqrt (corrSumL2 * corrSumR2);
                float corr = (denom > 1e-12) ? (float)(corrSumLR / denom) : 1.0f;
                stereoCorrelation.store (juce::jlimit (-1.0f, 1.0f, corr));

                // Width: 0 = mono, 1 = full stereo
                // Using mid/side ratio approach
                double mid2  = corrSumL2 + corrSumR2 + 2.0 * corrSumLR;
                double side2 = corrSumL2 + corrSumR2 - 2.0 * corrSumLR;
                float w = (mid2 > 1e-12) ? (float)(side2 / (mid2 + side2)) : 0.0f;
                stereoWidth.store (juce::jlimit (0.0f, 1.0f, w));

                corrSumLR = 0;
                corrSumL2 = 0;
                corrSumR2 = 0;
                corrSampleCount = 0;
            }
        }
    }

    ringWritePos.store (ringPos, std::memory_order_release);

    //------------------------------------------------------------------
    // 6. SILENCE DETECTION
    //------------------------------------------------------------------
    // Check if the entire buffer was silent (all samples below threshold)
    float peakLevel = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float absL = std::abs (dataL[i]);
        float absR = std::abs (dataR[i]);
        peakLevel = std::max (peakLevel, std::max (absL, absR));
    }
    isSilent.store (peakLevel < 1e-6f);  // ~-120 dBFS threshold
}

//==============================================================================
// LUFS BLOCK FINALIZATION
//==============================================================================
void SNIPBridgeAudioProcessor::finalizeLufsBlock()
{
    // Mean square for this 100ms block (ITU-R BS.1770-4 uses channel sum, not average)
    double blockEnergy = (kWeightedSumL + kWeightedSumR) / (double)blockSampleCount;

    // Store in circular buffer
    blockEnergies[blockWriteIdx] = blockEnergy;
    blockWriteIdx = (blockWriteIdx + 1) % kShortTermBlocks;
    blocksWritten++;

    // Short-term LUFS (3-second sliding window)
    int numBlocks = std::min (blocksWritten, kShortTermBlocks);
    double shortSum = 0;
    for (int j = 0; j < numBlocks; ++j)
        shortSum += blockEnergies[j];

    double shortMean = shortSum / (double)numBlocks;
    double shortLufs = -0.691 + 10.0 * std::log10 (shortMean + 1e-30);
    lufsShort.store ((float)juce::jlimit (-60.0, 0.0, shortLufs));

    // Integrated LUFS (gated — simplified: absolute gate at -70 LUFS)
    if (shortMean > 1e-7)  // Roughly -70 dB absolute gate
    {
        integratedEnergySum += blockEnergy;
        integratedBlockCount++;

        double intMean = integratedEnergySum / (double)integratedBlockCount;
        double intLufs = -0.691 + 10.0 * std::log10 (intMean + 1e-30);
        lufsIntegrated.store ((float)juce::jlimit (-60.0, 0.0, intLufs));
    }

    // Reset accumulators for next block
    kWeightedSumL = 0;
    kWeightedSumR = 0;
    blockSampleCount = 0;
}

//==============================================================================
// SPECTRUM COMPUTATION (called from timer thread)
//==============================================================================
bool SNIPBridgeAudioProcessor::computeSpectrum (float* output, int numBins)
{
    // Check if enough new data in ring buffer
    int writePos = ringWritePos.load (std::memory_order_acquire);
    int available = (writePos - ringReadPos + kRingSize) & (kRingSize - 1);

    if (available < kFFTSize)
        return false;  // Not enough new data

    // Copy kFFTSize samples from ring buffer, apply window
    // We read the most recent kFFTSize samples
    int readStart = (writePos - kFFTSize + kRingSize) & (kRingSize - 1);

    // FFT input buffer (needs 2x size for JUCE FFT: real + imaginary interleaved)
    std::array<float, kFFTSize * 2> fftData {};

    for (int i = 0; i < kFFTSize; ++i)
    {
        int idx = (readStart + i) & (kRingSize - 1);
        fftData[i] = ringBuffer[idx] * fftWindow[i];
    }

    // Update read position
    ringReadPos = writePos;

    // Perform forward FFT (in-place, JUCE expects kFFTSize*2 array)
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Map FFT bins to numBins log-spaced output bins
    // Frequency range: ~20 Hz to Nyquist
    float nyquist = (float)(currentSampleRate * 0.5);
    float minFreq = 20.0f;
    float maxFreq = nyquist;
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    int halfFFT = kFFTSize / 2;

    for (int bin = 0; bin < numBins; ++bin)
    {
        // Log-spaced frequency range for this output bin
        float f0 = std::pow (10.0f, logMin + (logMax - logMin) * (float)bin / (float)numBins);
        float f1 = std::pow (10.0f, logMin + (logMax - logMin) * (float)(bin + 1) / (float)numBins);

        // Map to FFT bin indices
        int b0 = juce::jlimit (0, halfFFT - 1, (int)(f0 * kFFTSize / (float)currentSampleRate));
        int b1 = juce::jlimit (b0 + 1, halfFFT, (int)(f1 * kFFTSize / (float)currentSampleRate));

        // Average magnitude in this range
        float sum = 0;
        int count = 0;
        for (int j = b0; j < b1; ++j)
        {
            sum += fftData[j];
            count++;
        }

        float mag = (count > 0) ? (sum / (float)count) : 0.0f;

        // Convert to dB, normalize
        float db = 20.0f * std::log10 (mag + 1e-10f);
        output[bin] = juce::jlimit (-100.0f, 0.0f, db);
    }

    // Update 6-band summary for atomic bridge
    // Band edges: Sub(<80Hz), Low(80-250), LMid(250-1k), Mid(1k-4k), HMid(4k-8k), High(>8k)
    float bandEdges[] = { 0.0f, 80.0f, 250.0f, 1000.0f, 4000.0f, 8000.0f, maxFreq };
    for (int b = 0; b < 6; ++b)
    {
        int i0 = juce::jlimit (0, halfFFT - 1, (int)(bandEdges[b] * kFFTSize / (float)currentSampleRate));
        int i1 = juce::jlimit (i0 + 1, halfFFT, (int)(bandEdges[b + 1] * kFFTSize / (float)currentSampleRate));

        float bSum = 0;
        int bCount = 0;
        for (int j = i0; j < i1; ++j)
        {
            bSum += fftData[j];
            bCount++;
        }

        float bMag = (bCount > 0) ? (bSum / (float)bCount) : 0.0f;
        float bDb = 20.0f * std::log10 (bMag + 1e-10f);
        spectralBands[b].store (juce::jlimit (-100.0f, 0.0f, bDb));
    }

    return true;
}

//==============================================================================
// GENRE PROFILES (matching JS genreData)
//==============================================================================
const SNIPBridgeAudioProcessor::GenreProfile SNIPBridgeAudioProcessor::genreProfiles[5] =
{
    // Hip-Hop / Trap
    { -11.0f, -8.0f,  -13.0f, -10.0f, 0.25f, 0.50f, 0.3f },
    // Pop
    { -12.0f, -9.0f,  -14.0f, -11.0f, 0.35f, 0.62f, 0.4f },
    // EDM / Dance
    { -10.0f, -7.0f,  -12.0f,  -9.0f, 0.40f, 0.70f, 0.3f },
    // Rock
    { -14.0f, -10.0f, -16.0f, -12.0f, 0.28f, 0.55f, 0.4f },
    // Lo-Fi
    { -18.0f, -14.0f, -20.0f, -16.0f, 0.15f, 0.40f, 0.5f },
};

//==============================================================================
// FEEDBACK SCORING ENGINE
//==============================================================================
SNIPBridgeAudioProcessor::FeedbackScores SNIPBridgeAudioProcessor::computeFeedback (
    int genreIndex, float currentLufs, float currentRms,
    float currentWidth, float currentCorr,
    const float* spectrum, int numBins) const
{
    FeedbackScores scores;
    genreIndex = juce::jlimit (0, 4, genreIndex);
    const auto& gp = genreProfiles[genreIndex];

    // --- DYNAMICS SCORE ---
    // How well LUFS and RMS fit the genre's target range
    auto rangeScore = [](float val, float lo, float hi) -> float
    {
        if (val >= lo && val <= hi)
            return 100.0f;

        float dist = (val < lo) ? (lo - val) : (val - hi);
        float range = hi - lo;
        float tolerance = range * 0.5f;  // Half-range tolerance outside
        return juce::jlimit (0.0f, 100.0f, 100.0f * (1.0f - dist / (tolerance + 1.0f)));
    };

    float lufsScore = rangeScore (currentLufs, gp.lufsLo, gp.lufsHi);
    float rmsScore  = rangeScore (currentRms, gp.rmsLo, gp.rmsHi);

    // If signal is silent, don't penalize
    if (currentLufs <= -55.0f) lufsScore = 0.0f;
    if (currentRms <= -55.0f) rmsScore = 0.0f;

    scores.dynamics = (lufsScore + rmsScore) * 0.5f;

    // --- TONALITY SCORE ---
    // How well the spectrum fits within the genre target band
    // Uses the 6 spectral bands from the atomic bridge
    if (spectrum != nullptr && numBins > 0)
    {
        float avgLevel = 0;
        for (int i = 0; i < numBins; ++i)
            avgLevel += spectrum[i];
        avgLevel /= (float)numBins;

        // Score based on how consistent the spectrum shape is
        // (deviation from smooth curve = worse tonality)
        float deviationSum = 0;
        for (int i = 1; i < numBins - 1; ++i)
        {
            float expected = (spectrum[i - 1] + spectrum[i + 1]) * 0.5f;
            float dev = std::abs (spectrum[i] - expected);
            deviationSum += dev;
        }

        float avgDeviation = deviationSum / (float)(numBins - 2);

        // Lower deviation = smoother = better tonal balance
        // Typical deviation range: 0-10 dB
        scores.tonality = juce::jlimit (0.0f, 100.0f, 100.0f * (1.0f - avgDeviation / 8.0f));

        // If silent, zero score
        if (avgLevel <= -90.0f)
            scores.tonality = 0.0f;
    }

    // --- WIDTH SCORE ---
    float widthScore = rangeScore (currentWidth, gp.widthLo, gp.widthHi);
    scores.width = widthScore;

    // --- CORRELATION SCORE ---
    // Higher correlation = better mono compatibility
    if (currentCorr >= gp.corrMin)
        scores.correlation = juce::jlimit (50.0f, 100.0f, 50.0f + 50.0f * ((currentCorr - gp.corrMin) / (1.0f - gp.corrMin + 0.01f)));
    else
        scores.correlation = juce::jlimit (0.0f, 50.0f, 50.0f * (currentCorr / (gp.corrMin + 0.01f)));

    // Negative correlation = phase issues = bad
    if (currentCorr < 0.0f)
        scores.correlation = juce::jlimit (0.0f, 20.0f, 20.0f * (1.0f + currentCorr));

    return scores;
}

//==============================================================================
// RESET INTEGRATED LUFS
//==============================================================================
void SNIPBridgeAudioProcessor::resetIntegratedLufs()
{
    integratedEnergySum = 0;
    integratedBlockCount = 0;
    lufsIntegrated.store (-60.0f);
}

//==============================================================================
// BUILD ANALYSIS SNAPSHOT (JSON for API submission)
//==============================================================================
juce::String SNIPBridgeAudioProcessor::buildAnalysisSnapshot (
    int genreIndex, const FeedbackScores& fb,
    const float* spectrum, int numBins) const
{
    auto* root = new juce::DynamicObject();

    // Timestamp
    root->setProperty ("timestamp", juce::Time::currentTimeMillis());

    // Dynamics
    root->setProperty ("lufsShort", (double) lufsShort.load());
    root->setProperty ("lufsIntegrated", (double) lufsIntegrated.load());
    root->setProperty ("rms", (double) rmsLevel.load());

    // Stereo
    root->setProperty ("stereoCorrelation", (double) stereoCorrelation.load());
    root->setProperty ("stereoWidth", (double) stereoWidth.load());

    // Genre
    genreIndex = juce::jlimit (0, 4, genreIndex);
    juce::StringArray genreNames { "Hip-Hop / Trap", "Pop", "EDM / Dance", "Rock", "Lo-Fi" };
    root->setProperty ("genre", genreNames[genreIndex]);
    root->setProperty ("genreIndex", genreIndex);

    // Spectral bands (6)
    juce::Array<juce::var> bandsArray;
    for (int i = 0; i < 6; ++i)
        bandsArray.add ((double) spectralBands[i].load());
    root->setProperty ("spectralBands", bandsArray);

    // Spectrum (full, if available)
    if (spectrum != nullptr && numBins > 0)
    {
        juce::Array<juce::var> specArray;
        for (int i = 0; i < numBins; ++i)
            specArray.add ((double) spectrum[i]);
        root->setProperty ("spectrum", specArray);
    }

    // Feedback scores
    auto* fbObj = new juce::DynamicObject();
    fbObj->setProperty ("dynamics", (double) fb.dynamics);
    fbObj->setProperty ("tonality", (double) fb.tonality);
    fbObj->setProperty ("width", (double) fb.width);
    fbObj->setProperty ("correlation", (double) fb.correlation);
    fbObj->setProperty ("overall", (double) ((fb.dynamics + fb.tonality + fb.width + fb.correlation) / 4.0f));
    root->setProperty ("feedback", juce::var (fbObj));

    // Sample rate
    root->setProperty ("sampleRate", currentSampleRate);

    return juce::JSON::toString (juce::var (root));
}

//==============================================================================
bool SNIPBridgeAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SNIPBridgeAudioProcessor::createEditor()
{
    return new SNIPBridgeAudioProcessorEditor (*this);
}

//==============================================================================
void SNIPBridgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SNIPBridgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SNIPBridgeAudioProcessor();
}
