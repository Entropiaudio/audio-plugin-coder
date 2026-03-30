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

    // Target Genre: Choice 0-11
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "target_genre", 1 },
        "Target Genre",
        juce::StringArray { "Hip-Hop / Trap", "Pop", "EDM / Dance",
                            "Rock", "Lo-Fi", "R&B / Soul", "Latin",
                            "Trance", "Bass", "House", "Techno",
                            "Downtempo" },
        0));

    // Analysis Window (Reaction Time): 0.5-10.0 seconds, default 3.0
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "analysis_window", 1 },
        "Reaction Time",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f),
        3.0f));

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

    // Long-term stereo accumulator
    ltCorrSumLR = 0;
    ltCorrSumL2 = 0;
    ltCorrSumR2 = 0;

    // FFT window (Hann)
    for (int i = 0; i < kFFTSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (float)kFFTSize));

    // Ring buffer
    ringBuffer.fill (0.0f);
    ringWritePos.store (0);

    // Reset atomic outputs
    lufsShort.store (-60.0f);
    lufsIntegrated.store (-60.0f);
    rmsLevel.store (-60.0f);
    truePeak.store (-144.0f);
    stereoCorrelation.store (1.0f);
    stereoWidth.store (0.0f);
    stereoBalance.store (0.0f);

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
        // 2. TRUE PEAK + RAW RMS
        //--------------------------------------------------------------
        float absPeak = std::max (std::abs (sL), std::abs (sR));
        if (absPeak > 0.0f)
        {
            float peakDb = 20.0f * std::log10f (absPeak);
            float currentPeak = truePeak.load (std::memory_order_relaxed);
            if (peakDb > currentPeak)
                truePeak.store (peakDb, std::memory_order_relaxed);
        }

        float sL2 = sL * sL;
        float sR2 = sR * sR;
        rawSquareSum += (double)(sL2 + sR2) * 0.5;

        //--------------------------------------------------------------
        // 3. STEREO CORRELATION & WIDTH
        //--------------------------------------------------------------
        corrSumLR += (double)(sL * sR);
        corrSumL2 += (double)sL2;
        corrSumR2 += (double)sR2;

        // Long-term accumulators (never reset — matches reference analyzer)
        ltCorrSumLR += (double)(sL * sR);
        ltCorrSumL2 += (double)sL2;
        ltCorrSumR2 += (double)sR2;

        //--------------------------------------------------------------
        // 4. FFT RING BUFFER (mono mix)
        //--------------------------------------------------------------
        ringBuffer[(size_t)ringPos] = (sL + sR) * 0.5f;
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

            // Stereo update: correlation from short-term, width from long-term
            if (corrSampleCount > 0)
            {
                // Correlation: short-term (responsive to moment-by-moment phase)
                double denom = std::sqrt (corrSumL2 * corrSumR2);
                float corr = (denom > 1e-12) ? (float)(corrSumLR / denom) : 1.0f;
                stereoCorrelation.store (juce::jlimit (-1.0f, 1.0f, corr));

                // Width: long-term accumulators (matches reference analyzer)
                double ltMid2  = ltCorrSumL2 + ltCorrSumR2 + 2.0 * ltCorrSumLR;
                double ltSide2 = ltCorrSumL2 + ltCorrSumR2 - 2.0 * ltCorrSumLR;
                float w = (ltMid2 > 1e-12) ? (float)(ltSide2 / (ltMid2 + ltSide2)) : 0.0f;
                stereoWidth.store (juce::jlimit (0.0f, 1.0f, w));

                // Balance: short-term (responsive L/R lean)
                double totalE = corrSumL2 + corrSumR2;
                float bal = (totalE > 1e-12)
                    ? (float)((corrSumR2 - corrSumL2) / totalE)
                    : 0.0f;
                stereoBalance.store (juce::jlimit (-1.0f, 1.0f, bal));

                corrSumLR = 0;
                corrSumL2 = 0;
                corrSumR2 = 0;
                corrSampleCount = 0;
            }
        }
    }

    ringWritePos.store (ringPos, std::memory_order_release);
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
    // Always read the most recent kFFTSize samples from ring buffer
    int writePos = ringWritePos.load (std::memory_order_acquire);
    int readStart = (writePos - kFFTSize + kRingSize) & (kRingSize - 1);

    // Fill reusable FFT work buffer (avoids 32KB stack alloc)
    auto& fftData = fftWorkBuffer;
    std::fill (fftData.begin(), fftData.end(), 0.0f);

    for (int i = 0; i < kFFTSize; ++i)
    {
        int idx = (readStart + i) & (kRingSize - 1);
        fftData[(size_t)i] = ringBuffer[(size_t)idx] * fftWindow[(size_t)i];
    }

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

        // Average magnitude across FFT bins in this band
        // (matches reference analyzer: sum/count, not peak)
        float magSum = 0.0f;
        int magCount = 0;
        for (int j = b0; j < b1; ++j)
        {
            magSum += fftData[j];
            magCount++;
        }
        float mag = (magCount > 0) ? (magSum / (float)magCount) : 0.0f;

        // Normalize: 2/N matches reference analyzer's 1/N with Hann window compensation
        mag *= 2.0f / (float)kFFTSize;

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

        float bMagSum = 0.0f;
        int bCount = 0;
        for (int j = i0; j < i1; ++j)
        {
            bMagSum += fftData[j];
            bCount++;
        }
        float bMag = (bCount > 0) ? (bMagSum / (float)bCount) : 0.0f;
        bMag *= 2.0f / (float)kFFTSize;
        float bDb = 20.0f * std::log10 (bMag + 1e-10f);
        spectralBands[b].store (juce::jlimit (-100.0f, 0.0f, bDb));
    }

    return true;
}

