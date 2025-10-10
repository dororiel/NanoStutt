/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NanoStuttAudioProcessorEditor::NanoStuttAudioProcessorEditor (NanoStuttAudioProcessor& p)
: AudioProcessorEditor (&p), autoStutterIndicator(p), visualizer(p), tuner(p), audioProcessor (p)
{
    // === Manual Stutter Button === //
    addAndMakeVisible(stutterButton);
    stutterButton.setButtonText("Stutter");

    stutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(),
        "stutterOn",
        stutterButton);
    
    // === Auto Stutter Indicator ===
    addAndMakeVisible(autoStutterIndicator);

    // === Auto Stutter Chance Slider ===
    addAndMakeVisible(autoStutterChanceSlider);
    autoStutterChanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    autoStutterChanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    autoStutterChanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "autoStutterChance", autoStutterChanceSlider);

    // === Reverse Chance Slider ===
    addAndMakeVisible(reverseChanceSlider);
    reverseChanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverseChanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    reverseChanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "reverseChance", reverseChanceSlider);

    // === Quantization Menu ===
    addAndMakeVisible(autoStutterQuantMenu);
    autoStutterQuantMenu.addItem("1/4", 1);
    autoStutterQuantMenu.addItem("1/8", 2);
    autoStutterQuantMenu.addItem("1/16", 3);
    autoStutterQuantMenu.addItem("1/32", 4);

    autoStutterQuantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "autoStutterQuant", autoStutterQuantMenu);

    // === Envelope Controls ===
    auto setupKnob = [this] (juce::Slider& slider, const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment)
    {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getParameters(), paramID, slider);
    };

    // Setup DualSliders for NanoGate and NanoShape with randomization
    addAndMakeVisible(nanoGateDualSlider);
    nanoGateDualSlider.setDefaultValues(1.0, 0.0);  // NanoGate default: 1.0, Random default: 0.0
    nanoGateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoGate", nanoGateDualSlider.getMainSlider());
    nanoGateRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoGateRandom", nanoGateDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for NanoGate
    nanoGateBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("NanoGateRandomBipolar"),
        [this](float newValue) {
            nanoGateDualSlider.setBipolarMode(newValue > 0.5f);
        });

    // Set initial state
    nanoGateDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("NanoGateRandomBipolar")->load() > 0.5f);

    // Listen for changes from UI (right-click toggle)
    nanoGateDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("NanoGateRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // Setup DualSlider for NanoShape with randomization
    addAndMakeVisible(nanoShapeDualSlider);
    nanoShapeDualSlider.setDefaultValues(0.5, 0.0);  // NanoShape default: 0.5, Random default: 0.0
    nanoShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoShape", nanoShapeDualSlider.getMainSlider());
    nanoShapeRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoShapeRandom", nanoShapeDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for NanoShape
    nanoShapeBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("NanoShapeRandomBipolar"),
        [this](float newValue) {
            nanoShapeDualSlider.setBipolarMode(newValue > 0.5f);
        });

    // Set initial state
    nanoShapeDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("NanoShapeRandomBipolar")->load() > 0.5f);

    // Listen for changes from UI (right-click toggle)
    nanoShapeDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("NanoShapeRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // NanoSmooth remains a regular slider (no randomization)
    setupKnob(nanoSmoothSlider, "NanoSmooth", nanoSmoothAttachment);

    // Setup DualSliders for MacroGate and MacroShape with randomization
    addAndMakeVisible(macroGateDualSlider);
    macroGateDualSlider.setDefaultValues(1.0, 0.0);  // MacroGate default: 1.0, Random default: 0.0
    macroGateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroGate", macroGateDualSlider.getMainSlider());
    macroGateRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroGateRandom", macroGateDualSlider.getRandomSlider());

    addAndMakeVisible(macroShapeDualSlider);
    macroShapeDualSlider.setDefaultValues(0.5, 0.0);  // MacroShape default: 0.5, Random default: 0.0
    macroShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroShape", macroShapeDualSlider.getMainSlider());
    macroShapeRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroShapeRandom", macroShapeDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for MacroGate
    macroGateBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("MacroGateRandomBipolar"),
        [this](float newValue) {
            macroGateDualSlider.setBipolarMode(newValue > 0.5f);
        });

    // Set initial state
    macroGateDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("MacroGateRandomBipolar")->load() > 0.5f);

    // Listen for changes from UI (right-click toggle)
    macroGateDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("MacroGateRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // Setup bipolar state synchronization for MacroShape
    macroShapeBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("MacroShapeRandomBipolar"),
        [this](float newValue) {
            macroShapeDualSlider.setBipolarMode(newValue > 0.5f);
        });

    // Set initial state
    macroShapeDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("MacroShapeRandomBipolar")->load() > 0.5f);

    // Listen for changes from UI (right-click toggle)
    macroShapeDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("MacroShapeRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    setupKnob(macroSmoothSlider, "MacroSmooth", macroSmoothAttachment);

    // === Timing Offset Slider ===
    addAndMakeVisible(timingOffsetSlider);
    timingOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timingOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    timingOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "TimingOffset", timingOffsetSlider);

    // === Labels ===
    auto setupLabel = [this] (juce::Label& label, const juce::String& text, juce::Component& component)
    {
        label.setText(text, juce::dontSendNotification);
        label.attachToComponent(&component, false);
        label.setJustificationType(juce::Justification::centredBottom);
        addAndMakeVisible(label);
    };

    setupLabel(nanoGateLabel, "Gate", nanoGateDualSlider);
    setupLabel(nanoShapeLabel, "Shape", nanoShapeDualSlider);
    setupLabel(nanoSmoothLabel, "Smooth", nanoSmoothSlider);
    setupLabel(macroGateLabel, "Gate", macroGateDualSlider);
    setupLabel(macroShapeLabel, "Shape", macroShapeDualSlider);
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
        label->setJustificationType(juce::Justification::centred);
        label->attachToComponent(slider, false);
        addAndMakeVisible(label);
        rateProbLabels.add(label);

        juce::String paramId = "rateProb_" + rateLabels[i];
        rateProbAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), paramId, *slider));
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
            audioProcessor.getParameters(), paramId, *slider));
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
        audioProcessor.getParameters(), "MixMode", mixModeMenu);

    mixModeLabel = std::make_unique<juce::Label>();
    mixModeLabel->setText("Mix Mode", juce::dontSendNotification);
    mixModeLabel->attachToComponent(&mixModeMenu, false);
    addAndMakeVisible(mixModeLabel.get());
    
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

                audioProcessor.setManualStutterRate(rate);
                audioProcessor.setManualStutterTriggered(true);
                audioProcessor.setAutoStutterActive(false);
            }
            else
            {
                audioProcessor.setManualStutterRate(-1);
                audioProcessor.setManualStutterTriggered(false);
                audioProcessor.setAutoStutterActive(false);
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
        audioProcessor.getParameters(), "nanoBlend", nanoBlendSlider);

    nanoBlendLabel.setText("Repeat/Nano", juce::dontSendNotification);
    nanoBlendLabel.attachToComponent(&nanoBlendSlider, false);
    addAndMakeVisible(nanoBlendLabel);

    // === Nano Tune Slider ===
    addAndMakeVisible(nanoTuneSlider);
    nanoTuneSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    nanoTuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    nanoTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "nanoTune", nanoTuneSlider);

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
    waveshaperAlgorithmMenu.addItem("Fold", 6);
    waveshaperAlgorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "WaveshapeAlgorithm", waveshaperAlgorithmMenu);

    addAndMakeVisible(waveshaperSlider);
    waveshaperSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    waveshaperSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    waveshaperAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "Drive", waveshaperSlider);

    waveshaperLabel.setText("Drive", juce::dontSendNotification);
    waveshaperLabel.attachToComponent(&waveshaperSlider, false);
    addAndMakeVisible(waveshaperLabel);

    // === Gain Compensation Toggle ===
    addAndMakeVisible(gainCompensationToggle);
    gainCompensationToggle.setButtonText("Gain Comp");
    gainCompensationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), "GainCompensation", gainCompensationToggle);

    // === Advanced View Toggle ===
    addAndMakeVisible(advancedViewToggle);
    advancedViewToggle.setButtonText("Advanced View");
    advancedViewToggle.onClick = [this]() {
        showAdvancedView = !showAdvancedView;
        resized();
    };

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
        float ratioVal = audioProcessor.getParameters().getRawParameterValue("nanoRatio_" + juce::String(i))->load();
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
            audioProcessor.getParameters(), paramId, *slider));
    }

    // === Nano Interval Labels (Roman Numerals) ===
    auto intervalLabels = juce::StringArray { "I", "ii", "II", "iii", "III", "IV", "#iv", "V", "vi", "VI", "vii", "VII" };
    for (int i = 0; i < 12; ++i)
    {
        auto* label = new juce::Label();
        label->setText(intervalLabels[i], juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        nanoIntervalLabels.add(label);
    }






    addAndMakeVisible(visualizer);
    addAndMakeVisible(tuner);

    setResizeLimits(800, 400, 1200, 600);
    setSize(900, 450);
    setResizable(true, true);


}


