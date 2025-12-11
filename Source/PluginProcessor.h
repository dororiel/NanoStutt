/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "TuningSystem.h"
#include "PresetManager.h"

//==============================================================================
/**
*/
class NanoStuttAudioProcessor  : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener

{
public:
    //==============================================================================
    NanoStuttAudioProcessor();
    ~NanoStuttAudioProcessor() override;

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

    // ==== Public Accessors ====
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    const juce::AudioBuffer<float>& getStutterBuffer() const { return stutterBuffer; }

    // Output visualization accessors
    const juce::AudioBuffer<float>& getOutputBuffer() const { return outputBuffer; }
    const std::vector<int>& getStutterStateBuffer() const { return stutterStateBuffer; }
    int getOutputBufferWritePos() const { return outputBufferWritePos.load(); }
    int getOutputBufferSize() const { return outputBufferMaxSamples; }

    void setManualStutterRate(int rate) { manualStutterRateDenominator = rate; }
    void setManualStutterTriggered(bool triggered) { manualStutterTriggered = triggered; }
    void setAutoStutterActive(bool active) { autoStutterActive = active; }

    bool isUsingNanoRate() const { return currentlyUsingNanoRate.load(); }
    float getNanoFrequency() const { return currentNanoFrequency.load(); }
    int getCurrentPlayingNanoRateIndex() const { return currentPlayingNanoRateIndex.load(); }
    int getCurrentPlayingRegularRateIndex() const { return currentPlayingRegularRateIndex.load(); }
    int getCurrentQuantIndex() const { return currentQuantIndex; }
    bool isAutoStutterActive() const { return autoStutterActive; }

    // Preset management accessor
    PresetManager& getPresetManager() { return presetManager; }

    // Custom tuning detection control (for programmatic updates)
    void setSuppressCustomDetection(bool suppress) { suppressCustomDetection = suppress; }

private:
    // ==== Timing Constants ====
    static constexpr double NANO_FADE_OUT_MS = 0.5;
    static constexpr double NANO_FADE_OUT_SECONDS = NANO_FADE_OUT_MS / 1000.0;

    // Quantization Constants
    static constexpr double THIRTY_SECOND_NOTE_PPQ = 0.125;
    static constexpr double QUARTER_NOTE_PPQ = 1.0;

    // Musical Constants
    static constexpr double SECONDS_PER_MINUTE = 60.0;
    static constexpr double WHOLE_NOTE_QUARTERS = 4.0;
    static constexpr double WHOLE_NOTE_SECONDS_MULTIPLIER = 240.0; // (WHOLE_NOTE_QUARTERS * SECONDS_PER_MINUTE)

    // Envelope Constants
    static constexpr float NANO_GATE_MIN = 0.25f;
    static constexpr float NANO_GATE_RANGE = 0.75f;
    static constexpr float MACRO_GATE_MIN = 0.25f;
    // NOTE: Hann window smoothing now uses 0.0-1.0 range directly (no scaling)

    // EMA Filter Constants
    static constexpr float NANO_EMA_MIN_ALPHA = 0.05f;  // Maximum smoothing (at knob=1.0)
    static constexpr float NANO_EMA_ALPHA_RANGE = 0.95f; // Range: 0.05-1.0

    // EMA Gain Compensation (4 dB at maximum smoothing to compensate for volume loss)
    static constexpr float NANO_EMA_MAX_GAIN_COMPENSATION_DB = 4.0f;
    static constexpr float NANO_EMA_MAX_GAIN_COMPENSATION_LINEAR = 1.5849f;  // 10^(4/20)

    // EMA Signal Chain Position (for testing - change to test different positions)
    enum class EmaPosition {
        BeforeNanoEnvelope,      // Position A: After buffer read, before nano envelope
        AfterNanoEnvelope,       // Position B: After nano envelope, before macro envelope
        AfterMacroEnvelope       // Position C: After macro envelope (final wet signal)
    };
    static constexpr EmaPosition NANO_EMA_POSITION = EmaPosition::AfterNanoEnvelope;

