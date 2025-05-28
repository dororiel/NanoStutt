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

    // Clear any output channels beyond the input range
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    const bool isStuttering = *parameters.getRawParameterValue("stutterOn");

    if (isStuttering)
    {
        if (!stutterLatched)
        {
            // Prepare stutterBuffer to record from this moment on
            stutterSamplesWritten = 0;
            maxStutterLenSamples = static_cast<int>(std::round(3.0 * sampleRate));
            stutterBuffer.setSize(buffer.getNumChannels(), maxStutterLenSamples);
            stutterBuffer.clear();
            stutterWritePos = 0;
            stutterPlayCounter = 0;
            stutterLatched = true;
        }

        // === [A] Write incoming audio into stutterBuffer ===
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* in  = buffer.getReadPointer(ch);
            auto* sb  = stutterBuffer.getWritePointer(ch);

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

        // === [B] Determine loop length based on BPM ===
        int currentLoopLen = static_cast<int>(std::round(0.03 * sampleRate)); // fallback
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                double bpm = position->getBpm().orFallback(120.0);
                double secondsPer64th = 60.0 / bpm / 16.0;
                currentLoopLen = std::clamp(static_cast<int>(std::round(secondsPer64th * sampleRate)), 1, stutterSamplesWritten);
            }
        }

        // === [C] Play from beginning of stutterBuffer ===
        for (int i = 0; i < numSamples; ++i)
        {
            int readIndex = stutterPlayCounter % currentLoopLen;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float* sb = stutterBuffer.getReadPointer(ch);
                buffer.getWritePointer(ch)[i] = sb[readIndex];
            }

            ++stutterPlayCounter;
        }
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

    params.push_back (std::make_unique<juce::AudioParameterBool>("stutterOn", "Stutter On", false));

    return { params.begin(), params.end() };
}

