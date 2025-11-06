/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NanoStuttAudioProcessor::NanoStuttAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager(parameters)
{
    initializeParameterListeners();
}

NanoStuttAudioProcessor::~NanoStuttAudioProcessor()
{
}

//==============================================================================
const juce::String NanoStuttAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NanoStuttAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NanoStuttAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NanoStuttAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NanoStuttAudioProcessor::getTailLengthSeconds() const
{
    return 0.1;
}

int NanoStuttAudioProcessor::getNumPrograms()
{
    return 1;
}

int NanoStuttAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NanoStuttAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NanoStuttAudioProcessor::getProgramName (int index)
{
    return {};
}

void NanoStuttAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NanoStuttAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Validate input parameters
    jassert(sampleRate > 0.0);
    jassert(samplesPerBlock > 0);

    if (sampleRate <= 0.0 || samplesPerBlock <= 0)
    {
        DBG("NanoStuttAudioProcessor::prepareToPlay - Invalid parameters: sampleRate="
            << sampleRate << ", samplesPerBlock=" << samplesPerBlock);
        return;
    }

    maxStutterLenSamples = static_cast<int>(sampleRate * MAX_STUTTER_BUFFER_SECONDS);
    stutterBuffer.setSize(getTotalNumOutputChannels(), maxStutterLenSamples, false, true, true);
    fadeLengthInSamples = static_cast<int>(sampleRate * FADE_DURATION_SECONDS);

    // Initialize EMA state for nano smooth filter
    nanoEmaState.resize(getTotalNumOutputChannels(), 0.0f);
    dryEmaStateForFade.resize(getTotalNumOutputChannels(), 0.0f);
    currentNanoEmaAlpha = 1.0f; // Start with bypass (no smoothing)
    shouldResetEmaState = false;
    isFirstReverseCycle = false;

    // Initialize output visualization buffer (dynamically sized to 1/4 note at current BPM)
    resizeOutputBufferForBpm(120.0, sampleRate);

    // Initialize JUCE DSP ProcessorChain
    dspSpec.sampleRate = sampleRate;
    dspSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    dspSpec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    waveshaperChain.prepare(dspSpec);

    // Set up waveshaper function (will be updated based on algorithm parameter)
    waveshaperChain.get<waveShaperIndex>().functionToUse = [](float x) { return x; }; // Default to no processing

    // Initialize smoothed parameters
    float smoothingTime0_3ms = 0.3f / 1000.0f;  // 0.3ms for fast response (prevents bleeding across events)

    // Real-time envelope parameters (0.3ms smoothing)
    smoothedNanoGate.reset(sampleRate, smoothingTime0_3ms);
    smoothedNanoShape.reset(sampleRate, smoothingTime0_3ms);
    smoothedNanoSmooth.reset(sampleRate, smoothingTime0_3ms);
    smoothedMacroGate.reset(sampleRate, smoothingTime0_3ms);
    smoothedMacroShape.reset(sampleRate, smoothingTime0_3ms);
    smoothedMacroSmooth.reset(sampleRate, smoothingTime0_3ms);

    // Held gate parameters (0.3ms smoothing)
    smoothedHeldNanoGate.reset(sampleRate, smoothingTime0_3ms);
    smoothedHeldMacroGate.reset(sampleRate, smoothingTime0_3ms);

    // Set initial values from parameters
    smoothedNanoGate.setCurrentAndTargetValue(parameters.getRawParameterValue("NanoGate")->load());
    smoothedNanoShape.setCurrentAndTargetValue(parameters.getRawParameterValue("NanoShape")->load());
    smoothedNanoSmooth.setCurrentAndTargetValue(parameters.getRawParameterValue("NanoSmooth")->load());
    smoothedMacroGate.setCurrentAndTargetValue(parameters.getRawParameterValue("MacroGate")->load());
    smoothedMacroShape.setCurrentAndTargetValue(parameters.getRawParameterValue("MacroShape")->load());
    smoothedMacroSmooth.setCurrentAndTargetValue(parameters.getRawParameterValue("MacroSmooth")->load());

    smoothedHeldNanoGate.setCurrentAndTargetValue(parameters.getRawParameterValue("NanoGate")->load());
    smoothedHeldMacroGate.setCurrentAndTargetValue(parameters.getRawParameterValue("MacroGate")->load());

    // Initialize current and next envelope parameters from current parameter values
    currentMacroGateParam = parameters.getRawParameterValue("MacroGate")->load();
    currentMacroShapeParam = parameters.getRawParameterValue("MacroShape")->load();
    currentMacroSmoothParam = parameters.getRawParameterValue("MacroSmooth")->load();
    currentNanoGateParam = parameters.getRawParameterValue("NanoGate")->load();
    currentNanoShapeParam = parameters.getRawParameterValue("NanoShape")->load();
    currentNanoSmoothParam = parameters.getRawParameterValue("NanoSmooth")->load();

    nextMacroGateParam = currentMacroGateParam;
    nextMacroShapeParam = currentMacroShapeParam;
    nextMacroSmoothParam = currentMacroSmoothParam;
    nextNanoGateParam = currentNanoGateParam;
    nextNanoShapeParam = currentNanoShapeParam;
    nextNanoSmoothParam = currentNanoSmoothParam;
}

void NanoStuttAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NanoStuttAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    if (layouts.getMainOutputChannelSet().size() != 1 && layouts.getMainOutputChannelSet().size() != 2)
        return false;

    return true;
  #endif
}
#endif

void NanoStuttAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples             = buffer.getNumSamples();
    auto sampleRate             = getSampleRate();

    // Check if host transport is playing - only process stuttering when transport is running
    auto playHead = getPlayHead();
    if (playHead == nullptr) {
        // No playhead info available, pass through dry audio
        return;
    }

    auto position = playHead->getPosition();
    bool isPlaying = position && position->getIsPlaying();
    double currentPpqPosition = position ? position->getPpqPosition().orFallback(0.0) : 0.0;

    // Get parameters early for transport reset logic
    auto& params = parameters;
    float chance = params.getRawParameterValue("autoStutterChance")->load();

    // Copy cached parameters for thread safety - ensures consistent values throughout buffer
    auto cachedRegularWeights = regularRateWeights;
    auto cachedNanoWeights = nanoRateWeights;
    auto cachedQuantWeights = quantUnitWeights;
    float cachedNanoBlend = nanoBlend;

    // TRANSPORT STATE DETECTION AND QUANTIZATION RESET
    if (!isPlaying) {
        // Transport is not playing, pass through dry audio and reset stutter state
        autoStutterActive = false;
        parametersHeld = false;
        wasPlaying = false;
        writePos = 0;
        currentlyUsingNanoRate.store(false);
        currentNanoFrequency.store(0.0f);
        return;
    }

    // Check for transport restart or position jump (using RAW PPQ, before timing offset)
    bool transportJustStarted = !wasPlaying && isPlaying;
    bool positionJumped = wasPlaying && std::abs(currentPpqPosition - lastPpqPosition) > THIRTY_SECOND_NOTE_PPQ; // Allow small timing variations

    if (transportJustStarted || positionJumped) {
        // Reset quantization alignment based on new PPQ position
        // Find the smallest active quantization unit to align to its grid
        static const std::array<double, 9> quantUnits = {
            QUARTER_NOTE_PPQ * 16.0,       // 4bar
            QUARTER_NOTE_PPQ * 8.0,        // 2bar
            QUARTER_NOTE_PPQ * 4.0,        // 1bar
            QUARTER_NOTE_PPQ * 2.0,        // 1/2
            QUARTER_NOTE_PPQ,              // 1/4
            QUARTER_NOTE_PPQ * 0.75,       // d1/8 (dotted eighth)
            THIRTY_SECOND_NOTE_PPQ * 2.0,  // 1/8
            THIRTY_SECOND_NOTE_PPQ,        // 1/16
            THIRTY_SECOND_NOTE_PPQ / 2.0   // 1/32
        };
        static const std::array<int, 9> quantToNewBeatValues = {128, 64, 32, 16, 8, 6, 4, 2, 1}; // 1/32nds per unit

        // Find smallest active quantization unit (highest frequency)
        double activeQuantUnit = THIRTY_SECOND_NOTE_PPQ * 2.0; // Default to 1/8th
        int activeQuantToNewBeat = 4;   // Default to 1/8th (4 x 1/32nds)

        // Debug: ensure we're finding the right quantization unit (commented out - working correctly)

        for (size_t i = cachedQuantWeights.size(); i > 0; --i) {
            if (cachedQuantWeights[i-1] > 0.0f) {
                activeQuantUnit = quantUnits[i-1];
                activeQuantToNewBeat = quantToNewBeatValues[i-1];

                // Debug output for transport restart quantization
                static const std::array<const char*, 9> quantLabels = {"4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32"};
                DBG("[TRANSPORT RESTART] Active quant unit: " << quantLabels[i-1] << " (index " << (i-1)
                    << ") | quantToNewBeat: " << activeQuantToNewBeat);

                break; // Found smallest (work backwards from 1/32 to 1/4)
            }
        }

        // Calculate alignment to the active quantization unit's grid
        // (Note: activeQuantNotes calculation removed as it was unused)

        quantToNewBeat = activeQuantToNewBeat;
        // Convert from active quant units to 1/32nd note positions
        // Don't use modulo here - let quantCount reach quantToNewBeat and trigger events
        double adjustedPpqPosition = currentPpqPosition;
        // Note: timing offset will be applied later, but we need consistent alignment
        double thirtySecondNotes = adjustedPpqPosition / THIRTY_SECOND_NOTE_PPQ;
        int totalThirtySeconds = static_cast<int>(std::floor(thirtySecondNotes));
        // Set quantCount to align with the quantization boundary
        // For 1/8th (quantToNewBeat=4): trigger should happen ON the boundary, not after
        int currentBoundary = (totalThirtySeconds / quantToNewBeat) * quantToNewBeat;
        quantCount = totalThirtySeconds - currentBoundary;

        // If we're exactly on a boundary, trigger immediately
        if (quantCount == 0) {
            quantCount = quantToNewBeat; // Will trigger immediately
        }



        // Reset all stutter and timing variables
        //autoStutterActive = false;
        //stutterIsScheduled = false;
        //autoStutterRemainingSamples = 0;
        //postStutterSilence = 0;
        //parametersHeld = false;
        //macroEnvelopeCounter = 1;
        //writePos = 0;

    }

    // Update transport state tracking (using RAW PPQ position, not offset-adjusted)
    wasPlaying = isPlaying;
    lastPpqPosition = currentPpqPosition; // Store RAW position for accurate jump detection

    bool autoStutter  = params.getRawParameterValue("autoStutterEnabled")->load();
    auto mixMode      = (int) params.getRawParameterValue("MixMode")->load();

    // Update smoothed real-time parameters (0.3ms smoothing for fast response, prevents bleeding across events)
    smoothedNanoGate.setTargetValue(params.getRawParameterValue("NanoGate")->load());
    smoothedNanoShape.setTargetValue(params.getRawParameterValue("NanoShape")->load());
    smoothedNanoSmooth.setTargetValue(params.getRawParameterValue("NanoSmooth")->load());
    smoothedMacroGate.setTargetValue(params.getRawParameterValue("MacroGate")->load());
    smoothedMacroShape.setTargetValue(params.getRawParameterValue("MacroShape")->load());
    smoothedMacroSmooth.setTargetValue(params.getRawParameterValue("MacroSmooth")->load());

    double ppqAtStartOfBlock = 0.0;
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            ppqAtStartOfBlock = position->getPpqPosition().orFallback(0.0);
            bpm = position->getBpm().orFallback(120.0);
        }

    // Check if BPM changed and resize output buffer if needed
    if (std::abs(bpm - lastKnownBpm) > 0.01) // Small threshold to avoid constant resizing
    {
        resizeOutputBufferForBpm(bpm, sampleRate);
    }

    // Apply timing offset for master track delay compensation
    // NOTE: Applied AFTER transport state tracking to avoid corrupting jump detection
    float timingOffsetMs = parameters.getRawParameterValue("TimingOffset")->load();
    double timingOffsetSamples = (timingOffsetMs / 1000.0) * sampleRate;
    double timingOffsetPpq = (timingOffsetSamples / sampleRate) * (bpm / SECONDS_PER_MINUTE);
    ppqAtStartOfBlock += timingOffsetPpq;

    double ppqPerSample = (bpm / SECONDS_PER_MINUTE) / sampleRate;

    // True stereo buffer capture - preserve stereo separation with circular buffer handling
    if (maxStutterLenSamples > 0 && numSamples > 0) {
        for (int ch = 0; ch < totalNumOutputChannels && ch < stutterBuffer.getNumChannels(); ++ch) {
            int sourceChannel = juce::jmin(ch, buffer.getNumChannels() - 1);

            if (writePos + numSamples <= maxStutterLenSamples) {
                // Simple case: no wraparound needed
                stutterBuffer.copyFrom(ch, writePos, buffer, sourceChannel, 0, numSamples);
            } else {
                // Wraparound case: split the copy into two parts
                int firstPartSize = maxStutterLenSamples - writePos;
                int secondPartSize = numSamples - firstPartSize;

                if (firstPartSize > 0) {
                    // Copy first part (to end of buffer)
                    stutterBuffer.copyFrom(ch, writePos, buffer, sourceChannel, 0, firstPartSize);
                }

                if (secondPartSize > 0) {
                    // Copy second part (from start of buffer)
                    stutterBuffer.copyFrom(ch, 0, buffer, sourceChannel, firstPartSize, secondPartSize);
                }
            }
        }
    }

    // =================================================================================
    // MAIN PROCESSING LOOP - THREE DECISION POINT ARCHITECTURE
    //
    // Decision Point 1: START OF STUTTER EVENT (at beat boundaries)
    //   - Decide nano vs rhythmical system
    //   - Select rate from chosen system
    //   - Update engagement flags and quantization
    //   - Schedule next event and quantization unit
    //
    // Decision Point 2: PARAMETER SAMPLING (2ms before event end)
    //   - Sample macro envelope controls for NEXT stutter event
    //
    // Decision Point 3: FADE CONTROL (1ms before event boundaries)
    //   - Calculate wet/dry crossfading gains
    //   - Handle transitions between stutter and dry states
    // =================================================================================

    for (int i = 0; i < numSamples; ++i)
    {
        double currentPpq = ppqAtStartOfBlock + (i * ppqPerSample);

        // Fixed quantization logic: 1/32nd static beat detection
        double staticQuantUnit = THIRTY_SECOND_NOTE_PPQ;
        
        
        // Calculate timing for all decision points
        double quantizedBeat = std::floor(currentPpq / staticQuantUnit);
        double nextBeatPpq = (quantizedBeat + 1.0) * staticQuantUnit;
        int samplesToNextBeat = static_cast<int>((nextBeatPpq - currentPpq) / ppqPerSample);
        bool isNewBeat = (quantizedBeat != lastQuantizedBeat);
       
        
        // =============================================================================
        // STUTTER COMPLETION CHECK (must happen before Decision Point 1)
        // Handle stutter event completion and state reset BEFORE starting new events
        // =============================================================================


        // End stutter event when duration expires (SIMPLIFIED: only auto-stutter)
        if (autoStutterActive && autoStutterRemainingSamples <= 0)
        {

            autoStutterActive = false;
            parametersHeld = false; // Reset for next event

            // Reset nano rate tracking
            currentlyUsingNanoRate.store(false);
            currentNanoFrequency.store(0.0f);

            // Add post-stutter silence if macro gate < 1.0
            // Use CURRENT parameters (the event that just ended)
            if (currentMacroGateParam < 1.0f) {
                postStutterSilence = fadeLengthInSamples;
            }
        }


        // =============================================================================
        // DECISION POINT 1: START OF STUTTER EVENT
        // At the start of a stutter event:
        // - Decide if nano or rhythmical system will be used
        // - Select which rate from the chosen system
        // - Update stutter engagement flag and current quant unit
        // - Use held envelope values from Decision Point 2
        // - Decide next event scheduling and quantization
        // =============================================================================
        if (isNewBeat)
        {
            ++quantCount;
            lastQuantizedBeat = quantizedBeat;


            if (quantCount >= quantToNewBeat){
                if (postStutterSilence > 0) postStutterSilence = 0;
                
                // Update quantization from previous decision
                if (currentQuantIndex != nextQuantIndex) {
                    currentQuantIndex = nextQuantIndex;

                    // Use lookup table for quantToNewBeat values (matches quantization system)
                    // Array order: 4bar, 2bar, 1bar, 1/2, 1/4, d1/8, 1/8, 1/16, 1/32
                    static const std::array<int, 9> quantToNewBeatValues = {128, 64, 32, 16, 8, 6, 4, 2, 1};
                    quantToNewBeat = quantToNewBeatValues[currentQuantIndex];

                    // Debug output
                    static const std::array<const char*, 9> quantLabels = {"4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32"};
                    DBG("[QUANT UPDATE] Index: " << currentQuantIndex << " (" << quantLabels[currentQuantIndex]
                        << ") | quantToNewBeat: " << quantToNewBeat);
                }
                
                // Reset quantCount based on current position, not arbitrarily to 0
                // This prevents timing drift between consecutive stutters
                double currentPpqInLoop = ppqAtStartOfBlock + (i * ppqPerSample);
                double thirtySecondNotes = currentPpqInLoop / THIRTY_SECOND_NOTE_PPQ;
                int totalThirtySeconds = static_cast<int>(std::floor(thirtySecondNotes));
                int currentBoundary = (totalThirtySeconds / quantToNewBeat) * quantToNewBeat;
                quantCount = totalThirtySeconds - currentBoundary;
                
                // Calculate stutter event duration for this quantization unit
                float gateScale = params.getRawParameterValue("autoStutterGate")->load();
                double quantDurationSeconds = (WHOLE_NOTE_SECONDS_MULTIPLIER / bpm) * staticQuantUnit * (quantToNewBeat-quantCount);
                double gateDurationSeconds = juce::jlimit(quantDurationSeconds / 8.0, quantDurationSeconds, quantDurationSeconds * gateScale);
                stutterEventLengthSamples = static_cast<int>(sampleRate * gateDurationSeconds);
                
                // ACTIVATE SCHEDULED STUTTER EVENT
                if (stutterIsScheduled)
                {

                    // Engage stutter with rate system selection
                    autoStutterActive = true;
                    secondsPerWholeNote = WHOLE_NOTE_SECONDS_MULTIPLIER / bpm;

                    // DECISION: Whether this stutter event should be reversed
                    float reverseChance = parameters.getRawParameterValue("reverseChance")->load();
                    currentStutterIsReversed = juce::Random::getSystemRandom().nextFloat() < reverseChance;
                    firstRepeatCyclePlayed = false;
                    cycleCompletionCounter = 0; // Reset cycle counter for new stutter event
                    lastLoopPos = -1; // Reset cycle detection for new stutter event


                    // DECISION: Nano vs Rhythmical system selection
                    bool useNano = juce::Random::getSystemRandom().nextFloat() < cachedNanoBlend;
                    int selectedIndex = useNano ? selectWeightedIndex(cachedNanoWeights, 0) : selectWeightedIndex(cachedRegularWeights, 0);
                    
                    // DECISION: Rate selection from chosen system
                    if (useNano) {
                        double currentNanoTune = params.getRawParameterValue("nanoTune")->load();
                        double octaveMultiplier = std::pow(2.0, currentNanoOctaveParam);

                        double nanoBase;
                        if (currentNanoBase == NanoTuning::NanoBase::BPMSynced) {
                            // Original BPM-synced calculation
                            nanoBase = ((SECONDS_PER_MINUTE / bpm) / 16.0) / currentNanoTune / octaveMultiplier;
                        } else {
                            // Note-based frequency calculation
                            float noteFreq = NanoTuning::getNoteFrequency(currentNanoBase);
                            if (noteFreq > 0.0f) {
                                nanoBase = (1.0 / noteFreq) / currentNanoTune / octaveMultiplier;
                            } else {
                                // Fallback to BPM-synced if something goes wrong
                                nanoBase = ((SECONDS_PER_MINUTE / bpm) / 16.0) / currentNanoTune / octaveMultiplier;
                            }
                        }

                        double sliceDuration = nanoBase / runtimeNanoRatios[selectedIndex];
                        chosenDenominator = WHOLE_NOTE_SECONDS_MULTIPLIER / (bpm * sliceDuration);

                        // Update nano rate tracking for tuner
                        currentlyUsingNanoRate.store(true);
                        currentNanoFrequency.store(static_cast<float>(1.0 / sliceDuration));
                    } else {
                        chosenDenominator = regularDenominators[selectedIndex];

                        // Not using nano rate
                        currentlyUsingNanoRate.store(false);
                        currentNanoFrequency.store(0.0f);
                    }
                    
                    autoStutterRemainingSamples = stutterEventLengthSamples;
                    stutterIsScheduled = false;

                    // RESET MACRO ENVELOPE for each new stutter event (including continuous stuttering)
                    macroEnvelopeCounter = 1; // Start at 1 to avoid zero-progress spikes

                    // Update macro envelope duration for this quantization unit
                    double quantDurationSeconds = (SECONDS_PER_MINUTE / bpm) * staticQuantUnit * (quantToNewBeat-quantCount);
                    int quantUnitLengthSamples = static_cast<int>(sampleRate * quantDurationSeconds);
                    macroEnvelopeLengthInSamples = quantUnitLengthSamples;

                    // CAPTURE FRESH AUDIO for each new stutter event (including continuous stuttering)
                    stutterPlayCounter = 0;
                    stutterWritePos = (writePos+i) % maxStutterLenSamples;

                    // SWAP NEXT → CURRENT: New event starts, so next event parameters become current
                    currentMacroGateParam = nextMacroGateParam;
                    currentMacroShapeParam = nextMacroShapeParam;
                    currentMacroSmoothParam = nextMacroSmoothParam;

                    // Update smoothed held macroGate target NOW (at exact event start, not 2ms before)
                    // This prevents the 3ms transition from bleeding into the end of the previous event
                    smoothedHeldMacroGate.setTargetValue(currentMacroGateParam);

                    // SWAP NEXT → CURRENT for nano parameters
                    // The random offsets were already calculated and stored at Decision Point 2 (2ms before)
                    // This ensures fades use the correct randomized values
                    currentNanoGateParam = nextNanoGateParam;
                    currentNanoShapeParam = nextNanoShapeParam;
                    currentNanoSmoothParam = nextNanoSmoothParam;
                    currentNanoOctaveParam = nextNanoOctaveParam;

                    // Note: heldNanoGateRandomOffset and heldNanoShapeRandomOffset were already set at Decision Point 2
                    // These offsets will be added to per-cycle base values throughout the event

                    smoothedHeldNanoGate.setTargetValue(currentNanoGateParam);

                    // Pre-calculate nano envelope length for first cycle
                    float nanoGateMultiplier = NANO_GATE_MIN + currentNanoGateParam * NANO_GATE_RANGE;
                    int loopLen = std::clamp(static_cast<int>((secondsPerWholeNote / chosenDenominator) * sampleRate + 1), 1, maxStutterLenSamples);
                    heldNanoEnvelopeLengthInSamples = std::max(1, static_cast<int>((float)loopLen * nanoGateMultiplier));

                    // Transfer EMA state from crossfade to wet processing for seamless continuation
                    // Scale by first sample's envelope gain to prevent jumps when shape curve starts near zero
                    if (currentNanoSmoothParam > 0.0f) {  // Only if EMA filtering is active
                        // Calculate what the first sample's nano envelope gain will be (progress = 0.0)
                        float firstSampleNanoGain = calculateEnvelopeGain(0.0f, currentNanoShapeParam);

                        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
                            // Scale transferred state by first sample gain to respect envelope curve
                            nanoEmaState[ch] = dryEmaStateForFade[ch] * firstSampleNanoGain;
                        }
                    }
                    // Note: shouldResetEmaState remains false, allowing smooth continuation

                }
                else autoStutterActive = false;
                
                // SCHEDULE NEXT STUTTER EVENT
                float randomValue = juce::Random::getSystemRandom().nextFloat();
                if (autoStutter && randomValue < chance) {
                    stutterIsScheduled = true;
                } else {
                    stutterIsScheduled = false; // Explicitly set to false when not scheduling
                }

                // DECIDE NEXT QUANTIZATION UNIT for future events
                // Default to 1/8th (index 6, not 1!) when all weights are 0
                nextQuantIndex = selectWeightedIndex(cachedQuantWeights, 6);

                // Debug output for quant selection
                static const std::array<const char*, 9> quantLabels = {"4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32"};
                DBG("[QUANT SELECT] Index: " << nextQuantIndex << " (" << quantLabels[nextQuantIndex] << ") | "
                    << "Weights: [" << cachedQuantWeights[0] << "," << cachedQuantWeights[1] << ","
                    << cachedQuantWeights[2] << "," << cachedQuantWeights[3] << "," << cachedQuantWeights[4] << ","
                    << cachedQuantWeights[5] << "," << cachedQuantWeights[6] << "," << cachedQuantWeights[7] << ","
                    << cachedQuantWeights[8] << "]");


            }
                
        }
 
        // =============================================================================
        // DECISION POINT 2: PARAMETER SAMPLING (2ms before stutter event BEGINS)
        // Sample macro envelope controls 2ms before ANY stutter event starts
        // This ensures the next stutter event uses fresh parameter values
        // =============================================================================
        int twoMsInSamples = static_cast<int>(sampleRate * (PARAMETER_SAMPLE_ADVANCE_MS / 1000.0));
        // parametersSampledForUpcomingEvent is now a member variable (prevent multiple sampling for same upcoming event)

        // Check if a stutter event will start soon (using corrected quantization boundary logic)
        bool stutterStartingSoon = (stutterIsScheduled && quantCount >= std::max(1, quantToNewBeat - 1) && samplesToNextBeat <= twoMsInSamples);

        if (stutterStartingSoon) {
            if (!parametersSampledForUpcomingEvent) {
                // Sample ALL macro envelope parameters into NEXT event parameters
                // These will be swapped to current when the new event starts
                // This prevents automation bleeding - new values only apply to upcoming events

                // Sample MacroGate with randomization
                float macroGateBase = params.getRawParameterValue("MacroGate")->load();
                float macroGateRandom = params.getRawParameterValue("MacroGateRandom")->load();
                bool macroGateBipolar = params.getRawParameterValue("MacroGateRandomBipolar")->load() > 0.5f;
                float gateRandomOffset;
                if (macroGateBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    gateRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(macroGateRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    // If positive: randomize 0 to +value, if negative: randomize -value to 0
                    if (macroGateRandom > 0.0f)
                        gateRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroGateRandom;
                    else
                        gateRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroGateRandom;
                }
                nextMacroGateParam = juce::jlimit(0.25f, 1.0f, macroGateBase + gateRandomOffset);

                // Snap to quarters if snap mode is enabled
                if (params.getRawParameterValue("MacroGateSnapMode")->load() > 0.5f)
                    nextMacroGateParam = std::round(nextMacroGateParam / 0.25f) * 0.25f;

                // Sample MacroShape with randomization
                float macroShapeBase = params.getRawParameterValue("MacroShape")->load();
                float macroShapeRandom = params.getRawParameterValue("MacroShapeRandom")->load();
                bool macroShapeBipolar = params.getRawParameterValue("MacroShapeRandomBipolar")->load() > 0.5f;
                float shapeRandomOffset;
                if (macroShapeBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    shapeRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(macroShapeRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    // If positive: randomize 0 to +value, if negative: randomize -value to 0
                    if (macroShapeRandom > 0.0f)
                        shapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroShapeRandom;
                    else
                        shapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroShapeRandom;
                }
                nextMacroShapeParam = juce::jlimit(0.0f, 1.0f, macroShapeBase + shapeRandomOffset);

                nextMacroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();

                // Sample nano parameters with randomization for NEXT stutter event
                // Calculate random offsets (these will be held for the entire upcoming event)

                // Calculate NanoGate random offset
                float nanoGateRandom = params.getRawParameterValue("NanoGateRandom")->load();
                bool nanoGateBipolar = params.getRawParameterValue("NanoGateRandomBipolar")->load() > 0.5f;
                float nanoGateRandomOffset;
                if (nanoGateBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    nanoGateRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(nanoGateRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    if (nanoGateRandom > 0.0f)
                        nanoGateRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoGateRandom;
                    else
                        nanoGateRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoGateRandom;
                }

                // Calculate NanoShape random offset
                float nanoShapeRandom = params.getRawParameterValue("NanoShapeRandom")->load();
                bool nanoShapeBipolar = params.getRawParameterValue("NanoShapeRandomBipolar")->load() > 0.5f;
                float nanoShapeRandomOffset;
                if (nanoShapeBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    nanoShapeRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(nanoShapeRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    if (nanoShapeRandom > 0.0f)
                        nanoShapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoShapeRandom;
                    else
                        nanoShapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoShapeRandom;
                }

                // Calculate NanoOctave random offset (integer steps only)
                float nanoOctaveRandom = std::round(params.getRawParameterValue("NanoOctaveRandom")->load());
                bool nanoOctaveBipolar = params.getRawParameterValue("NanoOctaveRandomBipolar")->load() > 0.5f;
                float nanoOctaveRandomOffset;
                if (nanoOctaveBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    // Generate random integer offset
                    int maxOffset = static_cast<int>(std::abs(nanoOctaveRandom));
                    nanoOctaveRandomOffset = static_cast<float>(juce::Random::getSystemRandom().nextInt(maxOffset * 2 + 1) - maxOffset);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    int maxOffset = static_cast<int>(std::abs(nanoOctaveRandom));
                    if (nanoOctaveRandom > 0.0f)
                        nanoOctaveRandomOffset = static_cast<float>(juce::Random::getSystemRandom().nextInt(maxOffset + 1));
                    else
                        nanoOctaveRandomOffset = static_cast<float>(-juce::Random::getSystemRandom().nextInt(maxOffset + 1));
                }

                // Apply offsets to base values for next event parameters (used in fade calculations)
                float nanoGateBase = params.getRawParameterValue("NanoGate")->load();
                float nanoShapeBase = params.getRawParameterValue("NanoShape")->load();

                nextNanoGateParam = juce::jlimit(0.0f, 1.0f, nanoGateBase + nanoGateRandomOffset);

                // Snap to quarters if snap mode is enabled
                if (params.getRawParameterValue("NanoGateSnapMode")->load() > 0.5f)
                    nextNanoGateParam = std::round(nextNanoGateParam / 0.25f) * 0.25f;

                nextNanoShapeParam = juce::jlimit(0.0f, 1.0f, nanoShapeBase + nanoShapeRandomOffset);
                nextNanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();

                // Apply octave random offset (round base and clamp result)
                float nanoOctaveBase = std::round(params.getRawParameterValue("NanoOctave")->load());
                nextNanoOctaveParam = std::round(juce::jlimit(-1.0f, 3.0f, nanoOctaveBase + nanoOctaveRandomOffset));

                // Store the random offsets for use throughout the event (will be copied to held offsets at event start)
                heldNanoGateRandomOffset = nanoGateRandomOffset;
                heldNanoShapeRandomOffset = nanoShapeRandomOffset;

                parametersHeld = true;
                parametersSampledForUpcomingEvent = true; // Prevent re-sampling for same event

            }
        } else {
            // Reset flag when not approaching a stutter event
            parametersSampledForUpcomingEvent = false;
        }

        // INITIALIZE STUTTER EVENT when triggered (SIMPLIFIED: auto-stutter only)
        // bool isStutteringEvent = autoStutterActive || *params.getRawParameterValue("stutterOn"); // MANUAL STUTTER DISABLED
        // stutterInitialized is now a member variable

        // Reset initialization flag when not active
        if (!autoStutterActive) {
            stutterInitialized = false;
        }

        if (autoStutterActive && !stutterInitialized)
        {
            // Initialize stutter playback state (only for first-time initialization)
            stutterInitialized = true;

            // Note: stutterPlayCounter, stutterWritePos, and macroEnvelopeCounter are set when each stutter event activates

            // No need for universal countdown - using autoStutterRemainingSamples only

            // ENSURE macro envelope parameters ARE HELD - if not already sampled, sample them now
            if (!parametersHeld) {
                // Sample MacroGate with randomization
                float macroGateBase = params.getRawParameterValue("MacroGate")->load();
                float macroGateRandom = params.getRawParameterValue("MacroGateRandom")->load();
                bool macroGateBipolar = params.getRawParameterValue("MacroGateRandomBipolar")->load() > 0.5f;
                float gateRandomOffset;
                if (macroGateBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    gateRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(macroGateRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    // If positive: randomize 0 to +value, if negative: randomize -value to 0
                    if (macroGateRandom > 0.0f)
                        gateRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroGateRandom;
                    else
                        gateRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroGateRandom;
                }
                nextMacroGateParam = juce::jlimit(0.25f, 1.0f, macroGateBase + gateRandomOffset);

                // Snap to quarters if snap mode is enabled
                if (params.getRawParameterValue("MacroGateSnapMode")->load() > 0.5f)
                    nextMacroGateParam = std::round(nextMacroGateParam / 0.25f) * 0.25f;

                // Sample MacroShape with randomization
                float macroShapeBase = params.getRawParameterValue("MacroShape")->load();
                float macroShapeRandom = params.getRawParameterValue("MacroShapeRandom")->load();
                bool macroShapeBipolar = params.getRawParameterValue("MacroShapeRandomBipolar")->load() > 0.5f;
                float shapeRandomOffset;
                if (macroShapeBipolar)
                {
                    // Bipolar: ±random (symmetric around center)
                    shapeRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(macroShapeRandom);
                }
                else
                {
                    // Unipolar: + or - random (based on sign)
                    // If positive: randomize 0 to +value, if negative: randomize -value to 0
                    if (macroShapeRandom > 0.0f)
                        shapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroShapeRandom;
                    else
                        shapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * macroShapeRandom;
                }
                nextMacroShapeParam = juce::jlimit(0.0f, 1.0f, macroShapeBase + shapeRandomOffset);

                nextMacroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();

                // Sample nano parameters with randomization (same as Decision Point 2)
                float nanoGateRandom = params.getRawParameterValue("NanoGateRandom")->load();
                bool nanoGateBipolar = params.getRawParameterValue("NanoGateRandomBipolar")->load() > 0.5f;
                float nanoGateRandomOffset;
                if (nanoGateBipolar)
                {
                    nanoGateRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(nanoGateRandom);
                }
                else
                {
                    if (nanoGateRandom > 0.0f)
                        nanoGateRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoGateRandom;
                    else
                        nanoGateRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoGateRandom;
                }

                float nanoShapeRandom = params.getRawParameterValue("NanoShapeRandom")->load();
                bool nanoShapeBipolar = params.getRawParameterValue("NanoShapeRandomBipolar")->load() > 0.5f;
                float nanoShapeRandomOffset;
                if (nanoShapeBipolar)
                {
                    nanoShapeRandomOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * std::abs(nanoShapeRandom);
                }
                else
                {
                    if (nanoShapeRandom > 0.0f)
                        nanoShapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoShapeRandom;
                    else
                        nanoShapeRandomOffset = juce::Random::getSystemRandom().nextFloat() * nanoShapeRandom;
                }

                float nanoGateBase = params.getRawParameterValue("NanoGate")->load();
                float nanoShapeBase = params.getRawParameterValue("NanoShape")->load();

                nextNanoGateParam = juce::jlimit(0.0f, 1.0f, nanoGateBase + nanoGateRandomOffset);

                // Snap to quarters if snap mode is enabled
                if (params.getRawParameterValue("NanoGateSnapMode")->load() > 0.5f)
                    nextNanoGateParam = std::round(nextNanoGateParam / 0.25f) * 0.25f;

                nextNanoShapeParam = juce::jlimit(0.0f, 1.0f, nanoShapeBase + nanoShapeRandomOffset);
                nextNanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();

                heldNanoGateRandomOffset = nanoGateRandomOffset;
                heldNanoShapeRandomOffset = nanoShapeRandomOffset;

                // Update smoothed held gate parameter
                smoothedHeldMacroGate.setTargetValue(nextMacroGateParam);

                parametersHeld = true;
            }

        }

        // =============================================================================
        // DECISION POINT 3: FADE CONTROL (1ms before event boundaries)
        // Control wet/dry crossfading based on upcoming state transitions:
        // - If next stutter scheduled: wet fades to 0, dry fades to next event gain
        // - If no next stutter: wet fades to 0, dry fades to 1.0
        // =============================================================================

        // SIMPLIFIED: Use only autoStutterActive - eliminate currentlyStuttering race condition
        // Fixed: Only fade when approaching actual stutter boundaries, not every 1/32nd



        // Default gain states
        float currentDryGain = 1.0f;   // Dry signal on by default


        // SEPARATED FADE LOGIC - Wet and Dry fades are independent

        // WET FADE-OUT: Now handled by macro envelope internal fade logic

        // DRY FADE LOGIC: 1ms before new beat (based on quantCount and stutter scheduling)
        bool dryFading = false;
        bool isFadingStutterToStutter = false;  // Flag for EMA filtering during Stutter→Stutter fade
        bool isFadingDryToStutter = false;      // Flag for gradual EMA during Dry→Stutter fade
        float dryFadeProgress = 0.0f;           // Fade progress for EMA ramping

        // Gate mode: only fade when transitioning INTO a stutter (skip unnecessary fades)
        bool shouldProcessFade = (samplesToNextBeat <= fadeLengthInSamples && samplesToNextBeat >= 0 && (quantCount+1) == quantToNewBeat);
        bool shouldFadeInGateMode = (mixMode != 0) || stutterIsScheduled;

        if (shouldProcessFade && shouldFadeInGateMode) {
            dryFading = true;
            // Ensure proper boundary handling: when samplesToNextBeat=0, progress=1.0; when samplesToNextBeat=fadeLengthInSamples, progress=0.0
            dryFadeProgress = juce::jlimit(0.0f, 1.0f, 1.0f - (float)samplesToNextBeat / (float)fadeLengthInSamples);

            if (!stutterIsScheduled && autoStutterActive) {
                // Stutter→Dry: dry fades from 0 to 1 (no EMA needed)
                // Gate mode: no fade needed, immediately cut to silence
                if (mixMode != 0) {
                    // Insert/Mix modes: fade from stutter to dry audio
                    float startGain = 0.0f;
                    float endGain = 1.0f;
                    currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;
                } else {
                    // Gate mode: remain silent (macro envelope handles wet fadeout)
                    currentDryGain = 0.0f;
                }

            } else if (stutterIsScheduled && autoStutterActive) {
                // Stutter→Stutter: dry fades from 0 to firstSampleGain (apply full EMA to dry)
                isFadingStutterToStutter = true;
                // Use NEXT parameters because we're calculating what we're fading TO
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * nextMacroGateParam));
                float firstSampleProgress = 1.0f / (float)effectiveMacroLength;
                float nextFirstSampleMacroGain = calculateEnvelopeGain(firstSampleProgress, nextMacroShapeParam);

                float macroSmoothAmount = nextMacroSmoothParam * MACRO_SMOOTH_SCALE;
                if (macroSmoothAmount > 0.0f && firstSampleProgress < macroSmoothAmount) {
                    float fadeInGain = firstSampleProgress / macroSmoothAmount;
                    nextFirstSampleMacroGain *= fadeInGain;
                }

                // Calculate nano envelope gain for first sample (using NEXT parameters - what we're fading to)
                float nanoGateMultiplier = NANO_GATE_MIN + smoothedHeldNanoGate.getNextValue() * NANO_GATE_RANGE;
                int nanoEnvelopeLengthSamples = std::max(1, static_cast<int>(stutterEventLengthSamples / chosenDenominator * nanoGateMultiplier));
                float firstSampleNanoProgress = 1.0f / (float)nanoEnvelopeLengthSamples;
                float nextFirstSampleNanoGain = calculateEnvelopeGain(firstSampleNanoProgress, nextNanoShapeParam);

                // Combine macro and nano gains for accurate first sample gain
                float nextFirstSampleGain = nextFirstSampleMacroGain * nextFirstSampleNanoGain;

                float startGain = 0.0f;
                float endGain = nextFirstSampleGain;
                currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;


            } else if (stutterIsScheduled && !autoStutterActive) {
                // Dry→Stutter: dry fades to firstSampleGain (gradually introduce EMA)
                // Insert mode: fades from 1.0 (current audio), Gate mode: fades from 0.0 (silence)
                isFadingDryToStutter = true;
                // Use NEXT parameters because we're calculating what we're fading TO
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * nextMacroGateParam));
                float firstSampleProgress = 1.0f / (float)effectiveMacroLength;
                float nextFirstSampleMacroGain = calculateEnvelopeGain(firstSampleProgress, nextMacroShapeParam);

                float macroSmoothAmount = nextMacroSmoothParam * MACRO_SMOOTH_SCALE;
                if (macroSmoothAmount > 0.0f && firstSampleProgress < macroSmoothAmount) {
                    float fadeInGain = firstSampleProgress / macroSmoothAmount;
                    nextFirstSampleMacroGain *= fadeInGain;
                }

                // Calculate nano envelope gain for first sample (using NEXT parameters - what we're fading to)
                float nanoGateMultiplier = NANO_GATE_MIN + smoothedHeldNanoGate.getNextValue() * NANO_GATE_RANGE;
                int nanoEnvelopeLengthSamples = std::max(1, static_cast<int>(stutterEventLengthSamples / chosenDenominator * nanoGateMultiplier));
                float firstSampleNanoProgress = 1.0f / (float)nanoEnvelopeLengthSamples;
                float nextFirstSampleNanoGain = calculateEnvelopeGain(firstSampleNanoProgress, nextNanoShapeParam);

                // Combine macro and nano gains for accurate first sample gain
                float nextFirstSampleGain = nextFirstSampleMacroGain * nextFirstSampleNanoGain;

                // Gate mode starts from silence (0.0), Insert mode starts from current audio (1.0)
                float startGain = (mixMode == 0) ? 0.0f : 1.0f;
                float endGain = nextFirstSampleGain;
                currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;

            }
        } else if (!dryFading) {
            // No dry fade happening - set default states
            if (autoStutterActive && postStutterSilence <= 0) {
                // Active stuttering, no fade: dry silent
                currentDryGain = 0.0f;
            } else if (!autoStutterActive) {
                // No stutter currently happening
                if (!autoStutter) {
                    // AutoStutter feature OFF: bypass mode, always pass dry through (all mix modes)
                    currentDryGain = 1.0f;
                } else {
                    // AutoStutter feature ON but not stuttering: Gate mode = silence, others = dry
                    currentDryGain = (mixMode == 0) ? 0.0f : 1.0f;
                }
            }
            // else: keep whatever dry gain was set by fade logic
        }
        // else: No stuttering, no fade - defaults remain (dry=1.0, wet=0.0) from line 422-423


        // =============================================================================
        // MAIN AUDIO PROCESSING
        // Apply envelopes and generate output based on current stutter state
        // =============================================================================

        // PRE-CALCULATE INDICES AND PARAMETERS (once per sample, before channel processing)
        int loopLen = 0;
        int readIndex = 0;
        int loopPos = 0;

        // Pre-calculate all smoothed parameters ONCE per sample (before channel loop)
        // Real-time parameters (always advance smoothly)
        // Note: smoothNanoGate and smoothNanoSmooth are advanced but not used (randomized values held per event)
        smoothedNanoGate.getNextValue();
        float smoothNanoShape = smoothedNanoShape.getNextValue();
        smoothedNanoSmooth.getNextValue();

        // Advance macro smoothed parameters (needed for proper ramp behavior, even if not used directly)
        smoothedMacroShape.getNextValue();
        smoothedMacroSmooth.getNextValue();

        // Held gate parameters (advance during events/cycles to smooth transitions)
        float smoothHeldMacroGate = smoothedHeldMacroGate.getNextValue();
        float smoothHeldNanoGate = smoothedHeldNanoGate.getNextValue();

        if (autoStutterActive)
        {
            // Calculate stutter loop parameters
            loopLen = std::clamp(static_cast<int>((secondsPerWholeNote / chosenDenominator) * sampleRate + 1), 1, maxStutterLenSamples);

            // Handle reverse playback logic
            loopPos = stutterPlayCounter % (loopLen);

            // DETECT NEW CYCLE (wrap-around) and sample parameters for next cycle
            if (loopPos < lastLoopPos) {
                // New cycle detected
                // Resample base values and add the event-held random offsets
                // Random offsets are calculated once per event and added to each cycle's base values

                // Increment cycle counter for all stutters (used for crossfade skip-first-cycle logic)
                cycleCompletionCounter++;

                // Clear first reverse cycle flag after cycle 2
                if (currentStutterIsReversed && firstRepeatCyclePlayed) {
                    if (cycleCompletionCounter >= 2) {
                        isFirstReverseCycle = false;
                    }
                }

                // NanoGate: Resample base value and add held random offset
                float nanoGateBase = smoothedNanoGate.getCurrentValue();
                currentNanoGateParam = juce::jlimit(0.0f, 1.0f, nanoGateBase + heldNanoGateRandomOffset);
                smoothedHeldNanoGate.setTargetValue(currentNanoGateParam);

                // NanoShape: Resample base value and add held random offset
                float nanoShapeBase = smoothNanoShape;
                currentNanoShapeParam = juce::jlimit(0.0f, 1.0f, nanoShapeBase + heldNanoShapeRandomOffset);

                // NanoSmooth: Resample (no randomization)
                currentNanoSmoothParam = smoothedNanoSmooth.getCurrentValue();

                // Calculate EMA alpha coefficient from nano smooth parameter
                // 0.0 = bypass (alpha=1.0), 1.0 = max smooth (alpha=0.05)
                currentNanoEmaAlpha = 1.0f - (currentNanoSmoothParam * NANO_EMA_ALPHA_RANGE);

                // Set flag to reset EMA state at next sample (loop wraparound)
                // Skip reset if cycle crossfade is active (EMA continues smoothly through crossfade)
                float cycleCrossfade = parameters.getRawParameterValue("CycleCrossfade")->load();
                if (cycleCrossfade < 0.01f) {
                    shouldResetEmaState = true;
                }
                // With crossfade enabled, EMA state persists through transition

                // Pre-calculate nano envelope length for this cycle (using randomized nanoGate value)
                float nanoGateMultiplier = NANO_GATE_MIN + currentNanoGateParam * NANO_GATE_RANGE;
                heldNanoEnvelopeLengthInSamples = std::max(1, static_cast<int>((float)loopLen * nanoGateMultiplier));
            }
            lastLoopPos = loopPos;

            // Note: First cycle completion detection moved to counter update section

            if (currentStutterIsReversed && firstRepeatCyclePlayed) {
                // After first cycle, play in reverse within each loop cycle
                int reversedLoopPos = loopLen - 1 - loopPos;
                readIndex = (stutterWritePos + reversedLoopPos) % maxStutterLenSamples;

            } else {
                // Normal forward playback (including first cycle of reversed events)
                readIndex = (stutterWritePos + loopPos) % maxStutterLenSamples;
            }
        }

        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            float drySample = buffer.getSample(ch, i);
            float wetSample = 0.0f;

            // GENERATE WET SIGNAL when stuttering (SIMPLIFIED: only autoStutterActive)
            if (autoStutterActive)
            {

                // NANO ENVELOPE (controls internal loop behavior)
                // Use currentNanoShapeParam (base + random offset) for envelope shape
                // For reversed playback (after first cycle), mirror both gate position and envelope
                float nanoProgress;
                float nanoGain = 0.0f;

                if (currentStutterIsReversed && firstRepeatCyclePlayed) {
                    // REVERSE CYCLES: Mirror gate - silence first, then audio at end
                    int gateStartPos = loopLen - heldNanoEnvelopeLengthInSamples;  // Audio starts here

                    if (loopPos >= gateStartPos) {
                        // We're in the gated (audible) region
                        int positionInGatedRegion = loopPos - gateStartPos;

                        // Envelope runs backwards (1.0 → 0.0) through the gated region
                        nanoProgress = 1.0f - ((float)positionInGatedRegion / (float)heldNanoEnvelopeLengthInSamples);
                        nanoGain = calculateEnvelopeGain(nanoProgress, currentNanoShapeParam);

                        // Fade-out at END of loop (last 0.1ms before wraparound)
                        if (smoothHeldNanoGate < 1.0f) {
                            int fadeOutLen = static_cast<int>(sampleRate * NANO_FADE_OUT_SECONDS);
                            int fadeOutStart = std::max(gateStartPos, loopLen - fadeOutLen);

                            if (loopPos >= fadeOutStart && fadeOutLen > 0) {
                                float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(loopPos - fadeOutStart) / (float)fadeOutLen);
                                nanoGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                            }
                        }
                    }
                    // else: nanoGain stays 0.0 (silent before gateStartPos)

                } else {
                    // FORWARD CYCLES: Original behavior - audio first, then silence
                    if (loopPos < heldNanoEnvelopeLengthInSamples) {
                        // Envelope runs forward (0.0 → 1.0)
                        nanoProgress = (float)loopPos / (float)heldNanoEnvelopeLengthInSamples;
                        nanoGain = calculateEnvelopeGain(nanoProgress, currentNanoShapeParam);

                        // Fade-out at END of gated region (last 0.1ms before silence)
                        if (smoothHeldNanoGate < 1.0f) {
                            int fadeOutLen = static_cast<int>(sampleRate * NANO_FADE_OUT_SECONDS);
                            int fadeOutStart = std::max(0, heldNanoEnvelopeLengthInSamples - fadeOutLen);

                            if (loopPos >= fadeOutStart && fadeOutLen > 0) {
                                float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(loopPos - fadeOutStart) / (float)fadeOutLen);
                                nanoGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                            }
                        }
                    }
                    // else: nanoGain stays 0.0 (silent after heldNanoEnvelopeLengthInSamples)
                }

                // MACRO ENVELOPE (controls overall event shape)
                // Use CURRENT parameters (per event) for stable, event-locked envelope behavior
                float macroGateScale = juce::jlimit(MACRO_GATE_MIN, 1.0f, smoothHeldMacroGate);
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * macroGateScale));
                float macroProgress = juce::jlimit(0.0f, 1.0f, (float)macroEnvelopeCounter / (float)effectiveMacroLength);
                float macroGain = calculateEnvelopeGain(macroProgress, currentMacroShapeParam);

                // Apply macro smooth fades (using current parameters for event-locked behavior)
                float macroSmoothAmount = currentMacroSmoothParam * MACRO_SMOOTH_SCALE;
                if (macroSmoothAmount > 0.0f) {
                    if (macroProgress < macroSmoothAmount) {
                        macroGain *= macroProgress / macroSmoothAmount;
                    } else if (macroProgress > (1.0f - macroSmoothAmount)) {
                        macroGain *= (1.0f - macroProgress) / macroSmoothAmount;
                    }
                }

                if (macroEnvelopeCounter <= effectiveMacroLength) {
                    // Check if we're in fade-out region (constant 1ms fade)
                    int fadeOutLen = static_cast<int>(sampleRate * FADE_DURATION_SECONDS);
                    int fadeOutStart = std::max(1, effectiveMacroLength - fadeOutLen);

                    if (macroEnvelopeCounter >= fadeOutStart && fadeOutLen > 0) {
                        // We're in the fade-out region
                        float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(macroEnvelopeCounter - fadeOutStart) / (float)fadeOutLen);
                        macroGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                    }
                } else {
                    macroGain = 0.0f; // Silent after effective gate
                }



                

                // Generate wet sample with EMA filtering at configurable position
                float processedSample = stutterBuffer.getSample(ch, readIndex);

                // Apply cycle boundary crossfade to smooth loop transitions
                // ENVELOPE-AWARE: Calculate envelope gains for both samples before mixing
                float cycleCrossfade = parameters.getRawParameterValue("CycleCrossfade")->load();
                bool crossfadeWasApplied = false;  // Track if we applied envelope-aware crossfade
                if (cycleCrossfade > 0.0f && autoStutterActive) {
                    int crossfadeLen = static_cast<int>(cycleCrossfade * loopLen * CYCLE_CROSSFADE_MAX_PERCENT);
                    crossfadeLen = std::clamp(crossfadeLen, 0, loopLen / 2);  // Max 50% of loop

                    if (crossfadeLen > 0) {
                        // Handle forward playback (only for truly forward events, not first cycle of reverse)
                        if (!currentStutterIsReversed) {
                            // Crossfade at END of cycle - fades to audio BEFORE stutter buffer capture
                            // Works on all cycles including first (pre-stutter audio always exists)
                            if (loopPos >= (loopLen - crossfadeLen)) {
                                // In crossfade region (end of loop)
                                int tailOffset = loopPos - (loopLen - crossfadeLen);
                                float fadeOutGain = 1.0f - ((float)tailOffset / (float)crossfadeLen);
                                float fadeInGain = 1.0f - fadeOutGain;

                                // Read head sample from BEFORE stutter buffer (leads to start of repeat)
                                // This mirrors reverse reading tailPos = loopPos (forward samples)
                                int headPos = -crossfadeLen + tailOffset;  // Negative = before stutterWritePos
                                int headReadIndex = (stutterWritePos + headPos + maxStutterLenSamples) % maxStutterLenSamples;
                                float headSample = stutterBuffer.getSample(ch, headReadIndex);

                                // Calculate envelope gains for both positions
                                // Tail envelope (current position near end of loop)
                                float tailEnvelopeGain = 0.0f;
                                if (loopPos < heldNanoEnvelopeLengthInSamples) {
                                    float tailProgress = (float)loopPos / (float)heldNanoEnvelopeLengthInSamples;
                                    tailEnvelopeGain = calculateEnvelopeGain(tailProgress, currentNanoShapeParam);

                                    // Apply nano fade-out if gate < 1.0
                                    if (smoothHeldNanoGate < 1.0f) {
                                        int fadeOutLen = static_cast<int>(sampleRate * NANO_FADE_OUT_SECONDS);
                                        int fadeOutStart = std::max(0, heldNanoEnvelopeLengthInSamples - fadeOutLen);
                                        if (loopPos >= fadeOutStart && fadeOutLen > 0) {
                                            float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(loopPos - fadeOutStart) / (float)fadeOutLen);
                                            tailEnvelopeGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                                        }
                                    }
                                }

                                // Head envelope (audio before stutter - use beginning envelope)
                                // This audio "leads to" position 0, so calculate as if at beginning
                                int headLoopPos = tailOffset;  // Maps to 0..crossfadeLen-1
                                float headEnvelopeGain = 0.0f;
                                if (headLoopPos < heldNanoEnvelopeLengthInSamples) {
                                    float headProgress = (float)headLoopPos / (float)heldNanoEnvelopeLengthInSamples;
                                    headEnvelopeGain = calculateEnvelopeGain(headProgress, currentNanoShapeParam);
                                }

                                // Apply envelope gains to raw samples, then crossfade
                                processedSample = (processedSample * tailEnvelopeGain) * fadeOutGain +
                                                 (headSample * headEnvelopeGain) * fadeInGain;
                                crossfadeWasApplied = true;
                            }
                        }
                        // Handle reverse playback (skip during first reverse cycle)
                        else if (!isFirstReverseCycle) {
                            if (loopPos < crossfadeLen) {
                                // In crossfade region (beginning of loopPos in reverse)
                                float fadeInGain = (float)loopPos / (float)crossfadeLen;
                                float fadeOutGain = 1.0f - fadeInGain;

                                // Read tail sample (end of previous cycle in reverse playback)
                                // Tail should come from near start of buffer (where previous cycle ended)
                                int tailPos = loopPos;  // Position in previous cycle's ending
                                int tailReadIndex = (stutterWritePos + tailPos) % maxStutterLenSamples;
                                float tailSample = stutterBuffer.getSample(ch, tailReadIndex);

                                // Calculate envelope gains for both positions
                                // Head envelope (current position at start of new cycle)
                                int gateStartPos = loopLen - heldNanoEnvelopeLengthInSamples;
                                float headEnvelopeGain = 0.0f;
                                if (loopPos >= gateStartPos) {
                                    int positionInGatedRegion = loopPos - gateStartPos;
                                    float headProgress = 1.0f - ((float)positionInGatedRegion / (float)heldNanoEnvelopeLengthInSamples);
                                    headEnvelopeGain = calculateEnvelopeGain(headProgress, currentNanoShapeParam);

                                    // Apply nano fade-out if gate < 1.0 (at end of loop in reverse)
                                    if (smoothHeldNanoGate < 1.0f) {
                                        int fadeOutLen = static_cast<int>(sampleRate * NANO_FADE_OUT_SECONDS);
                                        int fadeOutStart = std::max(gateStartPos, loopLen - fadeOutLen);
                                        if (loopPos >= fadeOutStart && fadeOutLen > 0) {
                                            float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(loopPos - fadeOutStart) / (float)fadeOutLen);
                                            headEnvelopeGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                                        }
                                    }
                                }

                                // Tail envelope (end of previous cycle in reverse)
                                // Previous cycle ended with loopPos near loopLen, which had low envelope gain
                                float tailEnvelopeGain = 0.0f;
                                int tailLoopPos = loopLen - crossfadeLen + loopPos;
                                if (tailLoopPos >= gateStartPos) {
                                    int tailPosInGatedRegion = tailLoopPos - gateStartPos;
                                    float tailProgress = 1.0f - ((float)tailPosInGatedRegion / (float)heldNanoEnvelopeLengthInSamples);
                                    tailEnvelopeGain = calculateEnvelopeGain(tailProgress, currentNanoShapeParam);

                                    // Apply nano fade-out if gate < 1.0
                                    if (smoothHeldNanoGate < 1.0f) {
                                        int fadeOutLen = static_cast<int>(sampleRate * NANO_FADE_OUT_SECONDS);
                                        int fadeOutStart = std::max(gateStartPos, loopLen - fadeOutLen);
                                        if (tailLoopPos >= fadeOutStart && fadeOutLen > 0) {
                                            float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(tailLoopPos - fadeOutStart) / (float)fadeOutLen);
                                            tailEnvelopeGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                                        }
                                    }
                                }

                                // Apply envelope gains to raw samples, then crossfade
                                processedSample = (processedSample * headEnvelopeGain) * fadeInGain +
                                                 (tailSample * tailEnvelopeGain) * fadeOutGain;
                                crossfadeWasApplied = true;
                            }
                        }
                    }
                }

                // Position A: Apply EMA before nano envelope (if selected)
                if constexpr (NANO_EMA_POSITION == EmaPosition::BeforeNanoEnvelope) {
                    if (shouldResetEmaState) {
                        nanoEmaState[ch] = processedSample;
                    }
                    processedSample = currentNanoEmaAlpha * processedSample + (1.0f - currentNanoEmaAlpha) * nanoEmaState[ch];
                    nanoEmaState[ch] = processedSample;
                }

                // Apply nano envelope (skip if already applied in crossfade)
                if (!crossfadeWasApplied) {
                    processedSample *= nanoGain;
                }
                // else: envelope gains already baked into crossfaded sample

                // Position B: Apply EMA after nano envelope, before macro envelope (if selected)
                if constexpr (NANO_EMA_POSITION == EmaPosition::AfterNanoEnvelope) {
                    if (shouldResetEmaState) {
                        nanoEmaState[ch] = processedSample;
                    }
                    processedSample = currentNanoEmaAlpha * processedSample + (1.0f - currentNanoEmaAlpha) * nanoEmaState[ch];
                    nanoEmaState[ch] = processedSample;
                }

                processedSample *= macroGain; // Apply macro envelope

                // Position C: Apply EMA after macro envelope (if selected)
                if constexpr (NANO_EMA_POSITION == EmaPosition::AfterMacroEnvelope) {
                    if (shouldResetEmaState) {
                        nanoEmaState[ch] = processedSample;
                    }
                    processedSample = currentNanoEmaAlpha * processedSample + (1.0f - currentNanoEmaAlpha) * nanoEmaState[ch];
                    nanoEmaState[ch] = processedSample;
                }

                wetSample = processedSample;
            }

            // APPLY FADE GAINS from Decision Point 3
            // Apply conditional EMA filtering to dry signal during crossfades to eliminate clicks
            float fadedDrySample;

            if (isFadingStutterToStutter && currentNanoSmoothParam > 0.0f) {
                // Stutter→Stutter: Apply full EMA to dry using current alpha
                float filteredDry = currentNanoEmaAlpha * drySample
                    + (1.0f - currentNanoEmaAlpha) * dryEmaStateForFade[ch];
                dryEmaStateForFade[ch] = filteredDry;
                fadedDrySample = filteredDry * currentDryGain;

            } else if (isFadingDryToStutter && currentNanoSmoothParam > 0.0f) {
                // Dry→Stutter: Gradually introduce EMA (alpha ramps from 1.0 → currentNanoEmaAlpha)
                float rampedAlpha = 1.0f - ((1.0f - currentNanoEmaAlpha) * dryFadeProgress);
                float filteredDry = rampedAlpha * drySample
                    + (1.0f - rampedAlpha) * dryEmaStateForFade[ch];
                dryEmaStateForFade[ch] = filteredDry;
                fadedDrySample = filteredDry * currentDryGain;

            } else {
                // Default: No EMA filtering (Stutter→Dry or no fade active)
                fadedDrySample = drySample * currentDryGain;
            }

            float fadedWetSample = wetSample;


            // MIX MODES - determine final output
            float outputSample;
            if (mixMode == 0) {         // GATE MODE: stutter or silence, no dry signal
                // Same as Insert Mode - fade preview uses dry signal ramping to firstSampleGain
                outputSample = fadedDrySample + fadedWetSample;
            } else if (mixMode == 1) {  // INSERT MODE: stutter replaces dry signal
                // In insert mode, fade gains control replacement:
                // - During stutter: currentDryGain=0, currentWetGain=1 (wet replaces dry)
                // - During fade: gains transition smoothly
                // - When not stuttering: currentDryGain=1, currentWetGain=0 (dry only)
                outputSample = fadedDrySample + fadedWetSample;

            } else {                    // MIX MODE: blend during stutter, dry otherwise
                outputSample = (autoStutterActive && postStutterSilence <= 0) ? (drySample + fadedWetSample) * 0.5f : fadedDrySample;
            }

            buffer.setSample(ch, i, outputSample);

            // Copy final output to visualization buffer (only once per sample, not per channel)
            if (ch == 0 && outputBufferMaxSamples > 0)
            {
                // Calculate write position directly from PPQ (modulo 1.0 gives position within quarter note)
                double currentPpqForSample = ppqAtStartOfBlock + (i * ppqPerSample);
                double ppqWithinQuarter = currentPpqForSample - std::floor(currentPpqForSample);
                int writeIndex = static_cast<int>(ppqWithinQuarter * outputBufferMaxSamples) % outputBufferMaxSamples;

                // Determine stutter state for visualization (0=none, 1=repeat, 2=nano)
                int currentState = 0;
                if (autoStutterActive)
                {
                    currentState = currentlyUsingNanoRate.load() ? 2 : 1;
                }

                // Fill gaps between last write and current write to avoid aliasing
                // Skip gap-filling during wraparound (quarter note boundary crossing)
                if (lastOutputWriteIndex >= 0 && lastOutputWriteIndex != writeIndex && writeIndex > lastOutputWriteIndex)
                {
                    // Only fill gaps within the same quarter note (no wraparound)
                    int gapStart = lastOutputWriteIndex + 1;
                    for (int idx = gapStart; idx < writeIndex; ++idx)
                    {
                        for (int outCh = 0; outCh < totalNumOutputChannels && outCh < outputBuffer.getNumChannels(); ++outCh)
                        {
                            outputBuffer.setSample(outCh, idx, buffer.getSample(outCh, i));
                        }
                        stutterStateBuffer[idx] = currentState;
                    }
                }

                // Store output sample and state at writeIndex
                for (int outCh = 0; outCh < totalNumOutputChannels && outCh < outputBuffer.getNumChannels(); ++outCh)
                {
                    outputBuffer.setSample(outCh, writeIndex, buffer.getSample(outCh, i));
                }
                stutterStateBuffer[writeIndex] = currentState;

                // Update tracking for next iteration
                lastOutputWriteIndex = writeIndex;
                outputBufferWritePos.store(writeIndex);
            }
        }

        // UPDATE COUNTERS (after all channels have been processed with same indices)
        if (autoStutterActive) {
            ++stutterPlayCounter;

            // Clear EMA reset flag after it's been used for all channels in this sample
            shouldResetEmaState = false;

            // Track when we complete the first repeat cycle for reverse playback
            if (currentStutterIsReversed && !firstRepeatCyclePlayed && stutterPlayCounter >= loopLen) {
                firstRepeatCyclePlayed = true;
                // Reset EMA state when switching from forward to reverse playback
                shouldResetEmaState = true;
                // Mark that we're entering first reverse cycle (skip crossfade during direction change)
                isFirstReverseCycle = true;
                cycleCompletionCounter = 0;  // Start counting completed cycles (0 = entering cycle 1)
            }

            if (stutterPlayCounter >= loopLen) {
                stutterPlayCounter = 0;  // Reset to 0 after playing loopLen samples
            }
            macroEnvelopeCounter++;
            --autoStutterRemainingSamples;

            // Universal countdown for Decision Point 2 (works for all stutter types)
            if (currentStutterRemainingSamples > 0) --currentStutterRemainingSamples;
        }

        // =============================================================================
        // END-OF-EVENT CLEANUP (moved to before Decision Point 1)
        // Handle stutter event completion and state reset
        // =============================================================================

        // Track postStutterSilence countdown
        if (postStutterSilence > 0) {
            --postStutterSilence;
        }

        // Update state tracking for next iteration
        wasStuttering = (autoStutterActive && postStutterSilence <= 0);
    }

    // Safety check: Ensure nano flags are always reset when not actively stuttering
    // This prevents the tuner from getting stuck showing stale frequency data
    if (!autoStutterActive) {
        currentlyUsingNanoRate.store(false);
        currentNanoFrequency.store(0.0f);
    }

    writePos = (writePos + numSamples) % maxStutterLenSamples;

    // Apply waveshaping using JUCE DSP ProcessorChain
    auto waveshapeAlgorithm = parameters.getRawParameterValue("WaveshapeAlgorithm")->load();
    auto driveAmount = parameters.getRawParameterValue("Drive")->load();
    auto gainCompensation = parameters.getRawParameterValue("GainCompensation")->load();

    if (driveAmount > 0.0f && waveshapeAlgorithm > 0)
    {
        // Update waveshaper function and gains
        updateWaveshaperFunction(static_cast<int>(waveshapeAlgorithm), driveAmount, gainCompensation > 0.5f);

        // Process through the DSP chain
        auto audioBlock = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(audioBlock);
        waveshaperChain.process(context);
    }
}

