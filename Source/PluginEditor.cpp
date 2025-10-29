/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>  // for std::shuffle
#include <random>     // for std::default_random_engine
#include <vector>     // for std::vector

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

    // === Section Labels ===
    repeatRatesLabel.setText("Repeat Rates", juce::dontSendNotification);
    repeatRatesLabel.setJustificationType(juce::Justification::centred);
    repeatRatesLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9933)); // Orange
    addAndMakeVisible(repeatRatesLabel);

    nanoRatesLabel.setText("Nano Rates", juce::dontSendNotification);
    nanoRatesLabel.setJustificationType(juce::Justification::centred);
    nanoRatesLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9966ff)); // Purple
    addAndMakeVisible(nanoRatesLabel);

    quantizationLabel.setText("Quantization", juce::dontSendNotification);
    quantizationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(quantizationLabel);
    
    // === Rate Sliders and buttons ===
    auto rateLabels = juce::StringArray { "1bar", "3/4", "1/2", "1/3", "1/4", "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32" };
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
        addAndMakeVisible(label);
        rateProbLabels.add(label);

        juce::String paramId = "rateProb_" + rateLabels[i];
        rateProbAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), paramId, *slider));

        // Create visibility toggle button (eye icon)
        auto* toggleButton = new juce::TextButton();
        toggleButton->setButtonText(juce::CharPointer_UTF8("\xf0\x9f\x91\x81")); // 👁 emoji
        toggleButton->setClickingTogglesState(true);
        toggleButton->onClick = [this]() { resized(); };
        addAndMakeVisible(toggleButton);
        rateActiveButtons.add(toggleButton);

        juce::String activeParamId = "rateActive_" + rateLabels[i];
        rateActiveAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.getParameters(), activeParamId, *toggleButton));
    }
    
    // === Quant Probability Sliders ===
    auto quantLabels = juce::StringArray { "4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32" };
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

        // Create visibility toggle button (eye icon)
        auto* toggleButton = new juce::TextButton();
        toggleButton->setButtonText(juce::CharPointer_UTF8("\xf0\x9f\x91\x81")); // 👁 emoji
        toggleButton->setClickingTogglesState(true);
        toggleButton->onClick = [this]() { resized(); };
        addAndMakeVisible(toggleButton);
        quantActiveButtons.add(toggleButton);

        juce::String activeParamId = "quantActive_" + quantLabels[i];
        quantActiveAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.getParameters(), activeParamId, *toggleButton));
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

        // Auto-resize window for better fit in advanced view
        const int currentWidth = getWidth();
        if (showAdvancedView) {
            // Entering advanced view: increase height for extra content
            setSize(currentWidth, 650);  // From 450 to 680 (+230px)
        } else {
            // Exiting advanced view: restore original height
            setSize(currentWidth, 430);
        }

        resized();
        repaint();  // Force repaint to update borders immediately
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

        // Create visibility toggle button (eye icon)
        auto* toggleButton = new juce::TextButton();
        toggleButton->setButtonText(juce::CharPointer_UTF8("\xf0\x9f\x91\x81")); // 👁 emoji
        toggleButton->setClickingTogglesState(true);
        toggleButton->onClick = [this]() { resized(); };
        addAndMakeVisible(toggleButton);
        nanoActiveButtons.add(toggleButton);

        juce::String activeParamId = "nanoActive_" + juce::String(i);
        nanoActiveAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.getParameters(), activeParamId, *toggleButton));
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

    // === Reset Buttons ===
    addAndMakeVisible(resetRateProbButton);
    resetRateProbButton.setButtonText("Reset");
    resetRateProbButton.onClick = [this, rateLabels]() {
        for (int i = 0; i < rateProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("rateProb_" + rateLabels[i]))
                param->setValueNotifyingHost(0.0f);
        }
    };

    addAndMakeVisible(resetNanoProbButton);
    resetNanoProbButton.setButtonText("Reset");
    resetNanoProbButton.onClick = [this]() {
        for (int i = 0; i < nanoRateProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("nanoProb_" + juce::String(i)))
                param->setValueNotifyingHost(0.0f);
        }
    };

    addAndMakeVisible(resetQuantProbButton);
    resetQuantProbButton.setButtonText("Reset");
    resetQuantProbButton.onClick = [this, quantLabels]() {
        for (int i = 0; i < quantProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("quantProb_" + quantLabels[i]))
                param->setValueNotifyingHost(0.0f);
        }
    };

    // === Randomize Buttons ===
    addAndMakeVisible(randomizeRateProbButton);
    randomizeRateProbButton.setButtonText("Random");
    randomizeRateProbButton.onClick = [this, rateLabels]() {
        // Pick random number of sliders (3-5)
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(3, 6)); // 3, 4, or 5

        // Create shuffled indices
        std::vector<int> indices;
        for (int i = 0; i < rateProbSliders.size(); ++i)
            indices.push_back(i);
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine(juce::Random::getSystemRandom().nextInt()));

        // Set first numToRandomize sliders to random values, rest to 0
        for (int i = 0; i < rateProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("rateProb_" + rateLabels[indices[i]]))
            {
                if (i < numToRandomize)
                    param->setValueNotifyingHost(0.3f + juce::Random::getSystemRandom().nextFloat() * 0.7f); // 0.3 to 1.0
                else
                    param->setValueNotifyingHost(0.0f);
            }
        }
    };

    addAndMakeVisible(randomizeNanoProbButton);
    randomizeNanoProbButton.setButtonText("Random");
    randomizeNanoProbButton.onClick = [this]() {
        // Pick random number of sliders (3-5)
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(3, 6));

        // Create shuffled indices
        std::vector<int> indices;
        for (int i = 0; i < nanoRateProbSliders.size(); ++i)
            indices.push_back(i);
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine(juce::Random::getSystemRandom().nextInt()));

        // Set first numToRandomize sliders to random values, rest to 0
        for (int i = 0; i < nanoRateProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("nanoProb_" + juce::String(indices[i])))
            {
                if (i < numToRandomize)
                    param->setValueNotifyingHost(0.3f + juce::Random::getSystemRandom().nextFloat() * 0.7f);
                else
                    param->setValueNotifyingHost(0.0f);
            }
        }
    };

    addAndMakeVisible(randomizeQuantProbButton);
    randomizeQuantProbButton.setButtonText("Random");
    randomizeQuantProbButton.onClick = [this, quantLabels]() {
        // Pick random number of sliders (3-5)
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(3, 6));

        // Create shuffled indices
        std::vector<int> indices;
        for (int i = 0; i < quantProbSliders.size(); ++i)
            indices.push_back(i);
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine(juce::Random::getSystemRandom().nextInt()));

        // Set first numToRandomize sliders to random values, rest to 0
        for (int i = 0; i < quantProbSliders.size(); ++i)
        {
            if (auto* param = audioProcessor.getParameters().getParameter("quantProb_" + quantLabels[indices[i]]))
            {
                if (i < numToRandomize)
                    param->setValueNotifyingHost(0.3f + juce::Random::getSystemRandom().nextFloat() * 0.7f);
                else
                    param->setValueNotifyingHost(0.0f);
            }
        }
    };


    addAndMakeVisible(visualizer);
    addAndMakeVisible(tuner);

    setResizeLimits(800, 400, 1200, 600);
    setSize(900, 430);
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
    // Grid rows depend on view mode
    if (showAdvancedView) {
        // Advanced: toggles (20) + sliders (90) + labels (15)
        rhythmicGrid.templateRows = { Track(Px(20)), Track(Px(90)), Track(Px(15)) };
    } else {
        // Simple: sliders (90) + labels (15)
        rhythmicGrid.templateRows = { Track(Px(90)), Track(Px(15)) };
    }

    // Determine which sliders are active
    auto rateLabels = juce::StringArray { "1bar", "3/4", "1/2", "1/3", "1/4", "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32" };
    std::vector<bool> activeStates;
    for (int i = 0; i < rateProbSliders.size(); ++i)
    {
        bool isActive = audioProcessor.getParameters().getRawParameterValue("rateActive_" + rateLabels[i])->load() > 0.5f;
        activeStates.push_back(isActive);
    }

    // Set up columns based on view mode
    rhythmicGrid.templateColumns.clear();
    if (showAdvancedView)
    {
        // Advanced view: show all sliders
        for (int i = 0; i < rateProbSliders.size(); ++i)
            rhythmicGrid.templateColumns.add(Track(Fr(1)));
    }
    else
    {
        // Simple view: only active sliders
        for (int i = 0; i < rateProbSliders.size(); ++i)
            if (activeStates[i])
                rhythmicGrid.templateColumns.add(Track(Fr(1)));
    }
    rhythmicGrid.columnGap = Px(4);
    rhythmicGrid.rowGap = Px(4);  // Add spacing between rows

    // Add grid items
    rhythmicGrid.items.clear();
    int columnIndex = 1;
    for (int i = 0; i < rateProbSliders.size(); ++i)
    {
        if (showAdvancedView)
        {
            // Advanced view: show all, grey out inactive
            rateActiveButtons[i]->setVisible(true);
            rateProbSliders[i]->setVisible(true);
            rateProbLabels[i]->setVisible(true);

            if (!activeStates[i])
            {
                rateProbSliders[i]->setAlpha(0.4f);
                rateProbSliders[i]->setEnabled(false);
            }
            else
            {
                rateProbSliders[i]->setAlpha(1.0f);
                rateProbSliders[i]->setEnabled(true);
            }

            rhythmicGrid.items.add(GridItem(*rateActiveButtons[i]).withArea(1, columnIndex));
            rhythmicGrid.items.add(GridItem(*rateProbSliders[i]).withArea(2, columnIndex));
            rhythmicGrid.items.add(GridItem(*rateProbLabels[i]).withArea(3, columnIndex));
            columnIndex++;
        }
        else if (activeStates[i])
        {
            // Simple view: only active sliders (NO toggles)
            rateActiveButtons[i]->setVisible(false);  // Hide toggles in simple view
            rateProbSliders[i]->setVisible(true);
            rateProbSliders[i]->setAlpha(1.0f);
            rateProbSliders[i]->setEnabled(true);
            rateProbLabels[i]->setVisible(true);

            // No toggle row in simple view - sliders start at row 1
            rhythmicGrid.items.add(GridItem(*rateProbSliders[i]).withArea(1, columnIndex));
            rhythmicGrid.items.add(GridItem(*rateProbLabels[i]).withArea(2, columnIndex));
            columnIndex++;
        }
        else
        {
            // Hide inactive sliders in simple view
            rateActiveButtons[i]->setVisible(false);
            rateProbSliders[i]->setVisible(false);
            rateProbLabels[i]->setVisible(false);
        }
    }

    // Only perform layout if we have valid configuration
    if (rhythmicGrid.templateColumns.size() > 0 &&
        rhythmicGrid.items.size() > 0 &&
        bounds.getWidth() > 0 &&
        bounds.getHeight() > 0)
    {
        rhythmicGrid.performLayout(bounds);
    }

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

    // Determine which sliders are active
    std::vector<bool> activeStates;
    for (int i = 0; i < 12; ++i)
    {
        bool isActive = audioProcessor.getParameters().getRawParameterValue("nanoActive_" + juce::String(i))->load() > 0.5f;
        activeStates.push_back(isActive);
    }

    // Set up grid rows based on advanced view state
    if (showAdvancedView)
    {
        // Advanced view: toggles (20) + numerators (20) + denominators (20) + sliders (90) + labels (15)
        nanoGrid.templateRows = { Track(Px(20)), Track(Px(20)), Track(Px(20)), Track(Px(90)), Track(Px(15)) };
    }
    else
    {
        // Simple view: sliders (90) + labels (15) - NO toggles
        nanoGrid.templateRows = { Track(Px(90)), Track(Px(15)) };
    }

    // Set up columns based on view mode
    nanoGrid.templateColumns.clear();
    if (showAdvancedView)
    {
        // Advanced view: show all sliders
        for (int i = 0; i < 12; ++i)
            nanoGrid.templateColumns.add(Track(Fr(1)));
    }
    else
    {
        // Simple view: only active sliders
        for (int i = 0; i < 12; ++i)
            if (activeStates[i])
                nanoGrid.templateColumns.add(Track(Fr(1)));
    }
    nanoGrid.columnGap = Px(3);
    nanoGrid.rowGap = Px(4);  // Increased spacing between rows

    // Add grid items
    nanoGrid.items.clear();
    int columnIndex = 1;
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
    {
        if (showAdvancedView)
        {
            // Advanced view: show all, grey out inactive
            nanoActiveButtons[i]->setVisible(true);
            nanoNumerators[i]->setVisible(true);
            nanoDenominators[i]->setVisible(true);
            nanoRateProbSliders[i]->setVisible(true);
            nanoIntervalLabels[i]->setVisible(true);

            if (!activeStates[i])
            {
                nanoRateProbSliders[i]->setAlpha(0.4f);
                nanoRateProbSliders[i]->setEnabled(false);
                nanoNumerators[i]->setEnabled(false);
                nanoDenominators[i]->setEnabled(false);
            }
            else
            {
                nanoRateProbSliders[i]->setAlpha(1.0f);
                nanoRateProbSliders[i]->setEnabled(true);
                nanoNumerators[i]->setEnabled(true);
                nanoDenominators[i]->setEnabled(true);
            }

            nanoGrid.items.add(GridItem(*nanoActiveButtons[i]).withArea(1, columnIndex));
            nanoGrid.items.add(GridItem(*nanoNumerators[i]).withArea(2, columnIndex));
            nanoGrid.items.add(GridItem(*nanoDenominators[i]).withArea(3, columnIndex));
            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(4, columnIndex));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i]).withArea(5, columnIndex));
            columnIndex++;
        }
        else if (activeStates[i])
        {
            // Simple view: only active sliders (NO toggles)
            nanoActiveButtons[i]->setVisible(false);  // Hide toggles in simple view
            nanoNumerators[i]->setVisible(false);
            nanoDenominators[i]->setVisible(false);
            nanoRateProbSliders[i]->setVisible(true);
            nanoRateProbSliders[i]->setAlpha(1.0f);
            nanoRateProbSliders[i]->setEnabled(true);
            nanoIntervalLabels[i]->setVisible(true);

            // No toggle row in simple view - sliders start at row 1
            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(1, columnIndex));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i]).withArea(2, columnIndex));
            columnIndex++;
        }
        else
        {
            // Hide inactive sliders in simple view
            nanoActiveButtons[i]->setVisible(false);
            nanoNumerators[i]->setVisible(false);
            nanoDenominators[i]->setVisible(false);
            nanoRateProbSliders[i]->setVisible(false);
            nanoIntervalLabels[i]->setVisible(false);
        }
    }

    // Only perform layout if we have valid configuration
    if (nanoGrid.templateColumns.size() > 0 &&
        nanoGrid.items.size() > 0 &&
        bounds.getWidth() > 0 &&
        bounds.getHeight() > 0)
    {
        nanoGrid.performLayout(bounds);
    }
}

