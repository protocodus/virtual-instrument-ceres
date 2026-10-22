// U4 parameter-extremes matrix over the plug-in processor, through the shared
// portfolio harness (Tests/ParameterMatrix.h is a byte-identical copy of the
// master; change it in reason-rack-extensions).
//
//   CeresParameterMatrixTests [--jobs N] [--rotation] [--shard K/N]
//                             [--case LABEL] [--count]
//
// --rotation runs each case at one rate and block size instead of all nine,
// --shard runs every Nth case starting at K (1-based), --case runs only the
// case a FAIL line names, and --count only prints how many cases there are.
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
    protocodus::matrix::Options options;
    int shard = 1, shards = 1;
    bool countOnly = false;
    std::string only;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--rotation") options.everyConfiguration = false;
        else if (arg == "--count") countOnly = true;
        else if (arg == "--jobs" && i + 1 < argc) options.jobs = std::atoi (argv[++i]);
        else if (arg == "--case" && i + 1 < argc) only = argv[++i];
        else if (arg == "--shard" && i + 1 < argc
                 && std::sscanf (argv[++i], "%d/%d", &shard, &shards) == 2
                 && shards >= 1 && shard >= 1 && shard <= shards) {}
        else
        {
            std::fprintf (stderr, "usage: %s [--jobs N] [--rotation] [--shard K/N] [--case LABEL] [--count]\n", argv[0]);
            return 2;
        }
    }
    const auto make = [] { return std::make_unique<CeresDevice>(); };
    if (countOnly)
    {
        auto probe = make();
        probe->Prepare (48000.0, 512);
        std::printf ("%zu cases\n", protocodus::matrix::Cases (*probe).size());
        return 0;
    }
    options.select = [shard, shards, only] (std::size_t index, const Case& c)
    {
        return static_cast<int> (index % static_cast<std::size_t> (shards)) == shard - 1
            && (only.empty() || c.label == only);
    };
    return protocodus::matrix::Run (make, options) == 0 ? 0 : 1;
}