    // Window types for nanoSmooth per-cycle windowing
    enum class WindowType {
        None = 0,           // Bypass (no windowing)
        Hann = 1,           // Default (current behavior) - FIXED
        Hamming = 2,        // Similar to Hann, slightly different shape - FIXED
        Blackman = 3,       // Wider main lobe, better side-lobe suppression - FIXED
        BlackmanHarris = 4, // Even better side-lobe suppression - FIXED
        Bartlett = 5,       // Triangular window - FIXED
        Kaiser = 6,         // ADJUSTABLE (beta controlled by nanoSmooth)
        Tukey = 7,          // ADJUSTABLE (alpha controlled by nanoSmooth)
        Gaussian = 8,       // ADJUSTABLE (sigma controlled by nanoSmooth)
        Planck = 9,         // ADJUSTABLE (epsilon controlled by nanoSmooth)
        Exponential = 10    // ADJUSTABLE (tau controlled by nanoSmooth)
    };

    // Cycle Crossfade Constants
    static constexpr float CYCLE_CROSSFADE_MAX_PERCENT = 0.1f;  // 10% max of loop length

    // Buffer Constants
    static constexpr double MAX_STUTTER_BUFFER_SECONDS = 3.0;

    // ==== Stutter variables ====
    juce::AudioBuffer<float> stutterBuffer;
    juce::AudioProcessorValueTreeState parameters;
    PresetManager presetManager;

    // ==== Output visualization buffers ====
    juce::AudioBuffer<float> outputBuffer;              // Ring buffer for output visualization (sized to 1/4 note)
    std::vector<int> stutterStateBuffer;                // Stutter state per sample (0=none, 1=repeat, 2=nano)
    std::atomic<int> outputBufferWritePos {0};          // Current write position in output buffer
    int outputBufferMaxSamples = 0;                     // Current size of output buffer (1/4 note at current BPM)
    double lastKnownBpm = 120.0;                        // Track BPM for dynamic buffer resizing
    int lastOutputWriteIndex = -1;                      // Track last write position for gap filling

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    int                       writePos            = 0;
    int                       maxStutterLenSamples = 0;
    bool                      stutterLatched      = false;   // true while slice is repeating
    int                       stutterLenSamples   = 0;       // length of the 1/64-note in samples
    int                       stutterPlayCounter  = 0;       // wraps 0…stutterLenSamples-1
    double                    lastQuantizedBeat   = -1.0;  // used to detect new quantized beat
    bool                      autoStutterActive = false; // NEW: controls stutter playback without changing the GUI
    int                       autoStutterRemainingSamples = 0;
    int                       currentStutterRemainingSamples = 0;  // Universal countdown for ANY active stutter event
    double                    chosenDenominator = 64;
    double                    secondsPerWholeNote = 4;
    int                       manualStutterRateDenominator = -1;
    bool                      manualStutterTriggered = false;
    int                       quantCount = 0;
    int                       stutterWritePos = 0; // Tracks where to record in the stutterBuffer (audio thread only)
    int                       quantToNewBeat = 4;
    // ==== New Fade & State Logic ====
    int fadeLengthInSamples = 0;
    bool stutterIsScheduled = false;
    double lastDecisionBeat = -1.0;
    int postStutterSilence = 0;
    int stutterEventLengthSamples = 0; // Pre-calculated stutter event length for fade logic
    
    // State tracking for fade logic
    bool wasStuttering = false;

    // ==== Envelope variables ====
    int nanoEnvelopeLengthInSamples = 0;
    int macroEnvelopeCounter = 0;
    int macroEnvelopeLengthInSamples = 0;
    
    // Cached parameters for real-time-safe access
    std::array<float, 13> regularRateWeights {{ 0.0f }};  // Was 12, now 13 (added 1/4d)
    std::array<float, 12> nanoRateWeights {{ 0.0f }};
    std::array<float, 9> quantUnitWeights {{ 0.0f }};
    float nanoBlend = 0.0f;

    // Sample-and-hold envelope parameters (sampled to keep event-locked behavior)
    // Current event parameters - used throughout the active stutter event
    float currentMacroGateParam = 1.0f;
    float currentMacroShapeParam = 0.5f;
    float currentMacroSmoothParam = 0.0f;
    float currentNanoGateParam = 1.0f;
    float currentNanoShapeParam = 0.5f;
    float currentNanoSmoothParam = 0.0f;      // Hann window smoothing per-cycle
    float currentNanoEmaParam = 0.0f;         // EMA filter (formerly NanoSmooth)
    float currentNanoOctaveParam = 0.0f;