void NanoStuttAudioProcessor::updateWaveshaperFunction(int algorithm, float drive, bool gainCompensation)
{
    // Drive controls input gain (1.0 to 10.0x range for aggressive saturation)
    float inputGain = 1.0f + (drive * 9.0f); // 1.0x to 10.0x drive range
    float outputGain = 1.0f;

    // Get references to chain processors
    auto& inputGainProcessor = waveshaperChain.get<inputGainIndex>();
    auto& waveShaperProcessor = waveshaperChain.get<waveShaperIndex>();
    auto& outputGainProcessor = waveshaperChain.get<outputGainIndex>();

    // Set waveshaper function and calculate compensation gains
    switch (algorithm)
    {
        case 0: // None
            inputGain = 1.0f;
            waveShaperProcessor.functionToUse = [](float x) { return x; };
            break;

        case 1: // Soft Clip
            waveShaperProcessor.functionToUse = [](float x) {
                return juce::jlimit(-1.0f, 1.0f, x);
            };
            // Subtle compensation for soft clipping
            if (gainCompensation)
                outputGain = 1.0f / std::sqrt(inputGain); // Gentle compensation
            break;

        case 2: // Tanh
            waveShaperProcessor.functionToUse = [](float x) {
                return std::tanh(x);
            };
            // Subtle compensation for tanh compression
            if (gainCompensation)
                outputGain = 1.0f / std::sqrt(inputGain); // Gentle compensation
            break;

        case 3: // Hard Clip
            waveShaperProcessor.functionToUse = [](float x) {
                return juce::jlimit(-1.0f, 1.0f, x);
            };
            // Subtle compensation for hard clipping
            if (gainCompensation)
                outputGain = 1.0f / std::sqrt(inputGain); // Gentle compensation
            break;

        case 4: // Tube
            waveShaperProcessor.functionToUse = [](float x) {
                return x / (1.0f + std::abs(x));
            };
            // Slightly stronger compensation for tube's asymptotic behavior
            if (gainCompensation)
                outputGain = 1.2f / std::sqrt(inputGain); // Gentle boost to compensate for tube saturation
            break;

        case 5: // Wavefolding
            waveShaperProcessor.functionToUse = [](float x) {
                // Proper wavefolding - strict bounds maintenance within [-1, 1]
                // Use absolute value and sign tracking for correct reflection
                float sign = x >= 0.0f ? 1.0f : -1.0f;
                float y = std::abs(x);

                // Fold the signal repeatedly until it's within [0, 1] range
                while (y > 1.0f) {
                    y = 2.0f - y;  // Reflect around 1.0
                    if (y < 0.0f) {
                        y = -y;    // If reflection goes negative, flip back positive
                        sign = -sign; // And invert the sign
                    }
                }

                return sign * y;
            };
            // Compensation for wavefolding (should be similar to other algorithms now)
            if (gainCompensation)
                outputGain = 1.0f / std::sqrt(inputGain); // Standard gentle compensation
            break;
    }

    // Set the gain values using the chain
    inputGainProcessor.setGainLinear(inputGain);
    outputGainProcessor.setGainLinear(outputGain);
}


