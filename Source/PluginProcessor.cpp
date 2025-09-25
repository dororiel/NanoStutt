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
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
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
    const double maxStutterSeconds = 3.0;
    maxStutterLenSamples = static_cast<int>(sampleRate * maxStutterSeconds);
    stutterBuffer.setSize(getTotalNumOutputChannels(), maxStutterLenSamples, false, true, true);
    fadeLengthInSamples = static_cast<int>(sampleRate * 0.001); // 1ms fade

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

    // TRANSPORT STATE DETECTION AND QUANTIZATION RESET
    if (!isPlaying) {
        // Transport is not playing, pass through dry audio and reset stutter state
        autoStutterActive = false;
        parametersHeld = false;
        wasPlaying = false;
        writePos = 0;
        return;
    }

    // Check for transport restart or position jump (using RAW PPQ, before timing offset)
    bool transportJustStarted = !wasPlaying && isPlaying;
    bool positionJumped = wasPlaying && std::abs(currentPpqPosition - lastPpqPosition) > 0.125; // Allow small timing variations

    if (transportJustStarted || positionJumped) {
        // Reset quantization alignment based on new PPQ position
        // Find the smallest active quantization unit to align to its grid
        static const std::array<double, 4> quantUnits = {0.25, 0.125, 0.0625, 0.03125}; // 1/4, 1/8, 1/16, 1/32
        static const std::array<int, 4> quantToNewBeatValues = {8, 4, 2, 1}; // 1/32nds per unit

        // Find smallest active quantization unit (highest frequency)
        double activeQuantUnit = 0.125; // Default to 1/8th
        int activeQuantToNewBeat = 4;   // Default to 1/8th (4 x 1/32nds)

        // Debug: ensure we're finding the right quantization unit (commented out - working correctly)

        for (size_t i = quantUnitWeights.size(); i > 0; --i) {
            if (quantUnitWeights[i-1] > 0.0f) {
                activeQuantUnit = quantUnits[i-1];
                activeQuantToNewBeat = quantToNewBeatValues[i-1];
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
        double thirtySecondNotes = adjustedPpqPosition / 0.125;
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

    auto nanoGateParam = params.getRawParameterValue("NanoGate")->load();
    auto nanoShapeParam = params.getRawParameterValue("NanoShape")->load();
    auto nanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();
    // Macro envelope parameters now handled via sample-and-hold mechanism
    // auto macroGateParam = params.getRawParameterValue("MacroGate")->load();
    // auto macroShapeParam = params.getRawParameterValue("MacroShape")->load();
    // auto macroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();

    double ppqAtStartOfBlock = 0.0;
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            ppqAtStartOfBlock = position->getPpqPosition().orFallback(0.0);
            bpm = position->getBpm().orFallback(120.0);
        }

    // Apply timing offset for master track delay compensation
    // NOTE: Applied AFTER transport state tracking to avoid corrupting jump detection
    float timingOffsetMs = parameters.getRawParameterValue("TimingOffset")->load();
    double timingOffsetSamples = (timingOffsetMs / 1000.0) * sampleRate;
    double timingOffsetPpq = (timingOffsetSamples / sampleRate) * (bpm / 60.0);
    ppqAtStartOfBlock += timingOffsetPpq;

    double ppqPerSample = (bpm / 60.0) / sampleRate;
    double quantUnit = 1.0 / std::pow(2.0, currentQuantIndex);

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
    
    auto calculateGain = [](float progress, float shapeParam) {
        float mappedShape = (shapeParam - 0.5f) * 2.0f;
        float curveAmount = std::abs(mappedShape);
        float exponent = 1.0f + curveAmount * 4.0f;
        float curvedGain = (mappedShape < 0.0f) ? std::pow(1.0f - progress, exponent) : std::pow(progress, exponent);
        return juce::jmap(curveAmount, 0.0f, 1.0f, 1.0f, curvedGain);
    };


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
        double staticQuantUnit = 0.125; // 1/32nd note
        
        
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

            // Add post-stutter silence if macro gate < 1.0
            if (heldMacroGateParam < 1.0f) {
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
                    quantUnit = 1.0 / std::pow(2.0, currentQuantIndex);
                    quantToNewBeat = static_cast<int>(quantUnit / staticQuantUnit); // How many 1/32nds in chosen quant
                    quantToNewBeat = std::max(1, quantToNewBeat); // Ensure at least 1

                }
                
                // Reset quantCount based on current position, not arbitrarily to 0
                // This prevents timing drift between consecutive stutters
                double currentPpqInLoop = ppqAtStartOfBlock + (i * ppqPerSample);
                double thirtySecondNotes = currentPpqInLoop / 0.125;
                int totalThirtySeconds = static_cast<int>(std::floor(thirtySecondNotes));
                int currentBoundary = (totalThirtySeconds / quantToNewBeat) * quantToNewBeat;
                quantCount = totalThirtySeconds - currentBoundary;
                
                // Calculate stutter event duration for this quantization unit
                float gateScale = params.getRawParameterValue("autoStutterGate")->load();
                double quantDurationSeconds = (240.0 / bpm) * staticQuantUnit * (quantToNewBeat-quantCount);
                double gateDurationSeconds = juce::jlimit(quantDurationSeconds / 8.0, quantDurationSeconds, quantDurationSeconds * gateScale);
                stutterEventLengthSamples = static_cast<int>(sampleRate * gateDurationSeconds);
                
                // ACTIVATE SCHEDULED STUTTER EVENT
                if (stutterIsScheduled)
                {

                    // Engage stutter with rate system selection
                    autoStutterActive = true;
                    secondsPerWholeNote = 240.0 / bpm;

                    // DECISION: Whether this stutter event should be reversed
                    float reverseChance = parameters.getRawParameterValue("reverseChance")->load();
                    currentStutterIsReversed = juce::Random::getSystemRandom().nextFloat() < reverseChance;
                    firstRepeatCyclePlayed = false;
                    cycleCompletionCounter = 0; // Reset cycle counter for new stutter event

                    
                    // DECISION: Nano vs Rhythmical system selection
                    bool useNano = juce::Random::getSystemRandom().nextFloat() < nanoBlend;
                    int selectedIndex = useNano ? selectWeightedIndex(nanoRateWeights, 0) : selectWeightedIndex(regularRateWeights, 0);
                    
                    // DECISION: Rate selection from chosen system
                    if (useNano) {
                        double currentNanoTune = params.getRawParameterValue("nanoTune")->load();
                        double nanoBase = ((60.0 / bpm) / 16.0) / currentNanoTune;
                        double sliceDuration = nanoBase / params.getRawParameterValue("nanoRatio_" + std::to_string(selectedIndex))->load();
                        chosenDenominator = 240.0 / (bpm * sliceDuration);
                    } else {
                        chosenDenominator = regularDenominators[selectedIndex];
                    }
                    
                    autoStutterRemainingSamples = stutterEventLengthSamples;
                    stutterIsScheduled = false;

                    // RESET MACRO ENVELOPE for each new stutter event (including continuous stuttering)
                    macroEnvelopeCounter = 1; // Start at 1 to avoid zero-progress spikes

                    // Update macro envelope duration for this quantization unit
                    double quantDurationSeconds = (60.0 / bpm) * staticQuantUnit * (quantToNewBeat-quantCount);
                    int quantUnitLengthSamples = static_cast<int>(sampleRate * quantDurationSeconds);
                    macroEnvelopeLengthInSamples = quantUnitLengthSamples;

                    // CAPTURE FRESH AUDIO for each new stutter event (including continuous stuttering)
                    stutterPlayCounter = 0;
                    stutterWritePos = (writePos+i) % maxStutterLenSamples;

                    
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
                nextQuantIndex = selectWeightedIndex(quantUnitWeights, 1); // Default to 1/8

                
            }
                
        }
 
        // =============================================================================
        // DECISION POINT 2: PARAMETER SAMPLING (2ms before stutter event BEGINS)
        // Sample macro envelope controls 2ms before ANY stutter event starts
        // This ensures the next stutter event uses fresh parameter values
        // =============================================================================
        int twoMsInSamples = static_cast<int>(sampleRate * 0.002); // 2ms
        static bool parametersSampledForUpcomingEvent = false; // Prevent multiple sampling for same upcoming event

        // Check if a stutter event will start soon (using corrected quantization boundary logic)
        bool stutterStartingSoon = (stutterIsScheduled && quantCount >= std::max(1, quantToNewBeat - 1) && samplesToNextBeat <= twoMsInSamples);

        if (stutterStartingSoon) {
            if (!parametersSampledForUpcomingEvent) {
                // Sample parameters for the upcoming stutter event
                heldMacroGateParam = params.getRawParameterValue("MacroGate")->load();
                heldMacroShapeParam = params.getRawParameterValue("MacroShape")->load();
                heldMacroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();
                heldNanoGateParam = params.getRawParameterValue("NanoGate")->load();
                heldNanoShapeParam = params.getRawParameterValue("NanoShape")->load();
                heldNanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();
                parametersHeld = true;
                parametersSampledForUpcomingEvent = true; // Prevent re-sampling for same event

            }
        } else {
            // Reset flag when not approaching a stutter event
            parametersSampledForUpcomingEvent = false;
        }

        // INITIALIZE STUTTER EVENT when triggered (SIMPLIFIED: auto-stutter only)
        // bool isStutteringEvent = autoStutterActive || *params.getRawParameterValue("stutterOn"); // MANUAL STUTTER DISABLED
        static bool stutterInitialized = false;

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

            // ENSURE PARAMETERS ARE HELD - if not already sampled, sample them now
            if (!parametersHeld) {
                heldMacroGateParam = params.getRawParameterValue("MacroGate")->load();
                heldMacroShapeParam = params.getRawParameterValue("MacroShape")->load();
                heldMacroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();
                heldNanoGateParam = params.getRawParameterValue("NanoGate")->load();
                heldNanoShapeParam = params.getRawParameterValue("NanoShape")->load();
                heldNanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();
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
        if (samplesToNextBeat <= fadeLengthInSamples && samplesToNextBeat >= 0 && (quantCount+1) == quantToNewBeat) {
            dryFading = true;
            // Ensure proper boundary handling: when samplesToNextBeat=0, progress=1.0; when samplesToNextBeat=fadeLengthInSamples, progress=0.0
            float dryFadeProgress = juce::jlimit(0.0f, 1.0f, 1.0f - (float)samplesToNextBeat / (float)fadeLengthInSamples);

            if (!stutterIsScheduled && autoStutterActive) {
                // Stutter→Dry: dry fades from 0 to 1
                float startGain = 0.0f;
                float endGain = 1.0f;
                currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;


            } else if (stutterIsScheduled && autoStutterActive) {
                // Stutter→Stutter: dry fades from 0 to firstSampleGain
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * heldMacroGateParam));
                float firstSampleProgress = 1.0f / (float)effectiveMacroLength;
                float nextFirstSampleMacroGain = calculateGain(firstSampleProgress, heldMacroShapeParam);

                float macroSmoothAmount = heldMacroSmoothParam * 0.3f;
                if (macroSmoothAmount > 0.0f && firstSampleProgress < macroSmoothAmount) {
                    float fadeInGain = firstSampleProgress / macroSmoothAmount;
                    nextFirstSampleMacroGain *= fadeInGain;
                }

                // Calculate nano envelope gain for first sample (matching main processing logic)
                float nanoGateMultiplier = 0.25f + heldNanoGateParam * 0.75f;
                int nanoEnvelopeLengthSamples = std::max(1, static_cast<int>(stutterEventLengthSamples / chosenDenominator * nanoGateMultiplier));
                float firstSampleNanoProgress = 1.0f / (float)nanoEnvelopeLengthSamples;
                float nextFirstSampleNanoGain = calculateGain(firstSampleNanoProgress, heldNanoShapeParam);

                // Combine macro and nano gains for accurate first sample gain
                float nextFirstSampleGain = nextFirstSampleMacroGain * nextFirstSampleNanoGain;

                float startGain = 0.0f;
                float endGain = nextFirstSampleGain;
                currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;


            } else if (stutterIsScheduled && !autoStutterActive) {
                // Dry→Stutter: dry fades from 1 to firstSampleGain
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * heldMacroGateParam));
                float firstSampleProgress = 1.0f / (float)effectiveMacroLength;
                float nextFirstSampleMacroGain = calculateGain(firstSampleProgress, heldMacroShapeParam);

                float macroSmoothAmount = heldMacroSmoothParam * 0.3f;
                if (macroSmoothAmount > 0.0f && firstSampleProgress < macroSmoothAmount) {
                    float fadeInGain = firstSampleProgress / macroSmoothAmount;
                    nextFirstSampleMacroGain *= fadeInGain;
                }

                // Calculate nano envelope gain for first sample (matching main processing logic)
                float nanoGateMultiplier = 0.25f + heldNanoGateParam * 0.75f;
                int nanoEnvelopeLengthSamples = std::max(1, static_cast<int>(stutterEventLengthSamples / chosenDenominator * nanoGateMultiplier));
                float firstSampleNanoProgress = 1.0f / (float)nanoEnvelopeLengthSamples;
                float nextFirstSampleNanoGain = calculateGain(firstSampleNanoProgress, heldNanoShapeParam);

                // Combine macro and nano gains for accurate first sample gain
                float nextFirstSampleGain = nextFirstSampleMacroGain * nextFirstSampleNanoGain;

                float startGain = 1.0f;
                float endGain = nextFirstSampleGain;
                currentDryGain = startGain + (endGain - startGain) * dryFadeProgress;

            }
        } else if (!dryFading) {
            // No dry fade happening - set default states
            if (autoStutterActive && postStutterSilence <= 0) {
                // Active stuttering, no fade: dry silent
                currentDryGain = 0.0f;
            } else if (!autoStutterActive) {
                // No stutter: dry at full volume
                currentDryGain = 1.0f;
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
        int startOfLoopReadIndex = 0;
        bool shouldApplyNanoSmooth = false;
        int nanoFadeLen = 0;
        float nanoFadeProgress = 0.0f;

        if (autoStutterActive)
        {
            // Calculate stutter loop parameters
            loopLen = std::clamp(static_cast<int>((secondsPerWholeNote / chosenDenominator) * sampleRate), 1, maxStutterLenSamples);

            // Handle reverse playback logic
            loopPos = stutterPlayCounter % (loopLen);

            // Note: First cycle completion detection moved to counter update section

            if (currentStutterIsReversed && firstRepeatCyclePlayed) {
                // After first cycle, play in reverse within each loop cycle
                int reversedLoopPos = loopLen - 1 - loopPos;
                readIndex = (stutterWritePos + reversedLoopPos) % maxStutterLenSamples;

            } else {
                // Normal forward playback (including first cycle of reversed events)
                readIndex = (stutterWritePos + loopPos) % maxStutterLenSamples;

            }

            // Pre-calculate nano smooth parameters
            nanoFadeLen = static_cast<int>((float)loopLen * nanoSmoothParam * 0.25f);
            shouldApplyNanoSmooth = (nanoFadeLen > 1 && loopPos >= loopLen - nanoFadeLen && nanoSmoothParam > 0.0f);
            if (shouldApplyNanoSmooth) {
                if (currentStutterIsReversed && firstRepeatCyclePlayed) {
                    // For reverse playback, the start of loop is at the end of the forward direction
                    startOfLoopReadIndex = (stutterWritePos + loopLen - 1) % maxStutterLenSamples;
                } else {
                    // Normal forward direction
                    startOfLoopReadIndex = (stutterWritePos + stutterPlayCounter - loopPos) % maxStutterLenSamples;
                }
                nanoFadeProgress = (float)(loopPos - (loopLen - nanoFadeLen)) / (float)nanoFadeLen;
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
                float nanoGateMultiplier = 0.25f + nanoGateParam * 0.75f;
                nanoEnvelopeLengthInSamples = std::max(1, static_cast<int>((float)loopLen * nanoGateMultiplier));
                float nanoProgress = (float)loopPos / (float)nanoEnvelopeLengthInSamples;
                float nanoGain = calculateGain(nanoProgress, nanoShapeParam);;

                if (loopPos < nanoEnvelopeLengthInSamples) {
                    // Check if we're in fade-out region (1ms before effective gate end)
                    int fadeOutLen = static_cast<int>(sampleRate * 0.0001f); // 1ms fade-out
                    int fadeOutStart = std::max(0, nanoEnvelopeLengthInSamples - fadeOutLen);

                    if (loopPos >= fadeOutStart && fadeOutLen > 0) {
                        // We're in the fade-out region
                        float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(loopPos - fadeOutStart) / (float)fadeOutLen);
                        nanoGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                    }
                } else {
                    nanoGain = 0.0f; // Silent after effective gate
                }

                // MACRO ENVELOPE (controls overall event shape - uses held parameters)
                // MacroGate scales the envelope length (0.25 = 25% of total length, 1.0 = 100% of length)
                float macroGateScale = juce::jlimit(0.25f, 1.0f, heldMacroGateParam);
                int effectiveMacroLength = std::max(1, static_cast<int>((float)macroEnvelopeLengthInSamples * macroGateScale));
                float macroProgress = juce::jlimit(0.0f, 1.0f, (float)macroEnvelopeCounter / (float)effectiveMacroLength);
                float macroGain = calculateGain(macroProgress, heldMacroShapeParam);
                
                // Apply macro smooth fades (using held parameters)
                float macroSmoothAmount = heldMacroSmoothParam * 0.3f;
                if (macroSmoothAmount > 0.0f) {
                    if (macroProgress < macroSmoothAmount) {
                        macroGain *= macroProgress / macroSmoothAmount;
                    } else if (macroProgress > (1.0f - macroSmoothAmount)) {
                        macroGain *= (1.0f - macroProgress) / macroSmoothAmount;
                    }
                }

                if (macroEnvelopeCounter <= effectiveMacroLength) {
                    // Check if we're in fade-out region (1ms before effective gate end)
                    int fadeOutLen = static_cast<int>(sampleRate * 0.001f); // 1ms fade-out
                    int fadeOutStart = std::max(1, effectiveMacroLength - fadeOutLen);

                    if (macroEnvelopeCounter >= fadeOutStart && fadeOutLen > 0) {
                        // We're in the fade-out region
                        float fadeOutProgress = juce::jlimit(0.0f, 1.0f, (float)(macroEnvelopeCounter - fadeOutStart) / (float)fadeOutLen);
                        macroGain *= juce::jlimit(0.0f, 1.0f, 1.0f - fadeOutProgress);
                    }
                } else {
                    macroGain = 0.0f; // Silent after effective gate
                }



                

                // Generate base wet sample
                wetSample = stutterBuffer.getSample(ch, readIndex) * macroGain * nanoGain;


                // Apply nano smooth (crossfade between loop repetitions) - using pre-calculated values
                if (shouldApplyNanoSmooth)
                {
                    // Calculate nano gain for loop start position (beginning of nano envelope)
                    float loopStartNanoProgress = 0.0f; // Beginning of loop = beginning of nano envelope
                    float loopStartNanoGain = calculateGain(loopStartNanoProgress, nanoShapeParam);

                    float incomingSample = stutterBuffer.getSample(ch, startOfLoopReadIndex) * macroGain * loopStartNanoGain;
                    wetSample = wetSample * (1.0f - nanoFadeProgress) + incomingSample * nanoFadeProgress;
                }
            }

            // APPLY FADE GAINS from Decision Point 3
            float fadedDrySample = drySample * currentDryGain;
            float fadedWetSample = wetSample;


            // MIX MODES - determine final output
            float outputSample;
            if (mixMode == 0) {         // GATE MODE: wet signal or silence only
                outputSample = fadedWetSample;
            } else if (mixMode == 1) {  // INSERT MODE: stutter replaces dry signal
                // In insert mode, fade gains control replacement:
                // - During stutter: currentDryGain=0, currentWetGain=1 (wet replaces dry)
                // - During fade: gains transition smoothly
                // - When not stuttering: currentDryGain=1, currentWetGain=0 (dry only)
                outputSample = fadedDrySample + fadedWetSample;

            } else {                    // MIX MODE: blend during stutter, dry otherwise
                outputSample = (autoStutterActive && postStutterSilence <= 0) ? (fadedDrySample + fadedWetSample) * 0.5f : fadedDrySample;
            }

            buffer.setSample(ch, i, outputSample);
        }

        // UPDATE COUNTERS (after all channels have been processed with same indices)
        if (autoStutterActive) {
            ++stutterPlayCounter;

            // Track when we complete the first repeat cycle for reverse playback
            if (currentStutterIsReversed && !firstRepeatCyclePlayed && stutterPlayCounter >= loopLen) {
                firstRepeatCyclePlayed = true;
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
    writePos = (writePos + numSamples) % maxStutterLenSamples;

    // Apply waveshaping to final mixed output
    auto waveshapeAlgorithm = parameters.getRawParameterValue("WaveshapeAlgorithm")->load();
    auto waveshapeIntensity = parameters.getRawParameterValue("WaveshapeIntensity")->load();

    if (waveshapeIntensity > 0.0f && waveshapeAlgorithm > 0)
    {
        // Apply waveshaping manually with optimized processing
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);

            switch (static_cast<int>(waveshapeAlgorithm))
            {
                case 1: // Soft Clip
                    for (int sample = 0; sample < numSamples; ++sample)
                    {
                        float x = channelData[sample];
                        channelData[sample] = juce::jlimit(-1.0f, 1.0f, x * (1.0f + waveshapeIntensity));
                    }
                    break;
                case 2: // Tanh
                    {
                        float drive = 1.0f + waveshapeIntensity * 4.0f;
                        float normalization = 1.0f / std::tanh(drive);
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            float x = channelData[sample];
                            channelData[sample] = std::tanh(x * drive) * normalization;
                        }
                    }
                    break;
                case 3: // Hard Clip
                    {
                        float threshold = 1.0f - waveshapeIntensity * 0.8f;
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            float x = channelData[sample];
                            channelData[sample] = juce::jlimit(-threshold, threshold, x);
                        }
                    }
                    break;
                case 4: // Tube
                    {
                        float drive = 1.0f + waveshapeIntensity * 3.0f;
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            float x = channelData[sample];
                            float driven = x * drive;
                            channelData[sample] = driven / (1.0f + std::abs(driven));
                        }
                    }
                    break;
                case 5: // Asymmetric
                    {
                        float drive = 1.0f + waveshapeIntensity * 2.0f;
                        float normalization = 1.0f / std::tanh(drive);
                        for (int sample = 0; sample < numSamples; ++sample)
                        {
                            float x = channelData[sample];
                            if (x > 0.0f)
                                channelData[sample] = std::tanh(x * drive) * normalization;
                            else
                                channelData[sample] = x * (1.0f + waveshapeIntensity * 0.5f);
                        }
                    }
                    break;
            }
        }
    }
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
}

void NanoStuttAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
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
    auto quantLabels = juce::StringArray { "1/4", "1/8", "1/16", "1/32" };
    for (int i = 0; i < quantLabels.size(); ++i)
    {
        juce::String id = "quantProb_" + quantLabels[i];
        float defaultValue = (quantLabels[i] == "1/8") ? 1.0f : (quantLabels[i] == "1/16") ? 0.5f : 0.0f; // 1/8th at max, 1/16th at 50%, others at 0
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.0f, 1.0f, defaultValue));
    }

    auto rateLabels = juce::StringArray { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
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

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoRatio_" + juce::String(i);
        float defaultRatio = std::pow(2.0f, static_cast<float>(i) / 12.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), id, 0.1f, 2.0f, defaultRatio));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoGate", 1), "Nano Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoShape", 1), "Nano Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("NanoSmooth", 1), "Nano Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroGate", 1), "Macro Gate", 0.25f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroShape", 1), "Macro Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MacroSmooth", 1), "Macro Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("MixMode", 1), "Mix Mode",
        juce::StringArray{"Gate", "Insert", "Mix"}, 1));

    // Timing offset parameter for master track delay compensation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TimingOffset", 1), "Timing Offset (ms)",
        -100.0f, 100.0f, 0.0f));

    // Waveshaping parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("WaveshapeAlgorithm", 1), "Waveshape Algorithm",
        juce::StringArray{"None", "Soft Clip", "Tanh", "Hard Clip", "Tube", "Asymmetric"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("WaveshapeIntensity", 1), "Waveshape Intensity",
        0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}

