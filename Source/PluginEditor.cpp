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
    gateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "autoStutterGate", gateSlider);
    
    // === Rate Sliders and buttons ===
    auto rateLabels = juce::StringArray { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (int i = 0; i < rateLabels.size(); ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        rateProbSliders.add(slider);

        auto* label = new juce::Label();
        label->setText(rateLabels[i], juce::dontSendNotification);
        label->attachToComponent(slider, false);
        addAndMakeVisible(label);
        rateProbLabels.add(label);

        juce::String paramId = "rateProb_" + rateLabels[i];
        rateProbAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, paramId, *slider));
    }
    
    // === Labels for main knobs ===
    chanceLabel.setText("Chance", juce::dontSendNotification);
    chanceLabel.attachToComponent(&autoStutterChanceSlider, false);
    addAndMakeVisible(chanceLabel);

    quantLabel.setText("Quant", juce::dontSendNotification);
    quantLabel.attachToComponent(&autoStutterQuantMenu, false);
    addAndMakeVisible(quantLabel);

    gateLabel.setText("Gate", juce::dontSendNotification);
    gateLabel.attachToComponent(&gateSlider, false);
    addAndMakeVisible(gateLabel);
    
    // === Manual Triggers ===
    for (int i = 0; i < manualStutterRates.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton>(std::to_string(manualStutterRates[i]));
        int rate = manualStutterRates[i];
        
        button->setClickingTogglesState(true);
        
        button->onClick = [this, rate, btn = button.get()] {
            if (btn->getToggleState())
            {
                for (auto& otherBtn : manualStutterButtons)
                {
                    if (otherBtn.get() != btn)
                        otherBtn->setToggleState(false, juce::dontSendNotification);
                }

                audioProcessor.manualStutterRateDenominator = rate;
                audioProcessor.manualStutterTriggered = true;
                audioProcessor.autoStutterActive = false;
            }
            else
            {
                audioProcessor.manualStutterRateDenominator = -1;
                audioProcessor.manualStutterTriggered = false;
                audioProcessor.autoStutterActive = false;
            }
        };


        addAndMakeVisible(*button);
        manualStutterButtons.push_back(std::move(button));
    }




    addAndMakeVisible(visualizer);

    setResizeLimits(800, 400, 1200, 600);
    setSize(900, 450);
    setResizable(true, true);


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
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void NanoStuttAudioProcessorEditor::resized()
{
    const int margin = 10;
    const int spacing = 10;
    const int sliderWidth = 50;
    const int sliderHeight = 110;
    const int buttonHeight = 25;

    const int numRates = rateProbSliders.size();
    const int totalSliderWidth = numRates * (sliderWidth + spacing) - spacing;
    const int startX = (getWidth() - totalSliderWidth) / 2;
    const int sliderY = margin + 20;
    const int buttonY = sliderY + sliderHeight + spacing;

    // === Rate Probability Sliders + Manual Buttons ===
    for (int i = 0; i < numRates; ++i)
    {
        rateProbSliders[i]->setBounds(startX + i * (sliderWidth + spacing), sliderY, sliderWidth, sliderHeight);
        manualStutterButtons[i]->setBounds(startX + i * (sliderWidth + spacing), buttonY, sliderWidth, buttonHeight);
    }

    // === Gate Knob (Left) ===
    const int gateKnobSize = 60;
    const int gateX = margin;
    const int gateY = buttonY + buttonHeight + spacing;
    gateSlider.setBounds(gateX, gateY, gateKnobSize, gateKnobSize);
    gateLabel.setBounds(gateX, gateY + gateKnobSize, gateKnobSize, 20);

    // === Auto Stutter Section (Right) ===
    const int controlPanelX = getWidth() - 120;
    const int panelY = buttonY + buttonHeight + spacing;
    int controlY = panelY;

    autoStutterToggle.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 30;

    chanceLabel.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 20;

    autoStutterChanceSlider.setBounds(controlPanelX, controlY, 110, 30);  // Larger slider
    controlY += 40;

    quantLabel.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 20;

    autoStutterQuantMenu.setBounds(controlPanelX, controlY, 110, 24);
    controlY += 40;

    stutterButton.setBounds(controlPanelX, controlY, 110, 24);

    // === Visualizer (Bottom) ===
    int visHeight = 70;
    visualizer.setBounds(margin, getHeight() - visHeight - margin, getWidth() - 2 * margin, visHeight);
}