//==============================================================================
bool NanoStuttAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NanoStuttAudioProcessor::createEditor()
{
    return new NanoStuttAudioProcessorEditor (*this);
}

//==============================================================================
void NanoStuttAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialize all parameters using AudioProcessorValueTreeState's built-in serialization
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NanoStuttAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore all parameters from saved state
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NanoStuttAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout NanoStuttAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("autoStutterGate", 1), "Auto Stutter Gate", 0.25f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("stutterOn",1), "Stutter On", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("autoStutterEnabled",1), "Auto Stutter Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("autoStutterChance",1), "Auto Stutter Chance", 0.0f, 1.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverseChance",1), "Reverse Chance", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("autoStutterQuant", 1), "Auto Stutter Quantization",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 1));

    // Quant unit probability parameters
    auto quantLabels = juce::StringArray { "4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32" };
    for (int i = 0; i < quantLabels.size(); ++i)
    {
        juce::String id = "quantProb_" + quantLabels[i];
        float defaultValue = (quantLabels[i] == "1/8") ? 1.0f : (quantLabels[i] == "1/16") ? 0.5f : 0.0f; // 1/8th at max, 1/16th at 50%, others at 0
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.0f, 1.0f, defaultValue));
    }

    auto rateLabels = juce::StringArray { "1bar", "3/4", "1/2", "1/3", "1/4", "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (const auto& label : rateLabels)
    {
        juce::String id = "rateProb_" + label;
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.0f, 1.0f, 0.0f));
    }

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoProb_" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.0f, 1.0f, 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("nanoBlend", 1), "Repeat/Nano", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("nanoTune", 1), "Nano Tune", 0.75f, 2.0f, 1.0f));

    // Nano tuning system parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("nanoBase", 1), "Nano Base",
        juce::StringArray { "BPM Synced", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tuningSystem", 1), "Tuning System",
        juce::StringArray { "Equal Temperament", "Just Intonation", "Pythagorean", "Quarter-comma Meantone", "Custom (Fraction)", "Custom (Decimal)", "Custom (Semitone)" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("scale", 1), "Scale",
        juce::StringArray { "Chromatic", "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic",
                           "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian",
                           "Harmonic Minor", "Melodic Minor", "Whole Tone", "Diminished", "Custom" }, 0));

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoRatio_" + juce::String(i);
        float defaultRatio = std::pow(2.0f, static_cast<float>(i) / 12.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.1f, 4.0f, defaultRatio));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoGate", 1), "Nano Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoShape", 1), "Nano Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoSmooth", 1), "Nano Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("CycleCrossfade", 1), "Cycle Crossfade", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoGateRandom", 1), "Nano Gate Random", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoShapeRandom", 1), "Nano Shape Random", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("NanoGateRandomBipolar", 1), "Nano Gate Random Bipolar", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("NanoShapeRandomBipolar", 1), "Nano Shape Random Bipolar", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("NanoOctave", 1), "Nano Octave",
        juce::NormalisableRange<float>(-1.0f, 3.0f, 1.0f),  // min, max, step (1.0 = integer snapping)
        0.0f));  // default
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("NanoOctaveRandom", 1), "Nano Octave Random",
        juce::NormalisableRange<float>(-4.0f, 4.0f, 1.0f),  // min, max, step (1.0 = integer snapping)
        0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("NanoOctaveRandomBipolar", 1), "Nano Octave Random Bipolar", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroGate", 1), "Macro Gate", 0.25f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroShape", 1), "Macro Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroSmooth", 1), "Macro Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroGateRandom", 1), "Macro Gate Random", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroShapeRandom", 1), "Macro Shape Random", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("MacroGateRandomBipolar", 1), "Macro Gate Random Bipolar", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("MacroShapeRandomBipolar", 1), "Macro Shape Random Bipolar", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("NanoGateSnapMode", 1), "Nano Gate Snap Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("MacroGateSnapMode", 1), "Macro Gate Snap Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("MixMode", 1), "Mix Mode",
        juce::StringArray{"Gate", "Insert", "Mix"}, 1));

    // Timing offset parameter for master track delay compensation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TimingOffset", 1), "Timing Offset (ms)",
        -100.0f, 100.0f, 0.0f));

    // Waveshaping parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("WaveshapeAlgorithm", 1), "Waveshape Algorithm",
        juce::StringArray{"None", "Soft Clip", "Tanh", "Hard Clip", "Tube", "Fold"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Drive", 1), "Drive",
        0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("GainCompensation", 1), "Gain Compensation", false));

    // Visibility/Active state parameters for repeat rates
    // Defaults: 1/6 through 1/32 active (indices 5-11)
    for (int i = 0; i < rateLabels.size(); ++i)
    {
        bool defaultActive = (i >= 5); // Indices 5-11: "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32"
        juce::String id = "rateActive_" + rateLabels[i];
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), id, defaultActive));
    }

    // Visibility/Active state parameters for nano rates
    // Defaults: minor scale active (indices 0, 2, 3, 5, 7, 8, 10)
    for (int i = 0; i < 12; ++i)
    {
        // Natural minor scale intervals: root, maj2, min3, P4, P5, min6, min7
        bool defaultActive = (i == 0 || i == 2 || i == 3 || i == 5 || i == 7 || i == 8 || i == 10);
        juce::String id = "nanoActive_" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), id, defaultActive));
    }

    // Visibility/Active state parameters for quantization
    // Defaults: 1/2 through 1/16 active (indices 3-7)
    for (int i = 0; i < quantLabels.size(); ++i)
    {
        bool defaultActive = (i >= 3 && i <= 7); // Indices 3-7: "1/2", "1/4", "d1/8", "1/8", "1/16"
        juce::String id = "quantActive_" + quantLabels[i];
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), id, defaultActive));
    }

    return { params.begin(), params.end() };
}

