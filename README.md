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
- **User-Controllable Fade Length**: Adjustable crossfade duration (0.0001ms to 30ms, default: 1.0ms)
- **Dynamic Parameter Sampling**: Timing adapts to fade length to prevent mid-fade jumps
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
- **Advanced Tuning System**: Comprehensive tuning and scale system with multiple options
  - **Nano Base**: BPM Synced or 12 chromatic note bases (C, C#, D, etc.)
  - **Tuning Systems**: Equal Temperament, Just Intonation, Pythagorean, Quarter-comma Meantone
  - **Custom Tuning Modes**: Custom (Fraction), Custom (Decimal), Custom (Semitone)
  - **Musical Scales**: 16 preset scales (Chromatic, Major, Natural Minor, Pentatonic modes, Church modes, etc.)
  - **Interval Variant Selection**: Choose between alternate interval options when available:
    - Pythagorean: Aug 4th (1.424) vs Dim 5th (1.405) at position 6
    - Just Intonation: Lesser/Greater Maj 2nd at position 2, Harmonic/Grave Min 7th at position 10
  - **Extended Ratio Range**: Ratios support up to 4.0 (2 octaves) for wider pitch range
- **Advanced View**: Toggleable advanced view showing:
  - Active/inactive state toggles for all 12 positions
  - Tuning-specific ratio editors (fractions, semitones, decimals, or variant selectors)
  - Full control over tuning system parameters

### Envelope Controls

#### Nano Envelope (Internal Loop Control)
- **Nano Gate**: Loop duration control (0.25-1.0) with Serum-style randomization (±1.0 bipolar or directional unipolar)
  - **Snap-to-Quarter Mode**: Right-click inner knob to toggle snap mode (cyan ring indicator)
  - Snap mode quantizes gate values and randomization to quarter increments (0.25, 0.5, 0.75, 1.0)
  - Snap state saved in presets for consistent behavior across sessions
- **Nano Shape**: Loop envelope curve (0.0-1.0) with Serum-style randomization (±1.0 bipolar or directional unipolar)
- **Nano Octave**: Octave offset control (-1 to +3, integer steps) with randomization (±4 octaves) for pitch shifting effects
- **Nano Smooth**: Multi-window envelope smoothing (0.0-1.0) applied per repeat cycle with selectable window types:
  - **Fixed Windows** (blended by intensity): None, Hann, Hamming, Blackman, Blackman-Harris, Bartlett
  - **Adjustable Windows** (intensity controls shape): Kaiser, Tukey, Gaussian, Planck, Exponential
  - At 0.0 = no smoothing, at 1.0 = full window applied to gated portion of envelope
  - Window type selection available in advanced view (Advanced View → Window Type dropdown)

#### Macro Envelope (Event-level Control)
- **Macro Gate**: Overall event duration (0.25-1.0) with Serum-style randomization (±1.0 bipolar or directional unipolar)
  - **Snap-to-Quarter Mode**: Right-click inner knob to toggle snap mode (cyan ring indicator)
  - Snap mode quantizes gate values and randomization to quarter increments (0.25, 0.5, 0.75, 1.0)
  - Snap state saved in presets for consistent behavior across sessions
- **Macro Shape**: Event envelope curve (0.0-1.0) with Serum-style randomization (±1.0 bipolar or directional unipolar)
- **Macro Smooth**: Multi-window envelope smoothing (0.0-1.0) applied to entire stutter event
  - Uses same window type as Nano Smooth (set in advanced view)
  - Fixed windows blend from no smoothing to full window; adjustable windows use intensity to control window shape

#### Damping Section
- **EMA Filter**: Exponential moving average low-pass filter (0.0-1.0) - alpha coefficient mapping (0.0 = bypass, 1.0 = maximum smoothing)
  - Per-channel state tracking with intelligent reset logic for reverse mode
  - Applied after nano envelope in signal chain
- **Cycle Crossfade**: Envelope-aware crossfading at nano cycle boundaries (0.0-1.0)
  - Smooths loop transitions with automatic envelope gain tracking
  - Applied before Hann windowing for click-free operation

### Mix Modes
- **Gate Mode**: Traditional gated stutter with silence gaps
- **Insert Mode**: Stutters inserted without replacing original audio
- **Mix Mode**: Blend between dry and stuttered signals

### Audio Processing
- **Waveshaping**: Built-in waveshaping with multiple algorithms (None, Soft Clip, Tanh, Hard Clip, Tube, Fold)
- **Drive**: Input gain control for waveshaping intensity (0.0-1.0, maps to 1.0x-10.0x)
  - Waveshaper processes audio at all drive levels (including 0) unless "None" algorithm is selected
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
- **Fade Length** (Advanced View): User-controllable crossfade duration (0.0001ms to 30ms, default: 1.0ms)
  - Located in left panel under window type combobox
  - Affects all crossfades: dry→wet, wet→dry, stutter→stutter transitions
  - Parameter sampling automatically adjusts timing to prevent mid-fade jumps
  - Logarithmic scale for precision at low values

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
- **User-Controllable Fade Length**: Adjustable crossfade duration (0.0001ms to 30ms, default: 1.0ms)
- **Pre-emptive Fades**: Crossfades scheduled before event boundaries to eliminate clicks
- **Dynamic Parameter Sampling**: Timing adjusts to `(fade_length + 1ms)` before events to prevent mid-fade jumps
- **Real-Time Updates**: Fade length recalculated every audio block for immediate response to parameter changes
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
- ✅ Dual envelope architecture (Nano + Macro) with snap-to-quarter mode for gate controls
- ✅ All three mix modes (Gate, Insert, Mix)
- ✅ **DAW-synced output visualizer** - color-coded waveform display with perfect timeline alignment
- ✅ Host synchronization and tempo tracking
- ✅ Real-time parameter automation
- ✅ Clean CMake build system

### Known Issues
- Manual stutter buttons may need GUI integration

### Recent Improvements
- **User-Controllable Fade Length and Dynamic Parameter Sampling** - replaced hardcoded fade timing with adjustable parameter
  - Added FadeLength parameter (0.0001ms to 30ms, default: 1.0ms) with logarithmic scale
  - Removed hardcoded FADE_DURATION_MS and PARAMETER_SAMPLE_ADVANCE_MS constants
  - fadeLengthInSamples recalculated every audio block for real-time response to parameter changes
  - Parameter sampling timing now dynamic: `(fade_length + 1ms)` before event start
  - Prevents mid-fade jumps when fade length exceeds old 2ms fixed sampling advance
  - UI control: horizontal slider in advanced view (left panel, under window type)
  - Affects all crossfades: dry→wet, wet→dry, stutter→stutter transitions
- **Waveshaper Bypass Fix** - corrected bypass logic to only check algorithm selection
  - Changed from `driveAmount > 0.0f && waveshapeAlgorithm > 0` to `waveshapeAlgorithm > 0`
  - Waveshaper now processes audio at all drive levels unless "None" algorithm is selected
  - Drive parameter correctly controls saturation intensity without affecting bypass state
- **Multi-Window System for Envelope Smoothing** - expanded from single Hann window to 11 window types
  - Fixed Windows (blended by intensity): None, Hann, Hamming, Blackman, Blackman-Harris, Bartlett
  - Adjustable Windows (intensity controls shape parameters): Kaiser, Tukey, Gaussian, Planck, Exponential
  - calculateWindowGain() replaces calculateHannWindow() with comprehensive window function library
  - Fixed windows blend from bypass to full window effect; adjustable windows use intensity as shape parameter
  - Window type selection in advanced view applies to both Nano Smooth and Macro Smooth
  - Backward compatible: Hann window (default) maintains existing behavior
- **Nano Rate Tracking Enhancements** - improved UI state tracking for visual feedback
  - Added currentPlayingNanoRateIndex atomic (-1 = not playing, 0-11 = active nano rate)
  - Enables per-rate visual indicators in UI for showing which nano rate is currently active
  - Thread-safe lock-free communication between audio and UI threads
- **Refactored Smoothing System with Hann Windowing** - separated EMA filtering from envelope smoothing
  - Renamed "Nano Smooth" → "EMA Filter" and moved to new "Damping" section with Cycle Crossfade
  - Implemented NEW "Nano Smooth" using true Hann window (0.5 * (1 - cos(2π × progress)))
  - Applied per repeat cycle to gated portion of nano envelope (resampled at wraparound)
  - Converted "Macro Smooth" from linear fade to Hann window applied to entire event envelope
  - Hann windowing brings edges to zero volume at maximum setting, making crossfades nearly invisible
  - Parameter held constant per cycle to prevent automation bleeding and crossfade mismatches
- **Fixed Critical Reverse Stutter Bugs** - comprehensive fix for crossfade and EMA issues in reverse mode
  - Fixed forward crossfade head envelope position (countdown formula: crossfadeLen - 1 - tailOffset)
  - Fixed reverse crossfade head envelope position (off-by-one: loopPos + 1)
  - Fixed reverse crossfade tail position (correct formula: loopLen - 1 - loopPos for backwards reading)
  - Fixed EMA reset logic to account for reverse envelope direction reversal at wraparound
  - EMA now always resets at reverse cycle boundaries regardless of crossfade state
  - Eliminated "exponential fading" artifact caused by incorrect tail buffer position
  - Resolved crossfade boundary discontinuities when both nano smooth and xfade are active
- **UI Layout Improvements** - optimized left panel spacing and tuner positioning
  - Increased row gap between envelope controls for better visual separation
  - Reduced knob value textbox height from 20px to 16px for more compact layout
  - Removed section gaps for tighter grouping of envelope controls
  - Repositioned tuner to match nano preset height (22px) and moved closer to visualizer (2px gap)
  - All three envelope sections (Macro, Nano, Damping) now visible without clipping
- **Added Snap-to-Quarter Mode for Gate Controls** - precision gate value control with visual feedback
  - Right-click inner knob on nano/macro gate to toggle snap mode
  - Cyan ring indicator shows when snap mode is active
  - Quantizes both main gate value and randomization amount to quarters (0.25, 0.5, 0.75, 1.0)
  - Works with all randomization modes (bipolar/unipolar)
  - Randomized values snap to quarters on processing side for consistent behavior
  - Snap state automatically saved in presets
  - Restricted to gate controls only (shape/octave controls remain smooth)
  - Bidirectional parameter synchronization for proper preset loading
- **Implemented Comprehensive Nano Tuning System** - advanced musical tuning and scale framework
  - Created TuningSystem.h with 4 historical tuning systems and 16 musical scales
  - Implemented interval variant selection system for choosing between alternate mathematical ratios
  - Added Custom Semitone mode for user-defined equal temperament tunings
  - Extended ratio range from 2.0 to 4.0 (full 2-octave range)
  - Updated tuning ratios to match historical/mathematical precision (from published tables)
  - Implemented advanced view with tuning-specific editors (fractions, semitones, decimals, variants)
  - Added automatic custom tuning detection when ratios are manually edited
  - Suppressed detection during programmatic updates (variant selection, scale changes)
  - Fixed layout issues: proper spacing in simple/advanced view, correct component visibility
  - Fixed Unicode assertion error by replacing superscripts with ASCII caret notation (3^6:2^9)
- **Implemented Nano Smooth EMA Filter** - replaced simple crossfade with exponential moving average low-pass filter
  - Configurable alpha coefficient: 0.0 = bypass (alpha=1.0), 1.0 = max smooth (alpha=0.05)
  - Per-channel EMA state tracking with smart reset at loop wraparound
  - Separate dry EMA state for seamless crossfade transitions
  - State transfer from dry crossfade to wet processing scaled by first sample envelope gain
  - Configurable signal chain position (BeforeNanoEnvelope/AfterNanoEnvelope/AfterMacroEnvelope)
- **Added Cycle Crossfade Control** - smoothing transitions at nano cycle boundaries
  - Parameter range 0.0-1.0 for adjustable crossfade intensity
  - Integrated with EMA state management for continuous smoothing through cycle transitions
  - When crossfade > 0.01, EMA state persists through loop wraparound for seamless transitions
- **Added Octave Offset and Randomization** - pitch shifting control for nano rates
  - Octave offset parameter: -1 to +3 octaves with integer snapping
  - Randomization range: ±4 octaves with bipolar/unipolar modes
  - Integer-only random generation at Decision Point 2 for event-locked behavior
  - Applied via octave multiplier (pow(2.0, octave)) to nano frequency calculation
  - Enhanced DualSlider component with setVisualRangeScale() and setRandomSensitivity() for integer parameters
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
