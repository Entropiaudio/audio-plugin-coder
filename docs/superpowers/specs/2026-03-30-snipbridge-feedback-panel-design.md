# SNIPBridge Feedback Panel Refine — Design Spec

## Summary

Rework the `#fb-panel` scoring engine and message system to provide 10-tier directional feedback across all 4 metrics (Dynamics, Tonality, Width, Correlation). Replace the current 3-tier generic messages with ~60-70 curated, actionable messages. Rework Tonality scoring from spectral smoothness to genre-comparative band analysis using data from `snip_reference_profiles.json`.

## Motivation

The current feedback panel has 3 problems:
1. **Only 3 tiers** — 51% and 74% get the same message
2. **Messages are static** — no context about what's wrong or how far off
3. **Tonality scoring is weak** — measures spectral smoothness, not genre match

## Scope

- **In scope:** C++ scoring rework, JS message table, direction flags, tonality genre-comparison
- **Out of scope:** Visual/layout changes to `#fb-panel`, spectrum display, stereo visualizer, LUFS/RMS readouts, genre selector, arc gauge appearance

## Architecture

### 1. C++ GenreProfile Expansion

Add 6-band spectral targets from `snip_reference_profiles.json` to the existing `GenreProfile` struct:

```cpp
struct GenreProfile
{
    // Existing
    float lufsLo, lufsHi;
    float rmsLo, rmsHi;
    float widthLo, widthHi;
    float corrMin;

    // NEW: 6-band spectral targets
    float bandMeans[6];    // expected dB per band (Sub, Low, LMid, Mid, HMid, High)
    float bandStdDevs[6];  // tolerance per band (1 stddev = normal variation)
};
```

Populate from `snip_reference_profiles.json` `spectral.bandMeans` and `spectral.bandStdDevs` for all 12 genres. The `genreProfiles[kNumGenres]` static array gains 12 floats per entry.

### 2. FeedbackScores Expansion

Add direction flags and worst-band index:

```cpp
struct FeedbackScores
{
    float dynamics    = 0.0f;
    float tonality    = 0.0f;
    float width       = 0.0f;
    float correlation = 0.0f;

    // Direction: -1 = below target, 0 = on target, +1 = above target
    int dynamicsDir    = 0;   // -1 too quiet, +1 too loud
    int tonalityDir    = 0;   // -1 too dark, +1 too bright
    int widthDir       = 0;   // -1 too narrow, +1 too wide
    int correlationDir = 0;   // -1 phase issues (always negative = bad)

    int tonalityWorstBand = -1;  // 0-5: which band deviates most from genre target
};
```

### 3. Scoring Algorithm Changes

#### Dynamics (existing logic, add direction)
- Score: unchanged (`rangeScore` comparing LUFS and RMS against genre range)
- Direction: compare current LUFS against genre range midpoint
  - `currentLufs > lufsHi` → `dynamicsDir = +1` (too loud)
  - `currentLufs < lufsLo` → `dynamicsDir = -1` (too quiet)
  - else → `dynamicsDir = 0` (on target)

#### Tonality (REWORKED — genre-comparative)
Replace the smoothness-based scoring with per-band genre comparison:

```
For each of the 6 spectral bands:
  deviation = abs(currentBandDb - genreBandMean) / genreBandStdDev
  bandScore = clamp(100 * (1 - deviation / 3), 0, 100)
    // Within 1 stddev = 67-100%, within 2 = 33-67%, beyond 3 = 0%

tonality = average of 6 bandScores

tonalityWorstBand = index of band with highest deviation

tonalityDir:
  if worstBand is 4 or 5 (HMid/High) and current > mean → +1 (too bright)
  if worstBand is 0 or 1 (Sub/Low) and current > mean → -1 (too dark / bass-heavy)
  if worstBand is 0 or 1 and current < mean → +1 (thin low end → bright-leaning)
  if worstBand is 4 or 5 and current < mean → -1 (dark / dull)
  if worstBand is 2 or 3 (LMid/Mid) and current > mean → -1 (boxy / muddy)
  if worstBand is 2 or 3 and current < mean → +1 (scooped / thin mids)
```

#### Width (existing logic, add direction)
- Score: unchanged (`rangeScore`)
- Direction: `currentWidth > widthHi` → +1 (too wide), `< widthLo` → -1 (too narrow), else 0

#### Correlation (existing logic, add direction)
- Score: unchanged
- Direction: `currentCorr < corrMin` → -1 (phase issues), else 0
- Negative correlation always → -1

### 4. Atomic Bridge (New Fields)

Add 5 atomics to `SNIPBridgeAudioProcessor`:

```cpp
std::atomic<int> fbDynamicsDir { 0 };
std::atomic<int> fbTonalityDir { 0 };
std::atomic<int> fbWidthDir { 0 };
std::atomic<int> fbCorrelationDir { 0 };
std::atomic<int> fbTonalityWorstBand { -1 };
```

Written by the timer thread after `computeFeedback()`, read by WebView via JUCE event system.

**Implementation note:** These atomics are NOT needed. Direction flags are computed inside `computeFeedback()` which returns `FeedbackScores`. The timer thread in `PluginEditor::timerCallback()` calls `computeFeedback()` and passes direction fields directly to the WebView via the event payload. No atomics required for direction data.

### 5. JUCE → WebView Event Data

Extend the existing feedback event payload:

```js
d.feedback = {
    dynamics: 85.2,
    tonality: 62.1,
    width: 91.0,
    correlation: 78.3,
    dynamicsDir: 0,      // NEW
    tonalityDir: 1,      // NEW
    widthDir: -1,         // NEW
    correlationDir: 0,    // NEW
    tonalityWorstBand: 5  // NEW
};
```