void NanoStuttAudioProcessorEditor::layoutQuantizationControls(juce::Rectangle<int> bounds)
{
    using Grid = juce::Grid;
    using GridItem = juce::GridItem;
    using Track = Grid::TrackInfo;
    using Px = Grid::Px;
    using Fr = Grid::Fr;

    Grid quantGrid;
    // Grid rows depend on view mode
    if (showAdvancedView) {
        // Advanced: toggles (20) + sliders (90) + labels (12)
        quantGrid.templateRows = { Track(Px(20)), Track(Px(90)), Track(Px(12)) };
    } else {
        // Simple: sliders (90) + labels (12) - NO toggles
        quantGrid.templateRows = { Track(Px(90)), Track(Px(12)) };
    }

    // Determine which sliders are active
    auto quantLabels = juce::StringArray { "4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32" };
    std::vector<bool> activeStates;
    for (int i = 0; i < quantProbSliders.size(); ++i)
    {
        bool isActive = audioProcessor.getParameters().getRawParameterValue("quantActive_" + quantLabels[i])->load() > 0.5f;
        activeStates.push_back(isActive);
    }

    // Set up columns based on view mode
    quantGrid.templateColumns.clear();
    if (showAdvancedView)
    {
        // Advanced view: show all sliders
        for (int i = 0; i < quantProbSliders.size(); ++i)
            quantGrid.templateColumns.add(Track(Fr(1)));
    }
    else
    {
        // Simple view: only active sliders
        for (int i = 0; i < quantProbSliders.size(); ++i)
            if (activeStates[i])
                quantGrid.templateColumns.add(Track(Fr(1)));
    }
    quantGrid.columnGap = Px(6);
    quantGrid.rowGap = Px(4);  // Add spacing between rows

    // Add grid items
    quantGrid.items.clear();
    int columnIndex = 1;
    for (int i = 0; i < quantProbSliders.size(); ++i)
    {
        if (showAdvancedView)
        {
            // Advanced view: show all, grey out inactive
            quantActiveButtons[i]->setVisible(true);
            quantProbSliders[i]->setVisible(true);
            quantProbLabels[i]->setVisible(true);

            if (!activeStates[i])
            {
                quantProbSliders[i]->setAlpha(0.4f);
                quantProbSliders[i]->setEnabled(false);
            }
            else
            {
                quantProbSliders[i]->setAlpha(1.0f);
                quantProbSliders[i]->setEnabled(true);
            }

            quantGrid.items.add(GridItem(*quantActiveButtons[i]).withArea(1, columnIndex));
            quantGrid.items.add(GridItem(*quantProbSliders[i]).withArea(2, columnIndex));
            quantGrid.items.add(GridItem(*quantProbLabels[i]).withArea(3, columnIndex));
            columnIndex++;
        }
        else if (activeStates[i])
        {
            // Simple view: only active sliders (NO toggles)
            quantActiveButtons[i]->setVisible(false);  // Hide toggles in simple view
            quantProbSliders[i]->setVisible(true);
            quantProbSliders[i]->setAlpha(1.0f);
            quantProbSliders[i]->setEnabled(true);
            quantProbLabels[i]->setVisible(true);

            // No toggle row in simple view - sliders start at row 1
            quantGrid.items.add(GridItem(*quantProbSliders[i]).withArea(1, columnIndex));
            quantGrid.items.add(GridItem(*quantProbLabels[i]).withArea(2, columnIndex));
            columnIndex++;
        }
        else
        {
            // Hide inactive sliders in simple view
            quantActiveButtons[i]->setVisible(false);
            quantProbSliders[i]->setVisible(false);
            quantProbLabels[i]->setVisible(false);
        }
    }

    // Only perform layout if we have valid configuration
    if (quantGrid.templateColumns.size() > 0 &&
        quantGrid.items.size() > 0 &&
        bounds.getWidth() > 0 &&
        bounds.getHeight() > 0)
    {
        quantGrid.performLayout(bounds);
    }
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

    // Bail out if we don't have valid bounds yet (can happen during construction)
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    // === Top-right corner controls (absolute positioning) ===
    autoStutterIndicator.setBounds(bounds.getWidth() - 158, 5, 28, 22);
    mixModeMenu.setBounds(bounds.getWidth() - 125, 5, 115, 22);

    // Calculate main layout areas
    auto contentBounds = bounds.reduced(8).withTrimmedTop(15); // Leave space for top controls

    const int leftWidth = 170;
    const int buttonColumnWidth = 60;
    const int rightWidth = 140;
    const int spacing = 10;
    const int buttonColumnSpacing = 8;
    const int visualizerHeight = 70;
    const int tunerHeight = 28;
    const int tunerGap = 8;

    auto leftBounds = juce::Rectangle<int>(
        contentBounds.getX(),
        contentBounds.getY() - 15,
        leftWidth,
        contentBounds.getHeight() - visualizerHeight - spacing + 15
    );

    auto centerBounds = juce::Rectangle<int>(
        contentBounds.getX() + leftWidth + spacing,
        contentBounds.getY(),
        contentBounds.getWidth() - leftWidth - buttonColumnWidth - rightWidth - spacing - buttonColumnSpacing - buttonColumnSpacing,
        contentBounds.getHeight() - visualizerHeight - spacing
    );

    auto buttonColumnBounds = juce::Rectangle<int>(
        centerBounds.getRight() + buttonColumnSpacing,
        contentBounds.getY(),
        buttonColumnWidth,
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
    // Split left bounds into envelope controls and tuner
    auto envelopeControlsBounds = leftBounds.withTrimmedBottom(tunerHeight + tunerGap + spacing);
    auto tunerBounds = juce::Rectangle<int>(
        leftBounds.getX(),
        leftBounds.getBottom() - tunerHeight - spacing,
        leftWidth,
        tunerHeight
    );

    layoutEnvelopeControls(envelopeControlsBounds);

    // Center panel: Rate sliders, Nano controls, and Quantization
    // Consistent spacing and heights for all sections
    const int sectionLabelHeight = 18;
    const int sectionLabelGap = 4;
    const int uniformSliderHeight = 90;  // Uniform visual height for all sliders (increased from 75)
    const int sectionGap = 10;  // Gap between sections for better visual separation

    int currentY = 0;

    // === Repeat Rates Section ===
    auto repeatRatesLabelBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(sectionLabelHeight);
    repeatRatesLabel.setBounds(repeatRatesLabelBounds);

    // === Advanced View Toggle (right-aligned on same line as Repeat Rates label) ===
    auto advancedToggleBounds = juce::Rectangle<int>(
        repeatRatesLabelBounds.getRight() - 120,
        repeatRatesLabelBounds.getY(),
        120,
        18  // Slightly shorter to fit with label
    );
    advancedViewToggle.setBounds(advancedToggleBounds);

    currentY += sectionLabelHeight + sectionLabelGap;

    // Grid layout height depends on view mode (includes rowGap spacing: 4px between rows)
    int rhythmicTotalHeight = showAdvancedView ? (20 + uniformSliderHeight + 15 + 8) : (uniformSliderHeight + 15 + 4);
    auto rhythmicBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(rhythmicTotalHeight);

    // Border includes toggles (advanced only), sliders, and labels (section label outside)
    // Add more padding in advanced view for toggle buttons
    if (showAdvancedView) {
        rhythmicSlidersBounds = rhythmicBounds.expanded(4, 0)
            .withTop(rhythmicBounds.getY() - 4)
            .withBottom(rhythmicBounds.getBottom() + 6);
    } else {
        rhythmicSlidersBounds = rhythmicBounds.expanded(3, 0)
            .withTop(rhythmicBounds.getY())
            .withBottom(rhythmicBounds.getBottom() + 6);
    }
    currentY += rhythmicTotalHeight + sectionGap;

    // === Nano Rates Section ===
    auto nanoRatesLabelBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(sectionLabelHeight);
    nanoRatesLabel.setBounds(nanoRatesLabelBounds);
    currentY += sectionLabelHeight + sectionLabelGap;

    // Nano bounds height depends on advanced view state (includes rowGap spacing: 4px between rows)
    int nanoTotalHeight;
    if (showAdvancedView) {
        // Advanced: toggles (20) + numerators (20) + denominators (20) + sliders (90) + labels (15) + gaps (4×4=16)
        nanoTotalHeight = 20 + 20 + 20 + uniformSliderHeight + 15 + 16;
    } else {
        // Simple: sliders (90) + labels (15) + gaps (4)
        nanoTotalHeight = uniformSliderHeight + 15 + 4;
    }

    auto nanoBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(nanoTotalHeight);
    // Border includes all nano content (toggles in advanced view, sliders, labels)
    // Add more padding in advanced view for toggle buttons
    if (showAdvancedView) {
        nanoSlidersBounds = nanoBounds.expanded(4, 0)
            .withTop(nanoBounds.getY() - 4)
            .withBottom(nanoBounds.getBottom() + 6);
    } else {
        nanoSlidersBounds = nanoBounds.expanded(3, 0)
            .withTop(nanoBounds.getY())
            .withBottom(nanoBounds.getBottom() + 6);
    }
    currentY += nanoTotalHeight + sectionGap;

    // === Quantization Section ===
    auto quantizationLabelBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(sectionLabelHeight);
    quantizationLabel.setBounds(quantizationLabelBounds);
    currentY += sectionLabelHeight + sectionLabelGap;

    // Quant height depends on view mode (includes rowGap spacing: 4px between rows)
    int quantTotalHeight = showAdvancedView ? (20 + uniformSliderHeight + 12 + 8) : (uniformSliderHeight + 12 + 4);
    auto quantBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(quantTotalHeight);

    // Add conditional border padding for quantization section (not shown in visible area, but for consistency)
    // Note: Quantization section border is not currently drawn in paint(), but keeping pattern consistent

    layoutRateSliders(rhythmicBounds);
    layoutNanoControls(nanoBounds);
    layoutQuantizationControls(quantBounds);

    // Position tuner in left panel
    tuner.setBounds(tunerBounds);

    // Position buttons in vertical column between center and right panels
    const int buttonWidth = 55;
    const int buttonHeight = 24;
    const int buttonSpacing = 8;
    const int buttonX = buttonColumnBounds.getX() + (buttonColumnBounds.getWidth() - buttonWidth) / 2;

    // Align buttons with their respective slider sections (vertically centered within slider area)
    int rhythmicButtonY = rhythmicBounds.getY() + (rhythmicBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2;
    int nanoButtonY = nanoBounds.getY() + (nanoBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2;
    int quantButtonY = quantBounds.getY() + (quantBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2;

    resetRateProbButton.setBounds(buttonX, rhythmicButtonY, buttonWidth, buttonHeight);
    randomizeRateProbButton.setBounds(buttonX, rhythmicButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

    resetNanoProbButton.setBounds(buttonX, nanoButtonY, buttonWidth, buttonHeight);
    randomizeNanoProbButton.setBounds(buttonX, nanoButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

    resetQuantProbButton.setBounds(buttonX, quantButtonY, buttonWidth, buttonHeight);
    randomizeQuantProbButton.setBounds(buttonX, quantButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

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
