# NanoStutt Audio Plugin

## Overview

NanoStutt is a sophisticated tempo-synced stutter and audio glitch plugin built on the JUCE framework. It moves beyond traditional stutter effects by incorporating a unique dual-rhythmic system that allows for both standard musical subdivisions and experimental "tonal" repeats based on harmonic ratios. The sound can be further sculpted with a powerful dual-envelope system and flexible signal routing modes.

## Core Features

#### Advanced Stutter Engine
- **Probabilistic Triggering**: An "Auto Stutter" mode can be enabled to automatically trigger stutter events on tempo-quantized boundaries.
- **Chance Control**: The probability of an auto-stutter event occurring is controlled by a dedicated "Chance" knob.

#### Dual Rhythmic System
- **Traditional & Nano Modes**: A `Repeat/Nano` slider seamlessly blends between two different methods for determining the stutter rate.
- **Traditional Repeats**: The engine can be configured to probabilistically select from standard musical note divisions (e.g., 1/4, 1/8, 1/16, 1/32) based on user-defined weights.
- **Nano Repeats**: An experimental mode where stutter lengths are calculated as musical ratios relative to a fundamental 1/64th note. This allows for "tonal" and harmonically-aware glitch effects.
- **Nano Tuning**: The fundamental pitch of the Nano repeats can be scaled with a `Tune` knob.

#### Dual Envelope Shaper
- **Macro & Nano Envelopes**: The output gain is shaped by two independent, layered envelopes for detailed control over the sound's dynamics.
- **Macro Envelope**: Shapes the amplitude of the *entire* stutter event.
- **Nano Envelope**: Shapes the amplitude of *each individual repeat* within the event.
- **Advanced Shaping**: Both envelopes are equipped with `Gate`, `Shape`, and `Smooth` controls.
    - **Gate**: Controls the envelope's length (25% - 100%).
    - **Shape**: Morphs the envelope from a sharp pluck to a square gate to a reversed, swelling attack.
    - **Smooth**: Applies logarithmic smoothing to round off sharp transients.

#### Flexible Mix Modes
- **Gate Mode**: The default behavior. The output is either the wet (stuttered) signal or pure silence.
- **Insert Mode**: The stutter event (wet signal + subsequent silence from the gate) replaces the dry signal for its full duration. The dry signal is audible at all other times.
- **Mix Mode**: A 50/50 blend of the wet and dry signal is output during a stutter event. The dry signal is audible at all other times.

## Implementation Strategy

- **Framework**: The plugin is developed in C++ using the JUCE framework.
- **State Management**: All parameters are managed by `juce::AudioProcessorValueTreeState` (APVTS) for robust host automation and session state saving.
- **Audio Processing**: The core logic resides in the `processBlock` method of the `NanoStuttAudioProcessor` class. It operates as a state machine (`stutterLatched`, `autoStutterActive`) that determines when to record audio into a circular buffer and when to play it back. The final output is assembled on a sample-by-sample basis, taking into account the selected Mix Mode and applying the dual-envelope gains.
