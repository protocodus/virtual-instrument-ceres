#include "ceres.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

// CPU-optimization audio protocol, version 1. Compile this SAME driver against
// the baseline and candidate ceres.cpp/ceres.hpp, with identical compiler flags:
//   c++ -std=c++17 -O2 -DNDEBUG -I <core> Tests/CpuParityDriver.cpp \
//       <core>/ceres.cpp -o <driver>
// Invocation: <driver> <output.f32>. The file contains only consecutive native-
// endian IEEE float32 samples, written with fwrite; no header or frozen audio.
// Scene order is factory preset order x {1,4,16} voices x {1,2,4} quality,
// then {44100,48000,96000,192000} Hz x voices x quality, then three long scenes.
// Factory scenes have 1536 samples, controller scenes 4096, long scenes 32768.
// Compare bytes, including signed zero: this test permits NO audio difference.
// Reconfiguration/reset/panic events are part of the stream, as are both live
// waveform routing and reversed Q ramps while capacitor targets are in flight.

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr std::array<unsigned, 3> voiceCounts{{1, 4, 16}};
constexpr std::array<unsigned, 3> factors{{1, 2, 4}};

void require(bool success, const char* message) {
    if (!success) {
        std::fprintf(stderr, "CPU parity driver: %s\n", message);
        std::exit(1);
    }
}

class Output {
public:
    explicit Output(const char* path) : file_(std::fopen(path, "wb")) {
        require(file_ != nullptr, "cannot open output");
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "the parity protocol requires IEEE float32");
    }
    void sample(ceres::Synth& synth) {
        const float value = synth.process();
        require(std::isfinite(value), "nonfinite audio");
        buffer_[used_++] = value;
        ++samples_;
        if (used_ == buffer_.size()) flush();
    }
    void finish() {
        flush();
        require(std::fclose(file_) == 0, "cannot close output");
        std::fprintf(stderr, "CPU parity driver: %llu finite float32 samples\n",
                     static_cast<unsigned long long>(samples_));
    }
private:
    void flush() {
        require(std::fwrite(buffer_.data(), sizeof(float), used_, file_) == used_,
                "cannot write output");
        used_ = 0;
    }
    std::FILE* file_;
    std::array<float, 1024> buffer_{};
    std::size_t used_ = 0;
    std::uint64_t samples_ = 0;
};

void chord(ceres::Synth& synth, unsigned voices, int root) {
    for (unsigned voice = 0; voice < voices; ++voice)
        synth.noteOn(root + int(voice) * 2, .17 + .8 * voice / voices);
}

void factoryScenes(Output& output) {
    unsigned seed = 0;
    for (const auto& name : ceres::presetNames()) {
        for (unsigned voices : voiceCounts) for (unsigned factor : factors) {
            ceres::Synth synth(48000, 100 + seed);
            require(synth.configure(voices, factor), "factory configuration rejected");
            synth.setPatch(ceres::preset(name));
            for (unsigned frame = 0; frame < 1536; ++frame) {
                if (frame == 32) synth.noteOn(48, .8);
                if (frame == 64) {
                    for (unsigned voice = 1; voice < voices; ++voice)
                        synth.noteOn(48 + int(voice) * 2, .2 + .7 * voice / voices);
                }
                if (frame == 1024) synth.allNotesOff();
                if (frame == 1280) synth.noteOn(76, .45);
                output.sample(synth);
            }
        }
        ++seed;
    }
}

