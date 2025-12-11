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
// Helper function to recursively tint all shapes in a Drawable
namespace
{
    void tintDrawable(juce::Drawable* drawable, juce::Colour tintColour)
    {
        if (drawable == nullptr)
            return;

        // Handle DrawableComposite (SVG with multiple child elements)
        if (auto* composite = dynamic_cast<juce::DrawableComposite*>(drawable))
        {
            for (int i = 0; i < composite->getNumChildComponents(); ++i)
            {
                if (auto* child = dynamic_cast<juce::Drawable*>(composite->getChildComponent(i)))
                    tintDrawable(child, tintColour);
            }
        }
        // Handle individual DrawableShape elements
        else if (auto* shape = dynamic_cast<juce::DrawableShape*>(drawable))
        {
            // Only tint if the shape has a visible fill
            if (!shape->getFill().isInvisible())
                shape->setFill(juce::FillType(tintColour));
        }
    }
}

//==============================================================================
NanoStuttAudioProcessorEditor::NanoStuttAudioProcessorEditor (NanoStuttAudioProcessor& p)
: AudioProcessorEditor (&p), autoStutterIndicator(p), visualizer(p), tuner(p), audioProcessor (p)
{
    // Apply modern LookAndFeel for futuristic/technical UI styling
    setLookAndFeel(&modernLookAndFeel);

    // Pre-generate neumorphic background texture (once, no paint() allocation)
    backgroundTexture = TextureGenerator::createNeumorphicNoise(800, 600, 0.03f);

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
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);  // Reduced textbox height from 20 to 16
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getParameters(), paramID, slider);
    };

    // Setup DualSliders for NanoGate and NanoShape with randomization
    addAndMakeVisible(nanoGateDualSlider);
    nanoGateDualSlider.setDefaultValues(1.0, 0.0);  // NanoGate default: 1.0, Random default: 0.0
    nanoGateDualSlider.setScaleMarkings(4, {".25", ".5", ".75", "1"});  // Scale: 0.25 to 1.0
    // Vertical gradient: exact panel background colors (orange top → purple bottom)
    auto panelOrange = ColorPalette::rhythmicOrange;
    auto panelPurple = ColorPalette::nanoPurple;
    nanoGateDualSlider.setSectionGradient(panelOrange, panelPurple);
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

    // Enable snap mode for gate control only
    nanoGateDualSlider.setSnapModeAvailable(true);

    // Setup snap mode state synchronization (parameter → UI)
    nanoGateSnapModeAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("NanoGateSnapMode"),
        [this](float newValue) {
            nanoGateDualSlider.setSnapMode(newValue > 0.5f);
        });

    // Set initial snap mode state from parameter
    nanoGateDualSlider.setSnapMode(
        audioProcessor.getParameters().getRawParameterValue("NanoGateSnapMode")->load() > 0.5f);

    // Listen for snap mode changes from UI (right-click inner knob)
    nanoGateDualSlider.onSnapModeChange = [this](bool snapEnabled) {
        auto* param = audioProcessor.getParameters().getParameter("NanoGateSnapMode");
        if (param)
            param->setValueNotifyingHost(snapEnabled ? 1.0f : 0.0f);
    };

    // Setup DualSlider for NanoShape with randomization
    addAndMakeVisible(nanoShapeDualSlider);
    nanoShapeDualSlider.setDefaultValues(0.5, 0.0);  // NanoShape default: 0.5, Random default: 0.0
    nanoShapeDualSlider.setScaleMarkings(5, {"0", ".25", ".5", ".75", "1"});  // Scale: 0.0 to 1.0
    // Vertical gradient: exact panel background colors (reuse panel colors)
    nanoShapeDualSlider.setSectionGradient(panelOrange, panelPurple);
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

    // Setup DualSlider for NanoOctave with randomization and integer snapping
    addAndMakeVisible(nanoOctaveDualSlider);
    nanoOctaveDualSlider.setDefaultValues(0.0, 0.0);  // NanoOctave default: 0, Random default: 0
    nanoOctaveDualSlider.setScaleMarkings(5, {"-1", "0", "1", "2", "3"});  // Scale: -1 to 3 octaves
    // Pure purple for nano-specific pitch control
    nanoOctaveDualSlider.setSectionColor(ColorPalette::nanoPurple);

    // Set integer snapping for both main and random sliders
    nanoOctaveDualSlider.getMainSlider().setRange(-1.0, 3.0, 1.0);  // Step of 1.0 for integers
    nanoOctaveDualSlider.getRandomSlider().setRange(-4.0, 4.0, 1.0);  // Step of 1.0 for integers (up to 4 octaves random)

    // Set visual range scale to match parameter range (-4 to 4 for full octave range)
    nanoOctaveDualSlider.setVisualRangeScale(4.0f);

    // Increase sensitivity for faster octave changes (default 0.003, using 0.012 for 4x sensitivity)
    nanoOctaveDualSlider.setRandomSensitivity(0.012f);

    // Set integer text display formatters BEFORE attachments (correct JUCE pattern for discrete parameters)
    nanoOctaveDualSlider.getMainSlider().textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(std::round(value)));
    };
    nanoOctaveDualSlider.getRandomSlider().textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(std::round(value)));
    };

    // Create attachments AFTER text formatters (will respect formatters for discrete parameters)
    nanoOctaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoOctave", nanoOctaveDualSlider.getMainSlider());
    nanoOctaveRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoOctaveRandom", nanoOctaveDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for NanoOctave
    nanoOctaveBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("NanoOctaveRandomBipolar"),
        [this](float newValue) {
            nanoOctaveDualSlider.setBipolarMode(newValue > 0.5f);
        });

    // Set initial state
    nanoOctaveDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("NanoOctaveRandomBipolar")->load() > 0.5f);

    // Listen for changes from UI (right-click toggle)
    nanoOctaveDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("NanoOctaveRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // NanoSmooth - Hann window smoothing (regular slider, no randomization)
    setupKnob(nanoSmoothSlider, "NanoSmooth", nanoSmoothAttachment);
    // Convert to horizontal slider
    nanoSmoothSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    nanoSmoothSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    // NanoEma DualSlider - EMA filter with randomization
    addAndMakeVisible(nanoEmaDualSlider);
    nanoEmaDualSlider.setDefaultValues(0.0, 0.0);  // EMA default: 0.0, Random default: 0.0
    nanoEmaDualSlider.setScaleMarkings(5, {"0", ".25", ".5", ".75", "1"});  // Scale: 0.0 to 1.0
    // Vertical gradient: exact panel background colors (reuse panel colors)
    nanoEmaDualSlider.setSectionGradient(panelOrange, panelPurple);
    nanoEmaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoEmaFilter", nanoEmaDualSlider.getMainSlider());
    nanoEmaRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "NanoEmaFilterRandom", nanoEmaDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for NanoEma
    nanoEmaBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("NanoEmaFilterRandomBipolar"),
        [this](float newValue) {
            nanoEmaDualSlider.setBipolarMode(newValue > 0.5f);
        });
    nanoEmaDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("NanoEmaFilterRandomBipolar")->load() > 0.5f);
    nanoEmaDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("NanoEmaFilterRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // CycleCrossfade DualSlider - cycle boundary smoothing with randomization
    addAndMakeVisible(nanoCycleCrossfadeDualSlider);
    nanoCycleCrossfadeDualSlider.setDefaultValues(0.02, 0.0);  // Default: 0.02 (subtle fade), Random: 0.0
    nanoCycleCrossfadeDualSlider.setScaleMarkings(5, {"0", ".25", ".5", ".75", "1"});  // Scale: 0.0 to 1.0
    // Vertical gradient: exact panel background colors (reuse panel colors)
    nanoCycleCrossfadeDualSlider.setSectionGradient(panelOrange, panelPurple);
    nanoCycleCrossfadeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "CycleCrossfade", nanoCycleCrossfadeDualSlider.getMainSlider());
    nanoCycleCrossfadeRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "CycleCrossfadeRandom", nanoCycleCrossfadeDualSlider.getRandomSlider());

    // Setup bipolar state synchronization for CycleCrossfade
    nanoCycleCrossfadeBipolarAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("CycleCrossfadeRandomBipolar"),
        [this](float newValue) {
            nanoCycleCrossfadeDualSlider.setBipolarMode(newValue > 0.5f);
        });
    nanoCycleCrossfadeDualSlider.setBipolarMode(
        audioProcessor.getParameters().getRawParameterValue("CycleCrossfadeRandomBipolar")->load() > 0.5f);
    nanoCycleCrossfadeDualSlider.onBipolarModeChange = [this](bool isBipolar) {
        auto* param = audioProcessor.getParameters().getParameter("CycleCrossfadeRandomBipolar");
        if (param)
            param->setValueNotifyingHost(isBipolar ? 1.0f : 0.0f);
    };

    // Fade Length slider (advanced view only - horizontal style like right section)
    addAndMakeVisible(fadeLengthSlider);
    fadeLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    fadeLengthSlider.setVisible(false);  // Hidden by default
    fadeLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "FadeLength", fadeLengthSlider);

    // Setup DualSliders for MacroGate and MacroShape with randomization
    addAndMakeVisible(macroGateDualSlider);
    macroGateDualSlider.setDefaultValues(1.0, 0.0);  // MacroGate default: 1.0, Random default: 0.0
    macroGateDualSlider.setScaleMarkings(4, {".25", ".5", ".75", "1"});  // Scale: 0.25 to 1.0
    macroGateDualSlider.setSectionColor(ColorPalette::accentCyan);  // Green for macro section
    macroGateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroGate", macroGateDualSlider.getMainSlider());
    macroGateRandomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "MacroGateRandom", macroGateDualSlider.getRandomSlider());

    addAndMakeVisible(macroShapeDualSlider);
    macroShapeDualSlider.setDefaultValues(0.5, 0.0);  // MacroShape default: 0.5, Random default: 0.0
    macroShapeDualSlider.setScaleMarkings(5, {"0", ".25", ".5", ".75", "1"});  // Scale: 0.0 to 1.0
    macroShapeDualSlider.setSectionColor(ColorPalette::accentCyan);  // Green for macro section
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

    // Enable snap mode for gate control only
    macroGateDualSlider.setSnapModeAvailable(true);

    // Setup snap mode state synchronization (parameter → UI)
    macroGateSnapModeAttachment = std::make_unique<juce::ParameterAttachment>(
        *audioProcessor.getParameters().getParameter("MacroGateSnapMode"),
        [this](float newValue) {
            macroGateDualSlider.setSnapMode(newValue > 0.5f);
        });

    // Set initial snap mode state from parameter
    macroGateDualSlider.setSnapMode(
        audioProcessor.getParameters().getRawParameterValue("MacroGateSnapMode")->load() > 0.5f);

    // Listen for snap mode changes from UI (right-click inner knob)
    macroGateDualSlider.onSnapModeChange = [this](bool snapEnabled) {
        auto* param = audioProcessor.getParameters().getParameter("MacroGateSnapMode");
        if (param)
            param->setValueNotifyingHost(snapEnabled ? 1.0f : 0.0f);
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
    // Convert to horizontal slider
    macroSmoothSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    macroSmoothSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

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
    setupLabel(nanoOctaveLabel, "Oct", nanoOctaveDualSlider);
    setupLabel(nanoSmoothLabel, "Smooth", nanoSmoothSlider);
    setupLabel(nanoEmaLabel, "EMA", nanoEmaDualSlider);
    setupLabel(nanoCycleCrossfadeLabel, "Xfade", nanoCycleCrossfadeDualSlider);
    setupLabel(macroGateLabel, "Gate", macroGateDualSlider);
    setupLabel(macroShapeLabel, "Shape", macroShapeDualSlider);
    setupLabel(macroSmoothLabel, "Smooth", macroSmoothSlider);

    nanoControlsLabel.setText("Nano Envelope", juce::dontSendNotification);
    nanoControlsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nanoControlsLabel);

    macroControlsLabel.setText("Macro Envelope", juce::dontSendNotification);
    macroControlsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(macroControlsLabel);

    dampingLabel.setText("Damping", juce::dontSendNotification);
    dampingLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dampingLabel);

    // === Section Labels ===
    repeatRatesLabel.setText("Repeat Rates", juce::dontSendNotification);
    repeatRatesLabel.setJustificationType(juce::Justification::centred);
    repeatRatesLabel.setColour(juce::Label::textColourId, ColorPalette::rhythmicOrange);
    addAndMakeVisible(repeatRatesLabel);

    nanoRatesLabel.setText("Nano Rates", juce::dontSendNotification);
    nanoRatesLabel.setJustificationType(juce::Justification::centred);
    nanoRatesLabel.setColour(juce::Label::textColourId, ColorPalette::nanoPurple);
    addAndMakeVisible(nanoRatesLabel);

    quantizationLabel.setText("Quantization", juce::dontSendNotification);
    quantizationLabel.setJustificationType(juce::Justification::centred);
    quantizationLabel.setColour(juce::Label::textColourId, ColorPalette::accentCyan);
    addAndMakeVisible(quantizationLabel);
    
    // === Rate Sliders and buttons (13 rates - added 1/4d) ===
    auto rateLabels = juce::StringArray { "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (int i = 0; i < rateLabels.size(); ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setName("rate");  // Color-code with orange theme
        addAndMakeVisible(slider);
        rateProbSliders.add(slider);

        // Note: Labels are created later after SVG loading (see after line 850)

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
    
    // === Quant Probability Sliders (updated naming) ===
    auto quantLabels = juce::StringArray { "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32" };
    for (int i = 0; i < quantLabels.size(); ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setName("quant");  // Color-code with cyan theme
        addAndMakeVisible(slider);
        quantProbSliders.add(slider);

        // Note: Labels are created later after SVG loading (see after line 850)

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

    // === Nano Tuning System Controls ===
    addAndMakeVisible(nanoBaseMenu);
    nanoBaseMenu.addItemList({ "BPM Synced", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    nanoBaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "nanoBase", nanoBaseMenu);

    addAndMakeVisible(tuningSystemMenu);
    tuningSystemMenu.addItemList({ "Equal Temperament", "Just Intonation", "Pythagorean", "Quarter-comma Meantone", "Custom (Fraction)", "Custom (Decimal)", "Custom (Semitone)" }, 1);
    tuningSystemAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "tuningSystem", tuningSystemMenu);

    addAndMakeVisible(scaleMenu);
    scaleMenu.addItemList({ "Chromatic", "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic",
                           "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian",
                           "Harmonic Minor", "Melodic Minor", "Whole Tone", "Diminished", "Custom" }, 1);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "scale", scaleMenu);

    // Window Type ComboBox (advanced view only)
    windowTypeLabel = std::make_unique<juce::Label>("", "Window Type");
    windowTypeLabel->setJustificationType(juce::Justification::centred);
    windowTypeLabel->setVisible(false);  // Hidden by default
    addAndMakeVisible(windowTypeLabel.get());

    addAndMakeVisible(windowTypeMenu);
    windowTypeMenu.addItemList({ "None", "Hann", "Hamming", "Blackman", "Blackman-Harris",
                                  "Bartlett", "Kaiser", "Tukey", "Gaussian", "Planck", "Exponential" }, 1);
    windowTypeMenu.setVisible(false);  // Hidden by default
    windowTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "WindowType", windowTypeMenu);

    // Fade Length label (advanced view only - attaches to slider)
    fadeLengthLabel.setText("Fade Length", juce::dontSendNotification);
    fadeLengthLabel.setJustificationType(juce::Justification::centred);
    fadeLengthLabel.attachToComponent(&fadeLengthSlider, false);
    fadeLengthLabel.setVisible(false);
    addAndMakeVisible(fadeLengthLabel);

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

    // === Preset UI Components ===
    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save Preset");
    savePresetButton.onClick = [this]() { onSavePresetClicked(); };

    addAndMakeVisible(presetMenu);
    presetMenu.onChange = [this]() { onPresetSelected(); };
    updatePresetMenu(); // Populate preset menu

    addAndMakeVisible(presetNameLabel);
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    presetNameLabel.setText("No Preset Loaded", juce::dontSendNotification);

    // Start timer for updating preset name label (30Hz)
    startTimerHz(30);

    // === Advanced View Toggle ===
    addAndMakeVisible(advancedViewToggle);
    advancedViewToggle.setButtonText("Advanced View");
    advancedViewToggle.onClick = [this]() {
        showAdvancedView = !showAdvancedView;

        // Auto-resize window for better fit in advanced view
        const int currentWidth = getWidth();
        if (showAdvancedView) {
            // Entering advanced view: increase height for extra content
            setSize(currentWidth, 690);  // Increased to prevent nano preset cutoff
        } else {
            // Exiting advanced view: restore original height
            setSize(currentWidth, 610);  // Increased to accommodate taller label rows (27px vs 12-15px)
        }

        resized();

        // Update ratio UI to show correct editors in advanced view
        if (showAdvancedView)
            updateNanoRatioUI();

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

        // === Semitone editors for Equal Temperament ===
        auto* semitoneBox = new juce::TextEditor();
        semitoneBox->setInputRestrictions(2, "0123456789");
        semitoneBox->setJustification(juce::Justification::centred);
        semitoneBox->setText(juce::String(i), juce::dontSendNotification);
        semitoneBox->onFocusLost = semitoneBox->onReturnKey = [this, i]() {
            updateNanoRatioFromSemitone(i);
        };
        addAndMakeVisible(semitoneBox);
        nanoSemitoneEditors.add(semitoneBox);
        semitoneBox->setVisible(false);  // Hidden by default

        // === Decimal labels for Quarter-comma Meantone (read-only) ===
        auto* decimalLabel = new juce::Label();
        decimalLabel->setJustificationType(juce::Justification::centred);
        decimalLabel->setText(juce::String(ratioVal, 3), juce::dontSendNotification);
        addAndMakeVisible(decimalLabel);
        nanoDecimalLabels.add(decimalLabel);
        decimalLabel->setVisible(false);  // Hidden by default

        // === Variant selectors for interval options (e.g., Aug 4th vs Dim 5th) ===
        auto* variantSelector = new juce::ComboBox();
        variantSelector->onChange = [this, i]() {
            updateNanoRatioFromVariant(i);
        };
        addAndMakeVisible(variantSelector);
        nanoVariantSelectors.add(variantSelector);
        variantSelector->setVisible(false);  // Hidden by default, shown when variants exist
    }





    // === Nano Rate Sliders ===
    for (int i = 0; i < 12; ++i)
    {
        auto* slider = new juce::Slider();
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setName("nano");  // Color-code with purple theme
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

    // Lambda function to load SVG from BinaryData
    auto loadSVGFromBinary = [](const char* svgData, const juce::String& name) -> std::unique_ptr<juce::Drawable>
    {
        if (auto svg = juce::parseXML(svgData))
        {
            // KEEP the viewBox and transform - they're essential for positioning!
            // Just log that we loaded it
            DBG("Loaded SVG XML for: " + name);

            auto drawable = juce::Drawable::createFromSVG(*svg);
            if (drawable != nullptr)
            {
                auto bounds = drawable->getDrawableBounds();
                DBG("Successfully loaded SVG: " + name +
                    " | Bounds: " + juce::String(bounds.getX()) + "," + juce::String(bounds.getY()) +
                    " " + juce::String(bounds.getWidth()) + "x" + juce::String(bounds.getHeight()));
                return drawable;
            }
            else
            {
                DBG("Failed to create drawable from SVG: " + name);
            }
        }
        else
        {
            DBG("Failed to parse XML for embedded SVG: " + name);
        }

        return nullptr;
    };

    // === Load Roman Numeral SVG Graphics FIRST (before creating labels) ===
    const char* svgDataArray[12] = {
        BinaryData::IM_svg,       // Index 0: "I" (root) - Capital
        BinaryData::ii_svg,       // Index 1: "ii" (minor 2nd) - Lowercase
        BinaryData::IIM_svg,      // Index 2: "II" (Major 2nd) - Capital
        BinaryData::iii_svg,      // Index 3: "iii" (minor 3rd) - Lowercase
        BinaryData::IIIM_svg,     // Index 4: "III" (Major 3rd) - Capital
        BinaryData::IVM_svg,      // Index 5: "IV" (Perfect 4th) - Capital
        BinaryData::iv_svg,       // Index 6: "#iv" (Augmented 4th) - Lowercase
        BinaryData::VM_svg,       // Index 7: "V" (Perfect 5th) - Capital
        BinaryData::vi_svg,       // Index 8: "vi" (minor 6th) - Lowercase
        BinaryData::VIM_svg,      // Index 9: "VI" (Major 6th) - Capital
        BinaryData::vii_svg,      // Index 10: "vii" (minor 7th) - Lowercase
        BinaryData::VIIM_svg      // Index 11: "VII" (Major 7th) - Capital
    };

    juce::StringArray svgNames = { "I", "ii", "IIM", "iii", "IIIM", "IVM", "iv", "V", "vi", "VIM", "vii", "VIIM" };

    for (int i = 0; i < 12; ++i)
    {
        nanoLabelSVGs[i] = loadSVGFromBinary(svgDataArray[i], svgNames[i]);

        // Tint SVG to much brighter purple for visibility
        if (nanoLabelSVGs[i] != nullptr)
            tintDrawable(nanoLabelSVGs[i].get(), ColorPalette::nanoPurple.brighter(2.0f));
    }

    // === Load Repeat Rate SVG Graphics ===
    const char* repeatRateSVGData[13] = {
        BinaryData::_1_svg,       // Index 0: "1" (whole note)
        BinaryData::_1_2d_svg,    // Index 1: "1/2d" (dotted half)
        BinaryData::_1_2_svg,     // Index 2: "1/2" (half note)
        BinaryData::_1_4d_svg,    // Index 3: "1/4d" (dotted quarter) - NEW
        BinaryData::_1_3_svg,     // Index 4: "1/3" (triplet)
        BinaryData::_1_4_svg,     // Index 5: "1/4" (quarter note)
        BinaryData::_1_8d_svg,    // Index 6: "1/8d" (dotted eighth)
        BinaryData::_1_6_svg,     // Index 7: "1/6" (triplet)
        BinaryData::_1_8_svg,     // Index 8: "1/8" (eighth note)
        BinaryData::_1_12_svg,    // Index 9: "1/12" (triplet)
        BinaryData::_1_16_svg,    // Index 10: "1/16" (sixteenth)
        BinaryData::_1_24_svg,    // Index 11: "1/24" (triplet)
        BinaryData::_1_32_svg     // Index 12: "1/32" (thirty-second)
    };

    juce::StringArray repeatRateSVGNames = { "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };

    for (int i = 0; i < 13; ++i)
    {
        repeatRateSVGs[i] = loadSVGFromBinary(repeatRateSVGData[i], repeatRateSVGNames[i]);

        // Tint SVG to brightened orange (rhythmic section color)
        if (repeatRateSVGs[i] != nullptr)
            tintDrawable(repeatRateSVGs[i].get(), ColorPalette::rhythmicOrange.brighter(2.0f));
    }

    // === Load Quant Rate SVG Graphics ===
    const char* quantRateSVGData[9] = {
        BinaryData::_4_svg,       // Index 0: "4" (4 bars)
        BinaryData::_2_svg,       // Index 1: "2" (2 bars)
        BinaryData::_1_svg,       // Index 2: "1" (1 bar)
        BinaryData::_1_2_svg,     // Index 3: "1/2" (half note)
        BinaryData::_1_4_svg,     // Index 4: "1/4" (quarter note)
        BinaryData::_1_8d_svg,    // Index 5: "1/8d" (dotted eighth)
        BinaryData::_1_8_svg,     // Index 6: "1/8" (eighth note)
        BinaryData::_1_16_svg,    // Index 7: "1/16" (sixteenth)
        BinaryData::_1_32_svg     // Index 8: "1/32" (thirty-second)
    };

    juce::StringArray quantRateSVGNames = { "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32" };

    for (int i = 0; i < 9; ++i)
    {
        quantRateSVGs[i] = loadSVGFromBinary(quantRateSVGData[i], quantRateSVGNames[i]);

        // Tint SVG to brightened cyan (quant section color)
        if (quantRateSVGs[i] != nullptr)
            tintDrawable(quantRateSVGs[i].get(), ColorPalette::accentCyan.brighter(2.0f));
    }

    // Helper function to determine vertical scale factor based on label complexity
    auto getScaleFactorForLabel = [](const juce::String& label) -> float {
        int charCount = label.length();

        // Simple labels (1-2 characters: "1", "2", "4")
        if (charCount <= 2) {
            return 1.0f;  // Full height
        }
        // Fraction labels (3 characters: "1/2", "1/3", "1/4", "1/6", "1/8")
        else if (charCount == 3) {
            return 0.75f;  // 75% height to compensate for wider aspect ratio
        }
        // Complex labels (4+ characters: "1/2d", "1/4d", "1/8d", "1/12", "1/16", "1/24", "1/32")
        else {
            return 0.70f;  // 70% height for extra wide labels
        }
    };

    // === Create Repeat Rate Labels (now that SVGs are loaded) ===
    auto rateLabelsForCreation = juce::StringArray { "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (int i = 0; i < rateLabelsForCreation.size(); ++i)
    {
        auto* label = new RomanNumeralLabel();

        // Clone tinted SVG for this label
        if (repeatRateSVGs[i] != nullptr)
        {
            auto svgCopy = repeatRateSVGs[i]->createCopy();
            label->setSVGDrawable(std::move(svgCopy));
        }

        // Orange border matching rhythmic section
        label->setBorderColour(ColorPalette::rhythmicOrange);

        // Opaque background to block panel SVG bleed-through
        label->setBackgroundFillColour(ColorPalette::mainBackground);

        // Apply scale factor based on label complexity to compensate for SVG aspect ratio differences
        label->setVerticalScaleFactor(getScaleFactorForLabel(rateLabelsForCreation[i]));

        addAndMakeVisible(label);
        rateProbLabels.add(label);
    }

    // === Create Quant Rate Labels (now that SVGs are loaded) ===
    auto quantLabelsForCreation = juce::StringArray { "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32" };
    for (int i = 0; i < quantLabelsForCreation.size(); ++i)
    {
        auto* label = new RomanNumeralLabel();

        // Clone tinted SVG for this label
        if (quantRateSVGs[i] != nullptr)
        {
            auto svgCopy = quantRateSVGs[i]->createCopy();
            label->setSVGDrawable(std::move(svgCopy));
        }

        // Cyan border matching quant section
        label->setBorderColour(ColorPalette::accentCyan);

        // Opaque background to block panel SVG bleed-through
        label->setBackgroundFillColour(ColorPalette::mainBackground);

        // Apply scale factor based on label complexity to compensate for SVG aspect ratio differences
        label->setVerticalScaleFactor(getScaleFactorForLabel(quantLabelsForCreation[i]));

        addAndMakeVisible(label);
        quantProbLabels.add(label);
    }

    // === Nano Interval Labels (Roman Numeral SVGs) ===
    // Array indicating which indices are capitals (ending in M) vs lowercase
    // Indices: 0=IM, 1=ii, 2=IIM, 3=iii, 4=IIIM, 5=IVM, 6=iv, 7=VM, 8=vi, 9=VIM, 10=vii, 11=VIIM
    bool isCapital[12] = { true, false, true, false, true, true, false, true, false, true, false, true };

    for (int i = 0; i < 12; ++i)
    {
        auto* label = new RomanNumeralLabel();

        // Clone SVG for this label (already tinted to brightened purple)
        if (nanoLabelSVGs[i] != nullptr)
        {
            auto svgCopy = nanoLabelSVGs[i]->createCopy();
            DBG("Setting SVG for label " + juce::String(i) + " | Copy created: " + (svgCopy != nullptr ? "YES" : "NO"));
            label->setSVGDrawable(std::move(svgCopy));
        }
        else
        {
            DBG("WARNING: nanoLabelSVGs[" + juce::String(i) + "] is nullptr!");
        }

        // Purple border matching nano section
        label->setBorderColour(ColorPalette::nanoPurple);

        // Opaque dark background to block panel SVG bleed-through
        label->setBackgroundFillColour(ColorPalette::mainBackground);

        // Set vertical scale: capitals = 100% height, lowercase = 80% height
        label->setVerticalScaleFactor(isCapital[i] ? 1.0f : 0.8f);

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
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 6)); // 3, 4, or 5

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
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(3, 8));

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
        int numToRandomize = juce::Random::getSystemRandom().nextInt(juce::Range<int>(3, 8));

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

    setResizeLimits(1000, 610, 1000, 690);
    setSize(1000, 610);
    setResizable(false, false);

    // Load SVG panel backgrounds from embedded BinaryData
    quantPanelSVG = loadSVGFromBinary(BinaryData::QuantPanel_svg, "QuantPanel");
    rhythmicPanelSVG = loadSVGFromBinary(BinaryData::RhythemPanel_svg, "RhythemPanel");
    nanoPanelSVG = loadSVGFromBinary(BinaryData::NanoPanel_svg, "NanoPanel");

    // Initialize tuning system UI
    updateNanoRatioUI();
}