void NanoStuttAudioProcessor::updateCachedParameters()
{
    static const std::array<std::string, 12> regularLabels = { "1bar", "3/4", "1/2", "1/3", "1/4", "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32" };
    static const std::array<std::string, 9> quantLabels = { "4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32" };

    // Update regular rate weights, respecting active state
    for (size_t i = 0; i < regularLabels.size(); ++i)
    {
        float weight = parameters.getRawParameterValue("rateProb_" + regularLabels[i])->load();
        bool isActive = parameters.getRawParameterValue("rateActive_" + regularLabels[i])->load() > 0.5f;
        regularRateWeights[i] = isActive ? weight : 0.0f;
    }

    // Update nano rate weights, respecting active state
    for (int i = 0; i < 12; ++i)
    {
        float weight = parameters.getRawParameterValue("nanoProb_" + std::to_string(i))->load();
        bool isActive = parameters.getRawParameterValue("nanoActive_" + std::to_string(i))->load() > 0.5f;
        nanoRateWeights[i] = isActive ? weight : 0.0f;
    }

    // Update quantization weights, respecting active state
    for (size_t i = 0; i < quantLabels.size(); ++i)
    {
        float weight = parameters.getRawParameterValue("quantProb_" + quantLabels[i])->load();
        bool isActive = parameters.getRawParameterValue("quantActive_" + quantLabels[i])->load() > 0.5f;
        quantUnitWeights[i] = isActive ? weight : 0.0f;
    }

    nanoBlend = parameters.getRawParameterValue("nanoBlend")->load();
}