ceres::Patch modulationPatch() {
    ceres::Patch p;
    p.saw = .7; p.pulse = .4; p.sine = .13; p.noise = .05;
    p.pulseWidth = .63; p.pwmDepth = .22; p.pwmRate = 3.7; p.pwmPhase = .41;
    p.hpCutoff = 70; p.lpCutoff = 3200; p.hpResonance = .71; p.lpResonance = .88;
    p.filterInitial = -2.7; p.filterPeak = 3.4;
    p.filterAttack = .003; p.filterDecay = .08; p.filterRelease = .02;
    p.ampAttack = .002; p.ampDecay = .03; p.ampRelease = .04; p.ampSustain = .6;
    p.drive = 2; p.filterLfo = .4; p.tremolo = .6; p.vibratoCents = 13;
    p.ringMix = .8; p.ringRate = 30; p.ringDepth = 2;
    p.ringAttack = .007; p.ringDecay = .08; p.resonanceOffset = -3;
    p.voiceSpreadCents = .3; p.driftCents = 1; p.expressionSource = 1;
    p.pressureVolume = .7; p.pressureFilter = .8; p.pressureVibrato = 14; p.pressureWah = .3;
    p.velocitySensitivity = .4; p.sustainMode = 2; p.outputTrimDb = -6;
    p.glideMode = 1; p.glideRate = 24; p.boardTemperatureRise = 25;
    return p;
}

void controllerScenes(Output& output) {
    for (double rate : {44100., 48000., 96000., 192000.})
    for (unsigned voices : voiceCounts) for (unsigned factor : factors) {
        ceres::Synth synth(rate, 50 + voices + factor);
        require(!synth.configure(0, factor) && !synth.configure(voices, 3),
                "invalid configuration accepted");
        require(synth.configure(voices, factor), "controller configuration rejected");
        auto p = modulationPatch();
        synth.setPatch(p);
        unsigned currentVoices = voices;
        for (unsigned frame = 0; frame < 4096; ++frame) {
            if (frame == 96) chord(synth, voices, 40);
            if (frame == 256) synth.noteOff(40);
            if (frame == 257) synth.noteOn(79, .23);
            if (frame == 512) {
                synth.pressure(.8); synth.pitchBend(-.37); synth.sustain(true);
                synth.noteOff(79);
            }
            if (frame == 600) {
                for (unsigned key = 0; key < voices + 2; ++key)
                    synth.noteOn(60, .15 + .7 * key / (voices + 2));
            }
            if (frame == 650 || frame == 660) synth.noteOff(60);
            if (frame == 700) synth.allNotesOff();
            if (frame == 720) synth.sustain(false);
            if (frame == 768) {
                p.saw = p.pulse = 0; p.sine = 1; p.ringMix = 0; p.lfoWaveform = 1;
                synth.setPatch(p); synth.noteOn(55, .9);
            }
            if (frame == 900) {
                p.sine = 0; p.pulse = 1; p.ringMix = .7; p.ringDepth = 8; p.lfoWaveform = 2;
                synth.setPatch(p);
            }
            // Opposite targets before the Q-control capacitors have settled.
            if (frame == 1024 || frame == 1072 || frame == 1120) {
                p.hpResonance = frame == 1072 ? .1 : .9;
                p.lpResonance = frame == 1072 ? .2 : .95;
                p.resonanceOffset = frame == 1072 ? 7 : -6;
                synth.setPatch(p);
            }
            if (frame == 1200) { p.saw = 1; p.lfoWaveform = 3; synth.setPatch(p); }
            if (frame == 1280) {
                p.boardPitchTolerance = p.boardFilterTolerance = p.boardEnvelopeTolerance = 0;
                p.boardEnvelopeTimeTolerance = p.boardVcaTolerance = 0;
                p.boardTemperatureRise = 0; p.glideMode = 2; synth.setPatch(p);
                synth.pitchBend(.73); synth.pressure(.12);
            }
            if (frame == 1408) {
                p.saw = p.pulse = 0; p.noise = 1; p.lfoWaveform = 4; synth.setPatch(p);
            }
            if (frame == 1536) {
                p.lfoWaveform = 5; p.extLevel = .7; p.drive = 8; synth.setPatch(p);
            }
            if (frame == 1792) { p.expression = .27; p.ringDepth = 0; synth.setPatch(p); }
            if (frame == 1900) { p.expressionSource = 0; synth.setPatch(p); }
            if (frame == 2048) {
                synth.reset();
                currentVoices = voices == 16 ? 1 : 16;
                require(synth.configure(currentVoices, factor == 4 ? 1 : 4),
                        "reset configuration rejected");
                synth.setPatch(p); chord(synth, currentVoices, 45);
            }
            if (frame == 2304) {
                p.saw = .6; p.pulse = .4; p.sine = .2; p.noise = .1;
                p.expression = 1; p.expressionSource = 1;
                p.boardPitchTolerance = p.boardFilterTolerance = p.boardEnvelopeTolerance = 1;
                p.boardEnvelopeTimeTolerance = p.boardVcaTolerance = 1;
                p.boardTemperatureRise = -25; synth.setPatch(p);
            }
            if (frame == 2560) synth.allSoundOff();
            if (frame == 2570) chord(synth, currentVoices, 36);
            if (frame == 2800) synth.allNotesOff();
            if (frame == 3072) {
                synth.reset();
                require(synth.configure(voices, factor), "second reset configuration rejected");
                synth.setPatch(p); chord(synth, voices, 51);
            }
            if (frame == 3200) { p.globalRelease = 1; p.glideRate = .7; synth.setPatch(p); }
            if (frame == 3500) synth.allNotesOff();
            if (frame == 3584) {
                p.saw = p.pulse = p.sine = p.noise = 0; synth.setPatch(p);
            }
            if (frame == 3840) synth.noteOn(83, .6);
            // Audio/DC/overload changes and invalid input test the EXT cache
            // even while FUNCTION selects an internal waveform (0 through 4).
            double input = .8 * std::sin(2 * pi * 137 * frame / rate)
                         + .3 * std::sin(2 * pi * 3.1 * frame / rate);
            if (frame >= 1700 && frame < 1750) input *= 30;
            if (frame >= 2400 && frame < 2600) input += .7;
            if (frame == 2001) input = std::numeric_limits<double>::quiet_NaN();
            if (frame == 2002) input = std::numeric_limits<double>::infinity();
            synth.externalInput(input);
            output.sample(synth);
        }
    }
}

