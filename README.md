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

You don't need Logic or a MIDI keyboard: the app has its own synth, on-screen keyboard and recorder.

1. Open Vocoder.app and allow microphone access when asked.
2. In the bar at the top, pick your **Mic** and **Output** (use headphones), then tick **Mic on**.
   A MIDI keyboard is picked up automatically; choose one under **Keyboard** if you have several.
3. Play notes any of these ways:
   - Click a **chord button** (pick the key and Major/Minor first). Click it again to stop.
   - Click keys on the **on-screen keyboard**. Turn on **Hold keys** to make clicked notes keep playing.
   - Type on your computer keyboard: **A S D F G H J K** are the white keys, **W E T Y U** the black keys.
     **Oct -** and **Oct +** move them up or down.
   - Or turn on **Drone (no keys)** in the Carrier section for a single held note.
4. Pick a sound from the **Sound** menu (or step through with **<** and **>**).
5. Set the **Tempo** and tick **Click** for a metronome. Press **Record**: you get a one-bar count-in,
   then recording starts exactly on the downbeat. Press **Stop** and it finishes at the end of that bar,
   so every take is a whole number of bars and loops cleanly in Logic at the same tempo. The click is
   never recorded. Takes are saved as WAVs in `Music/Vocoder Recordings`; **Files** opens that folder.
   Pressing **Cancel** during the count-in throws the take away.

Chord buttons change on the next beat by default, so a slightly early or late click still lands in time.
**Chord Timing** (in the Timing section) switches between Instant, Next Beat and Next Bar.

Your devices and settings are remembered for next time.

## The window

Pick a view at the top right:

- **Easy** builds a sound one step at a time with big buttons: **1 Voice** (Robot, Daft, Choir, Whisper,
  Monster, Chipmunk, Alien, Retro Game), **2 Chords** (Simple, Dreamy, Jazzy, Floaty, Tense, Sparkly, Rock,
  Huge), **3 Rhythm** (arps and gates), **4 Effects** (Echo, Space, Wide, Wobble, Wah, Crunch: turn on as many
  as you like) and **5 Auto-Tune**. Each step has a few knobs to fine-tune it. Good for kids and quick ideas.
- **Studio** shows the main controls as knobs in coloured sections.
- **Advanced** shows every setting as a list.

A new Vocoder starts as just your **raw voice**. The **Your sound** bar above the chord buttons shows the
chain your voice flows through, and each part switches on and off on its own:

**Your voice → Auto-Tune → Robot → Rhythm → Effects**

- **Your voice** is your own voice from the mic (auto-tuned when Auto-Tune is on).
- **Auto-Tune** snaps your voice to the notes of the key.
- **Robot** is the vocoder: your words, played by the synth on the notes of your chord. It needs notes.
- **Rhythm** chops everything into a beat (Rhythm Gate), or arpeggiates the robot's chord (Note Arp).
- **Effects** adds echo, reverb, chorus and distortion to everything.

Mix and match: auto-tune only, auto-tune and rhythm, robot and effects, and so on. The sliders next to
Your voice and Robot set their levels. Presets switch on the parts they use.

Hover over any control for a tip, and double-click a knob to reset it. The view you pick is saved with the song.

## Chord buttons

Pick a **Key**, a **Scale** (Major, Minor, Dorian, Mixolydian, Lydian, Phrygian, Harmonic Minor) and a
**Chord type** (Simple, Seventh, Ninth, Sus2, Sus4, Add9, Sixth, Power, Big). The seven buttons are the chords
of that scale, named like Cm7 or Fmaj7. Changing the type while a chord plays switches it straight away.

## Auto-tune

- **Auto-Tune** snaps your voice to the nearest note. Pick the song's **Key** and **Scale**, or leave the
  scale on Chromatic to work in any key. **Speed** at 0 gives the hard, robotic effect; higher sounds
  natural. **Amount** blends in less correction. You hear the tuned voice through **Your voice** in the Hear bar.
- **Sing to Play** makes the synth follow the melody you sing, so you don't need any keys. **Melody + Third**
  and **Full Chord** add harmony notes in your key. (In Chromatic, the harmony uses the major scale of the key.)
- The strip under the knobs shows the note you're singing and how far off it is.

## Sounds (presets)

