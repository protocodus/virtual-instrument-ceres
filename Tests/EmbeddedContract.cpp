#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <new>
#include <type_traits>

namespace {
bool watch=false;
std::size_t allocations=0;
void* allocate(std::size_t size) {
    if(watch)++allocations;
    if(void* p=std::malloc(size ? size : 1))return p;
    std::abort();
}
void require(bool good,const char* message) {
    if(!good){watch=false;std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
}
void* operator new(std::size_t size){return allocate(size);}
void* operator new[](std::size_t size){return allocate(size);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

int main() {
    static_assert(std::is_trivially_copyable<ceres::Patch>::value,"Embedded Patch is numeric realtime state");
    ceres::Synth synth(48000,50);
    ceres::Patch patch;
    const char* keys[]={"saw","pulse","sine","noise","pulse_width","pwm_depth","pwm_rate","pwm_phase",
        "hp_cutoff","lp_cutoff","hp_resonance","lp_resonance","key_track","hp_envelope_scale","lp_envelope_scale",
        "filter_initial","filter_peak","filter_attack","filter_attack_headroom","filter_attack_saturation",
        "filter_decay","filter_release","amp_attack","amp_attack_headroom","amp_attack_saturation","amp_decay",
        "amp_sustain","amp_release","global_release","filter_level","volume","octave","tune_cents",
        "voice_spread_cents","drift_cents","board_pitch_tolerance","board_filter_tolerance","board_envelope_tolerance",
        "board_envelope_time_tolerance","board_vca_tolerance","board_temperature_rise","lfo_rate","vibrato_cents","tremolo","filter_lfo","pressure_volume",
        "pressure_filter","pressure_vibrato","velocity_sensitivity","drive","reset_microseconds",
        "ring_mix","ring_rate","ring_depth","ring_attack","ring_decay","lfo_waveform","sustain_mode",
        "brilliance","resonance_offset","amp_level","output_trim_db","expression_source",
        "pressure_wah","glide_mode","glide_rate","expression","ext_level"};
    synth.setPatch(patch);
    watch=true;
    require(!ceres::setParameter(patch,nullptr,1),"null key rejected");
    require(!ceres::setParameter(patch,"unknown",1),"unknown key rejected");
    require(!ceres::setParameter(patch,"volume",std::numeric_limits<double>::quiet_NaN()),"nonfinite edit rejected");
    for(unsigned block=0;block<512;++block) {
        const auto key=keys[block%(sizeof(keys)/sizeof(keys[0]))];
        const double values[]={0.,.5,1.};
        require(ceres::setParameter(patch,key,values[(block/(sizeof(keys)/sizeof(keys[0])))%3]),"every shared key is accepted");
        synth.setPatch(patch);
        if(block%19==0)synth.noteOn(48+block%37,.73);
        if(block%23==0)synth.noteOff(48+(block+5)%37);
        if(block%29==0)synth.sustain(block%58==0);
        if(block%31==0){synth.pressure(.42);synth.pitchBend(-.23);}
        if(block%47==0)synth.allNotesOff();
        if(block%59==0)synth.allSoundOff();
        if(block%101==0)synth.reset();
        for(unsigned frame=0;frame<64;++frame)require(std::isfinite(synth.process()),"finite 64-frame render");
    }
    // Explicitly activate all expression routes with mixed velocities rather
    // than relying on the broad key loop to happen to leave them audible.
    patch=ceres::Patch{};patch.expressionSource=1;patch.pressureVolume=.7;
    patch.pressureFilter=1.2;patch.pressureVibrato=55;patch.ampRelease=2;
    synth.setPatch(patch);synth.reset();
    for(unsigned i=0;i<4;++i)synth.noteOn(48+int(i)*7,.2+.25*i);
    for(unsigned block=0;block<64;++block) {
        synth.pressure(block/63.);
        if(block==8){synth.noteOff(48);synth.sustain(true);synth.noteOff(55);}
        if(block==16)synth.noteOn(76,.31);
        if(block==24){patch.expressionSource=0;synth.setPatch(patch);}
        if(block==32){patch.expressionSource=1;synth.setPatch(patch);synth.sustain(false);}
        if(block==40)synth.allNotesOff();
        if(block==48)synth.allSoundOff();
        if(block==56){synth.reset();synth.noteOn(60,.9);}
        for(unsigned frame=0;frame<64;++frame)require(std::isfinite(synth.process()),"finite active expression render");
    }
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}) {
        synth.reset();require(synth.configure(voices,factor),"realtime-safe reset configuration");
        require(synth.polyphony()==voices && synth.oversamplingFactor()==factor,"configuration applied");
        patch.ringMix=.7;patch.ringRate=220;patch.pwmDepth=.3;patch.filterLfo=.5;
        synth.setPatch(patch);
        for(unsigned i=0;i<voices;++i)synth.noteOn(40+int(i)*3,.3+.7*i/voices);
        for(unsigned i=0;i<voices+2;++i)synth.noteOn(60,.1+.8*i/(voices+2));
        for(unsigned frame=0;frame<64;++frame)require(std::isfinite(synth.process()),"audible velocity reuse finite");
        synth.noteOff(60);synth.noteOff(60); // two presses were never stored
        for(unsigned block=0;block<24;++block) {
            if(block==2)require(!synth.configure(voices, factor==4?1:4),"live change rejected without allocation");
            if(block==3 || block==4 || block==6) {
                patch.ringDepth=block==6?1:6;synth.setPatch(patch);
            }
            if(block==4){synth.sustain(true);synth.allNotesOff();}
            if(block==8)synth.sustain(false);
            if(block==12)synth.noteOn(70,.8);
            if(block==16)synth.allSoundOff();
            for(unsigned frame=0;frame<64;++frame)require(std::isfinite(synth.process()),"configured render finite");
        }
    }
    // SUB-EXT: FUNCTION EXT with every SUB route open, fed every frame with
    // audio-rate, overdriven, DC and invalid jack samples, at each quality.
    for(unsigned factor:{1u,2u,4u}) {
        synth.reset();require(synth.configure(4,factor),"EXT configuration");
        patch=ceres::Patch{};
        require(ceres::setParameter(patch,"lfo_waveform",5) && ceres::setParameter(patch,"ext_level",.8),
                "EXT keys accepted");
        patch.vibratoCents=25;patch.filterLfo=.8;patch.tremolo=.7;patch.pressureVibrato=20;patch.pressureWah=.5;
        synth.setPatch(patch);synth.pressure(.6);synth.noteOn(57,.8);synth.noteOn(64,.5);
        for(unsigned frame=0;frame<4096;++frame) {
            const double jack[]={std::sin(frame*.05),40.,-1e300,std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),.5};
            synth.externalInput(jack[(frame/97)%6]*(frame%3 ? 1:.5));
            if(frame==1024){patch.lfoWaveform=1;synth.setPatch(patch);}
            if(frame==1536){patch.lfoWaveform=5;synth.setPatch(patch);}
            if(frame==2048)synth.allSoundOff();
            if(frame==3072){synth.reset();synth.noteOn(60,.7);}
            require(std::isfinite(synth.process()),"finite EXT IN render");
        }
    }
    require(allocations==0,"no allocations after construction across edits/render/reset/configure");
    watch=false;
    for(double rate:{0.,std::numeric_limits<double>::quiet_NaN(),400000.}) {
        ceres::Synth invalid(rate,1);require(invalid.sampleRate()==48000,"invalid target rate safely defaults");
    }
    std::printf("PASS embedded: no exceptions, numeric Patch, active expression, 9 render configurations, "
                "EXT IN at 3 qualities, %u control keys, zero post-construction new/new[] allocations\n",
                static_cast<unsigned>(sizeof(keys)/sizeof(keys[0])));
}