void longScenes(Output& output) {
    for (unsigned scene = 0; scene < 3; ++scene) {
        const unsigned voices = voiceCounts[scene], factor = factors[scene];
        ceres::Synth synth(48000, 901 + scene);
        require(synth.configure(voices, factor), "long configuration rejected");
        ceres::Patch p;
        p.saw = .6; p.pulse = .4; p.sine = .2;
        p.ampAttack = .001; p.ampSustain = 1; p.ampRelease = .001;
        p.filterInitial = p.filterPeak = 0; p.filterRelease = .001;
        p.boardPitchTolerance = p.boardFilterTolerance = p.boardEnvelopeTolerance = 0;
        p.boardEnvelopeTimeTolerance = p.boardVcaTolerance = p.driftCents = 0;
        synth.setPatch(p);
        for (unsigned frame = 0; frame < 32768; ++frame) {
            if (frame == 64) chord(synth, voices, 42);
            if (frame == 4096 || frame == 4128 || frame == 8192) {
                p.hpResonance = frame == 4128 ? .1 : .9;
                p.lpResonance = frame == 4128 ? .9 : .2;
                p.resonanceOffset = frame == 4128 ? 8 : -6; synth.setPatch(p);
            }
            if (frame == 12288) synth.allNotesOff();
            // Long stationary notes and truly idle boards exercise caches
            // after their envelopes/control capacitors approach their floors.
            if (frame == 28000) chord(synth, voices, 61);
            if (frame == 28672) { p.saw = p.pulse = 0; p.sine = 1; synth.setPatch(p); }
            output.sample(synth);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s output.f32\n", argv[0]);
        return 2;
    }
    Output output(argv[1]);
    factoryScenes(output);
    controllerScenes(output);
    longScenes(output);
    output.finish();
}
