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

    nanoTuneLabel.setText("Nano Tune", juce::dontSendNotification);
    nanoTuneLabel.attachToComponent(&nanoTuneSlider, false);
    addAndMakeVisible(nanoTuneLabel);

    // === Waveshaper Controls ===
    addAndMakeVisible(waveshaperAlgorithmMenu);
    waveshaperAlgorithmMenu.addItem("None", 1);
    waveshaperAlgorithmMenu.addItem("Soft Clip", 2);
    waveshaperAlgorithmMenu.addItem("Tanh", 3);
    waveshaperAlgorithmMenu.addItem("Hard Clip", 4);
    waveshaperAlgorithmMenu.addItem("Tube", 5);
    waveshaperAlgorithmMenu.addItem("Asymmetric", 6);
    waveshaperAlgorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "WaveshapeAlgorithm", waveshaperAlgorithmMenu);

    addAndMakeVisible(waveshaperSlider);
    waveshaperSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    waveshaperSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    waveshaperAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "WaveshapeIntensity", waveshaperSlider);

    waveshaperLabel.setText("Waveshaper", juce::dontSendNotification);
    waveshaperLabel.attachToComponent(&waveshaperSlider, false);
    addAndMakeVisible(waveshaperLabel);

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
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Fr = Grid::Fr;
    using Px = Grid::Px;

    auto bounds = getLocalBounds();

    // === Mix Mode in top-right corner (absolute positioning) ===
    mixModeMenu.setBounds(bounds.getWidth() - 125, 5, 115, 22);

    // Create main layout grid
    Grid mainGrid;
    mainGrid.templateRows = { Track(Fr(1)), Track(Px(70)) }; // Main content + Visualizer
    mainGrid.templateColumns = {
        Track(Px(170)),  // Left panel
        Track(Fr(1)),    // Center panel
        Track(Px(140))   // Right panel
    };
    mainGrid.columnGap = Px(10);
    mainGrid.rowGap = Px(10);

    // We need to position components directly since GridItem can't contain other grids
    auto contentBounds = bounds.reduced(8).withTrimmedTop(35); // Leave space for mix mode

    // Calculate panel bounds based on grid proportions
    const int leftWidth = 170;
    const int rightWidth = 140;
    const int spacing = 10;
    const int visualizerHeight = 70;

    auto leftBounds = juce::Rectangle<int>(
        contentBounds.getX(),
        contentBounds.getY() - 35,
        leftWidth,
        contentBounds.getHeight() - visualizerHeight - spacing + 35
    );

    auto centerBounds = juce::Rectangle<int>(
        contentBounds.getX() + leftWidth + spacing,
        contentBounds.getY(),
        contentBounds.getWidth() - leftWidth - rightWidth - 2 * spacing,
        contentBounds.getHeight() - visualizerHeight - spacing
    );

    auto rightBounds = juce::Rectangle<int>(
        contentBounds.getRight() - rightWidth,
        contentBounds.getY(),
        rightWidth,
        contentBounds.getHeight() - visualizerHeight - spacing
    );

    auto visualizerBounds = juce::Rectangle<int>(
        contentBounds.getX(),
        contentBounds.getBottom() - visualizerHeight,
        contentBounds.getWidth(),
        visualizerHeight
    );

    // === LEFT PANEL: Use Grid for Envelope Controls + Utilities ===
    Grid leftPanelGrid;
    leftPanelGrid.templateRows = { Track(Fr(2)), Track(Fr(1)) }; // Envelopes take 2/3, utilities 1/3
    leftPanelGrid.templateColumns = { Track(Fr(1)) };
    leftPanelGrid.rowGap = Px(10);

    // Split left panel into envelope and utility areas
    auto envelopeBounds = leftBounds.withTrimmedBottom(leftBounds.getHeight() / 3);
    auto utilityBounds = leftBounds.withTrimmedTop(2 * leftBounds.getHeight() / 3);

    // Envelope controls using Grid
    Grid envelopeGrid;
    envelopeGrid.templateRows = {
        Track(Px(18)), Track(Px(60)), Track(Px(60)), Track(Px(2)), // Macro section with less spacing
        Track(Px(18)), Track(Px(60)), Track(Px(60))  // Nano section
    };
    envelopeGrid.templateColumns = { Track(Fr(1)), Track(Fr(1)) }; // 2 equal columns
    envelopeGrid.columnGap = Px(8);
    envelopeGrid.rowGap = Px(15); // Reduced row gap

    envelopeGrid.items = {
        GridItem(macroControlsLabel).withArea(1, 1, 1, 3), // Span both columns
        GridItem(macroGateSlider).withArea(2, 1),
        GridItem(macroShapeSlider).withArea(2, 2),
        GridItem(macroSmoothSlider).withArea(3, 1, 3, 3), // Span both columns
        GridItem(), // Empty spacer
        GridItem(nanoControlsLabel).withArea(5, 1, 5, 3), // Span both columns
        GridItem(nanoGateSlider).withArea(6, 1),
        GridItem(nanoShapeSlider).withArea(6, 2),
        GridItem(nanoSmoothSlider).withArea(7, 1, 7, 3) // Span both columns
    };
    envelopeGrid.performLayout(envelopeBounds);

    // Utilities using Grid
    Grid utilitiesGrid;
    utilitiesGrid.templateRows = {
        Track(Fr(1)), Track(Fr(1)), Track(Fr(1)), Track(Fr(1))
    };
    utilitiesGrid.templateColumns = { Track(Fr(1)) };
    utilitiesGrid.rowGap = Px(4);

    utilitiesGrid.items = {
        GridItem(nanoTuneSlider),
        GridItem(waveshaperAlgorithmMenu),
        GridItem(waveshaperSlider),
        GridItem(timingOffsetSlider)
    };
    utilitiesGrid.performLayout(utilityBounds);

    // === CENTER PANEL: Rate Probabilities using Grid ===
    Grid centerPanelGrid;
    centerPanelGrid.templateRows = {
        Track(Px(100)), // Rhythmic sliders
        Track(Px(160)), // Nano sliders - further increased height
        Track(Px(30)),  // Auto toggle - slightly bigger
        Track(Px(80))   // Quantization sliders - slightly bigger
    };
    centerPanelGrid.templateColumns = { Track(Fr(1)) };
    centerPanelGrid.rowGap = Px(10);

    // Calculate sub-bounds with proper spacing (100 + 10 + 160 + 10 + 30 + 10 + 80 = 400px total)
    auto rhythmicBounds = centerBounds.withTrimmedBottom(centerBounds.getHeight() - 100);
    auto nanoBounds = centerBounds.withTrimmedTop(110).withHeight(160);  // Full 160px height
    auto toggleBounds = centerBounds.withTrimmedTop(280).withHeight(30); // Full 30px height
    auto quantBounds = centerBounds.withTrimmedTop(320);                 // Remaining space

    // Rhythmic rate sliders using Grid
    Grid rhythmicGrid;
    rhythmicGrid.templateRows = { Track(Fr(1)) };
    rhythmicGrid.templateColumns.clear();
    for (int i = 0; i < rateProbSliders.size(); ++i)
        rhythmicGrid.templateColumns.add(Track(Fr(1)));
    rhythmicGrid.columnGap = Px(4);

    rhythmicGrid.items.clear();
    for (int i = 0; i < rateProbSliders.size(); ++i)
        rhythmicGrid.items.add(GridItem(*rateProbSliders[i]));
    rhythmicGrid.performLayout(rhythmicBounds);

    // Hide manual stutter buttons
    for (int i = 0; i < manualStutterButtons.size(); ++i)
        manualStutterButtons[i]->setVisible(false);

    // Nano rate sliders with numerator/denominator using Grid
    Grid nanoGrid;
    nanoGrid.templateRows = { Track(Px(20)), Track(Px(20)), Track(Px(110)) }; // Num + Den + Slider - using full 150px of 160px bounds
    nanoGrid.templateColumns.clear();
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
        nanoGrid.templateColumns.add(Track(Fr(1)));
    nanoGrid.columnGap = Px(3);
    nanoGrid.rowGap = Px(0);

    nanoGrid.items.clear();
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
    {
        nanoGrid.items.add(GridItem(*nanoNumerators[i]).withArea(1, i + 1));
        nanoGrid.items.add(GridItem(*nanoDenominators[i]).withArea(2, i + 1));
        nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(3, i + 1));
    }
    nanoGrid.performLayout(nanoBounds);

    // Auto stutter toggle (center it)
    autoStutterToggle.setBounds(toggleBounds.withSizeKeepingCentre(100, 22));

    // Quantization sliders using Grid
    Grid quantGrid;
    quantGrid.templateRows = { Track(Px(55)), Track(Px(12)) }; // Sliders + Labels
    quantGrid.templateColumns.clear();
    for (int i = 0; i < quantProbSliders.size(); ++i)
        quantGrid.templateColumns.add(Track(Fr(1)));
    quantGrid.columnGap = Px(6);

    quantGrid.items.clear();
    for (int i = 0; i < quantProbSliders.size(); ++i)
    {
        quantGrid.items.add(GridItem(*quantProbSliders[i]).withArea(1, i + 1));
        quantGrid.items.add(GridItem(*quantProbLabels[i]).withArea(2, i + 1));
    }
    quantGrid.performLayout(quantBounds);

    // === RIGHT PANEL: Chance, Repeat/Nano, Reverse using Grid ===
    Grid rightGrid;
    rightGrid.templateRows = {
        Track(Px(20)), Track(Px(30)), Track(Px(8)),  // Chance
        Track(Px(20)), Track(Px(30)), Track(Px(8)),  // Repeat/Nano
        Track(Px(20)), Track(Px(30)), Track(Px(15)), // Reverse
        Track(Fr(1)), Track(Px(30))   // Flexible space + stutter button
    };
    rightGrid.templateColumns = { Track(Fr(1)) };
    rightGrid.rowGap = Px(4);

    rightGrid.items = {
        GridItem(chanceLabel),
        GridItem(autoStutterChanceSlider),
        GridItem(), // Spacer
        GridItem(nanoBlendLabel),
        GridItem(nanoBlendSlider),
        GridItem(), // Spacer
        GridItem(reverseLabel),
        GridItem(reverseChanceSlider),
        GridItem(), // Spacer
        GridItem(), // Flexible spacer
        GridItem(stutterButton)
    };
    rightGrid.performLayout(rightBounds);

    // === VISUALIZER ===
    visualizer.setBounds(visualizerBounds);

    // Hide unused components
    autoStutterQuantMenu.setVisible(false);
    quantLabel.setVisible(false);

    // Set minimum size for better layout
    setResizeLimits(1000, 650, 1600, 1000);
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