    // Next event parameters - sampled 2ms before event end for upcoming event
    float nextMacroGateParam = 1.0f;
    float nextMacroShapeParam = 0.5f;
    float nextMacroSmoothParam = 0.0f;
    float nextNanoGateParam = 1.0f;
    float nextNanoShapeParam = 0.5f;
    float nextNanoSmoothParam = 0.0f;         // Hann window smoothing per-cycle
    float nextNanoEmaParam = 0.0f;            // EMA filter (formerly NanoSmooth)
    float nextNanoOctaveParam = 0.0f;

    // Window type selection for nanoSmooth (sample-and-hold like other params)
    int currentWindowType = 7;  // Default: Tukey
    int nextWindowType = 7;

    // Held random offsets for nano parameters (calculated once per event, added to per-cycle values)
    float heldNanoGateRandomOffset = 0.0f;
    float heldNanoShapeRandomOffset = 0.0f;
    float heldNanoEmaRandomOffset = 0.0f;           // Held for entire stutter event
    float heldCycleCrossfadeRandomOffset = 0.0f;    // Held for entire stutter event

    bool parametersHeld = false;
    
    // Dynamic quantization
    int currentQuantIndex = 1; // Default to 1/8 (index 1)
    int nextQuantIndex = 1;
    bool quantDecisionPending = false;

    // Transport state tracking for quantization alignment
    bool wasPlaying = false;
    double lastPpqPosition = -1.0;

    // Transport stop fade-to-dry mechanism
    bool isFadingToStopTransport = false;
    int stopFadeRemainingSamples = 0;
    float stopFadeStartDryGain = 0.0f;
    float stopFadeStartWetGain = 0.0f;
    // Stutter state snapshot for stop fade playback continuation
    int stopFadeStutterPlayCounter = 0;
    int stopFadeMacroEnvelopeCounter = 0;
    int stopFadeLoopLen = 0;
    int stopFadeChosenDenominator = 1;
    // EMA filter state snapshot for stop fade continuation
    std::vector<float> stopFadeEmaState;
    float stopFadeNanoEmaParam = 0.0f;      // EMA filter (formerly NanoSmooth)

    // Loop boundary handling - prevent clicks at first sample after jump
    bool skipFadeOnNextSample = false;
    int samplesProcessedAfterJump = 0;

    // State flags for processBlock (moved from static variables)
    bool parametersSampledForUpcomingEvent = false;
    bool stutterInitialized = false;

    // Manual timing offset for master track delay compensation
    std::atomic<float>* timingOffsetParam = nullptr;

    // Nano rate tracking for tuner display and UI state
    std::atomic<bool> currentlyUsingNanoRate {false};
    std::atomic<float> currentNanoFrequency {0.0f};
    std::atomic<int> currentPlayingNanoRateIndex {-1};  // -1 = not playing, 0-11 = active nano rate index
    std::atomic<int> currentPlayingRegularRateIndex {-1};  // -1 = not playing, 0-12 = active regular rate index

    // Smoothed envelope parameters (0.3ms ramp time for fast response, prevents bleeding across events)
    juce::LinearSmoothedValue<float> smoothedNanoGate;
    juce::LinearSmoothedValue<float> smoothedNanoShape;
    juce::LinearSmoothedValue<float> smoothedNanoSmooth;      // Hann window smoothing
    juce::LinearSmoothedValue<float> smoothedNanoEma;         // EMA filter (formerly NanoSmooth)
    juce::LinearSmoothedValue<float> smoothedMacroGate;
    juce::LinearSmoothedValue<float> smoothedMacroShape;
    juce::LinearSmoothedValue<float> smoothedMacroSmooth;

    // Smoothed held gate parameters (0.3ms ramp time for stable lengths)
    juce::LinearSmoothedValue<float> smoothedHeldNanoGate;   // Per cycle
    juce::LinearSmoothedValue<float> smoothedHeldMacroGate;  // Per event

    // Reverse playback control
    bool currentStutterIsReversed = false;      // Whether current stutter event should be reversed
    bool firstRepeatCyclePlayed = false;        // Track if first repeat cycle has been played
    bool isFirstReverseCycle = false;           // Track first reverse cycle (skip crossfade during direction change)
    int cycleCompletionCounter = 0;             // Track how many cycles have been completed

