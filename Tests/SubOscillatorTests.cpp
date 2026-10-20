// SUB-OSCILLATOR lever laws and source shape (guide printed pp.27, 49, 55;
// SM1 printed p.14 R7; SM2 printed pp.16, 27). Expected values are solved here
// from the schematic as resistor networks and printed calibration points, not
// from the engine's own helper formulas.
#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
// A 10k linear lever between source and ground, travel x, its wiper loaded by
// load ohms to a 0 V node: Thevenin-free nodal solution.
double wiper(double source,double x,double load) {
    const double top=(1-x)*10e3,bottom=x*10e3;
    if(x<=0)return 0;
    if(x>=1)return source;
    const double conductance=1/top+1/bottom+1/load;
    return source/top/conductance;
}

void speedCalibration() {
    // Guide printed p.55 table: .8+/-.16, 6.7+/-1.3, 50+/-10 Hz at 0/5/10 V.
    near(ceres::subSpeedHz(0),.8,1e-15,"SPEED 0 V is .8 Hz");
    near(ceres::subSpeedHz(10),50,1e-12,"SPEED 10 V is 50 Hz");
    const double mid=ceres::subSpeedHz(5);
    require(mid>6.7-1.3 && mid<6.7+1.3,"SPEED 5 V within the printed tolerance");
    near(mid,std::sqrt(.8*50),1e-12,"Exponential law: 5 V is the geometric mean of the ends");
    for(double v=0;v<9.5;v+=.5)
        near(ceres::subSpeedHz(v+.5)/ceres::subSpeedHz(v),std::pow(62.5,.05),1e-12,
             "Equal volts give equal frequency ratios");
    near(ceres::subSpeedHz(-3),.8,0,"VC2 below 0 V is clamped");
    near(ceres::subSpeedHz(14),ceres::subSpeedHz(10),0,"VC2 above 10 V is clamped");
    // The lever sees pin 1's 180k to a current-summing node.
    for(double x=0;x<=1.0001;x+=.05)
        near(ceres::subSpeedLeverHz(x),ceres::subSpeedHz(wiper(10,x,180e3)),1e-12,
             "SPEED lever loaded by 180k");
    near(ceres::subSpeedLeverHz(0),.8,1e-15,"SPEED lever bottom");
    near(ceres::subSpeedLeverHz(1),50,1e-12,"SPEED lever top");
}

void depthLevers() {
    // VCO: V1 -> four (100k to VB + 100k to V2 at 0 V) = 50k; VB = V1/2 into
    // pin 15 at 50 cents per 1.5 V. SUB unit = 1.5 V peak.
    for(double x=0;x<=1.0001;x+=.05) {
        const double vb=wiper(1.5,x,1/(4/200e3))/2;
        near(ceres::subVibratoLeverCents(x),vb*50/1.5,1e-12,"VCO lever cents per SUB unit");
    }
    near(ceres::subVibratoLeverCents(1),25,1e-12,"Full VCO lever is 25 cents");
    // VCF: four 47k + four 150k; LP octaves per SUB unit as the 47k current
    // against FL's 1 oct/V through 39k (the printed IG00156 law read at the
    // lever, guide p.34; it replaced the provisional .65 oct/V on 2026-10-20).
    const double load=1/(4/47e3+4/150e3);
    for(double x=0;x<=1.0001;x+=.05) {
        const double lpCurrent=wiper(1.5,x,load)/47e3;
        near(ceres::subFilterLeverOctaves(x),39e3*lpCurrent,1e-12,"VCF lever LP octaves");
    }
    near(ceres::subFilterLeverOctaves(1),39./47*1.5,1e-12,"Full VCF lever");
    require(ceres::subFilterLeverOctaves(.5)<.4*ceres::subFilterLeverOctaves(1),
            "The heavily loaded VCF lever sags more than 20% at mid-travel");
    near(ceres::subVibratoLeverCents(-1),0,0,"Lever travel clamps low");
    near(ceres::subFilterLeverOctaves(2),ceres::subFilterLeverOctaves(1),0,"Lever travel clamps high");
}

ceres::Patch quiet() {
    ceres::Patch p;p.saw=1;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=5;
    p.filterInitial=p.filterPeak=0;p.lfoRate=3;
    return p;
}

void sourceShapes() {
    const double fs=48000;
    // Guide p.55 / PN3 order: sine, quick-rise saw (falls), slow-rise saw
    // (rises), square. Square: NJM4558 rails ~14V through 15k/1.8k, 1.5V peak.
    for(unsigned w=1;w<=3;++w) {
        auto p=quiet();p.lfoWaveform=w;ceres::Synth s(fs,1);s.setPatch(p);
        double previous=0,maximum=-9,minimum=9;unsigned rising=0,falling=0;
        for(unsigned n=0;n<48000;++n) {
            s.process();const double sub=s.modulationState().sub;
            if(n>0){if(sub>previous)++rising;else if(sub<previous)++falling;}
            previous=sub;maximum=std::max(maximum,sub);minimum=std::min(minimum,sub);
        }
        if(w==1)require(falling>40000,"Quick-rise saw falls between its jumps");
        if(w==2)require(rising>40000,"Slow-rise saw rises between its drops");
        if(w==3){near(maximum,14*1.8/16.8/1.5,.01,"Square high level");
                 near(minimum,-14*1.8/16.8/1.5,.01,"Square low level");}
    }
    // One free-running clock: note events never touch the SUB phase.
    auto p=quiet();p.lfoWaveform=1;p.lfoRate=ceres::subSpeedLeverHz(.37);
    ceres::Synth a(fs,2),b(fs,2);a.setPatch(p);b.setPatch(p);
    for(unsigned n=0;n<30000;++n) {
        if(n==1000)a.noteOn(60);
        if(n==7000){a.noteOn(64);a.noteOff(60);}
        if(n==20000)a.allNotesOff();
        a.process();b.process();
        require(a.modulationState().sub==b.modulationState().sub,"SUB is free-running, not key-synced");
    }
}

void vibratoThroughEngine() {
    // Full VCO lever on the sine SUB swings the VCO +/-25 cents.
    auto p=quiet();p.lfoWaveform=0;p.lfoRate=ceres::subSpeedLeverHz(.5);
    p.vibratoCents=ceres::subVibratoLeverCents(1);
    ceres::Synth s(48000,3);s.setPatch(p);s.noteOn(69);
    double high=0,low=1e9;
    for(unsigned n=0;n<96000;++n) {
        s.process();const double f=s.voiceState()[0].oscillatorFrequency;
        high=std::max(high,f);low=std::min(low,f);
    }
    near(1200*std::log2(high/440),25,.01,"Vibrato peak at full lever");
    near(1200*std::log2(low/440),-25,.01,"Vibrato trough at full lever");
}
} // namespace

int main() {
    try {
        speedCalibration();depthLevers();sourceShapes();vibratoThroughEngine();
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
    std::puts("SUB oscillator contracts passed");
    return 0;
}
