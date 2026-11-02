#include "PluginProcessor.h"
#include "Parameters.h"
#include "Presets.h"
#include "PluginEditor.h"

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
    clickDecay = (float) std::exp (-1.0 / (0.015 * sampleRate));

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
            if (auto sig = position->getTimeSignature(); sig && sig->numerator > 0 && sig->denominator > 0)
                t.beatsPerBar = sig->numerator * 4.0 / sig->denominator;
            if (auto ppq = position->getPpqPosition(); ppq && position->getIsPlaying())
            {
                t.ppq = *ppq;
                t.hostPlaying = true;
            }
        }

    if (! t.hostPlaying)
        t.ppq = clockPosition;
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
    cs.pulseWidth = raw (params, ParamIDs::pulseWidth);
    cs.pwmDepth = raw (params, ParamIDs::pwmDepth);
    static constexpr vox::Waveform osc2Waves[] = { vox::Waveform::saw, vox::Waveform::square, vox::Waveform::pulse,
                                                   vox::Waveform::triangle };
    cs.osc2Wave = osc2Waves[juce::jlimit (0, 3, index (params, ParamIDs::osc2Wave))];
    cs.osc2Semitones = raw (params, ParamIDs::osc2Semitones);
    cs.osc2FineCents = raw (params, ParamIDs::osc2Fine);
    cs.osc2Level = raw (params, ParamIDs::osc2Level);
    cs.subLevel = raw (params, ParamIDs::subLevel);
    cs.glideMs = raw (params, ParamIDs::glide);
    cs.vibratoHz = raw (params, ParamIDs::vibratoRate);
    cs.vibratoCents = raw (params, ParamIDs::vibratoDepth);
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

void VocoderProcessor::requestChord (const juce::Array<int>& notes, bool immediate)
{
    // Packed so the audio thread can take it with one atomic exchange:
    // bit 63 = present, bit 62 = immediate, bits 0-7 = count, then one byte per note.
    uint64_t packed = (uint64_t { 1 } << 63) | (immediate ? uint64_t { 1 } << 62 : 0);
    const int count = juce::jmin (4, notes.size());
    packed |= (uint64_t) count;
    for (int i = 0; i < count; ++i)
        packed |= (uint64_t) (notes[i] & 0x7f) << (8 * (i + 1));
    pendingChord = packed;
}

void VocoderProcessor::scheduleChord (uint64_t packed, const Timing& timing)
{
    queuedChord = packed;
    const bool immediate = (packed >> 62) & 1;
    const int mode = index (params, ParamIDs::chordTiming); // instant, next beat, next bar
    const double ppq = timing.ppq;

    // A click just after the beat (or bar) counts as on it, so a slightly late press isn't pushed a whole beat.
    if (immediate || mode == 0)
        chordApplyAt = -1.0e9;
    else if (mode == 1)
        chordApplyAt = ppq - std::floor (ppq) < 0.1 ? ppq : std::floor (ppq) + 1.0;
    else
    {
        const double bar = std::floor (ppq / timing.beatsPerBar) * timing.beatsPerBar;
        chordApplyAt = ppq - bar < 0.25 ? ppq : bar + timing.beatsPerBar;
    }
}

void VocoderProcessor::applyChord()
{
    for (int i = 0; i < chordCount; ++i)
        arp.noteOff (chordNotes[(size_t) i], synth);

    chordCount = (int) (queuedChord & 0xff);
    for (int i = 0; i < chordCount; ++i)
    {
        chordNotes[(size_t) i] = (int) ((queuedChord >> (8 * (i + 1))) & 0x7f);
        arp.noteOn (chordNotes[(size_t) i], 0.8f, synth);
    }
    queuedChord = 0;
}