//==============================================================================
NanoStuttAudioProcessorEditor::~NanoStuttAudioProcessorEditor()
{
    // Clean up LookAndFeel before destruction
    setLookAndFeel(nullptr);

    // unique_ptr handles cleanup automatically
}

//==============================================================================
void NanoStuttAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill with modern dark background
    g.fillAll (ColorPalette::mainBackground);

    // Draw subtle neumorphic noise texture
    g.drawImageAt(backgroundTexture, 0, 0);

    // Draw colored backgrounds and borders around slider sections with modern styling
    // Cyan section for quantization sliders
    if (!quantizationSlidersBounds.isEmpty())
    {
        auto quantBounds = quantizationSlidersBounds.toFloat();

        // SVG panel background with color tint
        if (quantPanelSVG != nullptr)
        {
            auto tintedSVG = quantPanelSVG->createCopy();
            tintDrawable(tintedSVG.get(), ColorPalette::accentCyan.withAlpha(0.85f));

            if (showAdvancedView)
            {
                // Advanced: Stretch to fit panel exactly
                tintedSVG->drawWithin(g, quantBounds,
                                      juce::RectanglePlacement::stretchToFit,
                                      1.0f);
            }
            else
            {
                // Simple: Bottom-aligned, clip top, NO scaling
                // Use clipping to prevent SVG from overflowing panel bounds
                g.saveState();
                g.reduceClipRegion(quantBounds.toNearestInt());

                tintedSVG->drawWithin(g, quantBounds,
                                      juce::RectanglePlacement::xMid |
                                      juce::RectanglePlacement::yBottom |
                                      juce::RectanglePlacement::fillDestination,
                                      1.0f);

                g.restoreState();
            }
        }

        // Glowing border
        juce::Path borderPath;
        borderPath.addRectangle(quantBounds);
        GlowEffect::drawStrokeWithGlow(g, borderPath,
                                        ColorPalette::accentCyan, 2.0f,
                                        ColorPalette::accentGlow, 4.0f);
    }

    // Orange section for rhythmic/regular rate sliders
    if (!rhythmicSlidersBounds.isEmpty())
    {
        auto rhythmicBounds = rhythmicSlidersBounds.toFloat();

        // SVG panel background with color tint
        if (rhythmicPanelSVG != nullptr)
        {
            auto tintedSVG = rhythmicPanelSVG->createCopy();
            tintDrawable(tintedSVG.get(), ColorPalette::rhythmicOrange.withAlpha(0.85f));

            if (showAdvancedView)
            {
                // Advanced: Stretch to fit panel exactly
                tintedSVG->drawWithin(g, rhythmicBounds,
                                      juce::RectanglePlacement::stretchToFit,
                                      1.0f);
            }
            else
            {
                // Simple: Bottom-aligned, clip top, NO scaling
                // Use clipping to prevent SVG from overflowing panel bounds
                g.saveState();
                g.reduceClipRegion(rhythmicBounds.toNearestInt());

                tintedSVG->drawWithin(g, rhythmicBounds,
                                      juce::RectanglePlacement::xMid |
                                      juce::RectanglePlacement::yBottom |
                                      juce::RectanglePlacement::fillDestination,
                                      1.0f);

                g.restoreState();
            }
        }

        // Glowing border
        juce::Path borderPath;
        borderPath.addRectangle(rhythmicBounds);
        GlowEffect::drawStrokeWithGlow(g, borderPath,
                                        ColorPalette::rhythmicOrange, 2.0f,
                                        ColorPalette::rhythmicGlow, 4.0f);
    }

    // Purple section for nano sliders
    if (!nanoSlidersBounds.isEmpty())
    {
        auto nanoBounds = nanoSlidersBounds.toFloat();

        // SVG panel background with color tint
        if (nanoPanelSVG != nullptr)
        {
            auto tintedSVG = nanoPanelSVG->createCopy();
            tintDrawable(tintedSVG.get(), ColorPalette::nanoPurple.withAlpha(0.85f));

            if (showAdvancedView)
            {
                // Advanced: Stretch to fit panel exactly
                tintedSVG->drawWithin(g, nanoBounds,
                                      juce::RectanglePlacement::stretchToFit,
                                      1.0f);
            }
            else
            {
                // Simple: Bottom-aligned, clip top, NO scaling
                // Use clipping to prevent SVG from overflowing panel bounds
                g.saveState();
                g.reduceClipRegion(nanoBounds.toNearestInt());

                tintedSVG->drawWithin(g, nanoBounds,
                                      juce::RectanglePlacement::xMid |
                                      juce::RectanglePlacement::yBottom |
                                      juce::RectanglePlacement::fillDestination,
                                      1.0f);

                g.restoreState();
            }
        }

        // Glowing border
        juce::Path borderPath;
        borderPath.addRectangle(nanoBounds);
        GlowEffect::drawStrokeWithGlow(g, borderPath,
                                        ColorPalette::nanoPurple, 2.0f,
                                        ColorPalette::nanoGlow, 4.0f);
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

    // Add Window Type and Fade Length rows in advanced view
    if (showAdvancedView) {
        envelopeGrid.templateRows = {
            Track(Px(20)), Track(Px(80)), Track(Px(40)), // Macro: label, knobs (larger), smooth (smaller)
            Track(Px(20)), Track(Px(80)), Track(Px(40)), // Nano: label, knobs (larger), smooth (smaller)
            Track(Px(20)), Track(Px(58)),                 // Damping: label, knobs
            Track(Px(20)), Track(Px(30)),                 // Window Type: label, menu (advanced only)
            Track(Px(10)),                                // Spacer row (advanced only)
            Track(Px(30))                                 // Fade Length: horizontal slider (advanced only)
        };
    } else {
        envelopeGrid.templateRows = {
            Track(Px(20)), Track(Px(80)), Track(Px(40)), // Macro: label, knobs (larger), smooth (smaller)
            Track(Px(20)), Track(Px(80)), Track(Px(40)), // Nano: label, knobs (larger), smooth (smaller)
            Track(Px(20)), Track(Px(58))  // Damping: label, knobs
        };
    }

    envelopeGrid.templateColumns = { Track(Fr(1)), Track(Fr(1)) }; // 2 equal columns
    envelopeGrid.columnGap = Px(8);
    envelopeGrid.rowGap = Px(12);  // Gap between rows to fit all sections

    if (showAdvancedView) {
        windowTypeLabel->setVisible(true);
        windowTypeMenu.setVisible(true);
        fadeLengthLabel.setVisible(true);
        fadeLengthSlider.setVisible(true);

        envelopeGrid.items = {
            // Macro Envelope section
            GridItem(macroControlsLabel).withArea(1, 1, 1, 3),
            GridItem(macroGateDualSlider).withArea(2, 1),
            GridItem(macroShapeDualSlider).withArea(2, 2),
            GridItem(macroSmoothSlider).withArea(3, 1, 3, 3),
            // Nano Envelope section
            GridItem(nanoControlsLabel).withArea(4, 1, 4, 3),
            GridItem(nanoGateDualSlider).withArea(5, 1),
            GridItem(nanoShapeDualSlider).withArea(5, 2),
            GridItem(nanoSmoothSlider).withArea(6, 1, 6, 3),
            // Damping section
            GridItem(dampingLabel).withArea(7, 1, 7, 3),
            GridItem(nanoEmaDualSlider).withArea(8, 1),
            GridItem(nanoCycleCrossfadeDualSlider).withArea(8, 2),
            // Window Type section (advanced view only)
            GridItem(*windowTypeLabel).withArea(9, 1, 9, 3),
            GridItem(windowTypeMenu).withArea(10, 1, 10, 3),
            // Row 11 is spacer (no item needed)
            // Fade Length section (advanced view only - label attaches to slider)
            GridItem(fadeLengthSlider).withArea(12, 1, 12, 3)
        };
    } else {
        windowTypeLabel->setVisible(false);
        windowTypeMenu.setVisible(false);
        fadeLengthLabel.setVisible(false);
        fadeLengthSlider.setVisible(false);

        envelopeGrid.items = {
            // Macro Envelope section
            GridItem(macroControlsLabel).withArea(1, 1, 1, 3),
            GridItem(macroGateDualSlider).withArea(2, 1),
            GridItem(macroShapeDualSlider).withArea(2, 2),
            GridItem(macroSmoothSlider).withArea(3, 1, 3, 3),
            // Nano Envelope section
            GridItem(nanoControlsLabel).withArea(4, 1, 4, 3),
            GridItem(nanoGateDualSlider).withArea(5, 1),
            GridItem(nanoShapeDualSlider).withArea(5, 2),
            GridItem(nanoSmoothSlider).withArea(6, 1, 6, 3),
            // Damping section
            GridItem(dampingLabel).withArea(7, 1, 7, 3),
            GridItem(nanoEmaDualSlider).withArea(8, 1),
            GridItem(nanoCycleCrossfadeDualSlider).withArea(8, 2)
        };
    }

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
        // Advanced: toggles (20) + sliders (90) + labels (27)
        rhythmicGrid.templateRows = { Track(Px(20)), Track(Px(90)), Track(Px(27)) };
    } else {
        // Simple: sliders (90) + labels (27)
        rhythmicGrid.templateRows = { Track(Px(90)), Track(Px(27)) };
    }

    // Determine which sliders are active
    auto rateLabels = juce::StringArray { "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
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
    rhythmicGrid.columnGap = Px(3);
    rhythmicGrid.rowGap = Px(0);  // No gap - labels directly under sliders

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
                // Label alpha removed - glow provides visual feedback
            }
            else
            {
                rateProbSliders[i]->setAlpha(1.0f);
                rateProbSliders[i]->setEnabled(true);
                // Label alpha removed - glow provides visual feedback
            }

            rhythmicGrid.items.add(GridItem(*rateActiveButtons[i]).withArea(1, columnIndex));
            rhythmicGrid.items.add(GridItem(*rateProbSliders[i]).withArea(2, columnIndex));
            rhythmicGrid.items.add(GridItem(*rateProbLabels[i])
                .withArea(3, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
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
            rhythmicGrid.items.add(GridItem(*rateProbLabels[i])
                .withArea(2, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
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
        // Advanced view: toggles (20) + numerators (20) + denominators (20) + sliders (90) + labels (27 for 40×27 = 1.5:1 aspect ratio)
        nanoGrid.templateRows = { Track(Px(20)), Track(Px(20)), Track(Px(20)), Track(Px(90)), Track(Px(27)) };
    }
    else
    {
        // Simple view: sliders (90) + labels (27 for 40×27 = 1.5:1 aspect ratio) - NO toggles
        nanoGrid.templateRows = { Track(Px(90)), Track(Px(27)) };
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
    nanoGrid.rowGap = Px(0);  // No gap - labels directly under sliders

    // Add grid items
    nanoGrid.items.clear();
    int columnIndex = 1;
    for (int i = 0; i < nanoRateProbSliders.size() && i < 12; ++i)
    {
        if (showAdvancedView)
        {
            // Advanced view: show all, grey out inactive
            nanoActiveButtons[i]->setVisible(true);
            // Note: Ratio display component visibility (numerators, denominators, semitones, decimals, variants)
            // is controlled by updateNanoRatioUI() based on tuning system
            nanoRateProbSliders[i]->setVisible(true);
            nanoIntervalLabels[i]->setVisible(true);  // Bug fix: Always show labels in advanced view

            if (!activeStates[i])
            {
                nanoRateProbSliders[i]->setAlpha(0.4f);
                nanoRateProbSliders[i]->setEnabled(false);
                nanoNumerators[i]->setEnabled(false);
                nanoDenominators[i]->setEnabled(false);
                nanoSemitoneEditors[i]->setEnabled(false);
                nanoVariantSelectors[i]->setEnabled(false);
            }
            else
            {
                nanoRateProbSliders[i]->setAlpha(1.0f);
                nanoRateProbSliders[i]->setEnabled(true);
                nanoNumerators[i]->setEnabled(true);
                nanoDenominators[i]->setEnabled(true);
                nanoSemitoneEditors[i]->setEnabled(true);
                nanoVariantSelectors[i]->setEnabled(true);
            }

            // Add all components to grid with proper row assignments
            // Row 1: toggles
            // Row 2: numerators OR semitones OR decimals OR variants (controlled by updateNanoRatioUI visibility)
            // Row 3: denominators (controlled by updateNanoRatioUI visibility)
            // Row 4: sliders
            // Row 5: interval labels (always visible with sliders)
            nanoGrid.items.add(GridItem(*nanoActiveButtons[i]).withArea(1, columnIndex));
            nanoGrid.items.add(GridItem(*nanoNumerators[i]).withArea(2, columnIndex));
            nanoGrid.items.add(GridItem(*nanoSemitoneEditors[i]).withArea(2, columnIndex));
            nanoGrid.items.add(GridItem(*nanoDecimalLabels[i]).withArea(2, columnIndex));
            nanoGrid.items.add(GridItem(*nanoVariantSelectors[i]).withArea(2, columnIndex));
            nanoGrid.items.add(GridItem(*nanoDenominators[i]).withArea(3, columnIndex));
            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(4, columnIndex));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i])
                .withArea(5, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})  // Negative top margin to move closer to sliders
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
            columnIndex++;
        }
        else if (activeStates[i])
        {
            // Simple view: only active sliders with labels (NO toggles, NO ratio editors)
            nanoActiveButtons[i]->setVisible(false);  // Hide toggles in simple view
            // Ratio display components hidden in simple view
            nanoNumerators[i]->setVisible(false);
            nanoDenominators[i]->setVisible(false);
            nanoSemitoneEditors[i]->setVisible(false);
            nanoDecimalLabels[i]->setVisible(false);
            nanoVariantSelectors[i]->setVisible(false);  // Hide variant selectors too

            // Keep interval labels visible in simple view
            nanoIntervalLabels[i]->setVisible(true);

            nanoRateProbSliders[i]->setVisible(true);
            nanoRateProbSliders[i]->setAlpha(1.0f);
            nanoRateProbSliders[i]->setEnabled(true);

            // Simple view: row 1 = sliders, row 2 = labels
            nanoGrid.items.add(GridItem(*nanoRateProbSliders[i]).withArea(1, columnIndex));
            nanoGrid.items.add(GridItem(*nanoIntervalLabels[i])
                .withArea(2, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})  // Negative top margin to move closer to sliders
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
            columnIndex++;
        }
        else
        {
            // Hide inactive sliders in simple view (hide ALL components)
            nanoActiveButtons[i]->setVisible(false);
            nanoNumerators[i]->setVisible(false);
            nanoDenominators[i]->setVisible(false);
            nanoSemitoneEditors[i]->setVisible(false);
            nanoDecimalLabels[i]->setVisible(false);
            nanoVariantSelectors[i]->setVisible(false);
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
        // Advanced: toggles (20) + sliders (90) + labels (27)
        quantGrid.templateRows = { Track(Px(20)), Track(Px(90)), Track(Px(27)) };
    } else {
        // Simple: sliders (90) + labels (27) - NO toggles
        quantGrid.templateRows = { Track(Px(90)), Track(Px(27)) };
    }

    // Determine which sliders are active
    auto quantLabels = juce::StringArray { "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32" };
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
    quantGrid.columnGap = Px(3);
    quantGrid.rowGap = Px(0);  // No gap - labels directly under sliders

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
                // Label alpha removed - glow provides visual feedback
            }
            else
            {
                quantProbSliders[i]->setAlpha(1.0f);
                quantProbSliders[i]->setEnabled(true);
                // Label alpha removed - glow provides visual feedback
            }

            quantGrid.items.add(GridItem(*quantActiveButtons[i]).withArea(1, columnIndex));
            quantGrid.items.add(GridItem(*quantProbSliders[i]).withArea(2, columnIndex));
            quantGrid.items.add(GridItem(*quantProbLabels[i])
                .withArea(3, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
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
            quantGrid.items.add(GridItem(*quantProbLabels[i])
                .withArea(2, columnIndex)
                .withWidth(40.0f)
                .withHeight(27.0f)
                .withMargin({-5, 0, 0, 0})
                .withAlignSelf(GridItem::AlignSelf::center)
                .withJustifySelf(GridItem::JustifySelf::center));
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

    // === Top-center: Preset controls (centered horizontally) ===
    const int presetControlsWidth = 200 + 5 + 90 + 5 + 200; // menu + gap + button + gap + label = 500
    const int presetStartX = (bounds.getWidth() - presetControlsWidth) / 2;
    presetMenu.setBounds(presetStartX, 5, 200, 22);
    savePresetButton.setBounds(presetStartX + 205, 5, 90, 22);
    presetNameLabel.setBounds(presetStartX + 300, 5, 200, 22);

    // Calculate main layout areas
    auto contentBounds = bounds.reduced(8).withTrimmedTop(15); // Leave space for top controls

    const int leftWidth = 170;
    const int buttonColumnWidth = 60;
    const int rightWidth = 140;
    const int spacing = 10;
    const int buttonColumnSpacing = 8;
    const int visualizerHeight = 70;
    const int tunerHeight = 22;  // Match nano preset dropdown height
    const int tunerGap = 2;  // Very small gap to position tuner close to visualizer

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
        contentBounds.getY() + 30,  // Push down 30px for better spacing from top
        rightWidth,
        contentBounds.getHeight() - visualizerHeight - spacing - 30
    );

    auto visualizerBounds = juce::Rectangle<int>(
        contentBounds.getX(),
        contentBounds.getBottom() - visualizerHeight,
        contentBounds.getWidth(),
        visualizerHeight
    );

    // Call layout helper methods
    // Split left bounds into envelope controls and tuner
    auto envelopeControlsBounds = leftBounds.withTrimmedBottom(tunerHeight + tunerGap + tunerGap);
    auto tunerBounds = juce::Rectangle<int>(
        leftBounds.getX(),
        leftBounds.getBottom() - tunerHeight - tunerGap,  // Use tunerGap instead of spacing for closer positioning
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

    int currentY = 15;  // Add initial top spacing to separate from preset controls

    // === Quantization Section ===
    auto quantizationLabelBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(sectionLabelHeight);
    quantizationLabel.setBounds(quantizationLabelBounds);

    // === Advanced View Toggle (right-aligned on same line as Quantization label) ===
    auto advancedToggleBounds = juce::Rectangle<int>(
        quantizationLabelBounds.getRight() - 120,
        quantizationLabelBounds.getY(),
        120,
        18  // Slightly shorter to fit with label
    );
    advancedViewToggle.setBounds(advancedToggleBounds);

    currentY += sectionLabelHeight + sectionLabelGap;

    // Quant height depends on view mode (rowGap is 0)
    int quantTotalHeight = showAdvancedView ? (20 + uniformSliderHeight + 27) : (uniformSliderHeight + 27);
    auto quantBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(quantTotalHeight);

    // Border includes toggles (advanced only), sliders, and labels (section label outside)
    // Add more padding in advanced view for toggle buttons
    if (showAdvancedView) {
        quantizationSlidersBounds = quantBounds.expanded(4, 0)
            .withTop(quantBounds.getY() - 4)
            .withBottom(quantBounds.getBottom() + 6);
    } else {
        quantizationSlidersBounds = quantBounds.expanded(3, 0)
            .withTop(quantBounds.getY())
            .withBottom(quantBounds.getBottom() + 6);
    }

    currentY += quantTotalHeight + sectionGap;

    // === Repeat Rates Section ===
    auto repeatRatesLabelBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(sectionLabelHeight);
    repeatRatesLabel.setBounds(repeatRatesLabelBounds);

    currentY += sectionLabelHeight + sectionLabelGap;

    // Grid layout height depends on view mode (rowGap is 0)
    int rhythmicTotalHeight = showAdvancedView ? (20 + uniformSliderHeight + 27) : (uniformSliderHeight + 27);
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

    // Nano bounds height depends on advanced view state (rowGap is 0)
    int nanoTotalHeight;
    if (showAdvancedView) {
        // Advanced: toggles (20) + numerators (20) + denominators (20) + sliders (90) + labels (27)
        nanoTotalHeight = 20 + 20 + 20 + uniformSliderHeight + 27;
    } else {
        // Simple: sliders (90) + labels (27)
        nanoTotalHeight = uniformSliderHeight + 27;
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

    // === Nano Tuning System ComboBoxes (horizontal layout) ===
    const int comboBoxHeight = 22;
    const int comboBoxSpacing = 4;
    const int nanoBaseWidth = 100; // Small fixed width for nanoBase
    auto nanoTuningBounds = centerBounds.withY(centerBounds.getY() + currentY).withHeight(comboBoxHeight);

    const int remainingWidth = nanoTuningBounds.getWidth() - nanoBaseWidth - comboBoxSpacing * 2;
    const int largeComboWidth = remainingWidth / 2;

    nanoBaseMenu.setBounds(nanoTuningBounds.withWidth(nanoBaseWidth));
    tuningSystemMenu.setBounds(nanoTuningBounds.withX(nanoTuningBounds.getX() + nanoBaseWidth + comboBoxSpacing).withWidth(largeComboWidth));
    scaleMenu.setBounds(nanoTuningBounds.withX(nanoTuningBounds.getX() + nanoBaseWidth + comboBoxSpacing + largeComboWidth + comboBoxSpacing).withWidth(largeComboWidth));

    currentY += comboBoxHeight + sectionGap;

    layoutQuantizationControls(quantBounds);
    layoutRateSliders(rhythmicBounds);
    layoutNanoControls(nanoBounds);

    // Position tuner in left panel
    tuner.setBounds(tunerBounds);

    // Position buttons in vertical column between center and right panels
    const int buttonWidth = 55;
    const int buttonHeight = 24;
    const int buttonSpacing = 8;
    const int buttonX = buttonColumnBounds.getX() + (buttonColumnBounds.getWidth() - buttonWidth) / 2;

    // Align buttons with their respective slider sections (vertically centered within slider area)
    int quantButtonY = quantBounds.getY() + (quantBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2;
    int rhythmicButtonY = rhythmicBounds.getY() + (rhythmicBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2;
    int nanoButtonY = nanoBounds.getY() + (nanoBounds.getHeight() - (2 * buttonHeight + buttonSpacing)) / 2 + 40;  // +40px to make room for octave control

    resetQuantProbButton.setBounds(buttonX, quantButtonY, buttonWidth, buttonHeight);
    randomizeQuantProbButton.setBounds(buttonX, quantButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

    resetRateProbButton.setBounds(buttonX, rhythmicButtonY, buttonWidth, buttonHeight);
    randomizeRateProbButton.setBounds(buttonX, rhythmicButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

    // Position octave control above reset button for nano section
    const int octaveControlSize = 60;  // Size for DualSlider
    const int octaveY = nanoButtonY - octaveControlSize - 12;  // 12px spacing above buttons
    nanoOctaveDualSlider.setBounds(buttonX - (octaveControlSize - buttonWidth) / 2, octaveY, octaveControlSize, octaveControlSize);
    nanoOctaveLabel.setBounds(buttonX - (octaveControlSize - buttonWidth) / 2, octaveY + octaveControlSize, octaveControlSize, 15);

    resetNanoProbButton.setBounds(buttonX, nanoButtonY, buttonWidth, buttonHeight);
    randomizeNanoProbButton.setBounds(buttonX, nanoButtonY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight);

    layoutRightPanel(rightBounds);
    layoutVisualizer(visualizerBounds);

    // Hide unused components
    autoStutterQuantMenu.setVisible(false);
    quantLabel.setVisible(false);
    stutterButton.setVisible(false);

    // Set minimum size for better layout
   // setResizeLimits(1000, 570, 1000, 690);
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
    ratio = juce::jlimit(0.1, 4.0, ratio);

    auto* param = audioProcessor.getParameters().getParameter("nanoRatio_" + juce::String(index));
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>((ratio - 0.1) / (4.0 - 0.1)));
}

void NanoStuttAudioProcessorEditor::updateNanoRatioFromSemitone(int index)
{
    auto* semitoneBox = nanoSemitoneEditors[index];

    // Check current tuning system to determine input mode
    auto* tuningSystemParam = audioProcessor.getParameters().getRawParameterValue("tuningSystem");
    if (tuningSystemParam == nullptr)
        return;  // Parameters not initialized yet

    int tuningIndex = static_cast<int>(tuningSystemParam->load());
    NanoTuning::TuningSystem tuning = static_cast<NanoTuning::TuningSystem>(tuningIndex);

    double ratio;

    if (tuning == NanoTuning::TuningSystem::CustomDecimal)
    {
        // Custom Decimal mode: direct decimal input
        ratio = semitoneBox->getText().getDoubleValue();
        ratio = juce::jlimit(0.1, 4.0, ratio);
    }
    else
    {
        // Equal Temperament / Custom Semitone mode: semitone input (0-24 for 2 octaves)
        int semitone = semitoneBox->getText().getIntValue();
        semitone = juce::jlimit(0, 24, semitone);
        semitoneBox->setText(juce::String(semitone), juce::dontSendNotification);

        // Convert semitone to ratio using equal temperament formula: 2^(semitone/12)
        ratio = std::pow(2.0, semitone / 12.0);
        ratio = juce::jlimit(0.1, 4.0, ratio);
    }

    auto* param = audioProcessor.getParameters().getParameter("nanoRatio_" + juce::String(index));
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>((ratio - 0.1) / (4.0 - 0.1)));
}

void NanoStuttAudioProcessorEditor::updateNanoRatioFromVariant(int index)
{
    // Get current tuning system
    auto* tuningSystemParam = audioProcessor.getParameters().getRawParameterValue("tuningSystem");
    if (tuningSystemParam == nullptr)
        return;

    int tuningIndex = static_cast<int>(tuningSystemParam->load());
    NanoTuning::TuningSystem tuning = static_cast<NanoTuning::TuningSystem>(tuningIndex);

    // Get variants for this tuning system
    auto variants = NanoTuning::getIntervalVariants(tuning);

    // Get selected variant index
    auto* variantSelector = nanoVariantSelectors[index];
    int selectedIndex = variantSelector->getSelectedItemIndex();

    // Bounds check
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(variants[index].size()))
        return;

    // Get the ratio for the selected variant
    float ratio = variants[index][selectedIndex].ratio;
    ratio = juce::jlimit(0.1f, 4.0f, ratio);

    // Suppress custom detection during variant selection (variant is part of tuning system)
    audioProcessor.setSuppressCustomDetection(true);

    // Update parameter (variant selection within tuning system)
    auto* param = audioProcessor.getParameters().getParameter("nanoRatio_" + juce::String(index));
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>((ratio - 0.1f) / (4.0f - 0.1f)));

    // Re-enable detection
    audioProcessor.setSuppressCustomDetection(false);
}

void NanoStuttAudioProcessorEditor::updateNanoRatioUI()
{
    // Get current tuning system with null check
    auto* tuningSystemParam = audioProcessor.getParameters().getRawParameterValue("tuningSystem");
    if (tuningSystemParam == nullptr)
        return;  // Parameters not initialized yet

    int tuningIndex = static_cast<int>(tuningSystemParam->load());
    NanoTuning::TuningSystem tuning = static_cast<NanoTuning::TuningSystem>(tuningIndex);

    // Get variants for current tuning system
    auto variants = NanoTuning::getIntervalVariants(tuning);

    // Hide all ratio editing UI components first (but NOT interval labels - those are controlled by layout)
    for (int i = 0; i < 12; ++i)
    {
        nanoNumerators[i]->setVisible(false);
        nanoDenominators[i]->setVisible(false);
        nanoSemitoneEditors[i]->setVisible(false);
        nanoDecimalLabels[i]->setVisible(false);
        nanoVariantSelectors[i]->setVisible(false);
        // Note: nanoIntervalLabels visibility is controlled by layoutNanoControls based on simple/advanced view
    }

    // Show appropriate UI based on tuning system (only in advanced view)
    if (!showAdvancedView)
    {
        resized();  // Trigger layout refresh
        return;  // Don't show any ratio editors in simple view
    }

    for (int i = 0; i < 12; ++i)
    {
        auto* ratioParam = audioProcessor.getParameters().getRawParameterValue("nanoRatio_" + juce::String(i));
        if (ratioParam == nullptr)
            continue;  // Skip if parameter not found

        float ratioVal = ratioParam->load();

        // Check if this position has multiple interval variants
        bool hasVariants = !variants[i].empty();

        if (hasVariants)
        {
            // Show variant dropdown selector
            auto* selector = nanoVariantSelectors[i];
            selector->clear();

            // Populate dropdown with variant names
            for (const auto& variant : variants[i])
            {
                selector->addItem(variant.displayName, selector->getNumItems() + 1);
            }

            // Select closest matching variant based on current ratio
            int closestIndex = 0;
            float minDiff = std::abs(ratioVal - variants[i][0].ratio);
            for (size_t j = 1; j < variants[i].size(); ++j)
            {
                float diff = std::abs(ratioVal - variants[i][j].ratio);
                if (diff < minDiff)
                {
                    minDiff = diff;
                    closestIndex = static_cast<int>(j);
                }
            }
            selector->setSelectedItemIndex(closestIndex, juce::dontSendNotification);
            selector->setVisible(true);
        }
        else
        {
            // No variants - show appropriate ratio editor based on tuning system
            switch (tuning)
            {
                case NanoTuning::TuningSystem::EqualTemperament:
                case NanoTuning::TuningSystem::CustomSemitone:
                {
                    // Show semitone editors
                    nanoSemitoneEditors[i]->setVisible(true);
                    nanoSemitoneEditors[i]->setInputRestrictions(3, "0123456789");  // Integers only

                    // Convert ratio back to semitone: semitone = 12 * log2(ratio)
                    int semitone = static_cast<int>(std::round(12.0 * std::log2(ratioVal)));
                    semitone = juce::jlimit(0, 24, semitone);
                    nanoSemitoneEditors[i]->setText(juce::String(semitone), juce::dontSendNotification);
                    break;
                }

                case NanoTuning::TuningSystem::QuarterCommaMeantone:
                {
                    // Show read-only decimal labels
                    nanoDecimalLabels[i]->setVisible(true);
                    nanoDecimalLabels[i]->setText(juce::String(ratioVal, 3), juce::dontSendNotification);
                    break;
                }

                case NanoTuning::TuningSystem::JustIntonation:
                case NanoTuning::TuningSystem::Pythagorean:
                case NanoTuning::TuningSystem::CustomFraction:
                {
                    // Show fraction editors (numerator/denominator)
                    nanoNumerators[i]->setVisible(true);
                    nanoDenominators[i]->setVisible(true);

                    // Convert ratio to fraction
                    int num = static_cast<int>(std::round(ratioVal * 100));
                    int denom = 100;
                    int gcd = std::gcd(num, denom);
                    nanoNumerators[i]->setText(juce::String(num / gcd), juce::dontSendNotification);
                    nanoDenominators[i]->setText(juce::String(denom / gcd), juce::dontSendNotification);
                    break;
                }

                case NanoTuning::TuningSystem::CustomDecimal:
                {
                    // Show editable decimal labels (use semitone editors but allow decimal input)
                    nanoSemitoneEditors[i]->setVisible(true);
                    nanoSemitoneEditors[i]->setInputRestrictions(0, "0123456789.");  // Allow decimals
                    nanoSemitoneEditors[i]->setText(juce::String(ratioVal, 3), juce::dontSendNotification);
                    break;
                }

                default:
                    break;
            }
        }
    }
}

void NanoStuttAudioProcessorEditor::refreshComboBoxesAndRatios()
{
    // Force ComboBoxAttachments to re-sync with current parameter values
    // by recreating them - this ensures display matches actual parameter state
    tuningSystemAttachment.reset();
    tuningSystemAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "tuningSystem", tuningSystemMenu);

    scaleAttachment.reset();
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "scale", scaleMenu);

    nanoBaseAttachment.reset();
    nanoBaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "nanoBase", nanoBaseMenu);

    // Update ratio displays
    updateNanoRatioUI();
}