The **Sound** menu (also in Logic's preset menu at the top of the plugin window) has ready-made starting
points: Raw Voice, Classic Robot, French Touch, Choir Pad, Whisper Ghost, Trance Gate, Stutter Chop, Arp Robot,
Acid Wobble, Auto-Wah Funk, Lo-Fi Radio, Monster, Chipmunk Bot, Space Echo, Hard Tune, Sing the Synth,
Harmony Choir, Talk Box Funk, 80s Synth Pop, Underwater, Glitch Bot, Cathedral, Ice Crystal, Lullaby,
Disco Bounce, Gallop Rider, Dubstep Growl and Pop Star, grouped by category. Choosing one resets the sound but leaves your input gain, gate, tempo, metronome, key, scale,
drone, chord settings and output level alone. Tweak from there; Logic
saves your changes with the song.

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
| Carrier | Waveform | Saw (bright, classic), Square (hollow), Supersaw (wide, trance), Pulse (nasal), Triangle (soft) |
| Carrier | Pulse Width, Pulse Width Motion | Shape of the Pulse wave, and a slow sweep of it for movement |
| Carrier | Osc 2 Waveform, Tune, Fine, Level | A second oscillator, e.g. +7 for fifths or +12 for an octave |
| Carrier | Sub Osc Level | A square wave an octave down for weight |
| Carrier | Glide | Slides between notes |
| Carrier | Vibrato Rate / Depth | Pitch wobble, for a more human or a seasick sound |
| Carrier | Octave, Supersaw Detune, Noise | Pitch range, supersaw width, and breathy noise mixed into the synth |
| Carrier | Carrier Attack / Release | Fade in and out of each note |
| Carrier | Drone, Drone Note | Holds one note when no keys are pressed |
| Arpeggiator | Arp Mode | **Note Arp** plays your held chord one note at a time. **Rhythm Gate** chops the sound into a pattern |
| Arpeggiator | Arp Rate, Pattern, Octaves | Step length (1/4 to 1/32, triplets), note order (up, down, up/down, random, as played) and range |
| Arpeggiator | Gate Length, Swing | How long each step sounds, and how much every second step is pushed late |
| Arpeggiator | Gate Pattern | Rhythm for the gate mode: Straight, Eighths, Offbeats, Trance, Gallop, Stutter, Sparse |
| Timing | Tempo (standalone) | Used when there's no DAW. In Logic everything follows the project tempo and locks to the bar |
| Timing | Metronome (standalone) | The click, the same as the **Click** box |
| Timing | Chord Timing | When a chord button change takes effect: Instant, Next Beat or Next Bar |
| Auto-Tune | Auto-Tune, Key, Scale | Pitch correction on your voice, to the notes of the chosen key and scale |
| Auto-Tune | Retune Speed, Tune Amount | How fast and how fully notes snap |
| Auto-Tune | Sing to Play | Off, Melody, Melody + Third, Full Chord: your sung notes play the synth |
| Filter | Filter Type, Cutoff, Resonance | Low pass, high pass, band pass or notch after the vocoder. High resonance whistles |
| Filter | Filter Drive | Saturates into the filter for a grittier, acid-style sweep |
| Filter | Voice Follow | Opens (or closes) the filter as you get louder, like an auto-wah |
| Filter | LFO Shape, Sync, Rate, LFO to Cutoff | Wobbles the cutoff. Sync locks it to bars and beats; Free uses Rate in Hz |
| Effects | Drive, Bit Crush | Distortion and lo-fi digital grit |
| Effects | Chorus Mix, Rate, Depth | Widens the sound in stereo |
| Effects | Delay Mix, Time, Feedback, Ping-Pong | Tempo-synced echoes, bouncing left and right |
| Effects | Reverb Mix, Size, Damping | Space, from a small room to a huge hall |
| Output | Your Voice On, Robot On, Rhythm On, Effects On | Switch each part of the chain on or off |
| Output | Robot Level, Voice Level, Output Gain | Levels for the robot and your (tuned) voice, and the final level (soft-clipped so it can't blast) |

## Build from source

Requires CMake 3.22+ and Xcode.

```sh
cmake -S . -B build -G Xcode
cmake --build build --config Release
ctest --test-dir build -C Release
```

`ctest` runs the DSP tests and a smoke test that plays the whole plugin offline with every section on.

The DSP tests also build without JUCE: `cmake -S . -B build -DVOCODER_TESTS_ONLY=ON`.

## Roadmap

1. ~~Vocoder core: filter bank, envelope followers, MIDI carrier synth~~
2. ~~Arpeggiator: note arp and rhythm gate, synced to Logic's tempo~~
3. ~~Filter (cutoff, resonance, drive) and LFO modulation~~
4. ~~FX chain: distortion, bit crush, chorus, delay, reverb~~
5. ~~Presets, metronome and bar-exact recording, more synth options~~
6. ~~Standalone live mode: device pickers, recorder, on-screen keyboard, chord buttons~~
7. Unison, chord memory, phaser, limiter
8. ~~Auto-tune, Sing to Play, designed interface~~
9. ~~Easy mode, more chords and scales, more presets~~
10. User presets, MIDI learn, live rig with the crate tools
