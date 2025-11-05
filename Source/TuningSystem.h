/*
  ==============================================================================

    TuningSystem.h
    Created: 5 Nov 2025
    Author: NanoStutt Development

    Defines tuning systems, scales, and note-based frequency calculations
    for the nano rate section.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <string>

namespace NanoTuning
{
    //==========================================================================
    // ENUMS
    //==========================================================================

    enum class NanoBase
    {
        BPMSynced = 0,
        C,
        CSharp,
        D,
        DSharp,
        E,
        F,
        FSharp,
        G,
        GSharp,
        A,
        ASharp,
        B,
        NumBases
    };

    enum class TuningSystem
    {
        EqualTemperament = 0,
        JustIntonation,
        Pythagorean,
        QuarterCommaMeantone,
        CustomFraction,
        CustomDecimal,
        CustomSemitone,
        NumTuningSystems
    };

    enum class Scale
    {
        Chromatic = 0,
        Major,
        NaturalMinor,
        MajorPentatonic,
        MinorPentatonic,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Aeolian,
        Locrian,
        HarmonicMinor,
        MelodicMinor,
        WholeTone,
        Diminished,
        Custom,
        NumScales
    };

    //==========================================================================
    // INTERVAL VARIANT STRUCTURE
    //==========================================================================

    // Represents a single interval option (used when multiple variants exist)
    struct IntervalVariant
    {
        juce::String displayName;  // Short name shown in dropdown (e.g., "Aug 4th", "Dim 5th")
        float ratio;               // Frequency ratio relative to root
        juce::String origin;       // Mathematical origin for tooltip (e.g., "3⁶:2⁹")
    };

    //==========================================================================
    // TUNING RATIO TABLES (12 ratios per tuning system)
    //==========================================================================

    // Equal Temperament: 2^(n/12) for n = 0..11
    constexpr std::array<float, 12> equalTemperamentRatios = {
        1.0f,       // C  (unison)
        1.059f,     // C# (minor 2nd)
        1.122f,     // D  (major 2nd)
        1.189f,     // D# (minor 3rd)
        1.260f,     // E  (major 3rd)
        1.335f,     // F  (perfect 4th)
        1.414f,     // F# (tritone)
        1.498f,     // G  (perfect 5th)
        1.587f,     // G# (minor 6th)
        1.682f,     // A  (major 6th)
        1.782f,     // A# (minor 7th)
        1.888f      // B  (major 7th)
    };

    // Just Intonation: Pure integer ratios (updated from table)
    constexpr std::array<float, 12> justIntonationRatios = {
        1.0f,           // C  = 1/1
        1.067f,         // C# = 16/15
        1.111f,         // D  = 10/9 (Lesser major second)
        1.200f,         // D# = 6/5
        1.250f,         // E  = 5/4
        1.333f,         // F  = 4/3
        1.406f,         // F# = 45/32
        1.500f,         // G  = 3/2
        1.600f,         // G# = 8/5
        1.667f,         // A  = 5/3
        1.750f,         // A# = 7/4 (Minor seventh)
        1.875f          // B  = 15/8
    };

    // Pythagorean: Based on perfect fifths (3/2) - updated from table
    constexpr std::array<float, 12> pythagoreanRatios = {
        1.0f,           // C  = 1/1
        1.054f,         // C# = 2^3:3^5
        1.125f,         // D  = 3^2:2^3
        1.185f,         // D# = 2^5:3^3
        1.266f,         // E  = 3^4:2^5
        1.333f,         // F  = 2^2:3
        1.424f,         // F# = 3^6:2^9 (Augmented 4th - default)
        1.500f,         // G  = 3:2
        1.580f,         // G# = 2^7:3^4
        1.688f,         // A  = 3^3:2^4
        1.778f,         // A# = 2^4:3^2
        1.898f          // B  = 3^5:2^7
    };

    // Quarter-comma Meantone: Renaissance tuning
    constexpr std::array<float, 12> quarterCommaMeantoneRatios = {
        1.0f,       // C
        1.070f,     // C#
        1.118f,     // D
        1.196f,     // D#
        1.250f,     // E
        1.337f,     // F
        1.430f,     // F#
        1.495f,     // G
        1.600f,     // G#
        1.671f,     // A
        1.788f,     // A#
        1.869f      // B
    };

    //==========================================================================
    // SCALE DEFINITIONS (12 bools: which semitones are active)
    //==========================================================================

    // Chromatic: All 12 notes
    constexpr std::array<bool, 12> chromaticScale = {
        true, true, true, true, true, true, true, true, true, true, true, true
    };

    // Major: W-W-H-W-W-W-H (C D E F G A B)
    constexpr std::array<bool, 12> majorScale = {
        true, false, true, false, true, true, false, true, false, true, false, true
    };

    // Natural Minor: W-H-W-W-H-W-W (C D D# F G G# A#)
    constexpr std::array<bool, 12> naturalMinorScale = {
        true, false, true, true, false, true, false, true, true, false, true, false
    };

    // Major Pentatonic: C D E G A
    constexpr std::array<bool, 12> majorPentatonicScale = {
        true, false, true, false, true, false, false, true, false, true, false, false
    };

    // Minor Pentatonic: C D# F G A#
    constexpr std::array<bool, 12> minorPentatonicScale = {
        true, false, false, true, false, true, false, true, false, false, true, false
    };

    // Dorian: W-H-W-W-W-H-W (C D D# F G A A#)
    constexpr std::array<bool, 12> dorianScale = {
        true, false, true, true, false, true, false, true, false, true, true, false
    };

    // Phrygian: H-W-W-W-H-W-W (C C# D# F G G# A#)
    constexpr std::array<bool, 12> phrygianScale = {
        true, true, false, true, false, true, false, true, true, false, true, false
    };

    // Lydian: W-W-W-H-W-W-H (C D E F# G A B)
    constexpr std::array<bool, 12> lydianScale = {
        true, false, true, false, true, false, true, true, false, true, false, true
    };

    // Mixolydian: W-W-H-W-W-H-W (C D E F G A A#)
    constexpr std::array<bool, 12> mixolydianScale = {
        true, false, true, false, true, true, false, true, false, true, true, false
    };

    // Aeolian (same as Natural Minor)
    constexpr std::array<bool, 12> aeolianScale = naturalMinorScale;

    // Locrian: H-W-W-H-W-W-W (C C# D# F F# G# A#)
    constexpr std::array<bool, 12> locrianScale = {
        true, true, false, true, false, true, true, false, true, false, true, false
    };

    // Harmonic Minor: W-H-W-W-H-W+H-H (C D D# F G G# B)
    constexpr std::array<bool, 12> harmonicMinorScale = {
        true, false, true, true, false, true, false, true, true, false, false, true
    };

    // Melodic Minor: W-H-W-W-W-W-H (C D D# F G A B)
    constexpr std::array<bool, 12> melodicMinorScale = {
        true, false, true, true, false, true, false, true, false, true, false, true
    };

    // Whole Tone: W-W-W-W-W-W (C D E F# G# A#)
    constexpr std::array<bool, 12> wholeToneScale = {
        true, false, true, false, true, false, true, false, true, false, true, false
    };

    // Diminished (Half-Whole): H-W-H-W-H-W-H-W (C C# D# E F# G A A#)
    constexpr std::array<bool, 12> diminishedScale = {
        true, true, false, true, true, false, true, true, false, true, true, false
    };

    //==========================================================================
    // INTERVAL VARIANT DEFINITIONS
    //==========================================================================

    // Returns array of interval variants for each position (0-11) in a tuning system
    // Empty vector = single option (use ratio editor)
    // Multiple variants = show dropdown selector
    inline std::array<std::vector<IntervalVariant>, 12> getIntervalVariants(TuningSystem tuning)
    {
        std::array<std::vector<IntervalVariant>, 12> variants;

        // Initialize all positions with empty vectors (single option by default)
        for (int i = 0; i < 12; ++i)
            variants[i] = std::vector<IntervalVariant>();

        switch (tuning)
        {
            case TuningSystem::Pythagorean:
                // Position 6 (F#/Gb): Augmented 4th vs Diminished 5th
                variants[6] = {
                    {"Aug 4th", 1.424f, "3^6:2^9"},
                    {"Dim 5th", 1.405f, "2^10:3^6"}
                };
                break;

            case TuningSystem::JustIntonation:
                // Position 2 (D): Lesser vs Greater major second
                variants[2] = {
                    {"Lesser Maj 2nd", 1.111f, "10:9"},
                    {"Greater Maj 2nd", 1.125f, "9:8"}
                };
                // Position 10 (A#/Bb): Harmonic vs Grave minor seventh
                variants[10] = {
                    {"Harm Min 7th", 1.778f, "16:9"},
                    {"Grave Min 7th", 1.800f, "9:5"}
                };
                break;

            case TuningSystem::EqualTemperament:
            case TuningSystem::QuarterCommaMeantone:
            case TuningSystem::CustomFraction:
            case TuningSystem::CustomDecimal:
            case TuningSystem::CustomSemitone:
            default:
                // No variants for these tuning systems
                break;
        }

        return variants;
    }

    //==========================================================================
    // HELPER FUNCTIONS
    //==========================================================================

    inline juce::String getNanoBaseName(NanoBase base)
    {
        switch (base)
        {
            case NanoBase::BPMSynced:   return "BPM Synced";
            case NanoBase::C:           return "C";
            case NanoBase::CSharp:      return "C#";
            case NanoBase::D:           return "D";
            case NanoBase::DSharp:      return "D#";
            case NanoBase::E:           return "E";
            case NanoBase::F:           return "F";
            case NanoBase::FSharp:      return "F#";
            case NanoBase::G:           return "G";
            case NanoBase::GSharp:      return "G#";
            case NanoBase::A:           return "A";
            case NanoBase::ASharp:      return "A#";
            case NanoBase::B:           return "B";
            default:                    return "BPM Synced";
        }
    }

    inline juce::String getTuningSystemName(TuningSystem tuning)
    {
        switch (tuning)
        {
            case TuningSystem::EqualTemperament:        return "Equal Temperament";
            case TuningSystem::JustIntonation:          return "Just Intonation";
            case TuningSystem::Pythagorean:             return "Pythagorean";
            case TuningSystem::QuarterCommaMeantone:    return "Quarter-comma Meantone";
            case TuningSystem::CustomFraction:          return "Custom (Fraction)";
            case TuningSystem::CustomDecimal:           return "Custom (Decimal)";
            case TuningSystem::CustomSemitone:          return "Custom (Semitone)";
            default:                                    return "Equal Temperament";
        }
    }

    inline juce::String getScaleName(Scale scale)
    {
        switch (scale)
        {
            case Scale::Chromatic:          return "Chromatic";
            case Scale::Major:              return "Major";
            case Scale::NaturalMinor:       return "Natural Minor";
            case Scale::MajorPentatonic:    return "Major Pentatonic";
            case Scale::MinorPentatonic:    return "Minor Pentatonic";
            case Scale::Dorian:             return "Dorian";
            case Scale::Phrygian:           return "Phrygian";
            case Scale::Lydian:             return "Lydian";
            case Scale::Mixolydian:         return "Mixolydian";
            case Scale::Aeolian:            return "Aeolian";
            case Scale::Locrian:            return "Locrian";
            case Scale::HarmonicMinor:      return "Harmonic Minor";
            case Scale::MelodicMinor:       return "Melodic Minor";
            case Scale::WholeTone:          return "Whole Tone";
            case Scale::Diminished:         return "Diminished";
            case Scale::Custom:             return "Custom";
            default:                        return "Chromatic";
        }
    }

    inline const std::array<float, 12>& getTuningRatios(TuningSystem tuning)
    {
        switch (tuning)
        {
            case TuningSystem::EqualTemperament:        return equalTemperamentRatios;
            case TuningSystem::JustIntonation:          return justIntonationRatios;
            case TuningSystem::Pythagorean:             return pythagoreanRatios;
            case TuningSystem::QuarterCommaMeantone:    return quarterCommaMeantoneRatios;
            case TuningSystem::CustomFraction:          return equalTemperamentRatios; // Fallback
            case TuningSystem::CustomDecimal:           return equalTemperamentRatios; // Fallback
            case TuningSystem::CustomSemitone:          return equalTemperamentRatios; // Fallback
            default:                                    return equalTemperamentRatios;
        }
    }

    inline const std::array<bool, 12>& getScaleNotes(Scale scale)
    {
        switch (scale)
        {
            case Scale::Chromatic:          return chromaticScale;
            case Scale::Major:              return majorScale;
            case Scale::NaturalMinor:       return naturalMinorScale;
            case Scale::MajorPentatonic:    return majorPentatonicScale;
            case Scale::MinorPentatonic:    return minorPentatonicScale;
            case Scale::Dorian:             return dorianScale;
            case Scale::Phrygian:           return phrygianScale;
            case Scale::Lydian:             return lydianScale;
            case Scale::Mixolydian:         return mixolydianScale;
            case Scale::Aeolian:            return aeolianScale;
            case Scale::Locrian:            return locrianScale;
            case Scale::HarmonicMinor:      return harmonicMinorScale;
            case Scale::MelodicMinor:       return melodicMinorScale;
            case Scale::WholeTone:          return wholeToneScale;
            case Scale::Diminished:         return diminishedScale;
            case Scale::Custom:             return chromaticScale; // Fallback
            default:                        return chromaticScale;
        }
    }

    // Get note name for a given semitone index (0-11)
    inline juce::String getNoteName(int semitoneIndex)
    {
        const juce::StringArray noteNames = { "C", "C#", "D", "D#", "E", "F",
                                              "F#", "G", "G#", "A", "A#", "B" };
        if (semitoneIndex >= 0 && semitoneIndex < 12)
            return noteNames[semitoneIndex];
        return "?";
    }

    // Calculate base frequency for a given note (A4 = 440 Hz reference)
    inline float getNoteFrequency(NanoBase base)
    {
        if (base == NanoBase::BPMSynced)
            return 0.0f; // Special case: not a note-based frequency

        // A4 = 440 Hz is our reference (index 9 in chromatic scale if C is index 0)
        // We'll use A1 = 55 Hz as our octave reference (2 octaves lower for nano frequencies)
        const float A1_FREQ = 55.0f;

        // Calculate semitones from A1 (A1 is semitone 9 in the octave starting at C)
        int noteIndex = static_cast<int>(base) - 1; // Subtract 1 to skip BPMSynced

        // Semitones from A1 (A1 is our reference)
        // C (noteIndex 0) is -9 semitones from A1
        int semitonesFromA1 = noteIndex - 9;

        // Calculate frequency using equal temperament: f = A1 * 2^(n/12)
        return A1_FREQ * std::pow(2.0f, semitonesFromA1 / 12.0f);
    }

} // namespace NanoTuning
