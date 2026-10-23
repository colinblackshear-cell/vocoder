#include "PluginProcessor.h"
#include "Parameters.h"

namespace
{
float raw (const juce::AudioProcessorValueTreeState& params, const char* id)
{
    return params.getRawParameterValue (id)->load();
}

int index (const juce::AudioProcessorValueTreeState& params, const char* id)
{
    return (int) raw (params, id);
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

void VocoderProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    vocoder.prepare (sampleRate);
    synth.prepare (sampleRate);
    arp.prepare (sampleRate);
    gate.prepare (sampleRate);
    filter.prepare (sampleRate);
    voiceLevel.setTimes (sampleRate, 5.0, 150.0);
    voiceLevel.reset();
    chorus.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    mix.reset (sampleRate, 0.02);
    resonance.reset (sampleRate, 0.02);
    cutoff.reset (sampleRate, 0.02);

    updateSettings (readTiming());
    inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
    mix.setCurrentAndTargetValue (mix.getTargetValue());
    resonance.setCurrentAndTargetValue (resonance.getTargetValue());
    cutoff.setCurrentAndTargetValue (cutoff.getTargetValue());
}

VocoderProcessor::Timing VocoderProcessor::readTiming()
{
    Timing t;
    t.bpm = raw (params, ParamIDs::tempo);

    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
        {
            if (auto bpm = position->getBpm(); bpm && *bpm > 0.0)
                t.bpm = *bpm;
            if (auto ppq = position->getPpqPosition(); ppq && position->getIsPlaying())
            {
                t.ppq = *ppq;
                t.hostPlaying = true;
            }
        }
    return t;
}

void VocoderProcessor::updateSettings (const Timing& timing)
{
    vox::Vocoder::Settings vs;
    vs.bands = index (params, ParamIDs::bands);
    vs.sharpness = raw (params, ParamIDs::sharpness);
    vs.attackMs = raw (params, ParamIDs::attack);
    vs.releaseMs = raw (params, ParamIDs::release);
    vs.formantSemitones = raw (params, ParamIDs::formant);
    vs.sibilance = raw (params, ParamIDs::sibilance);
    vocoder.setSettings (vs);

    vox::CarrierSynth::Settings cs;
    cs.waveform = (vox::Waveform) index (params, ParamIDs::waveform);
    cs.octave = index (params, ParamIDs::octave);
    cs.detuneCents = raw (params, ParamIDs::detune);
    cs.noise = raw (params, ParamIDs::noise);
    cs.attackMs = raw (params, ParamIDs::carrierAttack);
    cs.releaseMs = raw (params, ParamIDs::carrierRelease);
    cs.drone = raw (params, ParamIDs::drone) > 0.5f;
    cs.droneNote = index (params, ParamIDs::droneNote);
    synth.setSettings (cs);

    const double beatsPerSample = timing.bpm / 60.0 / sampleRate;

    vox::Arpeggiator::Settings as;
    as.mode = (vox::ArpMode) index (params, ParamIDs::arpMode);
    as.pattern = (vox::ArpPattern) index (params, ParamIDs::arpPattern);
    as.octaves = index (params, ParamIDs::arpOctaves);
    as.gate = raw (params, ParamIDs::arpGate);
    as.swing = raw (params, ParamIDs::arpSwing);
    as.gatePattern = index (params, ParamIDs::gatePattern);
    arp.setSettings (as, synth);

    const double stepBeats = NoteLengths::arpBeats[juce::jlimit (0, 5, index (params, ParamIDs::arpRate))];
    arp.setIncrement (beatsPerSample / stepBeats);
    if (timing.hostPlaying)
        arp.syncTo (timing.ppq / stepBeats);

    filter.setMode ((vox::FilterMode) index (params, ParamIDs::filterMode));
    cutoff.setTargetValue (raw (params, ParamIDs::cutoff));
    resonance.setTargetValue (raw (params, ParamIDs::resonance));
    filterDrive = raw (params, ParamIDs::filterDrive);
    voiceFollow = raw (params, ParamIDs::voiceFollow);
    lfoDepth = raw (params, ParamIDs::lfoDepth);

    lfo.setShape ((vox::LfoShape) index (params, ParamIDs::lfoShape));
    const double lfoBeats = NoteLengths::lfoBeats[juce::jlimit (0, 7, index (params, ParamIDs::lfoSync))];
    if (lfoBeats > 0.0)
    {
        lfo.setIncrement (beatsPerSample / lfoBeats);
        if (timing.hostPlaying)
            lfo.syncTo (timing.ppq / lfoBeats);
    }
    else
    {
        lfo.setIncrement (raw (params, ParamIDs::lfoRate) / sampleRate);
    }

    distortion.set (raw (params, ParamIDs::drive), raw (params, ParamIDs::crush));
    chorus.set (raw (params, ParamIDs::chorusRate), raw (params, ParamIDs::chorusDepth), raw (params, ParamIDs::chorusMix));

    const double delayBeats = NoteLengths::delayBeats[juce::jlimit (0, 5, index (params, ParamIDs::delayTime))];
    delay.set (delayBeats * 60.0 / timing.bpm, raw (params, ParamIDs::delayFeedback), raw (params, ParamIDs::delayMix),
               raw (params, ParamIDs::delayPingPong) > 0.5f);

    reverbMix = raw (params, ParamIDs::reverbMix);
    juce::Reverb::Parameters rp;
    rp.roomSize = raw (params, ParamIDs::reverbSize);
    rp.damping = raw (params, ParamIDs::reverbDamping);
    rp.wetLevel = reverbMix * 0.6f;
    rp.dryLevel = 1.0f - reverbMix * 0.5f;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    gate.setThresholdDb (raw (params, ParamIDs::gateThreshold));
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (raw (params, ParamIDs::inputGain)));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (raw (params, ParamIDs::outputGain)));
    mix.setTargetValue (raw (params, ParamIDs::mix));
}

void VocoderProcessor::handleMidi (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        arp.noteOn (message.getNoteNumber(), message.getFloatVelocity(), synth);
    else if (message.isNoteOff())
        arp.noteOff (message.getNoteNumber(), synth);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        arp.allNotesOff (synth);
}

void VocoderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    updateSettings (readTiming());

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
    const bool stereoOut = out.getNumChannels() > 1;

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

        // Vocoder, then the rhythm gate, then the filter.
        const float arpGain = arp.process (synth);
        float wet = vocoder.process (voice, synth.process()) * arpGain;

        const float follow = std::min (1.0f, voiceLevel.process (voice) * 4.0f);
        const float octaves = lfo.process() * lfoDepth + follow * voiceFollow;
        filter.setCutoff (cutoff.getNextValue() * std::exp2 (octaves), resonance.getNextValue());
        if (filterDrive > 0.0f)
            wet = std::tanh (wet * (1.0f + filterDrive * 6.0f)) / (1.0f + filterDrive * 2.0f);
        wet = filter.process (wet);

        wet = distortion.process (wet);

        // Stereo from here on.
        float left = wet, right = wet;
        chorus.process (left, right);
        delay.process (left, right);
        if (reverbMix > 0.0f)
            reverb.processStereo (&left, &right, 1);

        const float m = mix.getNextValue();
        const float gain = outputGain.getNextValue();
        left = std::tanh ((left * m + voice * (1.0f - m)) * gain);
        right = std::tanh ((right * m + voice * (1.0f - m)) * gain);

        out.setSample (0, i, stereoOut ? left : 0.5f * (left + right));
        if (stereoOut)
            out.setSample (1, i, right);
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