//==============================================================================
// GENRE PROFILES (matching JS genreData)
//==============================================================================
const SNIPBridgeAudioProcessor::GenreProfile SNIPBridgeAudioProcessor::genreProfiles[kNumGenres] =
{
    // 0: Hip-Hop / Trap (119 tracks)
    { -11.933f, -7.372f,  -12.699f, -7.990f,  0.000f, 0.089f, 0.822f,
      { 0.054f, -1.866f, -6.599f, -17.532f, -24.974f, -34.448f },
      { 9.419f, 4.521f, 3.632f, 4.472f, 5.390f, 5.975f } },
    // 1: Pop (86 tracks)
    { -10.570f, -7.635f,  -12.208f, -9.177f,  0.057f, 0.197f, 0.607f,
      { -2.193f, -0.005f, -5.580f, -18.714f, -28.241f, -37.369f },
      { 6.300f, 3.995f, 2.818f, 4.395f, 5.379f, 4.979f } },
    // 2: EDM / Dance (33 tracks)
    { -8.579f,  -5.896f,  -10.029f, -8.016f,  0.069f, 0.194f, 0.611f,
      { 0.944f, 0.583f, -5.081f, -14.338f, -23.483f, -33.413f },
      { 6.666f, 3.192f, 2.689f, 4.677f, 6.715f, 7.798f } },
    // 3: Rock (20 tracks)
    { -9.833f,  -8.055f,  -11.875f, -9.902f,  0.056f, 0.218f, 0.565f,
      { -3.639f, 5.122f, -5.789f, -20.105f, -33.134f, -44.743f },
      { 5.334f, 3.353f, 2.138f, 4.720f, 5.586f, 6.216f } },
    // 4: Lo-Fi (19 tracks)
    { -14.683f, -10.918f, -15.916f, -12.047f, 0.033f, 0.146f, 0.710f,
      { -11.600f, -2.456f, -10.512f, -31.059f, -43.614f, -56.980f },
      { 7.795f, 4.166f, 3.214f, 3.473f, 5.004f, 6.546f } },
    // 5: R&B / Soul (69 tracks)
    { -10.462f, -7.937f,  -12.625f, -9.633f,  0.052f, 0.199f, 0.602f,
      { -4.748f, 1.735f, -5.026f, -18.983f, -30.194f, -40.466f },
      { 8.466f, 3.434f, 2.025f, 4.459f, 5.745f, 6.109f } },
    // 6: Latin (102 tracks)
    { -9.305f,  -7.476f,  -10.896f, -9.168f,  0.039f, 0.126f, 0.748f,
      { -7.043f, -1.588f, -4.591f, -16.630f, -26.770f, -35.576f },
      { 5.429f, 3.124f, 2.082f, 2.535f, 3.518f, 3.961f } },
    // 7: Trance (303 tracks)
    { -10.086f, -7.318f,  -11.422f, -8.490f,  0.030f, 0.112f, 0.776f,
      { 1.085f, 1.944f, -6.436f, -16.010f, -25.181f, -34.887f },
      { 5.380f, 2.895f, 2.758f, 4.258f, 5.400f, 6.096f } },
    // 8: Bass (55 tracks)
    { -9.222f,  -6.296f,  -10.450f, -7.612f,  0.044f, 0.157f, 0.687f,
      { -1.884f, -0.993f, -5.743f, -16.711f, -25.993f, -36.742f },
      { 6.281f, 4.133f, 2.981f, 4.357f, 6.260f, 7.035f } },
    // 9: House (24 tracks)
    { -13.416f, -8.187f,  -14.628f, -9.370f,  0.000f, 0.049f, 0.902f,
      { -3.912f, -3.152f, -14.317f, -20.988f, -26.348f, -37.963f },
      { 6.807f, 4.220f, 3.707f, 5.598f, 7.102f, 8.441f } },
    // 10: Techno (25 tracks)
    { -11.327f, -8.081f,  -12.144f, -8.446f,  0.002f, 0.072f, 0.857f,
      { 5.376f, 2.519f, -10.051f, -21.895f, -27.912f, -38.969f },
      { 6.152f, 4.929f, 3.405f, 4.056f, 4.793f, 5.579f } },
    // 11: Downtempo (42 tracks)
    { -15.320f, -9.885f,  -17.095f, -10.902f, 0.064f, 0.262f, 0.476f,
      { -7.364f, -1.296f, -7.514f, -22.101f, -35.056f, -48.414f },
      { 9.152f, 5.711f, 4.235f, 4.404f, 8.620f, 10.834f } },
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
    genreIndex = juce::jlimit (0, kNumGenres - 1, genreIndex);
    const auto& gp = genreProfiles[genreIndex];

    // --- DYNAMICS SCORE + DIRECTION ---
    auto rangeScore = [](float val, float lo, float hi) -> float
    {
        if (val >= lo && val <= hi)
            return 100.0f;

        float dist = (val < lo) ? (lo - val) : (val - hi);
        float range = hi - lo;
        float tolerance = range * 0.5f;
        return juce::jlimit (0.0f, 100.0f, 100.0f * (1.0f - dist / (tolerance + 1.0f)));
    };

    float lufsScore = rangeScore (currentLufs, gp.lufsLo, gp.lufsHi);
    float rmsScore  = rangeScore (currentRms, gp.rmsLo, gp.rmsHi);

    if (currentLufs <= -55.0f) lufsScore = 0.0f;
    if (currentRms <= -55.0f) rmsScore = 0.0f;

    scores.dynamics = (lufsScore + rmsScore) * 0.5f;

    // Direction: based on LUFS position relative to genre range
    if (currentLufs > -55.0f)
    {
        if (currentLufs > gp.lufsHi)
            scores.dynamicsDir = 1;   // too loud
        else if (currentLufs < gp.lufsLo)
            scores.dynamicsDir = -1;  // too quiet
        else
            scores.dynamicsDir = 0;   // on target
    }

    // --- TONALITY SCORE + DIRECTION (genre-comparative) ---
    // Compare 6 spectral bands against genre bandMeans/bandStdDevs
    {
        float bandScoreSum = 0.0f;
        float worstDeviation = 0.0f;
        int worstBand = -1;
        float worstBandDelta = 0.0f;  // positive = above mean, negative = below

        // Read current 6-band levels from atomic bridge
        float currentBands[6];
        for (int b = 0; b < 6; ++b)
            currentBands[b] = spectralBands[b].load (std::memory_order_relaxed);

        bool hasBandData = false;
        for (int b = 0; b < 6; ++b)
        {
            if (currentBands[b] > -95.0f)
            {
                hasBandData = true;
                break;
            }
        }

        if (hasBandData)
        {
            for (int b = 0; b < 6; ++b)
            {
                float delta = currentBands[b] - gp.bandMeans[b];
                float stdDev = gp.bandStdDevs[b];
                float deviation = std::abs (delta) / (stdDev + 0.01f);

                // Score: within 1 stddev = 67-100%, within 2 = 33-67%, beyond 3 = 0%
                float bandScore = juce::jlimit (0.0f, 100.0f, 100.0f * (1.0f - deviation / 3.0f));
                bandScoreSum += bandScore;

                if (deviation > worstDeviation)
                {
                    worstDeviation = deviation;
                    worstBand = b;
                    worstBandDelta = delta;
                }
            }

            scores.tonality = bandScoreSum / 6.0f;
            scores.tonalityWorstBand = worstBand;

            // Direction based on worst band location and whether energy is above/below mean
            if (worstBand >= 0)
            {
                if (worstBand <= 1)  // Sub or Low
                {
                    // Above mean = bass-heavy (dark), below mean = thin (bright-leaning)
                    scores.tonalityDir = (worstBandDelta > 0) ? -1 : 1;
                }
                else if (worstBand >= 4)  // HMid or High
                {
                    // Above mean = too bright, below mean = dark/dull
                    scores.tonalityDir = (worstBandDelta > 0) ? 1 : -1;
                }
                else  // LMid or Mid (bands 2-3)
                {
                    // Above mean = boxy/muddy, below mean = scooped/thin
                    scores.tonalityDir = (worstBandDelta > 0) ? -1 : 1;
                }
            }
        }
        else
        {
            scores.tonality = 0.0f;
        }
    }

    // --- WIDTH SCORE + DIRECTION ---
    scores.width = rangeScore (currentWidth, gp.widthLo, gp.widthHi);

    if (currentWidth > gp.widthHi)
        scores.widthDir = 1;   // too wide
    else if (currentWidth < gp.widthLo)
        scores.widthDir = -1;  // too narrow
    else
        scores.widthDir = 0;   // on target

    // --- CORRELATION SCORE + DIRECTION ---
    if (currentCorr >= gp.corrMin)
        scores.correlation = juce::jlimit (50.0f, 100.0f, 50.0f + 50.0f * ((currentCorr - gp.corrMin) / (1.0f - gp.corrMin + 0.01f)));
    else
        scores.correlation = juce::jlimit (0.0f, 50.0f, 50.0f * (currentCorr / (gp.corrMin + 0.01f)));

    if (currentCorr < 0.0f)
        scores.correlation = juce::jlimit (0.0f, 20.0f, 20.0f * (1.0f + currentCorr));

    // Direction: low correlation or negative = phase issues
    if (currentCorr < gp.corrMin)
        scores.correlationDir = -1;
    else
        scores.correlationDir = 0;

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
    genreIndex = juce::jlimit (0, kNumGenres - 1, genreIndex);
    juce::StringArray genreNames { "Hip-Hop / Trap", "Pop", "EDM / Dance", "Rock", "Lo-Fi",
                                   "R&B / Soul", "Latin", "Trance", "Bass", "House",
                                   "Techno", "Downtempo" };
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
