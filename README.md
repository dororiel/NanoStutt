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
- **Reverse Chance**: Probability (0.0-1.0) that stutter events will reverse after first repeat cycle
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

### Audio Processing
- **Waveshaping**: Built-in waveshaping with multiple algorithms (None, Soft Clip, Tanh, Hard Clip, Tube, Fold)
- **Drive**: Input gain control for waveshaping intensity (0.0-1.0, bypassed at 0)
- **Gain Compensation**: Optional output compensation to maintain consistent volume levels (default: off)

### User Interface
- **Grid-based Layout**: Modern responsive layout using JUCE Grid system
- **Left Panel**: Envelope controls (Macro/Nano) + utility controls (Nano Tune, Waveshaper, Timing Offset)
- **Center Panel**: Rate probability sliders (regular rates → nano rates → auto toggle → quantization)
- **Right Panel**: Chance, Repeat/Nano blend, Reverse controls + manual stutter button
- **Optimized Spacing**: Expanded nano section with improved text box and slider proportions

### Visual Feedback
- **DAW-Synced Output Visualizer**: Real-time waveform display showing final processed output
  - **Fixed 1/4 Note Window**: Displays exactly one quarter note of audio at current BPM
  - **DAW Synchronization**: PPQ-based positioning ensures perfect timeline alignment
  - **Color-Coded States**: Waveform colors indicate stutter type
    - Green: Clean audio (no stutter)
    - Orange: Repeat rate stutter active
    - Purple: Nano rate stutter active
  - **Playhead Indicator**: White vertical line shows current write position
  - **Dynamic Buffer Sizing**: Automatically adjusts window size on BPM changes
  - **Gap-Free Rendering**: Anti-aliasing ensures smooth waveform display at all BPMs

### Timing Controls
- **Timing Offset**: Manual timing offset parameter (-100ms to +100ms) for master track delay compensation in Ableton Live

### Reverse Playback System
- **Intelligent Reverse Logic**: When reverse is triggered, the first repeat cycle plays forward, then subsequent cycles play in reverse
- **Seamless Integration**: Reverse playback works with all stutter rates, quantization units, and envelope settings
- **Per-Event Decision**: Each stutter event independently decides whether to reverse based on probability
- **Example**: For a 1/8th quantization with 1/16th repeat rate marked for reverse:
  - 1st 1/16th cycle: Forward playback
  - 2nd 1/16th cycle onward: Reverse playback until stutter event ends

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
- ✅ **Perfect grid alignment** - stutters trigger exactly on quantization boundaries
- ✅ **Master track delay compensation** - manual timing offset parameter
- ✅ **Reverse playback system** - intelligent reverse logic with first-cycle-forward behavior
- ✅ **Waveshaping system** - built-in audio processing with 6 algorithms and intensity control
- ✅ **Modern Grid-based UI** - responsive layout with organized control sections
- ✅ Pre-emptive fade system with click elimination
- ✅ Dual envelope architecture (Nano + Macro)
- ✅ All three mix modes (Gate, Insert, Mix)
- ✅ **DAW-synced output visualizer** - color-coded waveform display with perfect timeline alignment
- ✅ Host synchronization and tempo tracking
- ✅ Real-time parameter automation
- ✅ Clean CMake build system

### Known Issues
- Manual stutter buttons may need GUI integration

### Recent Improvements
- **Implemented DAW-synced output visualizer** - displays final processed audio with perfect timeline alignment
  - Fixed 1/4 note viewing window sized to current BPM
  - PPQ-based buffer positioning for sample-accurate synchronization
  - Color-coded waveform indicating stutter state (green/orange/purple)
  - Playhead indicator showing current write position
  - Anti-aliasing gap-filling for smooth waveform rendering
  - Dynamic buffer resizing on BPM changes
- **Implemented JUCE DSP-based waveshaping** - switched from manual processing to JUCE DSP objects for consistency
- **Added proper wavefolding algorithm** - maintains strict ±1.0 bounds with correct signal reflection
- **Replaced Waveshape Intensity with Drive** - now controls input gain for more intuitive saturation control
- **Added Gain Compensation toggle** - optional output compensation for consistent volume levels (default: off)
- **Updated waveshaping algorithms** - replaced Asymmetric with Fold for better wavefolding behavior
- **Improved gain staging** - proper input drive and output compensation chain
- **Enhanced audio processing chain** - uses JUCE DSP processing context for optimal performance

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
