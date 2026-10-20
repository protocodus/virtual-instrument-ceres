// PRA output preamp (SM2 printed p.22; guide printed pp.7 and 47; sibling
// CS-60 service manual printed pp.38-39, M-board VI and PRA III). Expected
// rails are recomputed here from the printed parts and the cited datasheet
// figures, not read back from the engine's constants.
#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
// HA1452 Icc 12mA through the 390/220 ohm droppers from +/-15V, less the
// NJM4558 sibling's typical 2V swing margin into 2k: HO +8.32V, -10.36V.
constexpr double hoPositive=15-.012*390-2, hoNegative=-(15-.012*220-2);
// Four sine-only voices (CP4 1Vpp, engine +/-1) at full LEVEL, .18 at full
// VOLUME, read 0 dBm (sqrt(.6) V RMS) at HO. Full SUSTAIN holds the aligned
// VCA EG at 9.8V of its 10V peak (TP2 floor -0.2V), so .98 of each sine.
const double referenceRms=.18*std::sqrt(4*.5)*.98;
const double perVolt=referenceRms/std::sqrt(.6);

void preampTransfer() {
    // The engine node is HO inverted, so HO's positive limit bounds it below.
    const double high=-hoNegative*perVolt, low=-hoPositive*perVolt;
    near(high,3.3365505491030696,1e-12,"Upper rail from the printed droppers");
    near(low,-2.679546386924473,1e-12,"Lower rail from the printed droppers");
    for(double volume:{0.,.1,.55,1.})
        for(double x:{-20.,-3.,-.7,0.,.3,2.,9.}) {
            const double linear=x*.18*volume;
            near(ceres::outputPreampResponse(x,volume),std::clamp(linear,low,high),1e-15,
                 "Linear inverting preamp, hard-limited at the op-amp rails");
        }
    // Four reference sines peaking together still leave 3.6dB to the rail.
    require(20*std::log10(-low/(.18*4))>3.5,"Rails sit above the four-sine coherent peak");
    // Cross-check (CS-60 VI.4, CS-50 O = 0.6V for .8Vpp at CP4): the bus is
    // four 1k branches into 47k, so the reference needs VR6 inside its range.
    const double busRms=std::sqrt(4.)*(.5*.6/.8/std::sqrt(2.))*(1/1e3)/(4/1e3+1/47e3);
    const double vr6=std::sqrt(.6)/busRms;
    require(vr6>150./147&&vr6<1150./147,"VR6 reaches 0 dBm from the calibrated bus");
}

ceres::Patch reference(double volume) {
    ceres::Patch p;p.saw=0;p.pulse=0;p.sine=1;p.filterLevel=0;
    p.filterInitial=p.filterPeak=0;p.ampAttack=.001;p.ampSustain=1;
    p.ampLevel=1;p.volume=volume;
    return p;
}

// Guide printed p.7: 0 dBm at HIGH with every volume control fully clockwise
// and four keys down; the CS-60 procedure makes that tone the sine alone. The real stage is far from its rails there, so VOLUME
// scales the whole output exactly (the old tanh took 1.5dB off its peaks).
void referenceIsLinear() {
    const int chord[4]={48,52,55,60};
    ceres::Synth full(48000,3),half(48000,3);
    full.setPatch(reference(1));half.setPatch(reference(.5));
    for(int n:chord){full.noteOn(n);half.noteOn(n);}
    double energy=0,peak=0;long frames=0;
    for(int i=0;i<3*48000;++i) {
        const double a=full.process(),b=half.process();
        near(a,2*b,2e-6*(1+std::abs(a)),"VOLUME is linear at the 0 dBm reference");
        if(i>=48000){energy+=a*a;++frames;peak=std::max(peak,std::abs(a));}
    }
    const double rms=std::sqrt(energy/frames);
    // The analytic anchor describes the engine's own sines (0.01dB measured).
    require(std::abs(20*std::log10(rms/referenceRms))<.1,"Reference RMS matches the anchor");
    require(peak<hoPositive*perVolt,"Reference peaks stay below the HO rail");
}
}

int main() {
    try {preampTransfer();referenceIsLinear();}
    catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    std::puts("PASS output preamp contracts");
}
