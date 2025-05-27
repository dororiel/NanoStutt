/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NanoStuttAudioProcessor::NanoStuttAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
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
    const int maxBufferSeconds = 4;
    circularBuffer.setSize (getTotalNumOutputChannels(),
                            (int) (maxBufferSeconds * sampleRate));
    circularBuffer.clear();
    writePos = 0;

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

void NanoStuttAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples             = buffer.getNumSamples();
    auto sampleRate             = getSampleRate();

    // Clear any output channels beyond the input range
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // === [A] Write live input to circular buffer ===
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* in  = buffer.getReadPointer  (ch);
        auto* cb  = circularBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            cb[(writePos + i) % circularBuffer.getNumSamples()] = in[i];
    }
    writePos = (writePos + numSamples) % circularBuffer.getNumSamples();

    // === [B] Handle stutter or pass-through ===
    if (stutterOn)
    {
        // Latch a new slice if this is the first block of the stutter
        if (!stutterLatched)
        {
            if (auto* playHead = getPlayHead())
            {
                if (auto position = playHead->getPosition())
                {
                    auto bpm = position->getBpm().orFallback(120.0); // fallback if not provided
                    double beatsPerSecond = bpm / 60.0;
                    double secondsPer64th = 1.0 / (beatsPerSecond * 16.0);
                    stutterLenSamples = static_cast<int>(std::round(secondsPer64th * sampleRate));
                }
                else
                {
                    stutterLenSamples = static_cast<int>(std::round(0.03 * sampleRate)); // fallback ~30ms
                }
            }


            stutterReadStart   = (writePos - stutterLenSamples + circularBuffer.getNumSamples())
                                 % circularBuffer.getNumSamples();
            stutterPlayCounter = 0;
            stutterLatched     = true;
        }

        // Read from circular buffer and fill output
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            const auto* cb = circularBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                int readIndex = (stutterReadStart + stutterPlayCounter) % circularBuffer.getNumSamples();
                out[i] = cb[readIndex];

                ++stutterPlayCounter;
                if (stutterPlayCounter >= stutterLenSamples)
                    stutterPlayCounter = 0;
            }
        }
    }
    else
    {
        stutterLatched = false; // Reset for next time
        // No need to copy — original buffer already contains the live audio
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
