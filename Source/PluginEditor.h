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
class StutterVisualizer : public juce::Component, private juce::Timer
{
public:
    StutterVisualizer(NanoStuttAudioProcessor& p) : processor(p)
    {
        startTimerHz(30); // update 30 times per second
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::lime);
        
        const auto& buffer = processor.stutterBuffer;
        const int numSamples = buffer.getNumSamples();
        const int channel = 0; // just show left or mono mix

        if (numSamples == 0 || buffer.getNumChannels() == 0)
            return;

        const float* data = buffer.getReadPointer(juce::jmin(channel, buffer.getNumChannels() - 1));
        juce::Path path;
        const float midY = getHeight() / 2.0f;
        const float scaleX = static_cast<float>(getWidth()) / numSamples;
        const float scaleY = midY;

        path.startNewSubPath(0, midY - data[0] * scaleY);
        for (int i = 1; i < numSamples; ++i)
        {
            float x = i * scaleX;
            float y = midY - data[i] * scaleY;
            path.lineTo(x, y);
        }

        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

private:
    void timerCallback() override { repaint(); }

    NanoStuttAudioProcessor& processor;
};

class NanoStuttAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NanoStuttAudioProcessorEditor (NanoStuttAudioProcessor&);
    ~NanoStuttAudioProcessorEditor() override;
    juce::ToggleButton stutterButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stutterAttachment;
    StutterVisualizer visualizer;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NanoStuttAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessorEditor)
};
