#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Noise-resume protocol, version 1. Compile this SAME driver against the
// baseline and candidate ceres.cpp/ceres.hpp with identical flags:
//   c++ -std=c++17 -O3 -DNDEBUG -I <core> Tests/NoiseResumeDriver.cpp \
//       <core>/ceres.cpp -o <driver>
// Invocation: <driver> <output.f64> <factor>. Brass 1, four voices, 48 kHz,
// seed 11: C3 and G3 from .5 s; NOISE 0 and FUNCTION sine until 1.5 s, then
// NOISE 1, FUNCTION N and VCF depth .5 until 3 s. Native-endian float64,
// no header. check_noise_resume.py compares the two streams around 1.5 s,
// where an engine that leaves the noise source idle while unused resumes it.
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s output.f64 factor\n", argv[0]);
        return 2;
    }
    const unsigned factor = static_cast<unsigned>(std::atoi(argv[2]));
    constexpr double rate = 48000;
    ceres::Synth synth(rate, 11);
    if (!synth.configure(4, factor)) return 2;
    ceres::Patch patch = ceres::preset("brass1");
    patch.noise = 0; patch.lfoWaveform = 0;
    synth.setPatch(patch);
    std::vector<double> output;
    const auto total = static_cast<unsigned>(rate * 3), enable = static_cast<unsigned>(rate * 1.5);
    for (unsigned n = 0; n < total; ++n) {
        if (n == static_cast<unsigned>(rate * .5)) { synth.noteOn(48, .8); synth.noteOn(55, .7); }
        if (n == enable) { patch.noise = 1; patch.lfoWaveform = 4; patch.filterLfo = .5; synth.setPatch(patch); }
        const double value = synth.process();
        if (!std::isfinite(value)) return 3;
        output.push_back(value);
    }
    std::FILE* file = std::fopen(argv[1], "wb");
    if (!file || std::fwrite(output.data(), sizeof(double), output.size(), file) != output.size()) return 1;
    return std::fclose(file) == 0 ? 0 : 1;
}
