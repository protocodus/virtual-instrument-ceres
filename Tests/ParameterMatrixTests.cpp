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

#include "PluginProcessor.h"
#include "ParameterMatrix.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
using protocodus::matrix::Case;
using protocodus::matrix::Limits;

class CeresDevice final : public protocodus::matrix::JuceDevice
{
public:
    CeresDevice() : JuceDevice (std::make_unique<CeresAudioProcessor>()) {}

    // The scaffold engine renders silence; this goes when it makes sound.
    Limits LimitsFor (const Case&) const override
    {
        Limits limits;
        limits.soundsWhileHeld = false;
        return limits;
    }
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