void NanoStuttAudioProcessor::updateNanoRatiosFromTuning()
{
    // Get current tuning system from parameter
    int tuningIndex = static_cast<int>(parameters.getRawParameterValue("tuningSystem")->load());
    NanoTuning::TuningSystem tuning = static_cast<NanoTuning::TuningSystem>(tuningIndex);

    // Don't update if Custom mode is selected (either Fraction or Decimal)
    if (tuning == NanoTuning::TuningSystem::CustomFraction || tuning == NanoTuning::TuningSystem::CustomDecimal)
        return;

    // Get the ratios for the selected tuning system
    const auto& tuningRatios = NanoTuning::getTuningRatios(tuning);

    // Suppress custom detection during programmatic updates
    suppressCustomDetection = true;

    // Copy to runtime array and update parameters
    for (int i = 0; i < 12; ++i)
    {
        runtimeNanoRatios[i] = tuningRatios[i];

        // Update the parameter value (this will trigger UI update)
        auto* param = parameters.getParameter("nanoRatio_" + juce::String(i));
        if (param != nullptr)
            param->setValueNotifyingHost(param->convertTo0to1(tuningRatios[i]));
    }

    // Re-enable custom detection
    suppressCustomDetection = false;

    currentTuningSystem = tuning;
}

void NanoStuttAudioProcessor::updateNanoVisibilityFromScale()
{
    // Get current scale from parameter
    int scaleIndex = static_cast<int>(parameters.getRawParameterValue("scale")->load());
    NanoTuning::Scale scale = static_cast<NanoTuning::Scale>(scaleIndex);

    // Don't update if Custom is selected
    if (scale == NanoTuning::Scale::Custom)
        return;

    // Get the scale notes
    const auto& scaleNotes = NanoTuning::getScaleNotes(scale);

    // Suppress custom detection during programmatic updates
    suppressCustomDetection = true;

    // Update active state parameters
    for (int i = 0; i < 12; ++i)
    {
        auto* param = parameters.getParameter("nanoActive_" + juce::String(i));
        if (param != nullptr)
            param->setValueNotifyingHost(scaleNotes[i] ? 1.0f : 0.0f);
    }

    // Re-enable custom detection
    suppressCustomDetection = false;

    currentScale = scale;
}

