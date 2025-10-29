/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

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
    bool isAutoStutterActive() const { return autoStutterActive; }

private:
    // ==== Timing Constants ====
    static constexpr double FADE_DURATION_MS = 1.0;
    static constexpr double FADE_DURATION_SECONDS = FADE_DURATION_MS / 1000.0;
    static constexpr double PARAMETER_SAMPLE_ADVANCE_MS = 2.0;
    static constexpr double NANO_FADE_OUT_MS = 0.1;
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
    static constexpr float MACRO_SMOOTH_SCALE = 0.3f;
    static constexpr float NANO_SMOOTH_SCALE = 0.25f;

    // Buffer Constants
    static constexpr double MAX_STUTTER_BUFFER_SECONDS = 3.0;

    // ==== Stutter variables ====
    juce::AudioBuffer<float> stutterBuffer;
    juce::AudioProcessorValueTreeState parameters;

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
    std::array<float, 12> regularRateWeights {{ 0.0f }};
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
    float currentNanoSmoothParam = 0.0f;

    // Next event parameters - sampled 2ms before event end for upcoming event
    float nextMacroGateParam = 1.0f;
    float nextMacroShapeParam = 0.5f;
    float nextMacroSmoothParam = 0.0f;
    float nextNanoGateParam = 1.0f;
    float nextNanoShapeParam = 0.5f;
    float nextNanoSmoothParam = 0.0f;

    // Held random offsets for nano parameters (calculated once per event, added to per-cycle values)
    float heldNanoGateRandomOffset = 0.0f;
    float heldNanoShapeRandomOffset = 0.0f;

    bool parametersHeld = false;
    
    // Dynamic quantization
    int currentQuantIndex = 1; // Default to 1/8 (index 1)
    int nextQuantIndex = 1;
    bool quantDecisionPending = false;

    // Transport state tracking for quantization alignment
    bool wasPlaying = false;
    double lastPpqPosition = -1.0;

    // State flags for processBlock (moved from static variables)
    bool parametersSampledForUpcomingEvent = false;
    bool stutterInitialized = false;

    // Manual timing offset for master track delay compensation
    std::atomic<float>* timingOffsetParam = nullptr;

    // Nano rate tracking for tuner display
    std::atomic<bool> currentlyUsingNanoRate {false};
    std::atomic<float> currentNanoFrequency {0.0f};

    // Smoothed envelope parameters (0.3ms ramp time for fast response, prevents bleeding across events)
    juce::LinearSmoothedValue<float> smoothedNanoGate;
    juce::LinearSmoothedValue<float> smoothedNanoShape;
    juce::LinearSmoothedValue<float> smoothedNanoSmooth;
    juce::LinearSmoothedValue<float> smoothedMacroGate;
    juce::LinearSmoothedValue<float> smoothedMacroShape;
    juce::LinearSmoothedValue<float> smoothedMacroSmooth;

    // Smoothed held gate parameters (0.3ms ramp time for stable lengths)
    juce::LinearSmoothedValue<float> smoothedHeldNanoGate;   // Per cycle
    juce::LinearSmoothedValue<float> smoothedHeldMacroGate;  // Per event

    // Reverse playback control
    bool currentStutterIsReversed = false;      // Whether current stutter event should be reversed
    bool firstRepeatCyclePlayed = false;        // Track if first repeat cycle has been played
    int cycleCompletionCounter = 0;             // Track how many cycles have been completed

    // Cycle detection for nanoGate holding
    int lastLoopPos = -1;                       // Track loop position to detect wrap-around
    int heldNanoEnvelopeLengthInSamples = 0;    // Pre-calculated nano envelope length per cycle

    // Ratio/denominator lookup
    static constexpr std::array<double, 12> regularDenominators {{ 1.0, 4.0/3.0, 2.0, 3.0, 4.0, 6.0, 16.0/3.0, 8.0, 12.0, 16.0, 24.0, 32.0 }};
    static constexpr std::array<float, 12> nanoRatios {{
        1.0f,
        15.0f / 16.0f,
        5.0f / 6.0f,
        4.0f / 5.0f,
        3.0f / 4.0f,
        2.0f / 3.0f,
        3.0f / 5.0f,
        0.5f,
        1.0f, 1.0f, 1.0f, 1.0f  // Fillers
    }};

    // Param listeners
    void updateCachedParameters();
    void initializeParameterListeners();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateWaveshaperFunction(int algorithm, float drive, bool gainCompensation);

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