//==============================================================================
// Preset Management Methods
//==============================================================================

void NanoStuttAudioProcessorEditor::updatePresetMenu()
{
    presetMenu.clear();

    auto& presetManager = audioProcessor.getPresetManager();

    // Add "No Preset" option
    presetMenu.addItem("No Preset", 1);
    presetMenu.addSeparator();

    int itemId = 2;

    // Add factory presets organized by category
    auto factoryPresets = presetManager.getFactoryPresets();
    if (!factoryPresets.isEmpty())
    {
        juce::PopupMenu factoryMenu;
        juce::StringArray categories;

        // Extract unique categories
        for (const auto& preset : factoryPresets)
        {
            if (!categories.contains(preset.category))
                categories.add(preset.category);
        }

        categories.sort(true);

        // Build hierarchical menu
        for (const auto& category : categories)
        {
            juce::PopupMenu categoryMenu;

            for (const auto& preset : factoryPresets)
            {
                if (preset.category == category)
                {
                    categoryMenu.addItem(itemId++, preset.name);
                }
            }

            factoryMenu.addSubMenu(category, categoryMenu);
        }

        presetMenu.getRootMenu()->addSubMenu("Factory Presets", factoryMenu);
    }

    // Add user presets
    auto userPresets = presetManager.getUserPresets();
    if (!userPresets.isEmpty())
    {
        presetMenu.addSeparator();
        juce::PopupMenu userMenu;

        for (const auto& preset : userPresets)
        {
            userMenu.addItem(itemId++, preset.name);
        }

        presetMenu.getRootMenu()->addSubMenu("User Presets", userMenu);
    }
}