void NanoStuttAudioProcessor::detectCustomTuning()
{
    // Skip detection if suppressed (during programmatic updates)
    if (suppressCustomDetection)
        return;

    // Check if current tuning ratios match the selected tuning system
    if (currentTuningSystem != NanoTuning::TuningSystem::CustomFraction &&
        currentTuningSystem != NanoTuning::TuningSystem::CustomDecimal &&
        currentTuningSystem != NanoTuning::TuningSystem::CustomSemitone)
    {
        const auto& expectedRatios = NanoTuning::getTuningRatios(currentTuningSystem);
        bool ratiosMatch = true;

        for (int i = 0; i < 12; ++i)
        {
            float currentRatio = parameters.getRawParameterValue("nanoRatio_" + juce::String(i))->load();
            if (std::abs(currentRatio - expectedRatios[i]) > 0.001f)
            {
                ratiosMatch = false;
                break;
            }
        }

        // If ratios don't match, switch to appropriate custom mode based on current tuning
        if (!ratiosMatch)
        {
            // If editing Equal Temperament, switch to Custom Semitone
            // Otherwise, switch to Custom Fraction
            NanoTuning::TuningSystem targetCustom = (currentTuningSystem == NanoTuning::TuningSystem::EqualTemperament)
                ? NanoTuning::TuningSystem::CustomSemitone
                : NanoTuning::TuningSystem::CustomFraction;

            currentTuningSystem = targetCustom;
            auto* param = parameters.getParameter("tuningSystem");
            if (param != nullptr)
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(targetCustom)));
        }
    }
}

