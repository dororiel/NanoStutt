/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NanoStuttAudioProcessorEditor::NanoStuttAudioProcessorEditor (NanoStuttAudioProcessor& p)
: AudioProcessorEditor (&p), visualizer(p), audioProcessor (p)
{
    addAndMakeVisible(stutterButton);
    stutterButton.setButtonText("Stutter");

    stutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters,
        "stutterOn",
        stutterButton);

    addAndMakeVisible(visualizer);

    setSize (400, 300);
}


NanoStuttAudioProcessorEditor::~NanoStuttAudioProcessorEditor()
{
}

//==============================================================================
void NanoStuttAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void NanoStuttAudioProcessorEditor::resized()
{
    stutterButton.setBounds(10, 10, 90, 30);
    visualizer.setBounds(10, 50, getWidth() - 20, getHeight() - 60);
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
