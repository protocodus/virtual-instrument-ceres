// One-float-ulp perturbation probe, driven by check_perturbation_probe.py:
// every factory preset with a chord of four voices (48 kHz, seed 7, released
// after three quarters of the length) at the given factor, unperturbed
// ("none") or with one control multiplied by 1 + 1.2e-7, as native-endian
// float64 streams named <preset>-<factor>x-<control>.f64 in the output
// directory. Compile against the engine under test:
//   c++ -std=c++17 -O3 -DNDEBUG -I Source/DSP Tests/PerturbationProbeDriver.cpp \
//       Source/DSP/ceres.cpp -o probe
#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: probe outdir factor seconds control|none\n"); return 2; }
    const std::string out = argv[1]; const unsigned factor = std::atoi(argv[2]);
    const double seconds = std::atof(argv[3]); const std::string control = argv[4];
    const double rate = 48000;
    for (const auto& name : ceres::presetNames()) {
        ceres::Synth synth(rate, 7);
        if (!synth.configure(4, factor)) return 2;
        ceres::Patch p = ceres::preset(name);
        const double k = 1 + 1.2e-7;
        if (control == "hp_cutoff") p.hpCutoff *= k;
        else if (control == "lp_cutoff") p.lpCutoff *= k;
        else if (control == "hp_resonance") p.hpResonance *= k;
        else if (control == "lp_resonance") p.lpResonance *= k;
        else if (control == "resonance_both") { p.hpResonance *= k; p.lpResonance *= k; }
        else if (control != "none") return 3;
        synth.setPatch(p);
        for (unsigned v = 0; v < 4; ++v) synth.noteOn(48 + int(v) * 3, .2 + .7 * v / 4);
        const unsigned total = unsigned(rate * seconds), release = unsigned(rate * seconds * .75);
        std::vector<double> y(total);
        for (unsigned n = 0; n < total; ++n) { if (n == release) synth.allNotesOff(); y[n] = synth.process(); }
        std::FILE* f = std::fopen((out + "/" + name + "-" + std::to_string(factor) + "x-" + control + ".f64").c_str(), "wb");
        std::fwrite(y.data(), sizeof(double), y.size(), f); std::fclose(f);
    }
    return 0;
}
