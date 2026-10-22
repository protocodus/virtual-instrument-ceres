// Panel 2 BRILLIANCE and RESONANCE lever laws (SM2 p.26 PN2; SM1 p.14 R7;
// SM1 parts list p.9; CS-60 parts list p.9 as sibling evidence). Expected values come from nodal equations written
// here directly, not from the a(1-a) source-resistance form the engine uses.
#include "ceres.hpp"
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
// R7: BR reaches each of four HP nodes through 680k and four LP nodes through
// 220k; RES reaches eight pin-7 nodes through 220k each.
constexpr double brLoad=1/(4/680e3+4/220e3);

void yamahaMatrixReferences() {
    // Guide printed pp.58-59: every R7 input at +10 V, every output into 1k.
    // The fH reference (0.45 V) is the 39k/680k/3x150k node and the QH one
    // (90 mV) is two 220k, so the drawn matrix is the one Yamaha measured.
    const double gh=1/39e3+1/680e3+3/150e3, gq=2/220e3;
    near(10*gh/(gh+1e-3),.45,.005,"fH 1k-load reference");
    near(10*gq/(gq+1e-3),.090,.0005,"QH 1k-load reference");
}

void brillianceLever() {
    near(ceres::brillianceLeverVolts(0),-15,0,"MIN is the -15 V end");
    near(ceres::brillianceLeverVolts(1),15,0,"MAX is the +15 V end");
    near(ceres::brillianceLeverVolts(.5),0,0,"centre tap is ground");
    near(ceres::brillianceLeverVolts(-3),-15,0,"travel clamps low");
    near(ceres::brillianceLeverVolts(7),15,0,"travel clamps high");
    for(double t:{.05,.25,.4,.6,.75,.9}) {
        // KCL at the wiper of a 5k half: rail-side and tap-side pieces.
        const double b=std::abs(2*t-1), rail=t>.5?15:-15;
        const double toRail=5e3*(1-b), toTap=5e3*b;
        const double v=(rail/toRail)/(1/toRail+1/toTap+1/brLoad);
        near(ceres::brillianceLeverVolts(t),v,1e-12,"loaded centre-tapped wiper");
        near(ceres::brillianceLeverVolts(t),-ceres::brillianceLeverVolts(1-t),1e-12,"symmetric halves");
    }
    // Loading costs 2.92 % at quarter travel and nothing at the ends.
    near(ceres::brillianceLeverVolts(.75)/7.5,1/(1+1250/brLoad),1e-12,"quarter-travel sag");
    require(std::isnan(ceres::brillianceLeverVolts(std::nan(""))),"nonfinite travel is rejected");
}

// The eight pin-7 networks solved directly. Pin 7 is a current input, taken
// as 1.72k to ground (four times the CS-30's rank-B VQ shunt, the inference
// beside qControlTau in ceres.cpp). Fixed point on r.
double resonanceByNodes(double upper,double lower,double hpPreset,double lpPreset) {
    // KCL at the wiper: (10-r)/upper - r/lower - sum_j (r-q_j)/220k = 0,
    // with each settled pin node from C dq/dt=(P-q)/220k+(r-q)/220k-q/Rin=0.
    // Gauss-Seidel on the pins: a contraction, so it converges to the root.
    constexpr double pin=1720;
    const auto settled=[](double preset,double r) {
        return (preset/220e3+r/220e3)/(2/220e3+1/pin);
    };
    double r=5;
    for(int i=0;i<400;++i) {
        const double qh=settled(hpPreset,r), ql=settled(lpPreset,r);
        r=(10/upper+(4*qh+4*ql)/220e3)/(1/upper+1/lower+8/220e3);
    }
    return r;
}

void resonanceLever() {
    for(double p:{0.,3.,10.}) {
        near(ceres::resonanceLeverVolts(0,p),10,0,"MIN is the +10 V end");
        near(ceres::resonanceLeverVolts(1,p),0,0,"MAX is the grounded end");
    }
    // B-10K: terminal 1 (+10 V) to wiper is 10k*t, wiper to ground 10k*(1-t).
    for(double t:{.1,.3,.5,.8}) for(double hp:{0.,2.,9.}) for(double lp:{1.,6.,10.}) {
        const double expected=resonanceByNodes(10e3*t,10e3*(1-t),hp,lp);
        near(ceres::resonanceLeverVolts(t,(hp+lp)/2),expected,1e-9,"27.7k to .78% of the mean preset bus is exact");
    }
    // Eight 220k into current inputs load the B-10K harder than the old
    // zero-current pins did: it sags up to .50 V from linear (near a third of
    // travel), where 55k held it to .26 V.
    for(int i=0;i<=100;++i) {
        const double t=i/100.;
        require(std::abs(ceres::resonanceLeverVolts(t,0)-10*(1-t))<.50,"B-10K stays near linear");
    }
    double last=11;
    for(int i=0;i<=100;++i) {
        const double v=ceres::resonanceLeverVolts(i/100.,5);
        require(v<last,"RES falls monotonically toward MAX");last=v;
    }
    require(std::isnan(ceres::resonanceLeverVolts(.5,std::nan(""))),"nonfinite preset mean is rejected");
}
}

int main() {
    try {
        yamahaMatrixReferences();std::puts("PASS R7 matrix matches Yamaha's 1k-load fH and QH references");
        brillianceLever();std::puts("PASS BRILLIANCE centre-tapped B-10K lever, loaded by 41.6k");
        resonanceLever();std::puts("PASS RESONANCE B-10K lever into eight pin-7 networks");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
