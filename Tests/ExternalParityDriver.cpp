#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

// SUB-EXT parity: one Patch, event and EXT IN stream through the native and
// embedded cores. A separate driver, so ParityDriver.cpp stays compilable
// against the frozen pre-EXT cores. Modes: "external" (the default) selects
// FUNCTION EXT and feeds the jack; "silent" never feeds it; "inert" and
// "inert-silent" run the internal sources 0..4 with and without the feed.
namespace {
constexpr double pi=3.14159265358979323846;
double jack(unsigned frame,double rate,unsigned scene) {
    if(frame%997==13)return std::numeric_limits<double>::quiet_NaN();
    if(frame%1499==7)return frame%2 ? 1e300 : -std::numeric_limits<double>::infinity();
    const double t=frame/rate;
    // An audio-rate tone over a slow sweep: FM, filter and VCA modulation.
    double x=.9*std::sin(2*pi*(55+scene*37)*t)+.6*std::sin(2*pi*3.3*t);
    if(frame>=2500 && frame<2800)x*=25; // past both op-amp swings
    if(frame>=4000 && frame<5200)x+=.7; // a DC step for the coupling to decay
    return x;
}
}

int main(int argc,char** argv) {
    if(argc<2 || argc>3)return 2;
    const char* mode=argc==3 ? argv[2] : "external";
    const bool inert=std::strcmp(mode,"inert")==0 || std::strcmp(mode,"inert-silent")==0;
    const bool silent=std::strcmp(mode,"silent")==0 || std::strcmp(mode,"inert-silent")==0;
    if(!inert && !silent && std::strcmp(mode,"external")!=0)return 2;
    std::FILE* file=std::fopen(argv[1],"wb");
    if(!file)return 3;
    for(double rate:{44100.,48000.,96000.})for(unsigned factor:{1u,2u,4u})for(unsigned scene=0;scene<5;++scene) {
        ceres::Synth synth(rate,70+scene);
        if(!synth.configure(4,factor))return 4;
        ceres::Patch patch;
        patch.saw=.8;patch.pulse=scene%2 ? .4 : 0;patch.sine=.1;patch.pwmDepth=.15;
        patch.lpCutoff=1800;patch.lpResonance=.6;patch.hpCutoff=60;
        patch.filterInitial=-1;patch.filterPeak=2;patch.filterAttack=.01;patch.filterDecay=.3;
        patch.ampAttack=.004;patch.ampDecay=.2;patch.ampSustain=.7;patch.ampRelease=.3;
        patch.lfoRate=4.1;patch.vibratoCents=25;patch.filterLfo=.6;patch.tremolo=.5+scene*.1;
        patch.pressureVibrato=18;patch.pressureWah=.4;patch.pressureFilter=.3;patch.pressureVolume=.3;
        patch.ringMix=scene==4 ? .5 : 0;patch.ringRate=40;
        if(!ceres::setParameter(patch,"lfo_waveform",inert ? scene : 5) ||
           !ceres::setParameter(patch,"ext_level",1-scene*.15))return 5;
        synth.setPatch(patch);
        for(unsigned frame=0;frame<6000;++frame) {
            if(frame==0)synth.noteOn(57,.8);
            if(frame==211)synth.noteOn(64,.6);
            if(frame==700)synth.pressure(.75);
            if(frame==1300)synth.noteOff(57);
            // FUNCTION away from EXT and back: the capacitor keeps its charge.
            if(!inert && (frame==1800 || frame==2300)) {
                ceres::setParameter(patch,"lfo_waveform",frame==1800 ? scene : 5);synth.setPatch(patch);
            }
            if(frame==3100){ceres::setParameter(patch,"ext_level",.35);synth.setPatch(patch);}
            if(frame==3600)synth.allSoundOff();
            if(frame==3601)synth.noteOn(60,.9);
            if(frame==4700){synth.reset();synth.noteOn(69,.7);}
            if(!silent)synth.externalInput(jack(frame,rate,scene));
            const float sample=synth.process();
            if(!std::isfinite(sample))return 6;
            if(std::fwrite(&sample,sizeof sample,1,file)!=1)return 7;
        }
    }
    return std::fclose(file)==0 ? 0 : 8;
}
