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
    circularBuffer.setSize(getTotalNumInputChannels(), static_cast<int>(sampleRate * maxStutterSeconds));
    circularBuffer.clear();
    writePos = 0;

    maxStutterLenSamples = static_cast<int>(sampleRate * maxStutterSeconds);


}

void NanoStuttAudioProcessor::setStutterOn (bool shouldStutter)
{
    stutterOn = shouldStutter;
    if (! stutterOn)
        stutterLatched = false;          // let normal audio flow next block
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
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples             = buffer.getNumSamples();
    auto sampleRate             = getSampleRate();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    auto& params = parameters;
    // === Auto-stutter triggering ===
    bool autoStutter  = parameters.getRawParameterValue("autoStutterEnabled")->load();
    float chance      = parameters.getRawParameterValue("autoStutterChance")->load();
    int quantIndex    = (int) parameters.getRawParameterValue("autoStutterQuant")->load();

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
                        secondsPerWholeNote = 240.0 / bpm; // 4 beats per whole note
                        std::vector<std::pair<double, int>> rates = {
                            { parameters.getRawParameterValue("rateProb_1/4")->load(), 4 },
                            { parameters.getRawParameterValue("rateProb_1/3")->load(), 3 },
                            { parameters.getRawParameterValue("rateProb_1/6")->load(), 6 },
                            { parameters.getRawParameterValue("rateProb_1/8")->load(), 8 },
                            { parameters.getRawParameterValue("rateProb_1/12")->load(), 12 },
                            { parameters.getRawParameterValue("rateProb_1/16")->load(), 16 },
                            { parameters.getRawParameterValue("rateProb_1/24")->load(), 24 },
                            { parameters.getRawParameterValue("rateProb_1/32")->load(), 32 }
                        };

                        double totalWeight = 0.0;
                        for (const auto& [weight, _] : rates)
                            totalWeight += weight;

                        chosenDenominator = 16; // default fallback
                        if (totalWeight > 0.0)
                        {
                            double r = juce::Random::getSystemRandom().nextDouble() * totalWeight;
                            double accum = 0.0;
                            for (const auto& [weight, denom] : rates)
                            {
                                accum += weight;
                                if (r <= accum)
                                {
                                    chosenDenominator = denom;
                                    break;
                                }
                            }
                        }
                        if (juce::Random::getSystemRandom().nextFloat() < chance)
                        {
                            float gateScale = parameters.getRawParameterValue("autoStutterGate")->load();
                            double quantDurationSeconds = 60.0 / bpm * quantUnit;
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
    
    // Handle manual stutter rate selection
    if (manualStutterTriggered && manualStutterRateDenominator > 0)
    {
        chosenDenominator = manualStutterRateDenominator;
        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
                bpm = position->getBpm().orFallback(120.0);
        }

        secondsPerWholeNote = 240.0 / bpm;
        autoStutterRemainingSamples = static_cast<int>((secondsPerWholeNote / chosenDenominator) * getSampleRate());
        autoStutterActive = false;
    }

    const bool guiStutter = *params.getRawParameterValue("stutterOn");

    bool isStuttering = guiStutter || autoStutterActive || manualStutterTriggered ;

    if (!guiStutter && autoStutterActive && autoStutterRemainingSamples <= 0)
    {
        autoStutterActive = false;
        stutterLatched = false;
        isStuttering = false;
    }


    if (isStuttering)
    {
        if (!stutterLatched)
        {
            stutterSamplesWritten = 0;
            stutterPlayCounter = 0;
            stutterWritePos = 0;
            maxStutterLenSamples = static_cast<int>(3.0 * sampleRate);
            stutterBuffer.setSize(buffer.getNumChannels(), maxStutterLenSamples);
            stutterBuffer.clear();
            stutterLatched = true;
        }

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* in = buffer.getReadPointer(ch);
            auto* sb = stutterBuffer.getWritePointer(ch);

            for (int i = 0; i < numSamples && stutterSamplesWritten < maxStutterLenSamples; ++i)
            {
                int writeIndex = (stutterWritePos + i) % maxStutterLenSamples;
                sb[writeIndex] = in[i];
                ++stutterSamplesWritten;
            }
        }

        if (stutterSamplesWritten < maxStutterLenSamples)
        {
            int writableSamples = std::min(numSamples, maxStutterLenSamples - stutterSamplesWritten);
            stutterWritePos = (stutterWritePos + writableSamples) % maxStutterLenSamples;
        }

        int loopLen = static_cast<int>(0.03 * sampleRate); // fallback
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                chosenDenominator = manualStutterRateDenominator > 0 ? manualStutterRateDenominator : chosenDenominator;
                double secondsPerSlice = secondsPerWholeNote / chosenDenominator;
                loopLen = std::clamp(static_cast<int>(secondsPerSlice * sampleRate), 1, stutterSamplesWritten);
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            int readIndex = stutterPlayCounter % loopLen;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float* sb = stutterBuffer.getReadPointer(ch);
                buffer.getWritePointer(ch)[i] = sb[readIndex];
            }
            ++stutterPlayCounter;
        }
        if (autoStutterActive && !guiStutter)
            autoStutterRemainingSamples -= numSamples;
    }
    else
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
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "autoStutterGate", "Auto Stutter Gate",
        juce::NormalisableRange<float>(0.25f, 1.0f, 0.001f), 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterBool>("stutterOn", "Stutter On", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>("autoStutterEnabled", "Auto Stutter Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("autoStutterChance", "Auto Stutter Chance", 0.0f, 1.0f, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "autoStutterQuant", "Auto Stutter Quantization",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    auto rateLabels = juce::StringArray { "1/4", "1/3", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
    for (int i = 0; i < rateLabels.size(); ++i)
    {
        juce::String id = "rateProb_" + rateLabels[i];
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.0f));
    }


    return { params.begin(), params.end() };
}