NanoStuttAudioProcessorEditor::~NanoStuttAudioProcessorEditor()
{
    // unique_ptr handles cleanup automatically
}

//==============================================================================
void NanoStuttAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);

    // Draw colored backgrounds and borders around slider sections
    // Orange section for regular rate sliders
    if (!rhythmicSlidersBounds.isEmpty())
    {
        // Draw semi-transparent fill first
        g.setColour(juce::Colour(0xffff9933).withAlpha(0.08f));
        g.fillRect(rhythmicSlidersBounds.toFloat());

        // Draw border on top
        g.setColour(juce::Colour(0xffff9933));  // Orange (matches regular stutter indicator)
        g.drawRect(rhythmicSlidersBounds.toFloat(), 2.0f);
    }

    // Purple section for nano sliders
    if (!nanoSlidersBounds.isEmpty())
    {
        // Draw semi-transparent fill first
        g.setColour(juce::Colour(0xff9966ff).withAlpha(0.08f));
        g.fillRect(nanoSlidersBounds.toFloat());

        // Draw border on top
        g.setColour(juce::Colour(0xff9966ff));  // Purple (matches nano stutter indicator)
        g.drawRect(nanoSlidersBounds.toFloat(), 2.0f);
    }
}

void NanoStuttAudioProcessorEditor::layoutEnvelopeControls(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    Grid envelopeGrid;
    envelopeGrid.templateRows = {
        Track(Px(18)), Track(Px(60)), Track(Px(60)), Track(Px(2)), // Macro section with less spacing
        Track(Px(18)), Track(Px(60)), Track(Px(60))  // Nano section
    };
    envelopeGrid.templateColumns = { Track(Fr(1)), Track(Fr(1)) }; // 2 equal columns
    envelopeGrid.columnGap = Px(8);
    envelopeGrid.rowGap = Px(15);

    envelopeGrid.items = {
        GridItem(macroControlsLabel).withArea(1, 1, 1, 3),
        GridItem(macroGateDualSlider).withArea(2, 1),
        GridItem(macroShapeDualSlider).withArea(2, 2),
        GridItem(macroSmoothSlider).withArea(3, 1, 3, 3),
        GridItem(),
        GridItem(nanoControlsLabel).withArea(5, 1, 5, 3),
        GridItem(nanoGateDualSlider).withArea(6, 1),
        GridItem(nanoShapeDualSlider).withArea(6, 2),
        GridItem(nanoSmoothSlider).withArea(7, 1, 7, 3)
    };
    envelopeGrid.performLayout(bounds);
}

