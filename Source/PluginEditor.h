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
#include "ModernLookAndFeel.h"
#include "ColorPalette.h"
#include "GlowEffect.h"
#include "TextureGenerator.h"
#include "RomanNumeralLabel.h"

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
        auto bounds = getLocalBounds().toFloat();
        double currentTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

        //==============================================================================
        // LAYER 1: BACKGROUND GRADIENT
        //==============================================================================
        juce::ColourGradient bgGradient(
            juce::Colour(0xff000000),
            bounds.getX(), bounds.getY(),
            juce::Colour(0xff0a0a10),
            bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(bgGradient);
        g.fillRect(bounds);

        //==============================================================================
        // LAYER 2: GRID PATTERN (Oscilloscope style)
        //==============================================================================
        g.setColour(juce::Colours::white.withAlpha(0.03f));

        // Vertical grid lines (every 1/16 note)
        int numVerticalLines = 16;
        for (int i = 0; i <= numVerticalLines; i++)
        {
            float x = bounds.getX() + (i * bounds.getWidth() / numVerticalLines);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }

        // Horizontal grid lines (amplitude markers)
        int numHorizontalLines = 5;
        for (int i = 0; i <= numHorizontalLines; i++)
        {
            float y = bounds.getY() + (i * bounds.getHeight() / numHorizontalLines);
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
        }

        // Brighter center line
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

        //==============================================================================
        // LAYER 3: WAVEFORM WITH GLOW
        //==============================================================================
        const auto& buffer = processor.getOutputBuffer();
        const auto& stateBuffer = processor.getStutterStateBuffer();
        const int bufferSize = processor.getOutputBufferSize();
        const int writePos = processor.getOutputBufferWritePos();

        if (bufferSize > 0 && buffer.getNumChannels() > 0)
        {
            const int channel = 0; // Show left/mono
            const float midY = bounds.getCentreY();
            const float scaleY = bounds.getHeight() * 0.35f;

            // Sample every N samples for efficiency
            const int sampleStep = juce::jmax(1, bufferSize / (static_cast<int>(bounds.getWidth()) * 2));

            // Define modern colors
            const juce::Colour colorNone = ColorPalette::activeGreen;
            const juce::Colour colorRepeat = ColorPalette::rhythmicOrange;
            const juce::Colour colorNano = ColorPalette::nanoPurple;

            // Build colored segments
            int currentState = -1;
            juce::Path currentPath;
            juce::Colour currentColor = colorNone;

            for (int i = 0; i < bufferSize; i += sampleStep)
            {
                float sample = buffer.getSample(channel, i);
                int state = stateBuffer[i];

                float x = bounds.getX() + (static_cast<float>(i) / bufferSize) * bounds.getWidth();
                float y = midY - sample * scaleY;

                if (state != currentState)
                {
                    // Draw previous segment with glow
                    if (!currentPath.isEmpty())
                    {
                        // Outer glow (blur effect with multiple strokes)
                        GlowEffect::drawStrokeWithGlow(g, currentPath,
                                                        currentColor, 2.5f,
                                                        currentColor.withSaturation(0.6f), 6.0f, 4);

                        // Inner highlight
                        g.setColour(juce::Colours::white.withAlpha(0.3f));
                        g.strokePath(currentPath, juce::PathStrokeType(1.0f));
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

            // Draw final segment with glow
            if (!currentPath.isEmpty())
            {
                GlowEffect::drawStrokeWithGlow(g, currentPath,
                                                currentColor, 2.5f,
                                                currentColor.withSaturation(0.6f), 6.0f, 4);

                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.strokePath(currentPath, juce::PathStrokeType(1.0f));
            }

            //==============================================================================
            // LAYER 4: REFLECTION (below waveform)
            //==============================================================================
            currentState = -1;
            currentPath.clear();
            currentColor = colorNone;

            for (int i = 0; i < bufferSize; i += sampleStep)
            {
                float sample = buffer.getSample(channel, i);
                int state = stateBuffer[i];

                float x = bounds.getX() + (static_cast<float>(i) / bufferSize) * bounds.getWidth();
                float y = midY + sample * scaleY * 0.3f; // Flipped and smaller

                if (state != currentState)
                {
                    if (!currentPath.isEmpty())
                    {
                        // Reflection with gradient fade
                        g.setColour(currentColor.withAlpha(0.15f));
                        g.strokePath(currentPath, juce::PathStrokeType(1.5f));
                    }

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

            if (!currentPath.isEmpty())
            {
                g.setColour(currentColor.withAlpha(0.15f));
                g.strokePath(currentPath, juce::PathStrokeType(1.5f));
            }

            //==============================================================================
            // LAYER 5: PLAYHEAD INDICATOR (Gradient with glow)
            //==============================================================================
            if (bufferSize > 0)
            {
                float playheadX = bounds.getX() + (static_cast<float>(writePos) / bufferSize) * bounds.getWidth();
                juce::Point<float> topPoint(playheadX, bounds.getY());
                juce::Point<float> bottomPoint(playheadX, bounds.getBottom());

                // Glow behind playhead
                GlowEffect::drawGlowingLine(g, topPoint, bottomPoint,
                                             juce::Colours::white, 2.0f,
                                             juce::Colours::white, 8.0f);

                // Top triangle marker
                juce::Path triangle;
                triangle.addTriangle(playheadX, bounds.getY(),
                                      playheadX - 5.0f, bounds.getY() + 8.0f,
                                      playheadX + 5.0f, bounds.getY() + 8.0f);
                g.setColour(juce::Colours::white);
                g.fillPath(triangle);
            }
        }

        //==============================================================================
        // LAYER 6: SCROLLING SCANLINES
        //==============================================================================
        float scrollOffset = std::fmod(static_cast<float>(currentTime * 20.0), 8.0f); // Slow scroll
        for (int i = 0; i < 15; i++)
        {
            float y = bounds.getY() + (i * 8.0f) + scrollOffset;
            if (y < bounds.getBottom())
            {
                g.setColour(juce::Colours::white.withAlpha(0.05f));
                g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
            }
        }

        //==============================================================================
        // LAYER 7: HUD CORNER BRACKETS
        //==============================================================================
        juce::Path brackets = TextureGenerator::createCornerBracket(bounds, 12, true, true, true, true);
        g.setColour(ColorPalette::accentCyan.withAlpha(0.4f));
        g.strokePath(brackets, juce::PathStrokeType(2.0f));

        //==============================================================================
        // LAYER 8: OUTER FRAME
        //==============================================================================
        g.setColour(ColorPalette::frameGrey);
        g.drawRect(bounds, 1.0f);
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
        auto bounds = getLocalBounds().toFloat();

        //==============================================================================
        // BACKGROUND - Recessed panel with gradient
        //==============================================================================
        juce::ColourGradient bgGradient = ColorPalette::createDepthGradient(bounds, ColorPalette::recessedPanel);
        g.setGradientFill(bgGradient);
        g.fillRect(bounds);

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

        juce::String displayText = "--";
        juce::Colour textColor = ColorPalette::textInactive;

        if (nanoTuneParam != nullptr && nanoBaseParam != nullptr && nanoOctaveParam != nullptr)
        {
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
                textColor = ColorPalette::activeGreen; // Glowing green when active
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

                textColor = ColorPalette::textInactive; // Grey when calculated
            }

            // Validate frequency and create display text
            if (std::isfinite(frequency) && frequency > 0.0f)
            {
                // Convert frequency to MIDI note and cents
                float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
                int noteNumber = static_cast<int>(std::round(midiNote));
                float cents = (midiNote - noteNumber) * 100.0f;

                // Note names
                const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                int noteIndex = ((noteNumber % 12) + 12) % 12;  // Ensure positive modulo
                int octave = (noteNumber / 12) - 1;

                // Format display string
                displayText = juce::String(noteNames[noteIndex]) + juce::String(octave);
                if (cents > 0.5f)
                    displayText += " +" + juce::String(static_cast<int>(cents)) + "¢";
                else if (cents < -0.5f)
                    displayText += " " + juce::String(static_cast<int>(cents)) + "¢";
            }
        }

        //==============================================================================
        // TEXT - Glowing technical font
        //==============================================================================
        auto font = juce::Font(juce::FontOptions(16.0f));
        font.setBold(true);
        g.setFont(font);

        // Draw glow behind text if active
        if (textColor == ColorPalette::activeGreen)
        {
            // Multiple passes for glow effect
            for (int i = 3; i > 0; --i)
            {
                float alpha = 0.15f * (4 - i) / 3.0f;
                g.setColour(ColorPalette::activeGlow.withAlpha(alpha));
                auto textBounds = bounds.expanded(i * 2.0f);
                g.drawText(displayText, textBounds, juce::Justification::centred);
            }
        }

        // Draw main text
        g.setColour(textColor);
        g.drawText(displayText, bounds, juce::Justification::centred);

        //==============================================================================
        // FRAME - Geometric border
        //==============================================================================
        auto framePath = TextureGenerator::createBeveledRectangle(bounds, 2.0f);

        if (textColor == ColorPalette::activeGreen)
        {
            // Glowing green border when active
            GlowEffect::drawStrokeWithGlow(g, framePath,
                                            ColorPalette::activeGreen, 1.5f,
                                            ColorPalette::activeGlow, 3.0f);
        }
        else
        {
            // Simple grey border when inactive
            g.setColour(ColorPalette::frameGrey);
            g.strokePath(framePath, juce::PathStrokeType(1.5f));
        }
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
    juce::Slider fadeLengthSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateAttachment, nanoShapeAttachment, nanoSmoothAttachment, nanoEmaAttachment, nanoCycleCrossfadeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoGateRandomAttachment, nanoShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> nanoOctaveAttachment, nanoOctaveRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateAttachment, macroShapeAttachment, macroSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroGateRandomAttachment, macroShapeRandomAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timingOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fadeLengthAttachment;

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
    juce::OwnedArray<RomanNumeralLabel> nanoIntervalLabels;  // Roman numeral SVG labels
    std::array<std::unique_ptr<juce::Drawable>, 12> nanoLabelSVGs;  // Stored SVG drawables
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

    // Window type selection for nanoSmooth (advanced view only)
    std::unique_ptr<juce::Label> windowTypeLabel;
    juce::ComboBox windowTypeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowTypeAttachment;

    // Fade length control (advanced view only)
    juce::Label fadeLengthLabel;

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
    juce::Rectangle<int> quantizationSlidersBounds;

    // SVG panel backgrounds
    std::unique_ptr<juce::Drawable> quantPanelSVG;
    std::unique_ptr<juce::Drawable> rhythmicPanelSVG;
    std::unique_ptr<juce::Drawable> nanoPanelSVG;

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

    // Modern LookAndFeel for futuristic/technical UI styling
    ModernLookAndFeel modernLookAndFeel;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NanoStuttAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoStuttAudioProcessorEditor)
};
