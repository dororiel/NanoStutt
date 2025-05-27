/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
struct StutterState;


class NanoStuttAudioProcessor  : public juce::AudioProcessor
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
    juce::AudioBuffer<float>  circularBuffer;
    int                       writePos            = 0;

    bool                      stutterOn           = false;   // set from the GUI
    bool                      stutterLatched      = false;   // true while slice is repeating
    int                       stutterLenSamples   = 0;       // length of the 1/64-note in samples
    int                       stutterReadStart    = 0;       // start index inside circularBuffer
    int                       stutterPlayCounter  = 0;       // wraps 0…stutterLenSamples-1

    void setStutterOn (bool shouldStutter);   // called by the editor


private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessor)
};
