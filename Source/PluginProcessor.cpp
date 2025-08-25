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
    return 0.0;
}

int NanoStuttAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    // ----- stutter: allocate ~4 s buffer -----
    const double maxStutterSeconds = 3.0;
    writePos = 0;
    maxStutterLenSamples = static_cast<int>(sampleRate * maxStutterSeconds);

}



void NanoStuttAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NanoStuttAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

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

    juce::AudioBuffer<float> originalInput;
    originalInput.makeCopyOf(buffer);

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

    if (!(*params.getRawParameterValue("stutterOn")) && autoStutter)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                auto ppq = position->getPpqPosition();
                if (ppq.hasValue())
                {
                    double bpm = position->getBpm().orFallback(120.0);
                    double quantUnit = 1.0 / std::pow(2.0, quantIndex);
                    double quantizedBeat = std::floor(*ppq / quantUnit);

                    if (quantizedBeat != lastQuantizedBeat)
                    {
                        lastQuantizedBeat = quantizedBeat;
                        secondsPerWholeNote = 240.0 / bpm;
                        bool useNano = juce::Random::getSystemRandom().nextFloat() < nanoBlend;

                        auto getWeightedIndex = [](const auto& weights) {
                            int idx = 0;
                            float total = std::accumulate(weights.begin(), weights.end(), 0.0f);
                            if (total > 0.0f) {
                                float r = juce::Random::getSystemRandom().nextFloat() * total;
                                float accum = 0.0f;
                                for (int i = 0; i < (int)weights.size(); ++i) {
                                    accum += weights[i];
                                    if (r <= accum) { idx = i; break; }
                                }
                            }
                            return idx;
                        };

                        int selectedIndex = useNano ? getWeightedIndex(nanoRateWeights) : getWeightedIndex(regularRateWeights);

                        if (useNano)
                        {
                            double currentNanoTune = params.getRawParameterValue("nanoTune")->load();
                            double nanoBase = ((60.0 / bpm) / 16.0) / currentNanoTune;
                            double sliceDuration = nanoBase / params.getRawParameterValue("nanoRatio_" + std::to_string(selectedIndex))->load();
                            chosenDenominator = 240.0 / (bpm * sliceDuration);
                        }
                        else
                        {
                            chosenDenominator = regularDenominators[selectedIndex];
                        }

                        if (juce::Random::getSystemRandom().nextFloat() < chance)
                        {
                            float gateScale = params.getRawParameterValue("autoStutterGate")->load();
                            double quantDurationSeconds = (240.0 / bpm) * (1.0 / std::pow(2.0, quantIndex + 2));
                            double gateDurationSeconds = juce::jlimit(quantDurationSeconds / 8.0, quantDurationSeconds, quantDurationSeconds * gateScale);
                            autoStutterRemainingSamples = static_cast<int>(sampleRate * gateDurationSeconds);
                            autoStutterActive = true;
                            stutterLatched = false;
                        }
                    }
                }
            }
        }
    }

    if (manualStutterTriggered && manualStutterRateDenominator > 0)
    {
        chosenDenominator = manualStutterRateDenominator;
        double bpm = 120.0;
        if (auto* playHead = getPlayHead()) { if (auto position = playHead->getPosition()) bpm = position->getBpm().orFallback(120.0); }
        secondsPerWholeNote = 240.0 / bpm;
        autoStutterRemainingSamples = static_cast<int>((secondsPerWholeNote / chosenDenominator) * getSampleRate());
        autoStutterActive = false;
    }

    const bool guiStutter = *params.getRawParameterValue("stutterOn");
    bool isStutteringEvent = guiStutter || autoStutterActive || manualStutterTriggered;

    if (isStutteringEvent && !stutterLatched)
    {
        stutterSamplesWritten = 0;
        stutterPlayCounter = 0;
        stutterWritePos = 0;
        maxStutterLenSamples = static_cast<int>(3.0 * sampleRate);
        stutterBuffer.setSize(buffer.getNumChannels(), maxStutterLenSamples);
        stutterBuffer.clear();
        stutterLatched = true;

        macroEnvelopeCounter = 0;
        if (autoStutterActive)
            macroEnvelopeLengthInSamples = autoStutterRemainingSamples;
        else 
            macroEnvelopeLengthInSamples = std::numeric_limits<int>::max();
    }

    if (stutterLatched)
    {
        for (int ch = 0; ch < originalInput.getNumChannels(); ++ch)
        {
            auto* in = originalInput.getReadPointer(ch);
            auto* sb = stutterBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples && stutterSamplesWritten < maxStutterLenSamples; ++i)
            {
                sb[(stutterWritePos + i) % maxStutterLenSamples] = in[i];
                ++stutterSamplesWritten;
            }
        }
        if (stutterSamplesWritten < maxStutterLenSamples)
            stutterWritePos = (stutterWritePos + std::min(numSamples, maxStutterLenSamples - stutterSamplesWritten)) % maxStutterLenSamples;
    }

    auto calculateGain = [](float progress, float shapeParam) {
        float mappedShape = (shapeParam - 0.5f) * 2.0f;
        float curveAmount = std::abs(mappedShape);
        float exponent = 1.0f + curveAmount * 4.0f;
        float curvedGain = (mappedShape < 0.0f) ? std::pow(1.0f - progress, exponent) : std::pow(progress, exponent);
        return juce::jmap(curveAmount, 0.0f, 1.0f, 1.0f, curvedGain);
    };

    int loopLen = 1;
    if (stutterLatched) {
        if (auto* playHead = getPlayHead()) { if (auto position = playHead->getPosition()) {
            double bpm = position->getBpm().orFallback(120.0);
            secondsPerWholeNote = 240.0 / bpm;
            chosenDenominator = manualStutterRateDenominator > 0 ? manualStutterRateDenominator : chosenDenominator;
            loopLen = std::clamp(static_cast<int>((secondsPerWholeNote / chosenDenominator) * sampleRate), 1, stutterSamplesWritten);
        }}
    }

    float nanoGateMultiplier = 0.25f + nanoGateParam * 0.75f;
    nanoEnvelopeLengthInSamples = std::max(1, static_cast<int>((float)loopLen * nanoGateMultiplier));

    float macroGateMultiplier = 0.25f + macroGateParam * 0.75f;
    int effectiveMacroLength = static_cast<int>((float)macroEnvelopeLengthInSamples * macroGateMultiplier);

    for (int i = 0; i < numSamples; ++i)
    {
        bool shouldPlayWetSignal = (guiStutter || (autoStutterActive && autoStutterRemainingSamples > 0) || manualStutterTriggered);
        
        float macroGain = 0.0f;
        float nanoGain = 0.0f;

        if (shouldPlayWetSignal)
        {
            if (stutterPlayCounter % loopLen == 0) nanoEnvelopeCounter = 0;

            // Macro Envelope
            float macroProgress = (float)macroEnvelopeCounter / (float)effectiveMacroLength;
            macroGain = (macroEnvelopeCounter < effectiveMacroLength) ? calculateGain(macroProgress, macroShapeParam) : 0.0f;
            
            // Macro Smooth: Windowing
            float macroFadeLength = macroSmoothParam * 0.5f; // 0 to 0.5
            if (macroProgress < macroFadeLength && macroFadeLength > 0.0f)
                macroGain *= macroProgress / macroFadeLength;
            else if (macroProgress > (1.0f - macroFadeLength) && macroFadeLength > 0.0f)
                macroGain *= (1.0f - macroProgress) / macroFadeLength;

            // Nano Envelope
            float nanoProgress = (float)nanoEnvelopeCounter / (float)nanoEnvelopeLengthInSamples;
            nanoGain = (nanoEnvelopeCounter < nanoEnvelopeLengthInSamples) ? calculateGain(nanoProgress, nanoShapeParam) : 0.0f;

            // Nano Smooth: Crossfading
            float nanoFadeTime = juce::jmap(nanoSmoothParam, 0.0f, 1.0f, 0.0f, 0.5f) * (float)nanoEnvelopeLengthInSamples;
            if (nanoEnvelopeCounter < nanoFadeTime && nanoFadeTime > 0)
                nanoGain *= (float)nanoEnvelopeCounter / nanoFadeTime;
            else if (nanoEnvelopeCounter > nanoEnvelopeLengthInSamples - nanoFadeTime && nanoFadeTime > 0)
                nanoGain *= (float)(nanoEnvelopeLengthInSamples - nanoEnvelopeCounter) / nanoFadeTime;
        }

        float finalGain = macroGain * nanoGain;
        int readIndex = stutterPlayCounter % loopLen;

        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            float wetSample = 0.0f;
            if (shouldPlayWetSignal)
            {
                int readChannel = (stutterBuffer.getNumChannels() > ch) ? ch : 0;
                if (stutterBuffer.getNumChannels() > 0)
                    wetSample = stutterBuffer.getReadPointer(readChannel)[readIndex] * finalGain;
            }

            float drySample = originalInput.getSample(ch, i);
            float outputSample = 0.0f;

            switch (mixMode)
            {
                case 0: // Gate
                    outputSample = shouldPlayWetSignal ? wetSample : 0.0f;
                    break;
                case 1: // Insert
                    outputSample = isStutteringEvent ? (shouldPlayWetSignal ? wetSample : 0.0f) : drySample;
                    break;
                case 2: // Mix
                    outputSample = shouldPlayWetSignal ? (wetSample * 0.5f + drySample * 0.5f) : drySample;
                    break;
            }
            buffer.getWritePointer(ch)[i] = outputSample;
        }

        if (shouldPlayWetSignal)
        {
            ++stutterPlayCounter;
            ++nanoEnvelopeCounter;
            ++macroEnvelopeCounter;
            if (autoStutterActive) --autoStutterRemainingSamples;
        }
    }

    if (autoStutterActive && autoStutterRemainingSamples <= 0)
    {
        autoStutterActive = false;
        stutterLatched = false;
    }
    if (!isStutteringEvent)
    {
        stutterLatched = false;
    }
}