void NanoStuttAudioProcessor::updateCachedParameters()
{
    static const std::array<std::string, 8> regularLabels = { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    static const std::array<std::string, 4> quantLabels = { "1/4", "1/8", "1/16", "1/32" };
   
    for (size_t i = 0; i < regularLabels.size(); ++i)
        regularRateWeights[i] = parameters.getRawParameterValue("rateProb_" + regularLabels[i])->load();

    for (int i = 0; i < 12; ++i)
        nanoRateWeights[i] = parameters.getRawParameterValue("nanoProb_" + std::to_string(i))->load();

    for (size_t i = 0; i < quantLabels.size(); ++i)
        quantUnitWeights[i] = parameters.getRawParameterValue("quantProb_" + quantLabels[i])->load();

    nanoBlend = parameters.getRawParameterValue("nanoBlend")->load();
}

void NanoStuttAudioProcessor::initializeParameterListeners()
{
    static const std::array<std::string, 8> regularLabels = { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    static const std::array<std::string, 4> quantLabels = { "1/4", "1/8", "1/16", "1/32" };

    for (const auto& label : regularLabels)
        parameters.addParameterListener("rateProb_" + label, this);

    for (const auto& label : quantLabels)
        parameters.addParameterListener("quantProb_" + label, this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoProb_" + std::to_string(i), this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoRatio_" + std::to_string(i), this);

    parameters.addParameterListener("nanoBlend", this);
    parameters.addParameterListener("TimingOffset", this);
    parameters.addParameterListener("WaveshapeAlgorithm", this);
    parameters.addParameterListener("WaveshapeIntensity", this);

    updateCachedParameters();
}

void NanoStuttAudioProcessor::parameterChanged(const juce::String&, float)
{
    updateCachedParameters();
}