void NanoStuttAudioProcessor::detectCustomScale()
{
    // Skip detection if suppressed (during programmatic updates)
    if (suppressCustomDetection)
        return;

    // Check if current active states match the selected scale
    if (currentScale != NanoTuning::Scale::Custom)
    {
        const auto& expectedNotes = NanoTuning::getScaleNotes(currentScale);
        bool scaleMatches = true;

        for (int i = 0; i < 12; ++i)
        {
            bool currentActive = parameters.getRawParameterValue("nanoActive_" + juce::String(i))->load() > 0.5f;
            if (currentActive != expectedNotes[i])
            {
                scaleMatches = false;
                break;
            }
        }

        // If scale doesn't match, switch to Custom
        if (!scaleMatches)
        {
            currentScale = NanoTuning::Scale::Custom;
            auto* param = parameters.getParameter("scale");
            if (param != nullptr)
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(NanoTuning::Scale::Custom)));
        }
    }
}

void NanoStuttAudioProcessor::initializeParameterListeners()
{
    static const std::array<std::string, 12> regularLabels = { "1bar", "3/4", "1/2", "1/3", "1/4", "1/6", "d1/8", "1/8", "1/12", "1/16", "1/24", "1/32" };
    static const std::array<std::string, 9> quantLabels = { "4bar", "2bar", "1bar", "1/2", "1/4", "d1/8", "1/8", "1/16", "1/32" };

    for (const auto& label : regularLabels)
        parameters.addParameterListener("rateProb_" + label, this);

    for (const auto& label : quantLabels)
        parameters.addParameterListener("quantProb_" + label, this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoProb_" + std::to_string(i), this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoRatio_" + std::to_string(i), this);

    // Add listeners for active state parameters
    for (const auto& label : regularLabels)
        parameters.addParameterListener("rateActive_" + label, this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoActive_" + std::to_string(i), this);

    for (const auto& label : quantLabels)
        parameters.addParameterListener("quantActive_" + label, this);

    parameters.addParameterListener("nanoBlend", this);
    parameters.addParameterListener("TimingOffset", this);
    parameters.addParameterListener("WaveshapeAlgorithm", this);
    parameters.addParameterListener("Drive", this);
    parameters.addParameterListener("GainCompensation", this);

    // Add listeners for nano tuning system parameters
    parameters.addParameterListener("nanoBase", this);
    parameters.addParameterListener("tuningSystem", this);
    parameters.addParameterListener("scale", this);

    updateCachedParameters();
}

void NanoStuttAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    updateCachedParameters();

    // Mark preset as modified (unless it's autoStutterEnabled which isn't saved in presets)
    if (parameterID != "autoStutterEnabled")
        presetManager.setModified(true);

    // Handle nano base changes
    if (parameterID == "nanoBase")
    {
        currentNanoBase = static_cast<NanoTuning::NanoBase>(static_cast<int>(newValue));
    }
    // Handle tuning system changes
    else if (parameterID == "tuningSystem")
    {
        int tuningIndex = static_cast<int>(newValue);
        // Only update ratios if not in a custom mode
        if (tuningIndex != static_cast<int>(NanoTuning::TuningSystem::CustomFraction) &&
            tuningIndex != static_cast<int>(NanoTuning::TuningSystem::CustomDecimal))
        {
            updateNanoRatiosFromTuning();
        }
    }
    // Handle scale changes
    else if (parameterID == "scale")
    {
        int scaleIndex = static_cast<int>(newValue);
        if (scaleIndex != static_cast<int>(NanoTuning::Scale::Custom))
        {
            updateNanoVisibilityFromScale();
        }
    }
    // Detect custom tuning when ratio parameters change
    else if (parameterID.startsWith("nanoRatio_"))
    {
        // Update runtime ratios array
        for (int i = 0; i < 12; ++i)
        {
            runtimeNanoRatios[i] = parameters.getRawParameterValue("nanoRatio_" + juce::String(i))->load();
        }
        detectCustomTuning();  // Only check tuning, not scale
    }
    // Detect custom scale when active state parameters change
    else if (parameterID.startsWith("nanoActive_"))
    {
        detectCustomScale();  // Only check scale, not tuning
    }
}

void NanoStuttAudioProcessor::resizeOutputBufferForBpm(double bpm, double sampleRate)
{
    if (bpm <= 0.0 || sampleRate <= 0.0)
        return;

    // Calculate samples needed for EXACTLY 1/4 note at this BPM
    double secondsPerQuarterNote = 60.0 / bpm;
    int newBufferSize = static_cast<int>(std::ceil(secondsPerQuarterNote * sampleRate));

    // Clamp to reasonable bounds (min 1 second, max 10 seconds worth of samples)
    newBufferSize = juce::jlimit(static_cast<int>(sampleRate), static_cast<int>(sampleRate * 10), newBufferSize);

    if (newBufferSize != outputBufferMaxSamples)
    {
        outputBufferMaxSamples = newBufferSize;
        outputBuffer.setSize(getTotalNumOutputChannels(), outputBufferMaxSamples, false, true, true);
        stutterStateBuffer.resize(outputBufferMaxSamples, 0);
        outputBufferWritePos.store(0);
        lastOutputWriteIndex = -1;  // Reset for new buffer
        lastKnownBpm = bpm;
    }
}