    // Cycle detection for nanoGate holding
    int lastLoopPos = -1;                       // Track loop position to detect wrap-around
    int heldNanoEnvelopeLengthInSamples = 0;    // Pre-calculated nano envelope length per cycle

    // EMA Filter State (exponential moving average for nano smooth)
    std::vector<float> nanoEmaState;            // Per-channel EMA history for wet signal
    std::vector<float> dryEmaStateForFade;      // Per-channel EMA history for dry signal during fades
    float currentNanoEmaAlpha = 1.0f;           // Current alpha coefficient (1.0 = bypass, 0.05 = max smooth)
    bool shouldResetEmaState = false;           // Flag to reset EMA at loop wraparound

    // Ratio/denominator lookup (updated for 13 rates with new order including 1/4d)
    // Order: 1, 1/2d, 1/2, 1/4d, 1/3, 1/4, 1/8d, 1/6, 1/8, 1/12, 1/16, 1/24, 1/32
    static constexpr std::array<double, 13> regularDenominators {{ 1.0, 4.0/3.0, 2.0, 8.0/3.0, 3.0, 4.0, 16.0/3.0, 6.0, 8.0, 12.0, 16.0, 24.0, 32.0 }};
    static constexpr std::array<float, 12> nanoRatios {{
        1.0f,           // C - Unison
        1.059463094f,   // C# - Minor 2nd
        1.122462048f,   // D - Major 2nd
        1.189207115f,   // D# - Minor 3rd
        1.259921050f,   // E - Major 3rd
        1.334839854f,   // F - Perfect 4th
        1.414213562f,   // F# - Tritone
        1.498307077f,   // G - Perfect 5th
        1.587401052f,   // G# - Minor 6th
        1.681792831f,   // A - Major 6th
        1.781797436f,   // A# - Minor 7th
        1.887748625f    // B - Major 7th
    }};

    // Nano tuning system state
    NanoTuning::NanoBase currentNanoBase = NanoTuning::NanoBase::BPMSynced;
    NanoTuning::TuningSystem currentTuningSystem = NanoTuning::TuningSystem::EqualTemperament;
    NanoTuning::Scale currentScale = NanoTuning::Scale::NaturalMinor;
    std::array<float, 12> runtimeNanoRatios = nanoRatios; // Runtime-modifiable copy
    bool suppressCustomDetection = false;  // Suppress detection during programmatic updates
    std::atomic<bool> pendingUIUpdate {false};  // Debounce UI update callbacks

    // Param listeners
    void updateCachedParameters();
    void initializeParameterListeners();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateWaveshaperFunction(int algorithm, float drive, bool gainCompensation);

    // Nano tuning system methods
    void updateNanoRatiosFromTuning();
    void updateNanoVisibilityFromScale();
    void detectCustomTuning();
    void detectCustomScale();

    // Output buffer management
    void resizeOutputBufferForBpm(double bpm, double sampleRate);

    // Weighted probability selection utility
    template<typename Container>
    static int selectWeightedIndex(const Container& weights, int defaultIndex = 0)
    {
        int idx = defaultIndex;
        float total = std::accumulate(weights.begin(), weights.end(), 0.0f);
        if (total > 0.0f) {
            float r = juce::Random::getSystemRandom().nextFloat() * total;
            float accum = 0.0f;
            for (int j = 0; j < (int)weights.size(); ++j) {
                accum += weights[j];
                if (r <= accum) {
                    idx = j;
                    break;
                }
            }
        }
        return idx;
    }

    // Envelope gain calculation utility
    static inline float calculateEnvelopeGain(float progress, float shapeParam)
    {
        float mappedShape = (shapeParam - 0.5f) * 2.0f;
        float curveAmount = std::abs(mappedShape);
        float exponent = 1.0f + curveAmount * 4.0f;
        float curvedGain = (mappedShape < 0.0f)
            ? std::pow(1.0f - progress, exponent)
            : std::pow(progress, exponent);
        return juce::jmap(curveAmount, 0.0f, 1.0f, 1.0f, curvedGain);
    }