void NanoStuttAudioProcessorEditor::updatePresetNameLabel()
{
    auto& presetManager = audioProcessor.getPresetManager();

    juce::String displayName = presetManager.getCurrentPresetName();

    if (displayName.isEmpty())
    {
        presetNameLabel.setText("No Preset Loaded", juce::dontSendNotification);
    }
    else
    {
        // Add "*" if modified
        if (presetManager.isModified())
            displayName += " *";

        presetNameLabel.setText(displayName, juce::dontSendNotification);
    }
}

void NanoStuttAudioProcessorEditor::onPresetSelected()
{
    int selectedId = presetMenu.getSelectedId();

    if (selectedId == 1) // "No Preset"
    {
        audioProcessor.getPresetManager().clearCurrentPreset();
        updatePresetNameLabel();
        return;
    }

    // Find the preset corresponding to the selected ID
    auto& presetManager = audioProcessor.getPresetManager();
    auto factoryPresets = presetManager.getFactoryPresets();
    auto userPresets = presetManager.getUserPresets();

    // Combine all presets
    juce::Array<PresetInfo> allPresets;
    allPresets.addArray(factoryPresets);
    allPresets.addArray(userPresets);

    // Calculate index (account for "No Preset" and separators)
    int presetIndex = selectedId - 2;

    if (presetIndex >= 0 && presetIndex < allPresets.size())
    {
        const auto& preset = allPresets[presetIndex];
        bool success = presetManager.loadPreset(preset);

        if (success)
        {
            updatePresetNameLabel();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Load Error",
                "Failed to load preset: " + preset.name,
                "OK");
        }
    }
}

