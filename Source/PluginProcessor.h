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
    int                       stutterWritePos     = 0; // Tracks where to record in the stutterBuffer
    int                       stutterSamplesWritten = 0; // total samples recorded into stutterBuffer
    double                    lastQuantizedBeat   = -1.0;  // used to detect new quantized beat
    bool                      autoStutterActive = false; // NEW: controls stutter playback without changing the GUI
    int                       autoStutterRemainingSamples = 0;
    double                    chosenDenominator = 64;
    double                    secondsPerWholeNote = 4;
    int                       manualStutterRateDenominator = -1;
    bool                      manualStutterTriggered = false;
    
    // ==== Envelope variables ====
    int nanoEnvelopeCounter = 0;
    int nanoEnvelopeLengthInSamples = 0;
    int macroEnvelopeCounter = 0;
    int macroEnvelopeLengthInSamples = 0;
    float iirMacroGain = 0.0f;
    float iirNanoGain = 0.0f;

    // Cached parameters for real-time-safe access
    std::array<float, 8> regularRateWeights {{ 0.0f }};
    std::array<float, 12> nanoRateWeights {{ 0.0f }};
    float nanoBlend = 0.0f;
    

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



private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessor)
};
