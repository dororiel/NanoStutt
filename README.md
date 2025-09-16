# NanoStutt - Advanced Stutter Audio Plugin

## Overview

NanoStutt is a sophisticated stutter/glitch audio plugin built with JUCE that provides intelligent, musical stuttering effects with weighted probability systems and advanced timing control.

## Core Principles

### 1. **Dual-Layer Architecture**
- **Regular Rate System**: Traditional stutter rates (1/4, 1/3, 1/6, 1/8, 1/12, 1/16, 1/24, 1/32)
- **Nano Rate System**: Fine-tuned fractional rates for micro-timing variations and pitch shifting effects

### 2. **Weighted Probability System**
- Each stutter rate has an adjustable probability weight (0.0 to 1.0)
- The system randomly selects rates based on weighted probabilities for musical variation
- **Dynamic Probabilistic Quantization**: Quantization units (1/4, 1/8, 1/16, 1/32) are selected using weighted probabilities
- Real-time quantization switching allows adaptive timing contexts

### 3. **Pre-emptive Fade System**
- **Mid-point Decision Strategy**: Auto-stutter decisions are made at the middle of quantization intervals
- **1ms Crossfades**: Pre-emptive crossfades eliminate clicks at event boundaries
- **Sample-accurate Timing**: All fade transitions are precisely timed to avoid artifacts

### 4. **Dual Envelope Architecture**
- **Nano Envelope**: Controls internal loop behavior within stutter events
- **Macro Envelope**: Controls overall stutter event shape and duration
- **Independent Shape Control**: Each envelope has gate, shape, and smooth parameters

## Current Features

### Auto Stutter System
- **Auto Stutter Toggle**: Enable/disable automatic stutter triggering
- **Stutter Chance**: Global probability (0.0-1.0) for stutter events to occur
- **Quantization Selection**: Choose timing resolution (1/4, 1/8, 1/16, 1/32 notes)
- **Weighted Rate Selection**: Each rate has individual probability settings
- **Probabilistic Quantization**: Each quantization unit (1/4, 1/8, 1/16, 1/32) has adjustable probability weights
- **Dynamic Quantization Switching**: System automatically selects next quantization unit based on weighted probabilities

### Manual Stutter Controls
- **Manual Stutter Button**: Instant stutter trigger
- **Rate-specific Buttons**: Direct access to specific stutter rates (4, 3, 6, 8, 12, 16, 24, 32)

### Nano System
- **Nano Blend**: Mix between repeat mode and nano fractional rates (0.0-1.0)
- **Nano Tune**: Global tuning adjustment for nano rates (0.75x - 2.0x)
- **12 Nano Rate Slots**: Individual probability controls for micro-timing variations
- **Fractional Ratios**: Precise timing ratios (1.0, 15/16, 5/6, 4/5, 3/4, 2/3, 3/5, 0.5, etc.)

### Envelope Controls

#### Nano Envelope (Internal Loop Control)
- **Nano Gate**: Loop duration control (0.0-1.0)
- **Nano Shape**: Loop envelope curve (0.0-1.0)
- **Nano Smooth**: Crossfade between loop repetitions (0.0-1.0) - eliminates internal clicks

#### Macro Envelope (Event-level Control)
- **Macro Gate**: Overall event duration (0.0-1.0)
- **Macro Shape**: Event envelope curve (0.0-1.0)
- **Macro Smooth**: Event boundary smoothing (0.0-1.0)

### Mix Modes
- **Gate Mode**: Traditional gated stutter with silence gaps
- **Insert Mode**: Stutters inserted without replacing original audio
- **Mix Mode**: Blend between dry and stuttered signals

### Visual Feedback
- **Stutter Visualizer**: Real-time waveform display of current stutter buffer contents

## Technical Architecture

### Core Processing
- **Sample-accurate Buffer Management**: Precise timing control for all operations
- **Stereo Processing**: Full stereo support with proper channel handling
- **Host Sync**: Automatic synchronization to DAW tempo and timing

### Fade System Implementation
- **Pre-emptive Fades**: 1ms crossfades scheduled before event boundaries
- **Multiple Fade Types**:
  - Dry to Wet (stutter event start)
  - Wet to Dry (stutter event end)
  - Wet to Silence (gated events)
  - Post-silence to Dry (after gated events)

### State Management
- **Real-time Safe Parameter Access**: Cached parameters for audio thread safety
- **Dynamic Quantization**: Adaptive timing based on current musical context with probabilistic selection
- **Lookahead Processing**: Mid-point decisions enable artifact-free transitions
- **Quantization Probability Engine**: Weighted selection system for adaptive timing resolution

## Build Instructions

### Prerequisites
- CMake 3.22 or higher
- Modern C++20 compiler
- Git (for submodules)

### Building
```bash
# Clone and setup
git clone [repository-url]
cd NanoStutt_Clean
git submodule update --init --recursive

# Build
mkdir build
cd build
cmake ..
make -j4

# Plugin outputs
# VST3: build/NanoStutt_artefacts/VST3/NanoStutt.vst3
# AU:   build/NanoStutt_artefacts/AU/NanoStutt.component
```

## Current Status

### Working Features
- ✅ Core stutter engine with sample-accurate timing
- ✅ Weighted probability system for all rates
- ✅ **Probabilistic quantization system** with dynamic timing resolution
- ✅ Pre-emptive fade system with click elimination
- ✅ Dual envelope architecture (Nano + Macro)
- ✅ All three mix modes (Gate, Insert, Mix)
- ✅ Visual stutter buffer display
- ✅ Host synchronization and tempo tracking
- ✅ Real-time parameter automation
- ✅ Clean CMake build system

### Known Issues
- Manual stutter buttons may need GUI integration
- Some edge cases in fade transitions may need refinement

### Recent Improvements (Last Commit: 4fe4226)
- Implemented pre-emptive fade system using mid-point decision strategy
- Enhanced Nano Smooth parameter for true loop crossfading
- Fixed sample-accurate timing for all stutter operations
- Eliminated clicks in most stutter scenarios
- Improved stereo processing and fade logic

## Development Notes

### Key Files
- `Source/PluginProcessor.cpp`: Core audio processing and stutter engine
- `Source/PluginProcessor.h`: Plugin interface and parameter definitions
- `Source/PluginEditor.cpp`: GUI implementation and parameter attachments

### Parameter System
The plugin uses JUCE's AudioProcessorValueTreeState with:
- Forward-slash parameter IDs (e.g., "rateProb_1/4") for compatibility
- Weighted probability arrays for real-time rate selection
- Cached parameter access for thread safety

### Debug Notes
- Built and tested with JUCE 8.0.9
- Clean parameter validation (no duplicate ID issues)
- Successfully builds VST3 and AU formats
- No segmentation faults in build process

## Future Development Opportunities

### Potential Enhancements
- Advanced pattern sequencing
- MIDI triggering for manual stutters
- Additional mix modes
- Preset management system
- Advanced visualizations
- Multi-tap delay integration

### Architecture Extensions
- Modular effect chain
- External sync options
- Advanced probability distributions
- Dynamic parameter morphing

---
