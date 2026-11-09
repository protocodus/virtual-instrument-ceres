// U4 parameter-extremes matrix over the plug-in processor, through the shared
// portfolio harness (Tests/ParameterMatrix.h is a byte-identical copy of the
// master; change it in reason-rack-extensions).
//
//   CeresParameterMatrixTests [--jobs N] [--rotation] [--shard K/N]
//                             [--case LABEL] [--count] [--list]
//   CeresParameterMatrixTests --reach [--jobs N] [--case PARAMETER]
//
// The command line is the shared protocodus::matrix::Main's (U9): shards are
// numbered from 0, and --case reruns the case a FAIL line names.
//
// The limits below follow from the circuit (Source/DSP/ceres.cpp); each one
// names the stage it comes from.

#include "PluginProcessor.h"
#include "ParameterMatrix.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace
{
using protocodus::matrix::Case;
using protocodus::matrix::Limits;

// No limiter follows the circuit, so the ceiling is the output stage's own
// bound. The PRA preamp clips at its rails (outputPreamp); the 33 uF line
// coupling after it is a trapezoidal high-pass whose capacitor voltage stays
// inside the rails, so its output stays inside their span; the decimation
// FIR can scale a bounded stream by at most the sum of its taps' magnitudes
// over every internal subframe.
double outputBound (unsigned factor)
{
    const double span = ceres::outputPreampResponse (1.0e9, 1.0) - ceres::outputPreampResponse (-1.0e9, 1.0);
    double taps = 0.0;
    for (unsigned subframe = 0; subframe < factor; ++subframe)
        for (const double tap : ceres::outputRateImpulse (factor, 512, subframe))
            taps += std::abs (tap);
    return span * taps;
}

class CeresDevice final : public protocodus::matrix::JuceDevice
{
public:
    CeresDevice() : JuceDevice (std::make_unique<CeresAudioProcessor>())
    {
        // A new instance draws its own unit; every case plays the fixed one,
        // so a failing case replays.
        juce::MemoryBlock state;
        processor->getStateInformation (state);
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())))
        {
            xml->setAttribute ("unit", juce::String (static_cast<juce::int64> (CeresAudioProcessor::kFixedUnit)));
            juce::AudioProcessor::copyXmlToBinary (*xml, state);
            processor->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        }
        for (auto* parameter : processor->getParameters())
            if (parameter->isAutomatable())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    columns[ranged->paramID.toStdString()] = { automatable++, ranged };
    }

    Limits LimitsFor (const Case& c) const override
    {
        Limits limits;
        // Automation ramps between the case and its base, so the ceiling
        // takes whichever of the two asks more of it.
        double trim = 0.0;
        for (const auto* row : { &c.values, &c.base })
            trim = std::max (trim, std::pow (10.0, value (*row, "output_trim_db") / 20.0));
        const auto* ceres = dynamic_cast<const CeresAudioProcessor*> (processor.get());
        const auto factor = static_cast<unsigned> (ceres != nullptr ? ceres->appliedOversampling() : 4);
        const double ceiling = outputBound (factor) * trim;
        limits.ceiling = static_cast<float> (ceiling);

        // The ramp has returned to the case's own values before note-off,
        // so the tail is the one the processor reports for them: its VCA EG
        // release, then the bus coupling capacitor falling 60 dB from full
        // scale. Whatever the bus still holds can start as high as the
        // ceiling, which takes that capacitor ln (ceiling) time constants
        // more to bring down to full scale.
        limits.releaseSeconds = processor->getTailLengthSeconds()
                              + CeresAudioProcessor::kBusCouplingSeconds * std::log (std::max (1.0, ceiling));
        limits.soundsWhileHeld = ! closesBeforeRamp (c);
        return limits;
    }

    // U11. Parameters the default patch leaves silent are tested on the first
    // factory preset that uses them, or here with what makes them audible.
    protocodus::matrix::ReachRule ReachFor (std::size_t index, float) const override
    {
        using protocodus::matrix::Reach;
        std::vector<protocodus::matrix::Parameter> all = Parameters();
        for (const auto& p : EditorOnlyParameters())
            all.push_back (p);
        const auto at = [&] (const char* name)
        {
            for (std::size_t i = 0; i < all.size(); ++i)
                if (all[i].name == name)
                    return i;
            std::fprintf (stderr, "no parameter %s\n", name);
            std::abort();
        };
        const std::string& name = all[index].name;
        // Each envelope runs its attack and decay once per key press (the
        // EG's own segments, ceres.cpp advanceAmpEnvelope): half a second in,
        // the held chord has finished them.
        if (name == "VCA EG attack" || name == "VCA EG attack headroom" || name == "VCF EG attack"
            || name == "VCF EG attack level" || name == "VCF EG decay" || name == "VCF EG attack headroom")
            return { Reach::NewNotes, "an attack or decay segment the held chord has finished" };
        // At its top a current limit ten times the EG's span is never
        // reached, so the attack follows the RC law, as at 0.
        if (name == "VCA EG attack saturation" || name == "VCF EG attack saturation")
            return { Reach::Exempt, "a limit the attack never reaches at either end" };
        // The ring carrier's EG starts at each key press (advanceRingCarrier).
        if (name == "Ring attack time" || name == "Ring decay time")
            return { Reach::NewNotes, "the ring carrier's sweep, which each key press starts",
                     { { at ("Ring modulation"), 1.0f }, { at ("Ring depth"), 0.5f } } };
        if (name == "Ring depth")
            return { Reach::NewNotes, "the depth of the ring carrier's sweep, which each key press starts",
                     { { at ("Ring modulation"), 1.0f } } };
        if (name == "Ring speed")
            return { Reach::Held, "", { { at ("Ring modulation"), 1.0f } },
                     "the ring carrier runs free, so after a change it is in the phase the old speed left" };
        // The phrase sends no aftertouch, so touch is tested with velocity as
        // its source; brilliance with the LPF low enough to open further.
        if (name == "Touch VCO" || name == "Touch VCF" || name == "Touch level")
            return { Reach::Held, "", { { at ("Touch source"), 1.0f } } };
        if (name == "Touch brilliance")
            return { Reach::Held, "", { { at ("Touch source"), 1.0f }, { at ("LPF cutoff"), 0.6f } } };
        if (name == "Touch source")
            return { Reach::Held, "", { { at ("Touch level"), 1.0f } } };
        if (name == "Sub osc function")
            return { Reach::Held, "", { { at ("Sub osc VCO"), 0.5f } } };
        if (name == "Sub osc speed")
            return { Reach::Held, "", { { at ("Sub osc VCO"), 0.5f } },
                     "the sub-oscillator runs free, so after a change it is in the phase the old speed left" };
        // A board glides from the key it last played to the one it is given.
        if (name == "Portamento mode")
            return { Reach::NewNotes, "a glide into each new key" };
        if (name == "Portamento speed")
            return { Reach::NewNotes, "a glide into each new key", { { at ("Portamento mode"), 0.5f } },
                     "each board glides from wherever the glides before left it" };
        if (name == "EXT level")
            return { Reach::Exempt, "the EXT IN jack's level; the phrase plays nothing into it" };
        if (name == "PWM phase")
            return { Reach::Exempt, "1 is a whole cycle, the same phase as 0" };
        // Near middle C, where the law pivots, tracking moves a 6 kHz cutoff
        // too little to hear; at a lower cutoff the held chord follows.
        if (name == "Filter key tracking")
            return { Reach::Held, "", { { at ("LPF cutoff"), 0.6f } } };
        // At its default the HPF corner sits under the chord, where its peak
        // has nothing to lift.
        if (name == "HPF resonance")
            return { Reach::Held, "", { { at ("HPF cutoff"), 0.4f } } };
        // The free-running VCOs keep their phases, so after a retune they
        // beat as the instrument's history leaves them.
        if (name == "Drift" || name == "Board pitch tolerance" || name == "Temperature rise")
            return { Reach::Held, "", {}, "the boards beat in the phase their old tuning left" };
        // The EG level and time residuals (updateBoardScales) meet the
        // segments each key press runs; held notes keep only the floor's.
        if (name == "Board envelope tolerance")
            return { Reach::NewNotes, "each board's EG peaks, which only new notes reach" };
        // Both are spreads of a few percent between the boards, under a
        // level JND on any chord even with the stage they act on exposed.
        if (name == "Board envelope time tolerance")
            return { Reach::Exempt, "a few percent of EG time between boards; Ceres.board_tolerance_contracts" };
        if (name == "Board VCA tolerance")
            return { Reach::Exempt, "a few percent of sine against filtered level between boards; "
                                    "Ceres.board_tolerance_contracts" };
        // FilterChain divides the drive out again: it only matters where it
        // drives the OTAs into their curve, above the default.
        if (name == "Drive")
            return { Reach::Exempt, "below the default the OTAs are already linear" };
        // The saw's reset lasts at most 60 us: at the probe's pitches it only
        // trims the top octave, under a level JND even with the LPF open.
        if (name == "VCO reset time")
            return { Reach::Exempt, "a reset of at most 60 us; Ceres.oscillator_circuit_contracts" };
        // CeresAudioProcessor::advanceConfiguration applies both once every
        // note has ended, behind a fade.
        if (name == "Voices" || name == "Oversampling")
            return { Reach::NewNotes, "applied once every note has ended" };
        return {};
    }