void VocoderProcessor::scheduleRecording (Timing& timing)
{
    const auto state = recorder.getState();
    const double bar = timing.beatsPerBar;

    if (state == Recorder::State::idle)
        takeScheduled = stopScheduled = false;

    if (state == Recorder::State::countIn && ! takeScheduled)
    {
        if (timing.hostPlaying)
        {
            // Next bar line at least half a bar away, so there's time to get ready.
            recordStart = (std::floor (timing.ppq / bar) + 1.0) * bar;
            if (recordStart - timing.ppq < bar * 0.5)
                recordStart += bar;
        }
        else
        {
            // Our own clock: rewind it to one bar before bar 1, for an exact one-bar count-in.
            clockPosition = timing.ppq = -bar;
            recordStart = 0.0;
        }
        uiRecordStart = recordStart;
        takeScheduled = true;
        stopScheduled = false;
    }

    if (state == Recorder::State::stopping && ! stopScheduled)
    {
        recordStop = (std::floor (timing.ppq / bar) + 1.0) * bar;
        if (recordStop <= recordStart)
            recordStop = recordStart + bar;
        stopScheduled = true;
    }
}

void VocoderProcessor::addMetronome (juce::AudioBuffer<float>& out, const Timing& timing, double beatsPerSample)
{
    const bool counting = recorder.getState() == Recorder::State::countIn;
    const bool wanted = wrapperType == wrapperType_Standalone && raw (params, ParamIDs::metronome) > 0.5f;
    if (! counting && ! wanted && clickLevel < 1.0e-3f)
        return;

    const auto beatsPerBar = juce::jmax<juce::int64> (1, (juce::int64) std::round (timing.beatsPerBar));
    for (int i = 0; i < out.getNumSamples(); ++i)
    {
        const auto beat = (juce::int64) std::floor (timing.ppq + i * beatsPerSample);
        if (beat != lastClickBeat)
        {
            lastClickBeat = beat;
            if (counting || wanted)
            {
                const bool downbeat = ((beat % beatsPerBar) + beatsPerBar) % beatsPerBar == 0;
                clickLevel = downbeat ? 0.45f : 0.3f;
                clickStep = (float) (2.0 * juce::MathConstants<double>::pi * (downbeat ? 1600.0 : 1000.0) / sampleRate);
                clickPhase = 0.0f;
            }
        }

        if (clickLevel > 1.0e-3f)
        {
            const float click = std::sin (clickPhase) * clickLevel;
            clickPhase += clickStep;
            clickLevel *= clickDecay;
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.addSample (ch, i, click);
        }
    }
}

void VocoderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto timing = readTiming();
    scheduleRecording (timing);
    updateSettings (timing);
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const double beatsPerSample = timing.bpm / 60.0 / sampleRate;
    if (const auto packed = pendingChord.exchange (0); packed != 0)
        scheduleChord (packed, timing);

    const auto recState = recorder.getState();
    int writeFrom = (recState == Recorder::State::recording || recState == Recorder::State::stopping) ? 0 : -1;
    int writeTo = buffer.getNumSamples();
    bool beginTake = false, endTake = false;

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

        const double ppq = timing.ppq + i * beatsPerSample;
        if (queuedChord != 0 && ppq >= chordApplyAt)
            applyChord();

        if (takeScheduled && recState == Recorder::State::countIn && writeFrom < 0 && ppq >= recordStart)
        {
            writeFrom = i;
            beginTake = true;
        }
        if (stopScheduled && recState == Recorder::State::stopping && ! endTake && ppq >= recordStop)
        {
            writeTo = i;
            endTake = true;
        }

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

    // Record the take before the metronome is added, so the click never ends up in the file.
    if (beginTake)
        recorder.beginTake();
    if (writeFrom >= 0)
        recorder.write (out.getArrayOfReadPointers(), out.getNumChannels(), writeFrom, writeTo - writeFrom);
    if (endTake)
    {
        recorder.endTake();
        takeScheduled = stopScheduled = false;
    }

    addMetronome (out, timing, beatsPerSample);

    const double next = timing.ppq + numSamples * beatsPerSample;
    if (! timing.hostPlaying)
        clockPosition = next;
    uiPosition = next;
}

juce::AudioProcessorEditor* VocoderProcessor::createEditor()
{
    return new VocoderEditor (*this);
}

int VocoderProcessor::getNumPrograms()
{
    return (int) Presets::all().size();
}

int VocoderProcessor::getCurrentProgram()
{
    return juce::jlimit (0, getNumPrograms() - 1, (int) params.state.getProperty ("program", 0));
}

void VocoderProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    Presets::apply (params, Presets::all()[(size_t) index]);
    params.state.setProperty ("program", index, nullptr); // saved with the song
}

const juce::String VocoderProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? Presets::all()[(size_t) index].name : "";
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
