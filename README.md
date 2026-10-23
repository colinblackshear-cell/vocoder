# Vocoder

A live vocoder for music production. Your voice shapes a synth you play from a MIDI keyboard. It runs as an
Audio Unit in Logic Pro, as a VST3, and as a standalone Mac app for live use with any microphone.

## Get the latest build

1. Open the [Actions tab](../../actions/workflows/build.yml) and click the newest green run.
2. Under **Artifacts**, download **Vocoder-mac** and unzip it. Inside are three zips: the AU (for Logic),
   the VST3, and the standalone app.
3. Install:
   - **Logic (AU):** unzip `Vocoder-AU.zip` and move `Vocoder.component` to
     `~/Library/Audio/Plug-Ins/Components/`.
   - **Standalone app:** unzip `Vocoder-App.zip` and move `Vocoder.app` to `Applications`.
4. These builds are not notarized by Apple, so macOS blocks them at first. Open Terminal and run:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Vocoder.component
   xattr -dr com.apple.quarantine /Applications/Vocoder.app
   ```

## Use it in Logic Pro

1. Create a **Software Instrument** track.
2. In the instrument slot choose **AU MIDI-controlled Effects > Colin Blackshear > Vocoder**.
3. At the top of the plugin window, set **Side Chain** to your vocal track, or to the input your mic is on.
4. Play chords on your MIDI keyboard while you sing or talk.

If Logic doesn't list it, open **Logic Pro > Settings > Plug-in Manager**, find Vocoder, and click
**Reset & Rescan Selection**.

## Use it live (standalone app)

1. Open Vocoder.app and allow microphone access when asked.
2. Click **Options > Audio/MIDI Settings**. Pick your mic (built-in, USB or interface) as the input, your
   headphones or speakers as the output, and tick your MIDI keyboard.
3. The app mutes the mic at first to avoid feedback. Put on headphones, then click **Unmute** in the yellow bar.
4. No keyboard handy? Turn on **Drone (no keys)** and pick a **Drone Note**.

## Controls

| Section | Control | What it does |
| --- | --- | --- |
| Input | Input Gain | Boosts or cuts the mic before the vocoder |
| Input | Gate Threshold | Mutes the mic below this level so room noise doesn't hum through |
| Vocoder | Bands | 8 to 40 bands. Fewer sound more robotic, more sound clearer |
| Vocoder | Band Sharpness | Higher values give a ringing, metallic tone |
| Vocoder | Attack / Release | How fast each band follows your voice. Long release smears words together |
| Vocoder | Formant Shift | Moves the character of your voice up (smaller, chipmunk) or down (bigger, monster) |
| Vocoder | Sibilance | Lets your "s" and "t" sounds through so words stay clear |
| Carrier | Waveform | Saw (bright, classic), Square (hollow), Supersaw (wide, trance) |
| Carrier | Octave, Supersaw Detune, Noise | Pitch range, supersaw width, and breathy noise mixed into the synth |
| Carrier | Carrier Attack / Release | Fade in and out of each note |
| Carrier | Drone, Drone Note | Holds one note when no keys are pressed |
| Output | Dry/Wet, Output Gain | Blend in your plain voice, and the final level (soft-clipped so it can't blast) |

## Build from source

Requires CMake 3.22+ and Xcode.

```sh
cmake -S . -B build -G Xcode
cmake --build build --config Release
ctest --test-dir build -C Release
```

The DSP tests also build without JUCE: `cmake -S . -B build -DVOCODER_TESTS_ONLY=ON`.

## Roadmap

1. ~~Vocoder core: filter bank, envelope followers, MIDI carrier synth~~
2. Vocoder styles (presets), unison and chord memory
3. Arpeggiator: note arp and rhythm gate, synced to Logic's tempo
4. Filter (cutoff, resonance, drive) and LFO modulation
5. FX chain: distortion, chorus, phaser, delay, reverb, limiter
6. Custom interface, preset browser, MIDI learn
