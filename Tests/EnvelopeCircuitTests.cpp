// Envelope-circuit contracts for the factory CV conversion. Expected values
// are recomputed here from the documented circuit (bisection on the node
// equations), not read back from the engine's own state.
//  - T71/T72 tone-preset pins (SM2 pp.10-11): 10V bus, series resistor,
//    1S1555 (1N4148-class estimate), 10k to ground.
//  - SUB-board sustain control (SM2 p.8; Guide p.61): release levers reach
//    RF/RA through 10k + 1k into a 2SC458 follower with 10k to -15V; decay
//    levers reach DF/DA directly. 2SC458: VBE .67V at 2mA (Hitachi typ.),
//    hFE C/D ranks 160-500, geometric centre 283.
//  - IG00159/IG00152 pin law (SM2 p.21, SM1 p.20): 0..10V gives 10s..10ms
//    for D/R, taken as 5 RC and expressed as the engine's T60.
#include "ceres.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
double bisect(double low,double high,double (*f)(double,double),double arg) {
    // f is increasing in x; find f(x,arg)=0.
    for(int i=0;i<200;++i){const double mid=(low+high)/2;(f(mid,arg)>0?high:low)=mid;}
    return (low+high)/2;
}
double pinResidual(double pin,double ideal) {
    const double vt=1.75*25.85e-3;
    return pin-(10-vt*std::log(pin/(10e3*2.52e-9)))*ideal/10;
}
double pin(double ideal) {return ideal<=0?ideal:bisect(1e-6,ideal,pinResidual,ideal);}
double followerResidual(double out,double lever) {
    const double emitter=(out+15)/10e3;
    return out-(lever-emitter/284*11e3-(.67+25.85e-3*std::log(emitter/2e-3)));
}
double follower(double lever) {return bisect(lever-2,lever,followerResidual,lever);}
double law(double volts) {return 10*std::pow(10,-.3*volts)*std::log(1000.)/5;}

struct Row {const char* name; double filterRelease, ampDecay, ampRelease;};
// Guide printed p.64 columns o (VCF R), s (VCA D) and u (VCA R); none of
// these three columns carries a T-board erratum.
const std::array<Row,13> rows{{
    {"string1",6.80,1.09,8.85},{"string2",6.80,10,6.99},{"brass1",10,2.04,10},
    {"brass2",7.19,4.35,6.62},{"flute",7.35,0,7.19},{"electric_piano",10,2.17,6.41},
    {"clavichord",7.52,3.33,10},{"harpsichord",0,2.17,7.35},{"guitar1",10,3.85,6.41},
    {"guitar2",7.19,.99,8.06},{"funky1",4.76,10,8.85},{"funky2",9.30,0,10},
    {"funky3",7.19,10,7.35}}};

void releaseFollowerDrop() {
    for(const auto& row:rows) {
        const auto p=ceres::preset(row.name);
        const double rel=1e-12;
        const double ampRelease=law(follower(pin(row.ampRelease)));
        const double filterRelease=law(follower(pin(row.filterRelease)));
        near(p.ampRelease,ampRelease,rel*ampRelease,"VCA release CV passes the sustain-control follower");
        near(p.filterRelease,filterRelease,rel*filterRelease,"VCF release CV passes the sustain-control follower");
        // Decay levers reach DF/DA without the follower.
        near(p.ampDecay,law(pin(row.ampDecay)),rel*law(pin(row.ampDecay)),"VCA decay CV reaches the EG pin directly");
    }
    // The drop at the follower's own operating point: .67V VBE plus the base
    // current (about 7uA) through 11k, so 0.7-0.8V for every lever voltage.
    for(double lever=0;lever<=10;lever+=.5) {
        const double drop=lever-follower(lever);
        require(drop>.70&&drop<.80,"Follower drop stays at its 1.5-2.5mA operating point");
    }
    // Same CV on D and R: R is 10^(.3*drop) longer, about 1.7x.
    const double ratio=law(follower(pin(6.41)))/law(pin(6.41));
    require(ratio>1.6&&ratio<1.75,"Release is about 1.7x the same CV's decay");
}
}

int main() {
    try {
        releaseFollowerDrop();std::puts("PASS factory release CVs through the sustain-control follower");
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