void NanoStuttAudioProcessorEditor::layoutRateSliders(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    Grid rhythmicGrid;
    rhythmicGrid.templateRows = { Track(Fr(1)) };
    rhythmicGrid.templateColumns.clear();
    for (int i = 0; i < rateProbSliders.size(); ++i)
        rhythmicGrid.templateColumns.add(Track(Fr(1)));
    rhythmicGrid.columnGap = Px(4);

    rhythmicGrid.items.clear();
    for (int i = 0; i < rateProbSliders.size(); ++i)
        rhythmicGrid.items.add(GridItem(*rateProbSliders[i]));
    rhythmicGrid.performLayout(bounds);

    // Hide manual stutter buttons
    for (int i = 0; i < manualStutterButtons.size(); ++i)
        manualStutterButtons[i]->setVisible(false);
}

void NanoStuttAudioProcessorEditor::layoutNanoControls(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    Grid nanoGrid;

    // Set up grid rows based on advanced view state
    if (showAdvancedView)
    {
        // Advanced view: numerators, denominators, sliders, labels
        nanoGrid.templateRows = { Track(Px(20)), Track(Px(20)), Track(Px(90)), Track(Px(15)) };
    }
    else
    {
        // Simple view: sliders and labels only
        nanoGrid.templateRows = { Track(Px(110)), Track(Px(15)) };
    }

    nanoGrid.templateColumns.clear();
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
        nanoGrid.templateColumns.add(Track(Fr(1)));
    nanoGrid.columnGap = Px(3);
    nanoGrid.rowGap = Px(2);

    nanoGrid.items.clear();
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
    {
        if (showAdvancedView)
        {
            // Show numerators and denominators
            nanoNumerators[i]->setVisible(true);
            nanoDenominators[i]->setVisible(true);

            nanoGrid.items.add(GridItem(*nanoNumerators[i]).withArea(1, i + 1));
            nanoGrid.items.add(GridItem(*nanoDenominators[i]).withArea(2, i + 1));
            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(3, i + 1));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i]).withArea(4, i + 1));
        }
        else
        {
            // Hide numerators and denominators
            nanoNumerators[i]->setVisible(false);
            nanoDenominators[i]->setVisible(false);

            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(1, i + 1));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i]).withArea(2, i + 1));
        }
    }
    nanoGrid.performLayout(bounds);
}

