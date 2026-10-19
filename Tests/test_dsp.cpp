// Engine contracts for the scaffold: it prepares at every supported rate,
// accepts the front-end calls and renders finite output.

#include "CeresEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    if (! condition)
    {
        ++failures;
        std::fprintf (stderr, "FAIL %s\n", what);
    }
}
} // namespace

int main()
{
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        ceres::CeresEngine engine;
        engine.prepare (rate);
        check (engine.sampleRate() == rate, "prepare keeps the sample rate");
        check (engine.latencySamples() >= 0, "latency is not negative");
        engine.noteOn (60, 100);
        std::vector<float> left (512, 1.0f), right (512, 1.0f);
        engine.process (left.data(), right.data(), left.size());
        engine.noteOff (60);
        engine.allNotesOff();
        bool finite = true;
        for (std::size_t i = 0; i < left.size(); ++i)
            finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
        check (finite, "every rendered sample is finite");
    }
    std::printf ("Ceres engine contracts: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
