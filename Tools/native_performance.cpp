// Bounded native DSP probe; excludes Rack/host/target scheduling.
#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

static double seconds(clockid_t id) {
    timespec value{};
    if (clock_gettime(id, &value)) std::abort();
    return value.tv_sec + value.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    const bool active = argc == 2 && !std::strcmp(argv[1], "active");
    constexpr unsigned blockSize=64, blocks=10000, warmup=500;
    constexpr double rate=48000, deadline=blockSize/rate;
    ceres::Patch patch;
    if (active) {
        struct Control { const char* key; double value; };
        for (const Control& c : {Control{"pulse", .5}, {"pwm_depth", .12},
             {"brilliance", 4}, {"resonance_offset", -2}, {"filter_lfo", .6},
             {"amp_level", .8}, {"tremolo", .2}, {"ring_mix", .35},
             {"ring_depth", 2}, {"output_trim_db", 3}})
            if (!ceres::setParameter(patch,c.key,c.value)) return 2;
    }
    ceres::Synth synth(rate,50);
    synth.setPatch(patch);
    for (int note : {48,55,60,64}) synth.noteOn(note,.8);
    std::vector<double> wall(blocks), cpu(blocks);
    double checksum=0;
    for (unsigned b=0;b<warmup;++b)
        for (unsigned s=0;s<blockSize;++s) checksum+=synth.process();
    unsigned wallMisses=0,cpuMisses=0;
    for (unsigned b=0;b<blocks;++b) {
        const double w=seconds(CLOCK_MONOTONIC), c=seconds(CLOCK_THREAD_CPUTIME_ID);
        for (unsigned s=0;s<blockSize;++s) {
            const double sample=synth.process();
            if (!std::isfinite(sample)) return 3;
            checksum+=sample;
        }
        cpu[b]=seconds(CLOCK_THREAD_CPUTIME_ID)-c;
        wall[b]=seconds(CLOCK_MONOTONIC)-w;
        wallMisses+=wall[b]>deadline;cpuMisses+=cpu[b]>deadline;
    }
    std::sort(wall.begin(),wall.end()); std::sort(cpu.begin(),cpu.end());
    std::printf("{\"workload\":\"%s\",\"rate\":48000,\"block_size\":64,\"voices\":4,"
        "\"oversampling\":4,\"measured_blocks\":10000,\"warmup_blocks\":500,"
        "\"deadline_ms\":%.9g,\"wall_median_ms\":%.9g,\"wall_p99_ms\":%.9g,"
        "\"wall_max_ms\":%.9g,\"wall_deadline_misses\":%u,\"cpu_median_ms\":%.9g,"
        "\"cpu_p99_ms\":%.9g,\"cpu_max_ms\":%.9g,\"cpu_deadline_misses\":%u,"
        "\"checksum\":%.17g,\"scope\":\"native DSP only; no Rack host or target CPU evidence\"}\n",
        active?"active sound paths":"engineering Init",deadline*1000,
        wall[blocks/2]*1000,wall[blocks*99/100]*1000,wall.back()*1000,wallMisses,
        cpu[blocks/2]*1000,cpu[blocks*99/100]*1000,cpu.back()*1000,cpuMisses,checksum);
}