### 6. JS Message Table

10 tiers per metric (0-10%, 10-20%, ..., 90-100%), with directional variants (`lo`/`hi`/`on`). Casual, punchy tone.

```js
var fbMessages = {
  dynamics: [
    // tier 0: 0-10%
    { lo: "Way too quiet — barely registering.", hi: "Smashed to bits — back off hard." },
    // tier 1: 10-20%
    { lo: "Very quiet — needs a big push.", hi: "Crushed — heavy limiting damage." },
    // tier 2: 20-30%
    { lo: "Well under target loudness.", hi: "Well over target — pull it back." },
    // tier 3: 30-40%
    { lo: "Noticeably quiet for this genre.", hi: "Noticeably loud — ease up." },
    // tier 4: 40-50%
    { lo: "Below target — push levels up.", hi: "Above target — trim it down." },
    // tier 5: 50-60%
    { lo: "Just under — nudge it up.", hi: "Just over — nudge it down." },
    // tier 6: 60-70%
    { lo: "Close — minor bump needed.", hi: "Close — minor cut needed." },
    // tier 7: 70-80%
    { on: "Loudness is looking good." },
    // tier 8: 80-90%
    { on: "Dynamics right in the pocket." },
    // tier 9: 90-100%
    { on: "Loudness is spot on!" },
  ],
  tonality: [
    // Tonality messages reference the worst band when directional.
    // Band labels: ["sub", "low end", "low-mids", "mids", "upper-mids", "highs"]
    // lo = too dark/bass-heavy, hi = too bright/thin
    { lo: "Tone is way off — serious imbalance.", hi: "Tone is way off — serious imbalance." },
    { lo: "Very dark — low end dominating.", hi: "Very bright — highs are harsh." },
    { lo: "Bass-heavy for this genre.", hi: "Too bright for this genre." },
    { lo: "Tonal balance leaning dark.", hi: "Tonal balance leaning bright." },
    { lo: "Slightly dark — could use air.", hi: "Slightly bright — tame the top." },
    { lo: "Low end is a touch heavy.", hi: "Top end is a touch hot." },
    { lo: "Close — minor tonal tweak.", hi: "Close — minor tonal tweak." },
    { on: "Tonal balance is solid." },
    { on: "Spectrum fits the genre nicely." },
    { on: "Tonal balance is spot on!" },
  ],
  width: [
    { lo: "Extremely narrow — almost mono.", hi: "Way too wide — phase risks." },
    { lo: "Very narrow stereo image.", hi: "Excessively wide — rein it in." },
    { lo: "Noticeably narrow for this genre.", hi: "Noticeably wide for this genre." },
    { lo: "Stereo image is tight.", hi: "Stereo spread is generous." },
    { lo: "Could use more width.", hi: "Could use less width." },
    { lo: "Slightly narrow — open it up.", hi: "Slightly wide — tighten up." },
    { lo: "Close — minor width tweak.", hi: "Close — minor width tweak." },
    { on: "Width is in a good place." },
    { on: "Stereo image fits the genre." },
    { on: "Width is spot on!" },
  ],
  correlation: [
    // Correlation is mostly one-directional: low = bad
    { lo: "Severe phase issues — check polarity." },
    { lo: "Major phase problems — mono will cancel." },
    { lo: "Significant phase issues." },
    { lo: "Mono compatibility is poor." },
    { lo: "Phase could be tighter." },
    { lo: "Minor mono compat concerns." },
    { lo: "Mostly mono-safe — small issues." },
    { on: "Mono compatibility is solid." },
    { on: "Phase is clean and tight." },
    { on: "Mono compatibility is spot on!" },
  ],
};
```

#### Tier Selection Logic

```js
var tier = Math.min(9, Math.floor(score / 10));
var dir = directionFlags[metric]; // from C++ atomics
var msg = fbMessages[metric][tier];
var text = dir < 0 ? (msg.lo || msg.on)
         : dir > 0 ? (msg.hi || msg.on)
         : (msg.on || msg.lo);
```

### 7. Files Modified

| File | Changes |
|------|---------|
| `Source/PluginProcessor.h` | Expand `GenreProfile` (add `bandMeans[6]`, `bandStdDevs[6]`), expand `FeedbackScores` (add direction fields), add 5 direction atomics |
| `Source/PluginProcessor.cpp` | Populate genre profiles with spectral data from reference JSON, rework `computeFeedback()` tonality scoring, add direction logic to all 4 metrics |
| `Source/PluginEditor.cpp` | Pass new direction atomics to WebView in timer callback event data |
| `Source/ui/public/index.html` | Replace `metricDefs` 3-tier system with 10-tier `fbMessages` table, read direction flags from event data, update `renderMetrics()` |

### 8. Genre Spectral Data Source

All `bandMeans` and `bandStdDevs` values come from `snip_reference_profiles.json`, field `spectral.bandMeans` and `spectral.bandStdDevs`. The 6 bands map to:

| Index | Band | Frequency Range |
|-------|------|-----------------|
| 0 | Sub | <80 Hz |
| 1 | Low | 80-250 Hz |
| 2 | Low-Mid | 250 Hz - 1 kHz |
| 3 | Mid | 1-4 kHz |
| 4 | High-Mid | 4-8 kHz |
| 5 | High | >8 kHz |

### 9. What Doesn't Change

- Arc gauge visual (same canvas, same colors, same animation)
- Bar fill rendering (same width%, same color thresholds)
- Percentage number display
- Genre selector / multi-genre mixing
- Spectrum analyzer display
- Stereo visualizer (V-shape)
- LUFS/RMS/Peak readouts
- Layout, sizing, splitter behavior
- API snapshot / Send to Meetsnip.com