void NanoStuttAudioProcessorEditor::onSavePresetClicked()
{
    // Show dialog to get preset name
    auto* window = new juce::AlertWindow("Save Preset",
                                         "Enter a name for this preset:",
                                         juce::AlertWindow::QuestionIcon);

    window->addTextEditor("presetName", "", "Preset Name:");
    window->addComboBox("category", { "Rhythmic", "Glitchy", "Ambient", "Experimental" }, "Category:");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true, juce::ModalCallbackFunction::create([this, window](int result)
    {
        if (result == 1)
        {
            juce::String presetName = window->getTextEditorContents("presetName").trim();

            if (presetName.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Invalid Name",
                    "Please enter a valid preset name.",
                    "OK");
                return;
            }

            int categoryIndex = window->getComboBoxComponent("category")->getSelectedItemIndex();
            juce::StringArray categories = { "Rhythmic", "Glitchy", "Ambient", "Experimental" };
            juce::String category = categories[categoryIndex];

            // Check if preset already exists
            auto& presetManager = audioProcessor.getPresetManager();
            auto userPresets = presetManager.getUserPresets();

            bool presetExists = false;
            for (const auto& preset : userPresets)
            {
                if (preset.name == presetName && preset.category == category)
                {
                    presetExists = true;
                    break;
                }
            }

            // Confirm overwrite if preset exists
            if (presetExists)
            {
                bool overwrite = juce::NativeMessageBox::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Overwrite Preset?",
                    "A preset with this name already exists. Do you want to overwrite it?",
                    nullptr,
                    nullptr);

                if (!overwrite)
                    return;
            }

            // Save the preset
            bool success = presetManager.savePreset(presetName, category, "", "", presetExists);

            if (success)
            {
                updatePresetMenu(); // Refresh menu
                updatePresetNameLabel();

                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Success",
                    "Preset saved successfully!",
                    "OK");
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Save Error",
                    "Failed to save preset. Please try again.",
                    "OK");
            }
        }
    }), true);
}

