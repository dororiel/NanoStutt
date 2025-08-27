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

    auto& params = parameters;
    bool autoStutter  = params.getRawParameterValue("autoStutterEnabled")->load();
    float chance      = params.getRawParameterValue("autoStutterChance")->load();
    int quantIndex    = (int) params.getRawParameterValue("autoStutterQuant")->load();
    auto mixMode      = (int) params.getRawParameterValue("MixMode")->load();

    auto nanoGateParam = params.getRawParameterValue("NanoGate")->load();
    auto nanoShapeParam = params.getRawParameterValue("NanoShape")->load();
    auto nanoSmoothParam = params.getRawParameterValue("NanoSmooth")->load();
    auto macroGateParam = params.getRawParameterValue("MacroGate")->load();
    auto macroShapeParam = params.getRawParameterValue("MacroShape")->load();
    auto macroSmoothParam = params.getRawParameterValue("MacroSmooth")->load();

    double ppqAtStartOfBlock = 0.0;
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            ppqAtStartOfBlock = position->getPpqPosition().orFallback(0.0);
            bpm = position->getBpm().orFallback(120.0);
        }

    double ppqPerSample = (bpm / 60.0) / sampleRate;
    double quantUnit = 1.0 / std::pow(2.0, quantIndex);

    stutterBuffer.copyFrom(0, writePos, buffer, 0, 0, numSamples);
    if (totalNumOutputChannels > 1)
        stutterBuffer.copyFrom(1, writePos, buffer, 1, 0, numSamples);
    
    auto calculateGain = [](float progress, float shapeParam) {
        float mappedShape = (shapeParam - 0.5f) * 2.0f;
        float curveAmount = std::abs(mappedShape);
        float exponent = 1.0f + curveAmount * 4.0f;
        float curvedGain = (mappedShape < 0.0f) ? std::pow(1.0f - progress, exponent) : std::pow(progress, exponent);
        return juce::jmap(curveAmount, 0.0f, 1.0f, 1.0f, curvedGain);
    };

    const float fadeIncrement = (fadeLengthInSamples > 0) ? 1.0f / (float)fadeLengthInSamples : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        double currentPpq = ppqAtStartOfBlock + (i * ppqPerSample);

        // --- Mid-point decision logic ---
        double decisionBeat = std::floor(currentPpq / quantUnit + 0.5);
        if (decisionBeat != lastDecisionBeat)
        {
            lastDecisionBeat = decisionBeat;
            if (autoStutter && juce::Random::getSystemRandom().nextFloat() < chance)
                stutterIsScheduled = true;
        }

        // --- Beat boundary logic ---
        double quantizedBeat = std::floor(currentPpq / quantUnit);
        bool isNewBeat = (quantizedBeat != lastQuantizedBeat);
        if (isNewBeat)
        {
            lastQuantizedBeat = quantizedBeat;
            if (postStutterSilence > 0) postStutterSilence = 0;

            if (stutterIsScheduled)
            {
                autoStutterActive = true;
                stutterLatched = false; 
                secondsPerWholeNote = 240.0 / bpm;
                bool useNano = juce::Random::getSystemRandom().nextFloat() < nanoBlend;
                auto getWeightedIndex = [](const auto& weights) {
                    int idx = 0; float total = std::accumulate(weights.begin(), weights.end(), 0.0f);
                    if (total > 0.0f) {
                        float r = juce::Random::getSystemRandom().nextFloat() * total; float accum = 0.0f;
                        for (int j = 0; j < (int)weights.size(); ++j) {
                            accum += weights[j]; if (r <= accum) { idx = j; break; }
                        }
                    } return idx;
                };
                int selectedIndex = useNano ? getWeightedIndex(nanoRateWeights) : getWeightedIndex(regularRateWeights);
                if (useNano) {
                    double currentNanoTune = params.getRawParameterValue("nanoTune")->load();
                    double nanoBase = ((60.0 / bpm) / 16.0) / currentNanoTune;
                    double sliceDuration = nanoBase / params.getRawParameterValue("nanoRatio_" + std::to_string(selectedIndex))->load();
                    chosenDenominator = 240.0 / (bpm * sliceDuration);
                } else {
                    chosenDenominator = regularDenominators[selectedIndex];
                }
                float gateScale = params.getRawParameterValue("autoStutterGate")->load();
                double quantDurationSeconds = (240.0 / bpm) * (1.0 / std::pow(2.0, quantIndex + 2));
                double gateDurationSeconds = juce::jlimit(quantDurationSeconds / 8.0, quantDurationSeconds, quantDurationSeconds * gateScale);
                autoStutterRemainingSamples = static_cast<int>(sampleRate * gateDurationSeconds);
                stutterIsScheduled = false;
            }
        }

        bool isStutteringEvent = autoStutterActive || *params.getRawParameterValue("stutterOn");
        if (isStutteringEvent && !stutterLatched)
        {
            stutterPlayCounter = 0;
            stutterWritePos = (writePos + i) % maxStutterLenSamples;
            stutterLatched = true;
            macroEnvelopeCounter = 0;
            macroEnvelopeLengthInSamples = autoStutterActive ? autoStutterRemainingSamples : std::numeric_limits<int>::max();
        }

        // --- Pre-emptive Crossfade Logic ---
        double nextBeatPpq = (quantizedBeat + 1.0) * quantUnit;
        int samplesToNextBeat = static_cast<int>((nextBeatPpq - currentPpq) / ppqPerSample);
        float targetCrossfade = eventCrossfade;

        bool needsFadeIn = (macroSmoothParam > 0.0f || macroShapeParam > 0.5f);
        bool isEndingFullEvent = (autoStutterActive && autoStutterRemainingSamples <= samplesToNextBeat && macroGateParam >= 1.0f);

        if (stutterIsScheduled && needsFadeIn && samplesToNextBeat < fadeLengthInSamples)
            targetCrossfade = 1.0f - (float)samplesToNextBeat / (float)fadeLengthInSamples;
        else if (isEndingFullEvent && samplesToNextBeat < fadeLengthInSamples)
            targetCrossfade = (float)samplesToNextBeat / (float)fadeLengthInSamples;
        else if (isStutteringEvent || postStutterSilence > 0)
            targetCrossfade = 1.0f;
        else
            targetCrossfade = 0.0f;

        if (eventCrossfade < targetCrossfade)
            eventCrossfade = std::min(targetCrossfade, eventCrossfade + fadeIncrement);
        else if (eventCrossfade > targetCrossfade)
            eventCrossfade = std::max(targetCrossfade, eventCrossfade - fadeIncrement);

        // --- Main Processing ---
        if (stutterLatched)
        {
            int loopLen = std::clamp(static_cast<int>((secondsPerWholeNote / chosenDenominator) * sampleRate), 1, maxStutterLenSamples);
            int readIndex = (stutterWritePos + stutterPlayCounter) % maxStutterLenSamples;
            int loopPos = stutterPlayCounter % loopLen;

            float nanoGateMultiplier = 0.25f + nanoGateParam * 0.75f;
            nanoEnvelopeLengthInSamples = std::max(1, static_cast<int>((float)loopLen * nanoGateMultiplier));
            float macroGateMultiplier = 0.25f + macroGateParam * 0.75f;
            int effectiveMacroLength = static_cast<int>((float)macroEnvelopeLengthInSamples * macroGateMultiplier);

            float macroProgress = (float)macroEnvelopeCounter / (float)effectiveMacroLength;
            float macroGain = (macroEnvelopeCounter < effectiveMacroLength) ? calculateGain(macroProgress, macroShapeParam) : 0.0f;
            
            float macroFadeLength = macroSmoothParam * 0.5f;
            if (macroProgress < macroFadeLength && macroFadeLength > 0.0f) macroGain *= macroProgress / macroFadeLength;
            else if (macroProgress > (1.0f - macroFadeLength) && macroFadeLength > 0.0f) macroGain *= (1.0f - macroProgress) / macroFadeLength;

            float nanoProgress = (float)loopPos / (float)nanoEnvelopeLengthInSamples;
            float nanoGain = (loopPos < nanoEnvelopeLengthInSamples) ? calculateGain(nanoProgress, nanoShapeParam) : 0.0f;

            float wetGain = 1.0f;
            if (autoStutterActive && autoStutterRemainingSamples <= fadeLengthInSamples)
                wetGain = (float)autoStutterRemainingSamples / (float)fadeLengthInSamples;
            if (postStutterSilence > 0) wetGain = 0.0f;

            float finalGain = macroGain * nanoGain * wetGain;

            for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            {
                float wetSample = stutterBuffer.getSample(ch, readIndex) * finalGain;

                int nanoFadeLen = static_cast<int>((float)loopLen * nanoSmoothParam * 0.5f);
                if (nanoFadeLen > 1 && loopPos >= loopLen - nanoFadeLen)
                {
                    int startOfLoopReadIndex = (stutterWritePos + stutterPlayCounter - loopPos) % maxStutterLenSamples;
                    float incomingSample = stutterBuffer.getSample(ch, startOfLoopReadIndex) * finalGain;
                    float fadeProgress = (float)(loopPos - (loopLen - nanoFadeLen)) / (float)nanoFadeLen;
                    wetSample = wetSample * (1.0f - fadeProgress) + incomingSample * fadeProgress;
                }

                float drySample = buffer.getSample(ch, i);
                float outputSample = drySample * (1.0f - eventCrossfade);

                if (mixMode == 0) outputSample += wetSample * eventCrossfade; // Gate
                if (mixMode == 1) outputSample = drySample * (1.0f - eventCrossfade) + wetSample * eventCrossfade; // Insert
                if (mixMode == 2) outputSample = drySample + (wetSample - drySample) * 0.5f * eventCrossfade; // Mix
                
                buffer.setSample(ch, i, outputSample);
            }
            stutterPlayCounter = (stutterPlayCounter + 1) % loopLen;
            macroEnvelopeCounter++;
            if (autoStutterActive) --autoStutterRemainingSamples;
        }
        else
        {
            for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            {
                float drySample = buffer.getSample(ch, i);
                buffer.setSample(ch, i, drySample * (1.0f - eventCrossfade));
            }
        }

        if (autoStutterActive && autoStutterRemainingSamples <= 0)
        {
            autoStutterActive = false;
            stutterLatched = false;
            if (macroGateParam < 1.0f) 
                postStutterSilence = fadeLengthInSamples;
        }
    }
    writePos = (writePos + numSamples) % maxStutterLenSamples;
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
        "autoStutterGate", "Auto Stutter Gate", 0.25f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("stutterOn", "Stutter On", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoStutterEnabled", "Auto Stutter Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("autoStutterChance", "Auto Stutter Chance", 0.0f, 1.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "autoStutterQuant", "Auto Stutter Quantization",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 1));

    auto rateLabels = juce::StringArray { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (const auto& label : rateLabels)
    {
        juce::String id = "rateProb_" + label;
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.0f));
    }

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoProb_" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>("nanoBlend", "Repeat/Nano", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("nanoTune", "Nano Tune", 0.75f, 2.0f, 1.0f));

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoRatio_" + juce::String(i);
        float defaultRatio = std::pow(2.0f, static_cast<float>(i) / 12.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.1f, 2.0f, defaultRatio));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoGate", "Nano Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoShape", "Nano Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoSmooth", "Nano Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroGate", "Macro Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroShape", "Macro Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroSmooth", "Macro Smooth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("MixMode", "Mix Mode", 
        juce::StringArray{"Gate", "Insert", "Mix"}, 0));

    return { params.begin(), params.end() };
}

void NanoStuttAudioProcessor::updateCachedParameters()
{
    static const std::array<std::string, 8> regularLabels = { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
   
    for (size_t i = 0; i < regularLabels.size(); ++i)
        regularRateWeights[i] = parameters.getRawParameterValue("rateProb_" + regularLabels[i])->load();

    for (int i = 0; i < 12; ++i)
        nanoRateWeights[i] = parameters.getRawParameterValue("nanoProb_" + std::to_string(i))->load();

    nanoBlend = parameters.getRawParameterValue("nanoBlend")->load();
}

void NanoStuttAudioProcessor::initializeParameterListeners()
{
    static const std::array<std::string, 8> regularLabels = { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };

    for (const auto& label : regularLabels)
        parameters.addParameterListener("rateProb_" + label, this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoProb_" + std::to_string(i), this);

    for (int i = 0; i < 12; ++i)
        parameters.addParameterListener("nanoRatio_" + std::to_string(i), this);

    parameters.addParameterListener("nanoBlend", this);

    updateCachedParameters();
}

void NanoStuttAudioProcessor::parameterChanged(const juce::String&, float)
{
    updateCachedParameters();
}