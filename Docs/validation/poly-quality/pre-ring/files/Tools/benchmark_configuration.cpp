// Bounded native-only comparison: one synth,48kHz,64frames,all voices held.
// 128 warmup +1000 measured blocks per configuration, no retries/selection.
// Wall/thread-CPU include process and clock overhead; not a Rack deadline claim.
#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
static double now(clockid_t id){timespec t{};if(clock_gettime(id,&t))std::abort();return t.tv_sec+t.tv_nsec*1e-9;}
int main(){
    constexpr unsigned blocks=1000,frames=64,warmup=128;
    constexpr double deadline=64./48000;
    std::puts("[");unsigned row=0;
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}){
        ceres::Synth s(48000,50);if(!s.configure(voices,factor))return 2;
        ceres::Patch p;p.pulse=.4;p.pwmDepth=.2;p.ringMix=.4;p.ringRate=220;
        p.filterLfo=.5;p.tremolo=.3;p.resonanceOffset=-2;p.lpResonance=.7;s.setPatch(p);
        for(unsigned i=0;i<voices;++i)s.noteOn(40+int(i)*3,.8);
        double checksum=0;std::array<double,blocks> wall{},cpu{};unsigned wm=0,cm=0;
        for(unsigned b=0;b<warmup;++b)for(unsigned f=0;f<frames;++f)checksum+=s.process();
        for(unsigned b=0;b<blocks;++b){
            const double w=now(CLOCK_MONOTONIC),c=now(CLOCK_THREAD_CPUTIME_ID);
            for(unsigned f=0;f<frames;++f){const double y=s.process();if(!std::isfinite(y))return 3;checksum+=y;}
            cpu[b]=now(CLOCK_THREAD_CPUTIME_ID)-c;wall[b]=now(CLOCK_MONOTONIC)-w;
            cm+=cpu[b]>deadline;wm+=wall[b]>deadline;
        }
        std::sort(wall.begin(),wall.end());std::sort(cpu.begin(),cpu.end());
        std::printf("%s{\"voices\":%u,\"oversampling\":%u,\"blocks\":1000,\"warmup_blocks\":128,\"rate\":48000,\"block_frames\":64,\"deadline_ms\":%.12g,\"wall_median_ms\":%.12g,\"wall_p99_ms\":%.12g,\"wall_max_ms\":%.12g,\"wall_overruns\":%u,\"thread_cpu_median_ms\":%.12g,\"thread_cpu_p99_ms\":%.12g,\"thread_cpu_max_ms\":%.12g,\"thread_cpu_overruns\":%u,\"checksum\":%.17g}",row++?",\n":"",voices,factor,1000*deadline,1000*wall[blocks/2],1000*wall[blocks*99/100],1000*wall.back(),wm,1000*cpu[blocks/2],1000*cpu[blocks*99/100],1000*cpu.back(),cm,checksum);
    }
    std::puts("\n]");
}
