/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class NanoStuttAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NanoStuttAudioProcessorEditor (NanoStuttAudioProcessor&);
    ~NanoStuttAudioProcessorEditor() override;
    juce::ToggleButton stutterButton { "Stutter" };

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NanoStuttAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessorEditor)
};
