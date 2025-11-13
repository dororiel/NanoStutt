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

        const auto& buffer = processor.getOutputBuffer();
        const auto& stateBuffer = processor.getStutterStateBuffer();
        const int bufferSize = processor.getOutputBufferSize();
        const int writePos = processor.getOutputBufferWritePos();

        if (bufferSize == 0 || buffer.getNumChannels() == 0)
            return;

        // Draw waveform with color-coding - display entire buffer directly (index 0 to bufferSize)
        const int channel = 0; // Show left/mono
        const float midY = getHeight() / 2.0f;
        const float scaleY = midY * 0.8f;

        // Sample every N samples for efficiency (adjust based on width)
        const int sampleStep = juce::jmax(1, bufferSize / (getWidth() * 2));

        // Define colors matching AutoStutterIndicator
        const juce::Colour colorNone = juce::Colours::lime;           // Green
        const juce::Colour colorRepeat = juce::Colour(0xffff9933);    // Orange
        const juce::Colour colorNano = juce::Colour(0xff9966ff);      // Purple

        // Build colored segments - display buffer directly
        int currentState = -1;
        juce::Path currentPath;
        juce::Colour currentColor = colorNone;

        for (int i = 0; i < bufferSize; i += sampleStep)
        {
            float sample = buffer.getSample(channel, i);
            int state = stateBuffer[i];

            float x = (static_cast<float>(i) / bufferSize) * getWidth();
            float y = midY - sample * scaleY;

            // Check if state changed - start new path segment
            if (state != currentState)
            {
                // Draw previous segment if it exists
                if (!currentPath.isEmpty())
                {
                    g.setColour(currentColor);
                    g.strokePath(currentPath, juce::PathStrokeType(1.5f));
                }

                // Start new segment
                currentState = state;
                currentColor = (state == 0) ? colorNone : (state == 1) ? colorRepeat : colorNano;
                currentPath.clear();
                currentPath.startNewSubPath(x, y);
            }
            else
            {
                currentPath.lineTo(x, y);
            }
        }

        // Draw final segment
        if (!currentPath.isEmpty())
        {
            g.setColour(currentColor);
            g.strokePath(currentPath, juce::PathStrokeType(1.5f));
        }

        // Draw playhead indicator (vertical line showing current write position)
        float playheadX = (static_cast<float>(writePos) / bufferSize) * getWidth();
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawLine(playheadX, 0, playheadX, static_cast<float>(getHeight()), 2.0f);
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

        // Get nano base and tuning parameters with null checks
        auto* nanoTuneParam = processor.getParameters().getRawParameterValue("nanoTune");
        auto* nanoBaseParam = processor.getParameters().getRawParameterValue("nanoBase");
        auto* nanoOctaveParam = processor.getParameters().getRawParameterValue("NanoOctave");

        if (nanoTuneParam == nullptr || nanoBaseParam == nullptr || nanoOctaveParam == nullptr)
        {
            // Parameters not initialized yet - show placeholder
            g.setColour(juce::Colours::grey);
            g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
            g.drawText("--", getLocalBounds(), juce::Justification::centred);
            return;
        }

        double currentNanoTune = nanoTuneParam->load();
        int nanoBaseValue = static_cast<int>(nanoBaseParam->load());
        NanoTuning::NanoBase nanoBase = static_cast<NanoTuning::NanoBase>(nanoBaseValue);
        float nanoOctave = nanoOctaveParam->load();
        float octaveMultiplier = std::pow(2.0f, nanoOctave);

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
            // Calculate base frequency based on nanoBase setting
            if (nanoBase == NanoTuning::NanoBase::BPMSynced)
            {
                // BPM-synced: use beat-based calculation
                double secondsPerCycle = ((60.0 / bpm) / 16.0) / currentNanoTune / octaveMultiplier;
                frequency = static_cast<float>(1.0 / secondsPerCycle);
            }
            else
            {
                // Note-based: use musical note frequency
                float noteFreq = NanoTuning::getNoteFrequency(nanoBase);
                frequency = noteFreq * currentNanoTune * octaveMultiplier;
            }

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

class NanoStuttAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       private juce::Timer
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
    DualSlider nanoOctaveDualSlider;
    juce::Slider nanoSmoothSlider;           // Hann window smoothing (Nano Envelope section)
    juce::Slider nanoEmaSlider;              // EMA filter (Damping section)
    juce::Slider nanoCycleCrossfadeSlider;   // Cycle crossfade (Damping section)
    DualSlider macroGateDualSlider, macroShapeDualSlider;
    juce::Slider macroSmoothSlider;
    juce::Slider timingOffsetSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateAttachment, nanoShapeAttachment, nanoSmoothAttachment, nanoEmaAttachment, nanoCycleCrossfadeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateRandomAttachment, nanoShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoOctaveAttachment, nanoOctaveRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateAttachment, macroShapeAttachment, macroSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateRandomAttachment, macroShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timingOffsetAttachment;

    // Listeners for bipolar state synchronization
    std::unique_ptr<juce::ParameterAttachment> nanoGateBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> nanoShapeBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> nanoOctaveBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> macroGateBipolarAttachment;
    std::unique_ptr<juce::ParameterAttachment> macroShapeBipolarAttachment;

    // Listeners for snap mode state synchronization
    std::unique_ptr<juce::ParameterAttachment> nanoGateSnapModeAttachment;
    std::unique_ptr<juce::ParameterAttachment> macroGateSnapModeAttachment;

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

    // Visibility toggle buttons (eye icons)
    juce::OwnedArray<juce::TextButton> rateActiveButtons;
    juce::OwnedArray<juce::TextButton> nanoActiveButtons;
    juce::OwnedArray<juce::TextButton> quantActiveButtons;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> rateActiveAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> nanoActiveAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> quantActiveAttachments;

    juce::Label chanceLabel, reverseLabel, quantLabel;
    juce::Label nanoGateLabel, nanoShapeLabel, nanoOctaveLabel, nanoSmoothLabel, nanoEmaLabel, nanoCycleCrossfadeLabel;
    juce::Label macroGateLabel, macroShapeLabel, macroSmoothLabel;
    juce::Label nanoControlsLabel, macroControlsLabel, dampingLabel;

    // Section labels for slider groups
    juce::Label repeatRatesLabel, nanoRatesLabel, quantizationLabel;

    juce::ComboBox mixModeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixModeAttachment;
    std::unique_ptr<juce::Label> mixModeLabel;

    juce::Slider nanoBlendSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoBlendAttachment;

    juce::OwnedArray<juce::Slider> nanoRateProbSliders;
    juce::OwnedArray<juce::TextEditor> nanoNumerators;
    juce::OwnedArray<juce::TextEditor> nanoDenominators;
    juce::OwnedArray<juce::TextEditor> nanoSemitoneEditors;  // For Equal Temperament (0-11 semitones)
    juce::OwnedArray<juce::Label> nanoDecimalLabels;  // For Quarter-comma Meantone (read-only)
    juce::OwnedArray<juce::ComboBox> nanoVariantSelectors;  // For interval variants (e.g., Aug 4th vs Dim 5th)
    juce::OwnedArray<juce::Label> nanoIntervalLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> nanoRatioAttachments;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> nanoRateProbAttachments;

    juce::ToggleButton advancedViewToggle;
    bool showAdvancedView = false;
    int lastTuningSystemIndex = -1;  // Track tuning system changes for UI updates
    int lastScaleIndex = -1;  // Track scale changes for layout updates

    juce::Label nanoBlendLabel;
    
    juce::Slider nanoTuneSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoTuneAttachment;

    juce::Label nanoTuneLabel;

    // Nano tuning system UI components
    juce::ComboBox nanoBaseMenu;
    juce::ComboBox tuningSystemMenu;
    juce::ComboBox scaleMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> nanoBaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tuningSystemAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    juce::ComboBox waveshaperAlgorithmMenu;
    juce::Slider waveshaperSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveshaperAlgorithmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveshaperAttachment;
    juce::Label waveshaperLabel;

    juce::ToggleButton gainCompensationToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gainCompensationAttachment;

    // Preset UI components
    juce::TextButton savePresetButton;
    juce::ComboBox presetMenu;
    juce::Label presetNameLabel;

    StutterVisualizer visualizer;
    NanoPitchTuner tuner;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateNanoRatioFromFraction(int index);
    void updateNanoRatioFromSemitone(int index);
    void updateNanoRatioFromVariant(int index);  // Updates ratio from variant selector choice
    void updateNanoRatioUI();  // Updates all nano ratio displays based on current tuning system
    void refreshComboBoxesAndRatios();  // Forces ComboBox attachment refresh and updates ratio displays

    // Preset management helper methods
    void updatePresetMenu();
    void updatePresetNameLabel();
    void onPresetSelected();
    void onSavePresetClicked();
    void timerCallback() override;

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
    std::vector<double> manualStutterRates { 1.0, 4.0/3.0, 2.0, 3.0, 4.0, 6.0, 16.0/3.0, 8.0, 12.0, 16.0, 24.0, 32.0 }; // Denominators

    // Reset and Randomize buttons for probability sliders
    juce::TextButton resetRateProbButton;
    juce::TextButton randomizeRateProbButton;
    juce::TextButton resetNanoProbButton;
    juce::TextButton randomizeNanoProbButton;
    juce::TextButton resetQuantProbButton;
    juce::TextButton randomizeQuantProbButton;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NanoStuttAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessorEditor)
};
