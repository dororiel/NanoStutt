/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DualSlider.h"
#include "AutoStutterIndicator.h"

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

        const auto& buffer = processor.getStutterBuffer();
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

class NanoPitchTuner : public juce::Component, private juce::Timer
{
public:
    NanoPitchTuner(NanoStuttAudioProcessor& p) : processor(p)
    {
        startTimerHz(30); // update 30 times per second
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        // Get current BPM from playhead
        auto playHead = processor.getPlayHead();
        double bpm = 120.0;
        if (playHead != nullptr)
        {
            if (auto posInfo = playHead->getPosition())
            {
                if (posInfo->getBpm())
                    bpm = *posInfo->getBpm();
            }
        }

        // Calculate base frequency (at ratio = 1.0, current tune)
        double currentNanoTune = processor.getParameters().getRawParameterValue("nanoTune")->load();
        double nanoBase = ((60.0 / bpm) / 16.0) / currentNanoTune;
        float frequency;
        bool isActive = processor.isUsingNanoRate();
        float storedFrequency = processor.getNanoFrequency();

        // Use stored frequency only if it's valid and we're in active mode
        if (isActive && storedFrequency > 0.0f && std::isfinite(storedFrequency))
        {
            // Use actual playing frequency
            frequency = storedFrequency;
            g.setColour(juce::Colours::lime);
        }
        else
        {
            // Use base frequency (ratio = 1.0) - shown in grey
            frequency = static_cast<float>(1.0 / nanoBase);
            g.setColour(juce::Colours::grey);
        }

        // Validate frequency
        if (!std::isfinite(frequency) || frequency <= 0.0f)
        {
            g.setColour(juce::Colours::grey);
            g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
            g.drawText("--", getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Convert frequency to MIDI note and cents
        float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
        int noteNumber = static_cast<int>(std::round(midiNote));
        float cents = (midiNote - noteNumber) * 100.0f;

        // Note names
        const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int noteIndex = ((noteNumber % 12) + 12) % 12;  // Ensure positive modulo
        int octave = (noteNumber / 12) - 1;

        // Format display string
        juce::String displayText = juce::String(noteNames[noteIndex]) + juce::String(octave);
        if (cents > 0.5f)
            displayText += " +" + juce::String(static_cast<int>(cents)) + "¢";
        else if (cents < -0.5f)
            displayText += " " + juce::String(static_cast<int>(cents)) + "¢";

        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawText(displayText, getLocalBounds(), juce::Justification::centred);
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
    AutoStutterIndicator autoStutterIndicator;
    juce::Slider autoStutterChanceSlider;
    juce::Slider reverseChanceSlider;
    juce::ComboBox autoStutterQuantMenu;

    DualSlider nanoGateDualSlider;
    DualSlider nanoShapeDualSlider;
    juce::Slider nanoSmoothSlider;
    DualSlider macroGateDualSlider, macroShapeDualSlider;
    juce::Slider macroSmoothSlider;
    juce::Slider timingOffsetSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateAttachment, nanoShapeAttachment, nanoSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateRandomAttachment, nanoShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateAttachment, macroShapeAttachment, macroSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateRandomAttachment, macroShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timingOffsetAttachment;

    // Listeners for bipolar state synchronization
    std::unique_ptr<juce::ParameterAttachment> nanoGateBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> nanoShapeBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> macroGateBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> macroShapeBipolarAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> autoStutterChanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverseChanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> autoStutterQuantAttachment;
    juce::OwnedArray<juce::Slider> rateProbSliders;
    juce::OwnedArray<juce::Label> rateProbLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> rateProbAttachments;
    
    juce::OwnedArray<juce::Slider> quantProbSliders;
    juce::OwnedArray<juce::Label> quantProbLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> quantProbAttachments;
    juce::Label chanceLabel, reverseLabel, quantLabel;
    juce::Label nanoGateLabel, nanoShapeLabel, nanoSmoothLabel;
    juce::Label macroGateLabel, macroShapeLabel, macroSmoothLabel;
    juce::Label nanoControlsLabel, macroControlsLabel;

    juce::ComboBox mixModeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixModeAttachment;
    std::unique_ptr<juce::Label> mixModeLabel;

    juce::Slider nanoBlendSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoBlendAttachment;

    juce::OwnedArray<juce::Slider> nanoRateProbSliders;
    juce::OwnedArray<juce::TextEditor> nanoNumerators;
    juce::OwnedArray<juce::TextEditor> nanoDenominators;
    juce::OwnedArray<juce::Label> nanoIntervalLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> nanoRatioAttachments;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> nanoRateProbAttachments;

    juce::ToggleButton advancedViewToggle;
    bool showAdvancedView = false;
   
    juce::Label nanoBlendLabel;
    
    juce::Slider nanoTuneSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoTuneAttachment;

    juce::Label nanoTuneLabel;

    juce::ComboBox waveshaperAlgorithmMenu;
    juce::Slider waveshaperSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveshaperAlgorithmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveshaperAttachment;
    juce::Label waveshaperLabel;

    juce::ToggleButton gainCompensationToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gainCompensationAttachment;




    StutterVisualizer visualizer;
    NanoPitchTuner tuner;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateNanoRatioFromFraction(int index);

private:
    // Stored bounds for drawing colored borders
    juce::Rectangle<int> rhythmicSlidersBounds;
    juce::Rectangle<int> nanoSlidersBounds;

    // Layout helper methods
    void layoutEnvelopeControls(juce::Rectangle<int> bounds);
    void layoutRateSliders(juce::Rectangle<int> bounds);
    void layoutNanoControls(juce::Rectangle<int> bounds);
    void layoutQuantizationControls(juce::Rectangle<int> bounds);
    void layoutRightPanel(juce::Rectangle<int> bounds);
    void layoutVisualizer(juce::Rectangle<int> bounds);
    std::vector<std::unique_ptr<juce::TextButton>> manualStutterButtons;
    std::vector<int> manualStutterRates { 4, 3, 6, 8, 12, 16, 24, 32 }; // Denominators
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NanoStuttAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessorEditor)
};
