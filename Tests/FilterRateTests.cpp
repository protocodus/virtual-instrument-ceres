// The default quality is 1x, so the internal rate must not set the tone.
// Until 1.1.7 it did: trapezoidal integration bent the low pass's skirt down
// to nothing at fs/2, the cutoff stopped at .22 fs, and the oscillator's
// two-sample edges cost every harmonic sinc^2(f/fs). Measured before the fix
// at 48kHz, 1x: a 6kHz low pass read -3.2dB at 12.5kHz and -8.4dB at 16kHz
// against the same network at 768kHz, the open filter (20kHz) reached -3dB
// at 7.2kHz (-5.9dB at 10kHz), and the sine's harmonics sat .9dB (8.5kHz)
// and 1.9dB (12.7kHz) under their 4x level.
// The reference is the engine's own circuit model run at a rate where the
// integration error is negligible, not a hardware measurement: this file
// holds the model to itself across rates.
#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
double decibels(const std::vector<double>& x,double f,double fs) {
    std::complex<double> sum=0;
    for(std::size_t n=0;n<x.size();++n) sum+=x[n]*std::polar(1.0,-2*pi*f*double(n)/fs);
    return 20*std::log10(std::abs(sum));
}
std::vector<double> lowPass(double fs,double cutoff,double resonance) {
    return ceres::filterImpulse(fs,cutoff,resonance,false,std::size_t(fs*.15));
}

void lowPassFollowsTheCircuit() {
    double worstBody=0,worstTop=0;
    for(double cutoff:{1000.,3000.,6000.}) for(double resonance:{0.,.6}) {
        const auto reference=lowPass(768000,cutoff,resonance);
        for(double fs:{44100.,48000.}) {
            const auto x=lowPass(fs,cutoff,resonance);
            const auto error=[&](double f) {
                return decibels(x,f,fs)-decibels(reference,f,768000);
            };
            for(double f:{4000.,8000.,12500.}) worstBody=std::max(worstBody,std::abs(error(f)));
            worstTop=std::max(worstTop,std::abs(error(16000)));
        }
    }
    std::printf("1x low pass against 768kHz: %.2fdB to 12.5kHz, %.2fdB at 16kHz\n",worstBody,worstTop);
    // Measured .69dB and 1.67dB; the engine before the fix reads 6.19dB and
    // 13.18dB here.
    check(worstBody<1.2,"A 1x low pass must follow the circuit to 12.5kHz");
    check(worstTop<2.5,"A 1x low pass must follow the circuit at 16kHz");
}

void openFilterStaysOpen() {
    // 20kHz at resonance 0 (Q .5): the circuit is -3dB at 11.8kHz. Measured
    // -2.0dB at 10kHz.
    const auto x=lowPass(48000,20000,0);
    const double passband=decibels(x,200,48000);
    check(decibels(x,10000,48000)>passband-3,"The open low pass must still be open at 10kHz at 1x");
}

double sineHarmonic(unsigned factor,int harmonic) {
    ceres::Synth s(48000,1);
    check(s.configure(1,factor),"configure");
    auto p=ceres::preset("init");
    p.saw=p.pulse=p.noise=0;p.sine=1;p.ampAttack=.001;p.ampSustain=1;p.driftCents=0;
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    s.setPatch(p);s.noteOn(48);
    for(int n=0;n<48000;++n) s.process();
    std::vector<double> x(96000);
    for(auto& sample:x) sample=s.process();
    // Hann window: the harmonics are 131Hz apart.
    for(std::size_t n=0;n<x.size();++n) x[n]*=.5-.5*std::cos(2*pi*double(n)/double(x.size()));
    const double f=130.8127826502993*harmonic;
    return decibels(x,f,48000)-decibels(x,130.8127826502993,48000);
}
void edgesKeepTheirLevel() {
    // C3's 65th and 97th harmonics, 8.5kHz and 12.7kHz, about -84dB under
    // the fundamental: the triangle's corners through the sine shaper.
    for(int harmonic:{65,97}) {
        const double difference=sineHarmonic(1,harmonic)-sineHarmonic(4,harmonic);
        std::printf("sine harmonic %d, 1x against 4x: %+.2fdB\n",harmonic,difference);
        check(std::abs(difference)<.5,"An oscillator edge must keep its level at 1x");
    }
}

void overdrivenFilterSettles() {
    // Everything at its end stop, cutoffs far above the audio band: the
    // integrator gain reaches its ceiling with both stages driven into their
    // limits, where Newton's steps alone stop closing (see solveBracketed).
    // A solve left open shows as level: the engine before the fix peaks at
    // 2.46 here, and 3.1 with the higher ceiling and no bracket.
    ceres::Synth s(48000,987);
    check(s.configure(4,1),"configure");
    auto p=ceres::preset("init");
    p.hpResonance=p.lpResonance=1;p.hpCutoff=19000;p.lpCutoff=20000;
    p.filterInitial=-12;p.filterPeak=12;p.filterAttack=.0001;
    p.ampAttack=.0001;p.drive=8;p.saw=p.pulse=p.sine=p.noise=1;
    p.pulseWidth=.9;p.pwmDepth=.4;p.pwmRate=50;
    s.setPatch(p);s.pitchBend(48);
    double peak=0;
    for(int i=0;i<96000;++i) {
        if(i%317==0) s.noteOn((i/317*13)%128);
        const double sample=s.process();
        check(std::isfinite(sample),"Extreme settings must stay finite at 1x");
        peak=std::max(peak,std::abs(sample));
    }
    std::printf("overdriven 1x peak: %.3f\n",peak);
    check(peak<1.5,"An overdriven filter at the cutoff ceiling must settle");
}
}

int main() {
    try {
        lowPassFollowsTheCircuit();openFilterStaysOpen();edgesKeepTheirLevel();overdrivenFilterSettles();
        std::puts("Filter rate contracts passed");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