//==============================================================================
bool NanoStuttAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NanoStuttAudioProcessor::createEditor()
{
    return new NanoStuttAudioProcessorEditor (*this);
}

//==============================================================================
void NanoStuttAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void NanoStuttAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
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

    // Traditional stutter rate probabilities
    auto rateLabels = juce::StringArray { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (const auto& label : rateLabels)
    {
        juce::String id = "rateProb_" + label;
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.0f));
    }

    // Nano stutter rate probabilities
    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoProb_" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.0f));
    }


    // Blend between traditional and nano set
    params.push_back(std::make_unique<juce::AudioParameterFloat>("nanoBlend", "Repeat/Nano", 0.0f, 1.0f, 0.0f));

    // Nano tune and editable ratios
    params.push_back(std::make_unique<juce::AudioParameterFloat>("nanoTune", "Nano Tune", 0.75f, 2.0f, 1.0f));

    for (int i = 0; i < 12; ++i)
    {
        juce::String id = "nanoRatio_" + juce::String(i);
        float defaultRatio = std::pow(2.0f, static_cast<float>(i) / 12.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.1f, 2.0f, defaultRatio));
    }

    // Envelope Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoGate", "Nano Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoShape", "Nano Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NanoSmooth", "Nano Smooth", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroGate", "Macro Gate", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroShape", "Macro Shape", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MacroSmooth", "Macro Smooth", 0.0f, 1.0f, 0.0f));

    // Mix Mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>("MixMode", "Mix Mode", 
        juce::StringArray{"Gate", "Insert", "Mix"}, 0));

    // ✅ RETURN AT THE END
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

    updateCachedParameters(); // Initialize
}

void NanoStuttAudioProcessor::parameterChanged(const juce::String&, float)
{
    updateCachedParameters(); // any change triggers full update
}