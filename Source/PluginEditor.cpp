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

    // === Reverse Chance Slider ===
    addAndMakeVisible(reverseChanceSlider);
    reverseChanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverseChanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    reverseChanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "reverseChance", reverseChanceSlider);

    // === Quantization Menu ===
    addAndMakeVisible(autoStutterQuantMenu);
    autoStutterQuantMenu.addItem("1/4", 1);
    autoStutterQuantMenu.addItem("1/8", 2);
    autoStutterQuantMenu.addItem("1/16", 3);
    autoStutterQuantMenu.addItem("1/32", 4);

    autoStutterQuantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "autoStutterQuant", autoStutterQuantMenu);

    // === Envelope Controls ===
    auto setupKnob = [this] (juce::Slider& slider, const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment)
    {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.parameters, paramID, slider);
    };

    setupKnob(nanoGateSlider, "NanoGate", nanoGateAttachment);
    setupKnob(nanoShapeSlider, "NanoShape", nanoShapeAttachment);
    setupKnob(nanoSmoothSlider, "NanoSmooth", nanoSmoothAttachment);
    setupKnob(macroGateSlider, "MacroGate", macroGateAttachment);
    setupKnob(macroShapeSlider, "MacroShape", macroShapeAttachment);
    setupKnob(macroSmoothSlider, "MacroSmooth", macroSmoothAttachment);

    // === Timing Offset Slider ===
    addAndMakeVisible(timingOffsetSlider);
    timingOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timingOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    timingOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "TimingOffset", timingOffsetSlider);

    // === Labels ===
    auto setupLabel = [this] (juce::Label& label, const juce::String& text, juce::Component& component)
    {
        label.setText(text, juce::dontSendNotification);
        label.attachToComponent(&component, false);
        label.setJustificationType(juce::Justification::centredBottom);
        addAndMakeVisible(label);
    };

    setupLabel(nanoGateLabel, "Gate", nanoGateSlider);
    setupLabel(nanoShapeLabel, "Shape", nanoShapeSlider);
    setupLabel(nanoSmoothLabel, "Smooth", nanoSmoothSlider);
    setupLabel(macroGateLabel, "Gate", macroGateSlider);
    setupLabel(macroShapeLabel, "Shape", macroShapeSlider);
    setupLabel(macroSmoothLabel, "Smooth", macroSmoothSlider);

    nanoControlsLabel.setText("Nano Envelope", juce::dontSendNotification);
    nanoControlsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nanoControlsLabel);

    macroControlsLabel.setText("Macro Envelope", juce::dontSendNotification);
    macroControlsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(macroControlsLabel);
    
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
    
    // === Quant Probability Sliders ===
    auto quantLabels = juce::StringArray { "1/4", "1/8", "1/16", "1/32" };
    for (int i = 0; i < quantLabels.size(); ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        quantProbSliders.add(slider);

        auto* label = new juce::Label();
        label->setText(quantLabels[i], juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        quantProbLabels.add(label);

        juce::String paramId = "quantProb_" + quantLabels[i];
        quantProbAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, paramId, *slider));
    }
    
    // === Labels for main knobs ===
    chanceLabel.setText("Chance", juce::dontSendNotification);
    chanceLabel.attachToComponent(&autoStutterChanceSlider, false);
    addAndMakeVisible(chanceLabel);

    reverseLabel.setText("Reverse", juce::dontSendNotification);
    reverseLabel.attachToComponent(&reverseChanceSlider, false);
    addAndMakeVisible(reverseLabel);

    quantLabel.setText("Quant", juce::dontSendNotification);
    quantLabel.attachToComponent(&autoStutterQuantMenu, false);
    addAndMakeVisible(quantLabel);

    // === Mix Mode Menu ===
    addAndMakeVisible(mixModeMenu);
    mixModeMenu.addItemList({ "Gate", "Insert", "Mix" }, 1);
    mixModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "MixMode", mixModeMenu);

    juce::Label* mixModeLabel = new juce::Label();
    mixModeLabel->setText("Mix Mode", juce::dontSendNotification);
    mixModeLabel->attachToComponent(&mixModeMenu, false);
    addAndMakeVisible(mixModeLabel);
    
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
    // === Repeat/Nano Blend Slider ===
    addAndMakeVisible(nanoBlendSlider);
    nanoBlendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    nanoBlendSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    nanoBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "nanoBlend", nanoBlendSlider);
    
    nanoBlendLabel.setText("Repeat/Nano", juce::dontSendNotification);
    nanoBlendLabel.attachToComponent(&nanoBlendSlider, false);
    addAndMakeVisible(nanoBlendLabel);
    
    // === Nano Tune Slider ===
    addAndMakeVisible(nanoTuneSlider);
    nanoTuneSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    nanoTuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    nanoTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "nanoTune", nanoTuneSlider);

    // === Editable Nano Ratio Numerator/Denominator Sliders ===
    for (int i = 0; i < 12; ++i)
    {
        auto* numBox = new juce::TextEditor();
        numBox->setInputRestrictions(3, "0123456789");
        numBox->setJustification(juce::Justification::centred);
        numBox->setText("1", juce::dontSendNotification);
        numBox->onFocusLost = numBox->onReturnKey = [this, i, numBox]() {
            updateNanoRatioFromFraction(i);
        };
        addAndMakeVisible(numBox);
        nanoNumerators.add(numBox);

        auto* denomBox = new juce::TextEditor();
        denomBox->setInputRestrictions(3, "0123456789");
        denomBox->setJustification(juce::Justification::centred);
        denomBox->setText("1", juce::dontSendNotification);
        denomBox->onFocusLost = denomBox->onReturnKey = [this, i, denomBox]() {
            updateNanoRatioFromFraction(i);
        };
        addAndMakeVisible(denomBox);
        nanoDenominators.add(denomBox);

        // Load initial value from parameter
        float ratioVal = audioProcessor.parameters.getRawParameterValue("nanoRatio_" + juce::String(i))->load();
        int num = static_cast<int>(std::round(ratioVal * 100));
        int denom = 100;
        int gcd = std::gcd(num, denom);
        numBox->setText(juce::String(num / gcd), juce::dontSendNotification);
        denomBox->setText(juce::String(denom / gcd), juce::dontSendNotification);
    }





    // === Nano Rate Sliders ===
    for (int i = 0; i < 12; ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        nanoRateProbSliders.add(slider);

        juce::String paramId = "nanoProb_" + juce::String(i);
        nanoRateProbAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, paramId, *slider));
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
    const int margin = 20;
    const int spacing = 10;
    const int sliderWidth = 50;
    const int sliderHeight = 110;
    const int buttonHeight = 25;
    const int knobSize = 70;

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
    
    // === Nano Rate Sliders (below main set) ===
    int nanoSliderY = buttonY + buttonHeight + spacing + 10;
    for (int i = 0; i < nanoRateProbSliders.size(); ++i)
    {
        nanoRateProbSliders[i]->setBounds(startX + i * (sliderWidth + spacing), nanoSliderY, sliderWidth, sliderHeight);
    }

    int numeratorY = nanoSliderY - 45;
    int denominatorY = nanoSliderY - 25;
    for (int i = 0; i < 12; ++i)
    {
        int x = startX + i * (sliderWidth + spacing);
        nanoNumerators[i]->setBounds(x, numeratorY, sliderWidth, 20);
        nanoDenominators[i]->setBounds(x, denominatorY, sliderWidth, 20);
    }

    // === Left-side controls ===
    juce::Rectangle<int> leftPanel(margin, sliderY, knobSize * 2 + spacing, getHeight() - sliderY - 80);
    
    // Macro Controls Column
    macroControlsLabel.setBounds(leftPanel.getX(), leftPanel.getY(), knobSize, 20);
    macroGateSlider.setBounds(leftPanel.getX(), leftPanel.getY() + 20, knobSize, knobSize);
    macroShapeSlider.setBounds(leftPanel.getX(), macroGateSlider.getBottom() + spacing, knobSize, knobSize);
    macroSmoothSlider.setBounds(leftPanel.getX(), macroShapeSlider.getBottom() + spacing, knobSize, knobSize);

    // Nano Controls Column
    int nanoColX = leftPanel.getX() + knobSize + spacing;
    nanoControlsLabel.setBounds(nanoColX, leftPanel.getY(), knobSize, 20);
    nanoGateSlider.setBounds(nanoColX, leftPanel.getY() + 20, knobSize, knobSize);
    nanoShapeSlider.setBounds(nanoColX, nanoGateSlider.getBottom() + spacing, knobSize, knobSize);
    nanoSmoothSlider.setBounds(nanoColX, nanoShapeSlider.getBottom() + spacing, knobSize, knobSize);

    // === Auto Stutter Section (Right) ===
    const int controlPanelX = getWidth() - 120;
    const int panelY = buttonY + buttonHeight + spacing;
    int controlY = panelY;

    autoStutterToggle.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 40;

    chanceLabel.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 40;

    autoStutterChanceSlider.setBounds(controlPanelX, controlY, 110, 30);  // Larger slider
    controlY += 40;

    reverseLabel.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 20;

    reverseChanceSlider.setBounds(controlPanelX, controlY, 110, 30);  // Larger slider
    controlY += 40;

    // === Quant Probability Sliders (larger, horizontal layout) ===
    const int quantSliderWidth = 35;
    const int quantSliderHeight = 80;
    const int quantSpacing = 5;
    const int numQuantSliders = quantProbSliders.size();
    const int totalQuantWidth = numQuantSliders * (quantSliderWidth + quantSpacing) - quantSpacing;
    const int quantStartX = controlPanelX + (120 - totalQuantWidth) / 2; // Center within expanded width
    
    for (int i = 0; i < numQuantSliders; ++i)
    {
        int sliderX = quantStartX + i * (quantSliderWidth + quantSpacing);
        quantProbSliders[i]->setBounds(sliderX, controlY, quantSliderWidth, quantSliderHeight);
        // Position labels below sliders
        quantProbLabels[i]->setBounds(sliderX, controlY + quantSliderHeight + 2, quantSliderWidth, 15);
    }
    controlY += quantSliderHeight + 20;

    quantLabel.setText("Quant Probabilities", juce::dontSendNotification);
    quantLabel.setBounds(controlPanelX, controlY, 110, 20);
    controlY += 25;

    // Hide the old static quantization menu since we now use dynamic probabilities
    // autoStutterQuantMenu.setBounds(controlPanelX, controlY, 110, 24);
    autoStutterQuantMenu.setVisible(false);
    // controlY += 40;

    stutterButton.setBounds(controlPanelX, controlY, 110, 24);
    controlY += 40;

    mixModeMenu.setBounds(controlPanelX, controlY, 110, 24);
    
    // === Tune & Blend Sliders ===
    int bottomControlsY = std::max(macroSmoothSlider.getBottom(), nanoSmoothSlider.getBottom());
    nanoBlendSlider.setBounds(margin, bottomControlsY + spacing, 150, 20);
    nanoTuneSlider.setBounds(margin, nanoBlendSlider.getBottom() + 10, 150, 20);

    // === Timing Offset Slider ===
    timingOffsetSlider.setBounds(margin, nanoTuneSlider.getBottom() + 10, 150, 20);
    
    // === Visualizer (Bottom) ===
    int visHeight = 70;
    visualizer.setBounds(margin, getHeight() - visHeight - margin, getWidth() - 2 * margin, visHeight);
}

void NanoStuttAudioProcessorEditor::updateNanoRatioFromFraction(int index)
{
    auto* numBox = nanoNumerators[index];
    auto* denomBox = nanoDenominators[index];

    int num = numBox->getText().getIntValue();
    int denom = denomBox->getText().getIntValue();

    if (num <= 0) num = 1;
    if (denom <= 0) denom = 1;

    double ratio = static_cast<double>(num) / denom;
    ratio = juce::jlimit(0.1, 2.0, ratio);

    auto* param = audioProcessor.parameters.getParameter("nanoRatio_" + juce::String(index));
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>((ratio - 0.1) / (2.0 - 0.1)));
}
