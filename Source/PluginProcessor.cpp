#include "PluginProcessor.h"
#include "Parameters.h"

namespace
{
float raw (const juce::AudioProcessorValueTreeState& params, const char* id)
{
    return params.getRawParameterValue (id)->load();
}
} // namespace

VocoderProcessor::VocoderProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Voice", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "VocoderState", createParameterLayout())
{
}

bool VocoderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    const auto side = layouts.getChannelSet (true, 1);
    return side.isDisabled() || side == juce::AudioChannelSet::mono() || side == juce::AudioChannelSet::stereo();
}

void VocoderProcessor::prepareToPlay (double sampleRate, int)
{
    vocoder.prepare (sampleRate);
    synth.prepare (sampleRate);
    gate.prepare (sampleRate);

    for (auto* smoothed : { &inputGain, &outputGain, &mix })
        smoothed->reset (sampleRate, 0.02);

    updateSettings();
    inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
    mix.setCurrentAndTargetValue (mix.getTargetValue());
}

void VocoderProcessor::updateSettings()
{
    vox::Vocoder::Settings vs;
    vs.bands = (int) raw (params, ParamIDs::bands);
    vs.sharpness = raw (params, ParamIDs::sharpness);
    vs.attackMs = raw (params, ParamIDs::attack);
    vs.releaseMs = raw (params, ParamIDs::release);
    vs.formantSemitones = raw (params, ParamIDs::formant);
    vs.sibilance = raw (params, ParamIDs::sibilance);
    vocoder.setSettings (vs);

    vox::CarrierSynth::Settings cs;
    cs.waveform = (vox::Waveform) (int) raw (params, ParamIDs::waveform);
    cs.octave = (int) raw (params, ParamIDs::octave);
    cs.detuneCents = raw (params, ParamIDs::detune);
    cs.noise = raw (params, ParamIDs::noise);
    cs.attackMs = raw (params, ParamIDs::carrierAttack);
    cs.releaseMs = raw (params, ParamIDs::carrierRelease);
    cs.drone = raw (params, ParamIDs::drone) > 0.5f;
    cs.droneNote = (int) raw (params, ParamIDs::droneNote);
    synth.setSettings (cs);

    gate.setThresholdDb (raw (params, ParamIDs::gateThreshold));
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (raw (params, ParamIDs::inputGain)));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (raw (params, ParamIDs::outputGain)));
    mix.setTargetValue (raw (params, ParamIDs::mix));
}

void VocoderProcessor::handleMidi (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        synth.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        synth.noteOff (message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        synth.allNotesOff();
}

void VocoderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    updateSettings();

    // In Logic the voice usually arrives on the sidechain; standalone it is the main input.
    // Summing both covers every setup, since the unused one is silent.
    const auto mainIn = getBusBuffer (buffer, true, 0);
    const auto sideIn = getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled()
                            ? getBusBuffer (buffer, true, 1)
                            : juce::AudioBuffer<float>();
    auto out = getBusBuffer (buffer, false, 0);

    const int numSamples = buffer.getNumSamples();
    const int mainChannels = mainIn.getNumChannels();
    const int sideChannels = sideIn.getNumChannels();
    const float mainScale = mainChannels > 0 ? 1.0f / (float) mainChannels : 0.0f;
    const float sideScale = sideChannels > 0 ? 1.0f / (float) sideChannels : 0.0f;

    auto nextEvent = midi.begin();

    for (int i = 0; i < numSamples; ++i)
    {
        for (; nextEvent != midi.end() && (*nextEvent).samplePosition <= i; ++nextEvent)
            handleMidi ((*nextEvent).getMessage());

        float voice = 0.0f;
        for (int ch = 0; ch < mainChannels; ++ch)
            voice += mainIn.getSample (ch, i) * mainScale;
        for (int ch = 0; ch < sideChannels; ++ch)
            voice += sideIn.getSample (ch, i) * sideScale;

        voice = gate.process (voice * inputGain.getNextValue());
        const float wet = vocoder.process (voice, synth.process());
        const float m = mix.getNextValue();
        const float sample = std::tanh ((wet * m + voice * (1.0f - m)) * outputGain.getNextValue());

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.setSample (ch, i, sample);
    }

    for (; nextEvent != midi.end(); ++nextEvent)
        handleMidi ((*nextEvent).getMessage());

    midi.clear();
}

juce::AudioProcessorEditor* VocoderProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void VocoderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocoderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocoderProcessor();
}