    // Multi-window calculation utility
    // Produces window gain from 0 → 1 → 0 (or other shapes) across progress [0,1]
    // windowType: current window selection
    // progress: position within cycle [0.0, 1.0]
    // intensity: nanoSmooth parameter value [0.0, 1.0]
    static inline float calculateWindowGain(int windowType, float progress, float intensity)
    {
        // Guard: bypass if intensity too low or progress invalid
        if (intensity < 0.01f || progress < 0.0f || progress > 1.0f) {
            return 1.0f;  // No windowing
        }

        float windowGain = 1.0f;

        switch (windowType) {
            case 0: // None - bypass
                windowGain = 1.0f;
                break;

            case 1: // Hann (default, maintains backward compatibility)
                windowGain = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * progress));
                break;

            case 2: // Hamming
                windowGain = 0.54f - 0.46f * std::cos(juce::MathConstants<float>::twoPi * progress);
                break;

            case 3: // Blackman
            {
                float a0 = 0.42f;
                float a1 = 0.5f;
                float a2 = 0.08f;
                windowGain = a0
                    - a1 * std::cos(juce::MathConstants<float>::twoPi * progress)
                    + a2 * std::cos(2.0f * juce::MathConstants<float>::twoPi * progress);
                break;
            }

            case 4: // Blackman-Harris
            {
                float a0 = 0.35875f;
                float a1 = 0.48829f;
                float a2 = 0.14128f;
                float a3 = 0.01168f;
                windowGain = a0
                    - a1 * std::cos(juce::MathConstants<float>::twoPi * progress)
                    + a2 * std::cos(2.0f * juce::MathConstants<float>::twoPi * progress)
                    - a3 * std::cos(3.0f * juce::MathConstants<float>::twoPi * progress);
                break;
            }

            case 5: // Bartlett (triangular)
                windowGain = 1.0f - std::abs(2.0f * progress - 1.0f);
                break;

            case 6: // Kaiser (parameterized by intensity)
            {
                // Map intensity to beta: 0.0 → 0, 1.0 → 10
                float beta = intensity * 10.0f;

                // Kaiser window formula using modified Bessel function approximation
                // w(n) = I0(beta * sqrt(1 - x^2)) / I0(beta), where x = 2*progress - 1
                float x = 2.0f * progress - 1.0f;  // Map [0,1] → [-1,1]
                float argument = beta * std::sqrt(std::max(0.0f, 1.0f - x * x));

                // I0(z) approximation using polynomial (accurate for z < 3.75)
                auto besselI0 = [](float z) -> float {
                    if (z < 3.75f) {
                        float t = z / 3.75f;
                        float t2 = t * t;
                        return 1.0f + 3.5156229f * t2 + 3.0899424f * t2 * t2
                            + 1.2067492f * t2 * t2 * t2 + 0.2659732f * t2 * t2 * t2 * t2
                            + 0.0360768f * t2 * t2 * t2 * t2 * t2 + 0.0045813f * t2 * t2 * t2 * t2 * t2 * t2;
                    } else {
                        float t = 3.75f / z;
                        return (std::exp(z) / std::sqrt(z)) * (0.39894228f + 0.01328592f * t
                            + 0.00225319f * t * t - 0.00157565f * t * t * t + 0.00916281f * t * t * t * t);
                    }
                };

                float i0Beta = besselI0(beta);
                windowGain = (i0Beta > 0.0001f) ? (besselI0(argument) / i0Beta) : 1.0f;
                break;
            }