void NanoStuttAudioProcessorEditor::layoutQuantizationControls(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    Grid quantGrid;
    quantGrid.templateRows = { Track(Px(55)), Track(Px(12)) };
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
    quantGrid.performLayout(bounds);
}

void NanoStuttAudioProcessorEditor::layoutRightPanel(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    auto rightMainBounds = bounds.withTrimmedBottom(bounds.getHeight() / 2);
    auto rightUtilityBounds = bounds.withTrimmedTop(bounds.getHeight() / 2).withTrimmedTop(10);

    // Main right panel controls
    Grid rightMainGrid;
    rightMainGrid.templateRows = {
        Track(Px(20)), Track(Px(30)), Track(Px(8)),
        Track(Px(20)), Track(Px(30)), Track(Px(8)),
        Track(Px(20)), Track(Px(30))
    };
    rightMainGrid.templateColumns = { Track(Fr(1)) };
    rightMainGrid.rowGap = Px(4);

    rightMainGrid.items = {
        GridItem(chanceLabel),
        GridItem(autoStutterChanceSlider),
        GridItem(),
        GridItem(nanoBlendLabel),
        GridItem(nanoBlendSlider),
        GridItem(),
        GridItem(reverseLabel),
        GridItem(reverseChanceSlider)
    };
    rightMainGrid.performLayout(rightMainBounds);

    // Utilities grid
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
        GridItem(gainCompensationToggle),
        GridItem(timingOffsetSlider)
    };
    utilitiesGrid.performLayout(rightUtilityBounds);
}

void NanoStuttAudioProcessorEditor::layoutVisualizer(juce::Rectangle<int> bounds)
{
    visualizer.setBounds(bounds);
}

void NanoStuttAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === Top-right corner controls (absolute positioning) ===
    autoStutterIndicator.setBounds(bounds.getWidth() - 158, 5, 28, 22);
    mixModeMenu.setBounds(bounds.getWidth() - 125, 5, 115, 22);

    // Calculate main layout areas
    auto contentBounds = bounds.reduced(8).withTrimmedTop(35); // Leave space for top controls

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

    // Call layout helper methods
    layoutEnvelopeControls(leftBounds);

    // Center panel: Rate sliders, Nano controls, and Quantization
    auto rhythmicBounds = centerBounds.withTrimmedBottom(centerBounds.getHeight() - 100);
    // Expand bounds to include labels above sliders (attached with attachToComponent)
    // Move border up and closer to labels
    rhythmicSlidersBounds = rhythmicBounds.expanded(3, 0).withTop(rhythmicBounds.getY() - 20).withBottom(rhythmicBounds.getBottom() + 2);

    // Advanced view toggle positioned above nano controls
    auto advancedToggleBounds = centerBounds.withTrimmedTop(100).withHeight(22);
    advancedViewToggle.setBounds(advancedToggleBounds.withSizeKeepingCentre(120, 22));

    auto nanoBounds = centerBounds.withTrimmedTop(130).withHeight(showAdvancedView ? 145 : 140);
    // Expand bounds to include all nano elements (numerators at top when advanced, labels at bottom)
    // Conditional sizing to prevent overlap with quant area in advanced view
    if (showAdvancedView) {
        // More top spacing for numerators, less bottom to avoid overlap
        nanoSlidersBounds = nanoBounds.expanded(3, 0).withTop(nanoBounds.getY() - 10).withBottom(nanoBounds.getBottom() + 6);
    } else {
        // Less room at bottom for interval labels only
        nanoSlidersBounds = nanoBounds.expanded(3, 0).withTop(nanoBounds.getY() - 2).withBottom(nanoBounds.getBottom() + 8);
    }

    auto quantBounds = centerBounds.withTrimmedTop(showAdvancedView ? 285 : 280).withHeight(70);
    auto tunerBounds = centerBounds.withTrimmedTop(showAdvancedView ? 360 : 355).withHeight(35);

    layoutRateSliders(rhythmicBounds);
    layoutNanoControls(nanoBounds);
    layoutQuantizationControls(quantBounds);
    tuner.setBounds(tunerBounds);

    layoutRightPanel(rightBounds);
    layoutVisualizer(visualizerBounds);

    // Hide unused components
    autoStutterQuantMenu.setVisible(false);
    quantLabel.setVisible(false);
    stutterButton.setVisible(false);

    // Set minimum size for better layout
    setResizeLimits(1000, 550, 1600, 900);
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

    auto* param = audioProcessor.getParameters().getParameter("nanoRatio_" + juce::String(index));
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>((ratio - 0.1) / (2.0 - 0.1)));
}
