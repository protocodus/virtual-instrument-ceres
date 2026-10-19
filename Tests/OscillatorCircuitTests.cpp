#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr double pi=3.14159265358979323846;
using C=std::complex<double>;
void check(bool ok,const std::string& why) { if(!ok)throw std::runtime_error(why); }
C exponentialIntegral(int k,double a,double b) {
    if(k==0)return b-a;
    const C iw(0,2*pi*k);
    return (std::exp(-iw*a)-std::exp(-iw*b))/iw;
}
C rawSawCoefficient(int n,double r) {
    const double w=2*pi*n;
    const C iw(0,w), e=std::exp(-iw*r);
    // Integral of2phi-1 over a full cycle minus2phi on[0,r].
    return -2.0/iw-2.0*(e*(-r/iw+1/(w*w))-1/(w*w));
}
C rawSineCoefficient(int n,double r) {
    return exponentialIntegral(n,0,r)+.5*(exponentialIntegral(n-1,r,1)+exponentialIntegral(n+1,r,1));
}
C measured(const std::vector<ceres::OscillatorFrame>& data,int harmonic,bool sine) {
    C result=0;
    for(std::size_t i=0;i<data.size();++i)
        result+=(sine ? data[i].sine:data[i].saw)*std::polar(1.0,-2*pi*harmonic*i/data.size());
    return result/double(data.size());
}
void nominalTiming() {
    ceres::Patch p;
    const double tau=p.resetMicroseconds*1e-6;
    check(std::abs(1/(40e3*800e-6*tau)-523.2)<1e-12,"Nominal one-shot must reproduce published Normal frequency/current slope");
    check(tau>58e-6 && tau<60e-6,"Nominal reset in source-derived interval");
    check(ceres::setParameter(p,"reset_microseconds",p.resetMicroseconds) && p.resetMicroseconds==tau*1e6,"Setter must accept the nominal reset");
    check(ceres::setParameter(p,"reset_microseconds",100) && p.resetMicroseconds==60,"Extended engineering reset bound");
}
void conditionedWaveforms() {
    ceres::Patch p; p.pwmDepth=0; p.pulseWidth=.7;
    constexpr double fs=1024000;
    for(double f:{1000.,4000.}) {
        const auto length=std::size_t(fs/f);
        const double dt=f/fs,r=f*p.resetMicroseconds*1e-6;
        const auto wave=ceres::oscillatorTrace(fs,f,p,length);
        for(std::size_t i=0;i<length;++i) {
            const double phase=double(i)/length;
            if(phase>dt && phase<r-dt) {
                check(wave[i].saw==-1 && wave[i].sine==1,"Shared conditioned plateau before WSC inversion/folding");
            }
            if(phase>r+dt && phase<1-dt) {
                check(std::abs(wave[i].saw-(2*phase-1))<2e-12,"Descending current-balance slope must not stretch with reset duration");
                check(std::abs(wave[i].sine-std::cos(2*pi*phase))<2e-12,"Sine must fold the same conditioned branch");
            }
            check(std::isfinite(wave[i].saw) && std::abs(wave[i].saw)<1.1,"Bounded conditioned saw");
            check(std::isfinite(wave[i].sine) && std::abs(wave[i].sine)<1.1,"Bounded folded sine");
        }
        check(std::abs(measured(wave,0,false).real()+r*r)<2e-6,"Conditioned saw DC must agree with independent piecewise integral");
        const double slope=-2*pi*std::sin(2*pi*r);
        const double mean=r-std::sin(2*pi*r)/(2*pi)+slope*dt*dt/12;
        check(std::abs(measured(wave,0,true).real()-mean)<2e-6,"Folded sine DC including finite BLAMP area");
        for(int n=1;n<=16;++n) {
            C expectedSaw=0,expectedSine=0;
            // Exact sampling identity: the DFT includes all continuous
            // Fourier coefficients at n+kN. The remaining high-frequency
            // curvature/smoothing tails must not be mistaken for a jump
            // correction error or hidden by widening a tolerance.
            for(int alias=-64;alias<=64;++alias) {
                const int m=n+alias*int(length);
                const double sinc=std::sin(pi*m*dt)/(pi*m*dt),t=sinc*sinc;
                expectedSaw+=rawSawCoefficient(m,r)*t;
                const double w=2*pi*m,jump=std::cos(2*pi*r)-1;
                const C edge=std::polar(1.0,-w*r);
                expectedSine+=rawSineCoefficient(m,r)+edge*(jump*(t-1)/C(0,w)+slope*(1-t)/(w*w));
            }
            check(std::abs(measured(wave,n,false)-expectedSaw)<1e-8,"Saw harmonics must match independent piecewise integral, edge filter and sampling identity");
            check(std::abs(measured(wave,n,true)-expectedSine)<1e-8,"Folded sine harmonics must match independent integral, jump/slope correction and sampling identity");
        }
        auto old=p; old.resetMicroseconds=6;
        const auto shortReset=ceres::oscillatorTrace(fs,f,old,length);
        double sineDifference=0,sawDifference=0;
        for(std::size_t i=0;i<length;++i) {
            check(wave[i].pulse==shortReset[i].pulse,"Monostable duration must not move documented fixed PWM edges");
            sineDifference+=std::pow(wave[i].sine-shortReset[i].sine,2);
            sawDifference+=std::pow(wave[i].saw-shortReset[i].saw,2);
        }
        check(sineDifference>1e-5 && sawDifference>1e-4,"Finite reset must affect both derived waveforms at upper notes");
    }
}
void extrapolationAndRate() {
    ceres::Patch p;
    p.pulseWidth=.5;p.pwmDepth=.4;p.pwmPhase=.75;p.pwmRate=17;
    auto shortReset=p;shortReset.resetMicroseconds=.1;
    const auto deep=ceres::oscillatorTrace(192000,3520,p,4096);
    const auto deepShort=ceres::oscillatorTrace(192000,3520,shortReset,4096);
    for(std::size_t i=0;i<deep.size();++i)
        check(deep[i].pulse==deepShort[i].pulse,"Independent fixed-edge WSC reduction must remain stable when deep PWM overlaps reset plateau");
    for(double fs:{32000.,176400.,192000.,384000.,1536000.})
        for(double f:{27.5,440.,3520.,8372.,20000.,100000.}) {
            const auto wave=ceres::oscillatorTrace(fs,f,p,512,.997);
            for(const auto& x:wave)
                check(std::isfinite(x.saw)&&std::isfinite(x.sine)&&std::isfinite(x.pulse)&&
                    std::abs(x.saw)<2&&std::abs(x.sine)<2&&std::abs(x.pulse)<2,"Extreme CV and rate guards must remain finite/bounded");
        }
}
}
int main() {
    try {nominalTiming();conditionedWaveforms();extrapolationAndRate();
        std::cout<<"PASS nominal VCO timing, conditioned current slope, WSC shared fold, Fourier/DC/edge and high-CV bounds\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