            case 7: // Tukey (parameterized by intensity)
            {
                // Map intensity to alpha: 0.0 → 0 (rectangular), 1.0 → 1 (full Hann)
                float alpha = intensity;

                if (alpha < 0.01f) {
                    // Rectangular window (no taper)
                    windowGain = 1.0f;
                } else {
                    // Taper width on each side
                    float taperWidth = alpha * 0.5f;

                    if (progress < taperWidth) {
                        // Rising taper (cosine)
                        float taperProgress = progress / taperWidth;
                        windowGain = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * taperProgress));
                    } else if (progress > (1.0f - taperWidth)) {
                        // Falling taper (cosine)
                        float taperProgress = (progress - (1.0f - taperWidth)) / taperWidth;
                        windowGain = 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * taperProgress));
                    } else {
                        // Flat-top region
                        windowGain = 1.0f;
                    }
                }
                break;
            }

            case 8: // Gaussian (parameterized by intensity)
            {
                // Map intensity to sigma: 0.0 → 0.2 (wide), 1.0 → 0.02 (narrow)
                // Narrower Gaussian = more pronounced fade-in/out
                float sigma = 0.2f - (intensity * 0.18f);
                sigma = std::max(0.02f, sigma);  // Clamp minimum

                // Gaussian formula: exp(-0.5 * ((x - 0.5) / sigma)^2)
                float x = progress - 0.5f;  // Center at 0.5
                float exponent = -0.5f * (x * x) / (sigma * sigma);
                windowGain = std::exp(exponent);
                break;
            }

            case 9: // Planck-taper (parameterized by intensity)
            {
                // Map intensity to epsilon: 0.0 → 0.05 (minimal taper), 1.0 → 0.5 (maximum taper)
                float epsilon = 0.05f + (intensity * 0.45f);
                epsilon = juce::jlimit(0.001f, 0.5f, epsilon);

                // Planck-taper window formula (correct implementation)
                // w(x) = 1 for epsilon < x < 1-epsilon (flat top)
                // w(x) = [1 + exp(epsilon/x + epsilon/(x-epsilon))]^-1 for 0 <= x <= epsilon (left taper)
                // w(x) = [1 + exp(epsilon/(1-x) + epsilon/(1-x-epsilon))]^-1 for 1-epsilon <= x <= 1 (right taper)

                if (progress >= epsilon && progress <= (1.0f - epsilon)) {
                    // Flat-top region
                    windowGain = 1.0f;
                } else if (progress < epsilon) {
                    // Left taper - smooth transition from 0 to 1
                    if (progress > 0.0001f && progress < epsilon - 0.0001f) {
                        // Standard Planck formula for left edge
                        float z = (epsilon / progress) + (epsilon / (progress - epsilon));
                        windowGain = 1.0f / (1.0f + std::exp(z));
                    } else if (progress <= 0.0001f) {
                        windowGain = 0.0f;
                    } else {
                        windowGain = 1.0f;  // At boundary
                    }
                } else {
                    // Right taper (progress > 1-epsilon) - smooth transition from 1 to 0
                    float distFromEnd = 1.0f - progress;
                    if (distFromEnd > 0.0001f && distFromEnd < epsilon - 0.0001f) {
                        // Standard Planck formula for right edge
                        float z = (epsilon / distFromEnd) + (epsilon / (distFromEnd - epsilon));
                        windowGain = 1.0f / (1.0f + std::exp(z));
                    } else if (distFromEnd <= 0.0001f) {
                        windowGain = 0.0f;
                    } else {
                        windowGain = 1.0f;  // At boundary
                    }
                }
                break;
            }

            case 10: // Exponential/Poisson (parameterized by intensity)
            {
                // Map intensity to alpha (decay rate): 0.0 → 0.5 (wide/slow decay), 1.0 → 8.0 (narrow/fast decay)
                float alpha = 0.5f + (intensity * 7.5f);

                // Exponential window formula (correct normalized version)
                // w(progress) = exp(-2 * alpha * |progress - 0.5|) where progress ∈ [0, 1]
                // At center (progress = 0.5): windowGain = 1.0
                // At edges (progress = 0 or 1): windowGain = exp(-alpha)
                float distanceFromCenter = std::abs(progress - 0.5f);
                windowGain = std::exp(-2.0f * alpha * distanceFromCenter);
                break;
            }

            default:
                windowGain = 1.0f;
                break;
        }

        return windowGain;
    }

    // Returns true if window type has adjustable parameters controlled by nanoSmooth
    static inline bool isAdjustableWindow(int windowType)
    {
        switch (windowType) {
            case 6:  // Kaiser
            case 7:  // Tukey
            case 8:  // Gaussian
            case 9:  // Planck
            case 10: // Exponential
                return true;
            default:
                return false;
        }
    }

    // JUCE DSP ProcessorChain for waveshaping
    enum
    {
        inputGainIndex,
        waveShaperIndex,
        outputGainIndex
    };

    using WaveshaperChain = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,      // Input gain
        juce::dsp::WaveShaper<float>, // Waveshaper
        juce::dsp::Gain<float>       // Output gain
    >;

    WaveshaperChain waveshaperChain;
    juce::dsp::ProcessSpec dspSpec;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessor)
};
