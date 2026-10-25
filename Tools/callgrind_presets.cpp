// Instruction-count workload for callgrind (perf-free containers): every
// factory preset in order, one Synth each (48 kHz, seed 7), a chord of
// `voices` notes from the start, released after three quarters of the blocks,
// at `factor` oversampling, 64-frame blocks. Prints the float output checksum.
//   c++ -std=c++17 -O3 -g -DNDEBUG -I Source/DSP Tools/callgrind_presets.cpp \
//       Source/DSP/ceres.cpp -o callgrind_presets
//   valgrind --tool=callgrind ./callgrind_presets <voices> <factor> <blocks> [noise]
// `noise` sets NOISE to .3 on every preset (all factory presets leave it at 0).
// Instruction counts are deterministic on a shared machine where timings are
// not; they are not a Reason host or target measurement.
//
// Protocol (Docs/benchmarks/cpu-inlining-2026-10-25.md). The primary workload
// imitates the Rack's build as nearly as a container without the SDK allows:
//   clang++ -std=c++17 -O3 -fno-builtin -DNDEBUG -I Source/DSP \
//       Tools/callgrind_presets.cpp Source/DSP/ceres.cpp -o callgrind_presets
//   valgrind --tool=callgrind ./callgrind_presets 4 1 240   (the Rack's 4 voices/1x)
//   valgrind --tool=callgrind ./callgrind_presets 4 4 240   (the heaviest quality)
//   valgrind --tool=callgrind ./callgrind_presets 1 1 240
// and the same three with g++ -std=c++17 -O3 -DNDEBUG for the host build.
// CERES_EMBEDDED_TARGET is left off because it removes preset() and
// presetNames(); it changes no audio-path code. Baseline and candidate are
// compiled fresh with identical flags; the printed checksum must match, and
// Tests/check_cpu_null.py --baseline-ref <sha> (also with
// CXX="clang++ -fno-builtin") must report a byte-identical float32 stream
// before a change counts as exact or last bit.
#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s voices factor blocks [noise]\n", argv[0]);
        return 2;
    }
    const unsigned voices = static_cast<unsigned>(std::atoi(argv[1]));
    const unsigned factor = static_cast<unsigned>(std::atoi(argv[2]));
    const unsigned blocks = static_cast<unsigned>(std::atoi(argv[3]));
    const bool noise = argc > 4 && std::strcmp(argv[4], "noise") == 0;
    double checksum = 0;
    for (const auto& name : ceres::presetNames()) {
        ceres::Synth synth(48000, 7);
        if (!synth.configure(voices, factor)) return 2;
        ceres::Patch patch = ceres::preset(name);
        if (noise) patch.noise = .3;
        synth.setPatch(patch);
        for (unsigned v = 0; v < voices; ++v) synth.noteOn(48 + static_cast<int>(v) * 3, .2 + .7 * v / voices);
        for (unsigned b = 0; b < blocks; ++b) {
            if (b == blocks * 3 / 4) synth.allNotesOff();
            for (unsigned s = 0; s < 64; ++s) {
                const double y = synth.process();
                if (!std::isfinite(y)) return 3;
                checksum += y;
            }
        }
    }
    std::printf("%.17g\n", checksum);
    return 0;
}
