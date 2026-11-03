#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace vox
{
// Chords built from a key and scale, for the chord buttons.
struct Chords
{
    static constexpr int numScales = 7;
    static constexpr int numFlavours = 9;
    static constexpr int maxNotes = 6;

    // Seven-note scales, as semitones above the key.
    static constexpr int scales[numScales][7] = {
        { 0, 2, 4, 5, 7, 9, 11 },  // Major
        { 0, 2, 3, 5, 7, 8, 10 },  // Minor
        { 0, 2, 3, 5, 7, 9, 10 },  // Dorian
        { 0, 2, 4, 5, 7, 9, 10 },  // Mixolydian
        { 0, 2, 4, 6, 7, 9, 11 },  // Lydian
        { 0, 1, 3, 5, 7, 8, 10 },  // Phrygian
        { 0, 2, 3, 5, 7, 8, 11 },  // Harmonic minor
    };

    enum Flavour
    {
        triad = 0,
        seventh,
        ninth,
        sus2,
        sus4,
        add9,
        sixth,
        power,
        big
    };

    // The scale degree (0-6) as a note: degrees past 6 continue into the next octave.
    static int degreeNote (int key, int scale, int degree)
    {
        const auto& s = scales[std::clamp (scale, 0, numScales - 1)];
        return key + s[degree % 7] + 12 * (degree / 7);
    }

    // Notes for the chord on scale degree `degree` (0 = the key's own chord), around C3 (MIDI 48).
    static std::vector<int> build (int key, int scale, int degree, int flavour)
    {
        key = ((key % 12) + 12) % 12;
        const int rootPitch = degreeNote (key, scale, degree) % 12;
        const int base = 48 + rootPitch - degreeNote (key, scale, degree); // shifts the chord so its root is in C3-B3

        auto stack = [&] (std::initializer_list<int> steps)
        {
            std::vector<int> notes;
            for (int step : steps)
                notes.push_back (base + degreeNote (key, scale, degree + step));
            return notes;
        };

        std::vector<int> notes;
        switch (flavour)
        {
            case seventh: notes = stack ({ 0, 2, 4, 6 }); notes.push_back (notes[0] + 12); break;
            case ninth:   notes = stack ({ 0, 2, 4, 6, 8 }); break;
            case sus2:    notes = stack ({ 0, 1, 4 }); notes.push_back (notes[0] + 12); break;
            case sus4:    notes = stack ({ 0, 3, 4 }); notes.push_back (notes[0] + 12); break;
            case add9:    notes = stack ({ 0, 2, 4, 8 }); break;
            case sixth:   notes = stack ({ 0, 2, 4, 5 }); notes.push_back (notes[0] + 12); break;
            case power:   notes = { 48 + rootPitch, 48 + rootPitch + 7, 48 + rootPitch + 12 }; break;
            case big:
            {
                // Spread wide: root low, then the fifth, and the third and fifth an octave up.
                auto t = stack ({ 0, 2, 4 });
                notes = { t[0] - 12, t[0], t[2], t[1] + 12, t[2] + 12 };
                break;
            }
            case triad:
            default:      notes = stack ({ 0, 2, 4 }); notes.push_back (notes[0] + 12); break;
        }
        if ((int) notes.size() > maxNotes)
            notes.resize ((size_t) maxNotes);
        return notes;
    }

    // A chord symbol such as "Am7", "Fmaj7", "Dsus4" or "Bdim".
    static std::string name (int key, int scale, int degree, int flavour)
    {
        static const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const auto notes = build (key, scale, degree, flavour);
        const int root = flavour == big ? notes[1] : notes[0];
        std::string out = noteNames[((root % 12) + 12) % 12];

        auto has = [&] (int semis)
        {
            return std::any_of (notes.begin(), notes.end(), [&] (int n) { return ((n - root) % 12 + 12) % 12 == semis; });
        };
        const bool minor3 = has (3), major3 = has (4), dim5 = has (6) && ! has (7), aug5 = has (8) && ! has (7) && major3;

        if (flavour == power)
            return out + "5";
        if (flavour == sus2)
            return out + (dim5 ? "sus2b5" : "sus2");
        if (flavour == sus4)
            return out + (has (6) && ! has (5) ? "sus#4" : dim5 ? "sus4b5" : "sus4");

        std::string quality = minor3 && dim5 ? "dim" : aug5 ? "aug" : minor3 ? "m" : "";
        if (flavour == seventh || flavour == ninth)
        {
            const bool maj7 = has (11), min7 = has (10);
            const char* ext = flavour == ninth ? "9" : "7";
            if (quality == "dim")
                quality = min7 ? std::string ("m") + ext + "b5" : std::string ("dim") + ext;
            else if (maj7)
                quality += std::string ("maj") + ext;
            else
                quality += ext;
            return out + quality;
        }
        if (flavour == add9)
            return out + quality + "add9";
        if (flavour == sixth)
            return out + quality + "6";
        return out + quality;
    }
};
} // namespace vox