void NanoStuttAudioProcessorEditor::timerCallback()
{
    // Update preset name label to reflect modification state
    updatePresetNameLabel();

    // Check if tuning system has changed
    auto* tuningSystemParam = audioProcessor.getParameters().getRawParameterValue("tuningSystem");
    if (tuningSystemParam != nullptr)
    {
        int currentTuningIndex = static_cast<int>(tuningSystemParam->load());
        if (currentTuningIndex != lastTuningSystemIndex)
        {
            lastTuningSystemIndex = currentTuningIndex;
            updateNanoRatioUI();
        }
    }

    // Check if scale has changed (affects visibility in simple view)
    auto* scaleParam = audioProcessor.getParameters().getRawParameterValue("scale");
    if (scaleParam != nullptr)
    {
        int currentScaleIndex = static_cast<int>(scaleParam->load());
        if (currentScaleIndex != lastScaleIndex)
        {
            lastScaleIndex = currentScaleIndex;
            resized();  // Trigger layout update for new visibility states
        }
    }

    // Update nano label glow effects and colors based on active/playing state
    int currentPlayingIndex = audioProcessor.getCurrentPlayingNanoRateIndex();

    for (int i = 0; i < nanoIntervalLabels.size(); ++i)
    {
        // Check if this nano rate is enabled
        bool isEnabled = audioProcessor.getParameters()
            .getRawParameterValue("nanoActive_" + juce::String(i))->load() > 0.5f;

        // Check if this is the currently playing rate
        bool isPlaying = (currentPlayingIndex == i);

        // Set border color based on enabled state (independent of playing glow)
        if (isEnabled) {
            nanoIntervalLabels[i]->setBorderColour(ColorPalette::nanoPurple);  // Bright purple for enabled
        } else {
            nanoIntervalLabels[i]->setBorderColour(ColorPalette::nanoPurple.darker(0.6f));  // Dimmed purple for disabled
        }

        // Set glow intensity based on playing state
        float glowIntensity = 0.0f;
        if (isPlaying) {
            glowIntensity = 1.0f;  // Brightest glow for currently playing rate
        } else if (isEnabled) {
            glowIntensity = 0.3f;  // Subtle glow for enabled rates
        }
        // else: 0.0f (no glow for disabled rates)

        nanoIntervalLabels[i]->setGlowIntensity(glowIntensity);
    }

    // Update repeat rate label glow effects and colors based on active/playing state
    int currentPlayingRegularIndex = audioProcessor.getCurrentPlayingRegularRateIndex();

    // Rate labels: "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32"
    auto rateLabels = juce::StringArray { "1", "1/2d", "1/2", "1/4d", "1/3", "1/4", "1/8d", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };

    for (int i = 0; i < rateProbLabels.size(); ++i)
    {
        // Check if this rate is enabled
        bool isEnabled = audioProcessor.getParameters()
            .getRawParameterValue("rateActive_" + rateLabels[i])->load() > 0.5f;

        // Check if this is the currently playing rate
        bool isPlaying = (currentPlayingRegularIndex == i);

        // Set border color based on enabled state
        if (isEnabled) {
            rateProbLabels[i]->setBorderColour(ColorPalette::rhythmicOrange);
        } else {
            rateProbLabels[i]->setBorderColour(ColorPalette::rhythmicOrange.darker(0.6f));
        }

        // Set glow intensity: playing=1.0, enabled=0.3, disabled=0.0
        float glowIntensity = 0.0f;
        if (isPlaying) {
            glowIntensity = 1.0f;
        } else if (isEnabled) {
            glowIntensity = 0.3f;
        }

        rateProbLabels[i]->setGlowIntensity(glowIntensity);
    }

    // Update quantization label glow effects
    int currentActiveQuantIndex = audioProcessor.getCurrentQuantIndex();

    // Quant labels: "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32"
    auto quantLabels = juce::StringArray { "4", "2", "1", "1/2", "1/4", "1/8d", "1/8", "1/16", "1/32" };

    for (int i = 0; i < quantProbLabels.size(); ++i)
    {
        // Check if this quant unit is enabled
        bool isEnabled = audioProcessor.getParameters()
            .getRawParameterValue("quantActive_" + quantLabels[i])->load() > 0.5f;

        // Check if this is the currently active quantization unit
        bool isActive = (currentActiveQuantIndex == i);

        // Set border color based on enabled state
        if (isEnabled) {
            quantProbLabels[i]->setBorderColour(ColorPalette::accentCyan);
        } else {
            quantProbLabels[i]->setBorderColour(ColorPalette::accentCyan.darker(0.6f));
        }

        // Set glow intensity: active=1.0, enabled=0.3, disabled=0.0
        float glowIntensity = 0.0f;
        if (isActive) {
            glowIntensity = 1.0f;
        } else if (isEnabled) {
            glowIntensity = 0.3f;
        }

        quantProbLabels[i]->setGlowIntensity(glowIntensity);
    }
}
