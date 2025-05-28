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
    // === Manual Stutter Button === //
    addAndMakeVisible(stutterButton);
    stutterButton.setButtonText("Stutter");

    stutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters,
        "stutterOn",
        stutterButton);
    
    // === Auto Stutter Toggle ===
    addAndMakeVisible(autoStutterToggle);
    autoStutterToggle.setButtonText("Auto Stutter");

    autoStutterToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "autoStutterEnabled", autoStutterToggle);

    // === Auto Stutter Chance Slider ===
    addAndMakeVisible(autoStutterChanceSlider);
    autoStutterChanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    autoStutterChanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    autoStutterChanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "autoStutterChance", autoStutterChanceSlider);

    // === Quantization Menu ===
    addAndMakeVisible(autoStutterQuantMenu);
    autoStutterQuantMenu.addItem("1/4", 1);
    autoStutterQuantMenu.addItem("1/8", 2);
    autoStutterQuantMenu.addItem("1/16", 3);
    autoStutterQuantMenu.addItem("1/32", 4);

    autoStutterQuantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "autoStutterQuant", autoStutterQuantMenu);
    
    // === Gate Slider ===
    addAndMakeVisible(gateSlider);
    gateSlider.setSliderStyle(juce::Slider::Rotary);
    gateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "autoStutterGate", gateSlider);


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
    int x = 10;
    int y = 10;
    int width = 180;
    int height = 30;
    int pad = 10;
    
    gateSlider.setBounds(210, 10, 70, 70);

    stutterButton.setBounds(x, y, width, height);
    y += height + pad;

    autoStutterToggle.setBounds(x, y, width, height);
    y += height + pad;

    autoStutterChanceSlider.setBounds(x, y, width, height);
    y += height + pad;

    autoStutterQuantMenu.setBounds(x, y, width, height);
    y += height + pad;

    // Place visualizer below all controls, fill remaining space
    const int visualizerTop = y + pad;
    visualizer.setBounds(x, visualizerTop, getWidth() - 2 * x, getHeight() - visualizerTop - pad);
}
