/*
  ==============================================================================

    AutoStutterIndicator.h
    A custom round button that indicates auto stutter state

    Visual states:
    - Black: Auto stutter disabled
    - Green: Auto stutter enabled but not active
    - Bright green: Currently stuttering

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AutoStutterIndicator : public juce::Component, private juce::Timer
{
public:
    AutoStutterIndicator(NanoStuttAudioProcessor& p) : processor(p)
    {
        startTimerHz(30); // Update at 30Hz for real-time feedback
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;

        // Get states
        bool isEnabled = processor.getParameters().getRawParameterValue("autoStutterEnabled")->load() > 0.5f;
        bool isStuttering = processor.isAutoStutterActive();
        bool isNanoStutter = processor.isUsingNanoRate();

        // Determine color
        juce::Colour fillColour;
        if (isStuttering && isNanoStutter)
            fillColour = juce::Colour(0xff9966ff); // Purple when nano stutter active
        else if (isStuttering)
            fillColour = juce::Colour(0xffff9933); // Orange when regular stutter active
        else if (isEnabled)
            fillColour = juce::Colours::lime; // Green when enabled but not stuttering
        else
            fillColour = juce::Colours::black; // Black when disabled

        // Draw filled circle
        g.setColour(fillColour);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);

        // Draw outline
        g.setColour(juce::Colours::darkgrey);
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2, 1.5f);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        // Toggle the parameter
        auto* param = processor.getParameters().getRawParameterValue("autoStutterEnabled");
        float currentValue = param->load();
        processor.getParameters().getParameter("autoStutterEnabled")->setValueNotifyingHost(currentValue > 0.5f ? 0.0f : 1.0f);
    }

private:
    void timerCallback() override { repaint(); }
    NanoStuttAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoStutterIndicator)
};
