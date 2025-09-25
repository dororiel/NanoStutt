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
struct StutterState;


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
    
    // ==== Stutter variables ====
    juce::AudioBuffer<float> stutterBuffer;
    juce::AudioProcessorValueTreeState parameters;

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
    std::atomic<int>          stutterWritePos = 0; // Tracks where to record in the stutterBuffer
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
    std::array<float, 8> regularRateWeights {{ 0.0f }};
    std::array<float, 12> nanoRateWeights {{ 0.0f }};
    std::array<float, 4> quantUnitWeights {{ 0.0f }};
    float nanoBlend = 0.0f;

    // Sample-and-hold envelope parameters (sampled 1ms before stutter events)
    float heldMacroGateParam = 1.0f;
    float heldMacroShapeParam = 0.5f;
    float heldMacroSmoothParam = 0.0f;
    float heldNanoGateParam = 1.0f;
    float heldNanoShapeParam = 0.5f;
    float heldNanoSmoothParam = 0.0f;
    bool parametersHeld = false;
    
    // Dynamic quantization
    int currentQuantIndex = 1; // Default to 1/8 (index 1)
    int nextQuantIndex = 1;
    bool quantDecisionPending = false;

    // Transport state tracking for quantization alignment
    bool wasPlaying = false;
    double lastPpqPosition = -1.0;

    // Manual timing offset for master track delay compensation
    std::atomic<float>* timingOffsetParam = nullptr;

    // Reverse playback control
    bool currentStutterIsReversed = false;      // Whether current stutter event should be reversed
    bool firstRepeatCyclePlayed = false;        // Track if first repeat cycle has been played
    int cycleCompletionCounter = 0;             // Track how many cycles have been completed

    // Ratio/denominator lookup
    static constexpr std::array<int, 8> regularDenominators {{ 4, 3, 6, 8, 12, 16, 24, 32 }};
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

    void setManualStutterRate(int rate) { manualStutterRateDenominator = rate; }

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

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessor)
};
