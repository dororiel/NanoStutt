/*
  ==============================================================================

    DualSlider.h
    Created for Serum-style randomization control

    A rotary slider with an outer ring for randomization amount control

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * A dual-control slider component similar to Xfer Serum's modulation system.
 * Features:
 * - Inner rotary knob: main parameter value
 * - Outer ring: randomization amount (0.0 to 1.0)
 *
 * Usage:
 * - Click and drag the center area to adjust main value
 * - Click and drag the outer ring to adjust randomization amount
 */
class DualSlider : public juce::Component
{
public:
    DualSlider()
    {
        // Setup main value slider
        addAndMakeVisible(mainSlider);
        mainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        mainSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f,
                                       true);
        // Disable mouse on main slider - we'll handle it ourselves
        mainSlider.setInterceptsMouseClicks(false, false);
        mainSlider.setDoubleClickReturnValue(true, 0.5);  // Will be overridden by setDefaultValues()

        // Setup randomization slider (hidden, we'll draw it ourselves)
        // Range -1.0 to 1.0: negative = subtract, positive = add (for unipolar mode)
        addAndMakeVisible(randomSlider);
        randomSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        randomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        randomSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                         juce::MathConstants<float>::pi * 2.8f,
                                         true);
        randomSlider.setRange(-1.0, 1.0, 0.01);
        randomSlider.setValue(0.0);

        // Make random slider invisible (we'll draw it manually)
        randomSlider.setAlpha(0.0f);
        randomSlider.setInterceptsMouseClicks(false, false);

        // Listen for value changes to trigger repaints
        mainSlider.onValueChange = [this]() { repaint(); };
        randomSlider.onValueChange = [this]() { repaint(); };
    }

    // Set bipolar mode (true = ±random, false = +random only)
    void setBipolarMode(bool shouldBeBipolar)
    {
        isBipolar = shouldBeBipolar;
        repaint();
    }

    bool isBipolarMode() const { return isBipolar; }

    // Callback when bipolar mode changes via right-click
    std::function<void(bool)> onBipolarModeChange;

    // Set default values for double-click reset
    void setDefaultValues(double mainDefault, double randomDefault = 0.0)
    {
        mainDefaultValue = mainDefault;
        randomDefaultValue = randomDefault;
        mainSlider.setDoubleClickReturnValue(true, mainDefault);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();

        // Calculate dimensions
        float outerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        float randomRingRadius = outerRadius * 0.85f;

        // Always draw outer ring guide (even at 0 value) for visual feedback
        float randomAmount = static_cast<float>(randomSlider.getValue());

        float startAngle = juce::MathConstants<float>::pi * 1.2f;
        float endAngle = juce::MathConstants<float>::pi * 2.8f;
        float angleRange = endAngle - startAngle;

        // Get main slider value to determine center position
        double mainValue = mainSlider.getValue();
        double mainMin = mainSlider.getMinimum();
        double mainMax = mainSlider.getMaximum();
        float mainProportion = static_cast<float>((mainValue - mainMin) / (mainMax - mainMin));
        float centerAngle = startAngle + (mainProportion * angleRange);

        // Draw guide ring (subtle)
        juce::Path guideRing;
        guideRing.addCentredArc(centreX, centreY,
                               randomRingRadius, randomRingRadius,
                               0.0f,
                               startAngle, endAngle,
                               true);
        g.setColour(juce::Colours::grey.withAlpha(0.15f));
        g.strokePath(guideRing, juce::PathStrokeType(6.0f));

        // Draw randomization range if value != 0
        if (std::abs(randomAmount) > 0.005f)
        {
            float minAngle, maxAngle;
            juce::Colour arcColour;

            if (isBipolar)
            {
                // Bipolar: ± random amount (orange)
                float randomAngleRange = angleRange * std::abs(randomAmount);
                minAngle = centerAngle - (randomAngleRange * 0.5f);
                maxAngle = centerAngle + (randomAngleRange * 0.5f);
                arcColour = juce::Colours::orange;
            }
            else
            {
                // Unipolar: + or - random amount (green for up, blue for down)
                float randomAngleRange = angleRange * std::abs(randomAmount);
                if (randomAmount > 0.0f)
                {
                    // Positive: randomize upward
                    minAngle = centerAngle;
                    maxAngle = centerAngle + randomAngleRange;
                    arcColour = juce::Colours::lightgreen;
                }
                else
                {
                    // Negative: randomize downward
                    minAngle = centerAngle - randomAngleRange;
                    maxAngle = centerAngle;
                    arcColour = juce::Colours::lightblue;
                }
            }

            // Draw the randomization arc
            juce::Path randomArc;
            randomArc.addCentredArc(centreX, centreY,
                                   randomRingRadius, randomRingRadius,
                                   0.0f,
                                   minAngle, maxAngle,
                                   true);

            // Draw with semi-transparent color
            g.setColour(arcColour.withAlpha(0.5f));
            g.strokePath(randomArc, juce::PathStrokeType(8.0f));

            // Draw endpoints
            g.setColour(arcColour.withAlpha(0.8f));
            float endPointRadius = 3.0f;

            // Draw center point (main value indicator)
            float centerX = centreX + randomRingRadius * std::cos(centerAngle - juce::MathConstants<float>::halfPi);
            float centerY = centreY + randomRingRadius * std::sin(centerAngle - juce::MathConstants<float>::halfPi);
            g.setColour(juce::Colours::white);
            g.fillEllipse(centerX - endPointRadius, centerY - endPointRadius, endPointRadius * 2, endPointRadius * 2);

            // Min endpoint (only draw if bipolar)
            if (isBipolar)
            {
                float minX = centreX + randomRingRadius * std::cos(minAngle - juce::MathConstants<float>::halfPi);
                float minY = centreY + randomRingRadius * std::sin(minAngle - juce::MathConstants<float>::halfPi);
                g.setColour(arcColour.withAlpha(0.8f));
                g.fillEllipse(minX - endPointRadius, minY - endPointRadius, endPointRadius * 2, endPointRadius * 2);
            }

            // Max endpoint
            float maxX = centreX + randomRingRadius * std::cos(maxAngle - juce::MathConstants<float>::halfPi);
            float maxY = centreY + randomRingRadius * std::sin(maxAngle - juce::MathConstants<float>::halfPi);
            g.setColour(arcColour.withAlpha(0.8f));
            g.fillEllipse(maxX - endPointRadius, maxY - endPointRadius, endPointRadius * 2, endPointRadius * 2);
        }
        else
        {
            // Draw center point even when random amount is 0
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            float endPointRadius = 2.5f;
            float centerX = centreX + randomRingRadius * std::cos(centerAngle - juce::MathConstants<float>::halfPi);
            float centerY = centreY + randomRingRadius * std::sin(centerAngle - juce::MathConstants<float>::halfPi);
            g.fillEllipse(centerX - endPointRadius, centerY - endPointRadius, endPointRadius * 2, endPointRadius * 2);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Main slider takes full space (we handle mouse separately)
        mainSlider.setBounds(bounds);

        // Random slider is invisible but needs bounds for value storage
        randomSlider.setBounds(bounds);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Right-click toggles bipolar/unipolar mode
        if (event.mods.isRightButtonDown())
        {
            isBipolar = !isBipolar;

            // When switching to bipolar mode, convert negative values to positive
            if (isBipolar && randomSlider.getValue() < 0.0)
            {
                randomSlider.setValue(std::abs(randomSlider.getValue()), juce::sendNotificationAsync);
            }

            if (onBipolarModeChange)
                onBipolarModeChange(isBipolar);
            repaint();
            return;
        }

        dragStartValue = randomSlider.getValue();
        dragStartY = event.position.y;
        mainDragStartY = event.position.y;
        mainDragStartValue = mainSlider.getValue();

        auto pos = event.getPosition().toFloat();
        auto centre = getLocalBounds().getCentre().toFloat();
        float distance = pos.getDistanceFrom(centre);

        // Calculate radii for hit detection
        float outerRadius = juce::jmin(getWidth(), getHeight()) * 0.45f;
        float ringInnerRadius = outerRadius * 0.75f;  // Larger hit area for outer ring
        float ringOuterRadius = outerRadius * 1.1f;

        // Determine if clicking outer ring (for randomization) or inner knob (for main value)
        if (distance > ringInnerRadius && distance < ringOuterRadius)
        {
            // Outer ring - control randomization
            isDraggingRandom = true;
            isDraggingMain = false;
        }
        else
        {
            // Inner area - control main value
            isDraggingRandom = false;
            isDraggingMain = true;
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (isDraggingRandom)
        {
            updateRandomFromMouse(event);
        }
        else if (isDraggingMain)
        {
            updateMainFromMouse(event);
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        isDraggingRandom = false;
        isDraggingMain = false;
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        auto pos = event.getPosition().toFloat();
        auto centre = getLocalBounds().getCentre().toFloat();
        float distance = pos.getDistanceFrom(centre);

        // Calculate radii for hit detection
        float outerRadius = juce::jmin(getWidth(), getHeight()) * 0.45f;
        float ringInnerRadius = outerRadius * 0.75f;
        float ringOuterRadius = outerRadius * 1.1f;

        // Double-click on outer ring resets randomization to default (0)
        if (distance > ringInnerRadius && distance < ringOuterRadius)
        {
            randomSlider.setValue(randomDefaultValue, juce::sendNotificationAsync);
            repaint();
        }
        // Double-click on inner area resets main parameter to default
        else
        {
            mainSlider.setValue(mainDefaultValue, juce::sendNotificationAsync);
            repaint();
        }
    }

    // Accessors
    juce::Slider& getMainSlider() { return mainSlider; }
    juce::Slider& getRandomSlider() { return randomSlider; }

private:
    juce::Slider mainSlider;
    juce::Slider randomSlider;
    bool isDraggingRandom = false;
    bool isDraggingMain = false;
    bool isBipolar = false;  // Default to unipolar mode
    double dragStartValue = 0.0;
    float dragStartY = 0.0f;
    double mainDragStartValue = 0.0;
    float mainDragStartY = 0.0f;
    double mainDefaultValue = 0.5;  // Default value for main parameter
    double randomDefaultValue = 0.0;  // Default value for random parameter

    void updateRandomFromMouse(const juce::MouseEvent& event)
    {
        // Calculate drag distance from start position
        float dragDistance = event.position.y - dragStartY;

        // Vertical drag changes random amount
        // Drag up (negative distance) = increase
        // Drag down (positive distance) = decrease
        float sensitivity = 0.003f;
        double newValue;

        if (isBipolar)
        {
            // Bipolar mode: only positive values (0 to 1), drag up increases magnitude
            newValue = juce::jlimit(0.0, 1.0, std::abs(dragStartValue) - dragDistance * sensitivity);
        }
        else
        {
            // Unipolar mode: allow negative values (-1 to 1)
            // Drag up = positive (add), drag down = negative (subtract)
            newValue = juce::jlimit(-1.0, 1.0, dragStartValue - dragDistance * sensitivity);
        }

        randomSlider.setValue(newValue, juce::sendNotificationAsync);
        repaint();
    }

    void updateMainFromMouse(const juce::MouseEvent& event)
    {
        // Calculate drag distance from start position
        float dragDistance = event.position.y - mainDragStartY;

        // Vertical drag changes main value (drag down = decrease, drag up = increase)
        // Use the slider's range for proper scaling
        double range = mainSlider.getMaximum() - mainSlider.getMinimum();
        float sensitivity = range * 0.005f;  // 0.5% of range per pixel
        double newValue = juce::jlimit(mainSlider.getMinimum(), mainSlider.getMaximum(),
                                      mainDragStartValue - dragDistance * sensitivity);

        mainSlider.setValue(newValue, juce::sendNotificationAsync);
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualSlider)
};
