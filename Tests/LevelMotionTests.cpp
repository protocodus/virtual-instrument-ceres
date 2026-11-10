// A host writes the levers once per block. The levels in the audio path
// (VOLUME, the mixer levels, ring MIX and the expression pedal) must reach
// the circuit without a step, as a lever under a hand does: a step in a gain
// is a click. Each case holds a 220 Hz sine, writes one new value while the
// output is near a peak, where a gain step shows most, and compares the
// largest sample-to-sample jump in the next 10 ms with the tone's own before
// it. Before the dezipper every case gave 9 to 32 times the tone's own jump;
// with its 5 ms line the tone's own slope is all that is left.
#include "ceres.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool good, const char* why) {
    if (!good) { std::fprintf(stderr, "FAIL %s\n", why); std::exit(1); }
}

struct Case {
    const char* name;
    const char* key;
    double from, to;
};

double jumpRatio(const Case& c, unsigned factor) {
    const double fs = 48000;
    ceres::Synth synth(fs, 20261017);
    require(synth.configure(4, factor), "configure");
    ceres::Patch patch;
    patch.saw = 0; patch.sine = 1; patch.ampSustain = 1; patch.driftCents = 0;
    patch.ringDepth = 0;
    require(ceres::setParameter(patch, c.key, c.from), "known key");
    synth.setPatch(patch);
    synth.noteOn(57, 1);
    double peak = 0, previous = 0, before = 0;
    const unsigned settle = static_cast<unsigned>(fs * .5), window = static_cast<unsigned>(fs * .01);
    for (unsigned n = 0; n < settle; ++n) {
        const double y = synth.process();
        if (n >= settle / 2) peak = std::max(peak, std::abs(y));
        if (n >= settle - window) before = std::max(before, std::abs(y - previous));
        previous = y;
    }
    require(peak > 1e-3, "the tone sounds");
    for (unsigned n = 0; n < window && std::abs(previous) < .9 * peak; ++n) previous = synth.process();
    require(std::abs(previous) >= .9 * peak, "the write lands near a peak");
    ceres::setParameter(patch, c.key, c.to);
    synth.setPatch(patch);
    double after = 0;
    for (unsigned n = 0; n < window; ++n) {
        const double y = synth.process();
        after = std::max(after, std::abs(y - previous));
        previous = y;
    }
    return after / before;
}
} // namespace

int main() {
    const Case cases[] = {
        {"VOLUME halved", "volume", 1, .5},
        {"sine level lowered", "sine", 1, .4},
        {"ring MIX opened", "ring_mix", 0, 1},
        {"pedal leaves full travel", "expression", 1, .8},
        {"pedal returns to full travel", "expression", .8, 1},
    };
    for (const unsigned factor : {1u, 4u})
        for (const auto& c : cases) {
            const double ratio = jumpRatio(c, factor);
            std::fprintf(stderr, "%ux %s: %.2f times the tone's own jump\n", factor, c.name, ratio);
            // A pedal returning to full travel raises the tone, and its
            // jump with it, by 1.55; 2 covers that and stays far below a step.
            require(ratio < 2, c.name);
        }
    std::puts("PASS level writes reach the audio path without a step at 1x and 4x");
    return 0;
}