private:
    struct Column
    {
        std::size_t index;
        juce::RangedAudioParameter* parameter;
    };

    // A percussive patch falls silent while its keys are held, as the CS-50
    // does: with the VCA EG's sustain level at zero in both rows, the ramp
    // never raises it, and each board's EG decays from its peak towards its
    // floor below zero and closes. From the attack peak that takes at most
    // 0.73 of the decay T60, as the release does (getTailLengthSeconds), and
    // the EG time trims stretch an attack this short by less than 1.22. When
    // every board has closed before the ramp first moves the parameters
    // (0.25 s; the chord's last notes start at 0.1 s), nothing reopens it
    // before note-off.
    bool closesBeforeRamp (const Case& c) const
    {
        if (value (c.values, "amp_sustain") > 0.0 || value (c.base, "amp_sustain") > 0.0)
            return false;
        constexpr double lastNoteOn = 0.1, rampStart = 0.25;
        return 1.22 * value (c.values, "amp_attack") + 0.73 * value (c.values, "amp_decay") < rampStart - lastNoteOn;
    }

    // A row's value of one parameter in its own units (a choice's index).
    double value (const std::vector<float>& row, const std::string& id) const
    {
        const auto found = columns.find (id);
        if (found == columns.end())
            return 0.0;
        const auto& column = found->second;
        const float normalised = column.index < row.size() ? row[column.index] : column.parameter->getValue();
        return column.parameter->convertFrom0to1 (normalised);
    }

    std::size_t automatable = 0;
    std::map<std::string, Column> columns;
};
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    return protocodus::matrix::Main (argc, argv, [] { return std::make_unique<CeresDevice>(); });
}
