#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
using C=std::complex<double>;
void check(bool ok,const std::string& why) { if(!ok)throw std::runtime_error(why); }
C rawSawCoefficient(int n,double r) {
    const double w=2*pi*n;
    const C iw(0,w), e=std::exp(-iw*r);
    // Integral of2phi-1 over a full cycle minus2phi on[0,r].
    return -2.0/iw-2.0*(e*(-r/iw+1/(w*w))-1/(w*w));
}
// WSC sine shaper, recomputed here from its definition rather than read from
// the engine: an undegenerated bipolar differential pair at unit peak,
// tanh(k*u)/tanh(k), on STI, u=tri-c: the folded conditioned ramp tri=2|p|-1,
// with p=1 on the one-shot plateau (phase<r) and 1-2*phase after it, less the
// voltage c of its coupling capacitor.
constexpr double shaperDrive=1.4020;
double shaped(double u,double k=shaperDrive) {return std::tanh(k*u)/std::tanh(k);}
double shapedSlope(double u) {
    const double t=std::tanh(shaperDrive*u);
    return shaperDrive*(1-t*t)/std::tanh(shaperDrive);
}
double foldedRamp(double phase,double r) {return phase<r ? 1 : 2*std::abs(1-2*phase)-1;}
// TO reaches STI through 4.7uF NP with 22k to ground (SM2 p.16), so c is the
// 103ms RC low-pass of the triangle, in units of its peak. A probe starts it
// precharged to the triangle's mean, plateau included: integral_0^r 4*phase
// over the zero-mean fold, 2r^2, or for r>=.5, where the plateau covers the
// fold, r plus the rising 4*phase-3 from r to 1. Each probe sample holds the
// triangle for one exact RC step (states[i], before sample i's step); between
// samples c relaxes toward that held value, which makes the continuous
// waveform the checks below integrate. Positions count probe samples from a
// start at phase 0.
struct Coupling {
    static constexpr double tau=22e3*4.7e-6;
    double fs,f,r;
    std::vector<double> states;
    Coupling(double sampleRate,double frequency,double plateau,std::size_t length)
        : fs(sampleRate),f(frequency),r(plateau),states(length+1) {
        states[0]=r<.5 ? 2*r*r : 4*r-1-2*r*r;
        const double step=-std::expm1(-1/(fs*tau));
        for(std::size_t i=0;i<length;++i)states[i+1]=states[i]+step*(triangle(i)-states[i]);
    }
    double triangle(double position) const {
        const double phase=position*f/fs;
        return foldedRamp(phase-std::floor(phase),r);
    }
    double at(double position) const {
        const auto i=std::size_t(position);
        const double held=triangle(i);
        return held+(states[i]-held)*std::exp(-(position-i)/(fs*tau));
    }
    double sine(double position) const {return shaped(triangle(position)-at(position));}
};
template<class F> C simpson(F f,double a,double b,int intervals) {
    const double h=(b-a)/intervals;
    C sum=f(a)+f(b);
    for(int i=1;i<intervals;++i)sum+=(i%2 ? 4.:2.)*f(a+i*h);
    return sum*(h/3);
}
// Continuous Fourier coefficient over the probe's first period of the shaped
// sine with plateau r<.5, by Simpson quadrature on each smooth piece (no
// closed form exists).
C shapedCoefficient(int n,const Coupling& c) {
    const auto coupling=[&c](double phase){return c.at(phase*c.fs/c.f);};
    const auto basis=[n](double phase){return std::polar(1.0,-2*pi*n*phase);};
    const auto plateau=[&](double phase){return shaped(1-coupling(phase))*basis(phase);};
    const auto piece=[&](double phase){return shaped(foldedRamp(phase,c.r)-coupling(phase))*basis(phase);};
    return simpson(plateau,0,c.r,64)+simpson(piece,c.r,.5,1<<14)+simpson(piece,.5,1,1<<14);
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
        const Coupling coupling(fs,f,r,length);
        for(std::size_t i=0;i<length;++i) {
            const double phase=double(i)/length,c=coupling.states[i];
            if(phase>dt && phase<r-dt) {
                check(wave[i].saw==-1,"Shared conditioned plateau before WSC inversion");
                check(std::abs(wave[i].sine-shaped(1-c))<2e-12,"Shared conditioned plateau before WSC folding and coupling");
            }
            if(phase>r+dt && phase<1-dt) {
                check(std::abs(wave[i].saw-(2*phase-1))<2e-12,"Descending current-balance slope must not stretch with reset duration");
                if(std::abs(phase-.5)>dt)
                    check(std::abs(wave[i].sine-shaped(foldedRamp(phase,r)-c))<2e-12,"Sine must shape the fold of the same conditioned branch through its coupling");
            }
            check(std::isfinite(wave[i].saw) && std::abs(wave[i].saw)<1.1,"Bounded conditioned saw");
            check(std::isfinite(wave[i].sine) && std::abs(wave[i].sine)<1.1,"Bounded folded sine");
        }
        check(std::abs(measured(wave,0,false).real()+r*r)<2e-6,"Conditioned saw DC must agree with independent piecewise integral");
        // Shaped sine mean: the continuous waveform's, by quadrature through
        // the coupling, plus each BLAMP's slope change times dt^2/12:
        // -4g'(1-c) at the reset corner, +8g'(-1-c) at the fold and
        // -4g'(1-4r-c) at the plateau step.
        const double cornerSum=-4*shapedSlope(1-coupling.at(0))+8*shapedSlope(-1-coupling.at(.5*length))
            -4*shapedSlope(1-4*r-coupling.at(r*length));
        const double mean=shapedCoefficient(0,coupling).real()+cornerSum*dt*dt/12;
        check(std::abs(measured(wave,0,true).real()-mean)<2e-6,"Shaped sine DC including finite BLAMP areas");
        for(int n=1;n<=16;++n) {
            C expectedSaw=0;
            // Exact sampling identity: the DFT includes all continuous
            // Fourier coefficients at n+kN. The remaining high-frequency
            // curvature/smoothing tails must not be mistaken for a jump
            // correction error or hidden by widening a tolerance.
            for(int alias=-64;alias<=64;++alias) {
                const int m=n+alias*int(length);
                const double sinc=std::sin(pi*m*dt)/(pi*m*dt),t=sinc*sinc;
                expectedSaw+=rawSawCoefficient(m,r)*t;
            }
            check(std::abs(measured(wave,n,false)-expectedSaw)<1e-8,"Saw harmonics must match independent piecewise integral, edge filter and sampling identity");
        }
        // The shaped sine has no closed-form spectrum to alias-sum, so its
        // harmonics are checked against quadrature in wscSineShaper(), at a
        // period long enough for the BLEP/BLAMP kernels to be negligible.
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
    // The comparator reads the conditioned T1 ramp (SM2 p.16 wiring, p.21
    // pin sheet). Its level is crossed at phase d on the ramp, but not while
    // the one-shot holds the plateau: the logical pulse falls at max(d,r).
    // The former fixed-edge reduction asserted that deep PWM ignores the
    // plateau; that contradicted the wiring and is replaced, not loosened.
    {
        constexpr double fs=192000,f=3520;
        const double tau=p.resetMicroseconds*1e-6,r=f*tau,dt=f/fs;
        const auto deep=ceres::oscillatorTrace(fs,f,p,4096);
        std::size_t swallowed=0;
        for(std::size_t i=0;i<deep.size();++i) {
            const double phase=std::fmod(i*dt,1.0);
            const double pwm=std::sin(2*pi*(std::fmod(i*p.pwmRate/fs,1.0)+p.pwmPhase));
            const double d=std::clamp(p.pulseWidth+p.pwmDepth*pwm,.01,.99);
            const double edge=std::max(d,r);
            if(d<r)++swallowed;
            // Two samples of guard either side of each band-limited edge.
            const double near=2.5*dt;
            if(phase<near||phase>1-near||std::abs(phase-edge)<near)continue;
            // Audio pulse is the negated logical comparator output.
            const double logical=phase<edge?1.:-1.;
            check(deep[i].pulse==-logical,"Deep PWM: comparator on the conditioned ramp falls at max(duty, one-shot fraction)");
        }
        check(swallowed>400,"Deep-PWM probe must actually place the threshold above the ramp start");
        auto shortReset=p;shortReset.resetMicroseconds=.1;
        const auto deepShort=ceres::oscillatorTrace(fs,f,shortReset,4096);
        double difference=0;
        for(std::size_t i=0;i<deep.size();++i)difference+=std::abs(deep[i].pulse-deepShort[i].pulse);
        check(difference>10,"A finite one-shot must widen deep-PWM pulses at upper notes");
    }
    // r=f*tau holds up to the CS-50's own top (C6 on 4', PITCH at TU 6V,
    // +14 cents: 6.33kHz, r=.378) and beyond; the old .35 clamp cut it at
    // 5.86kHz. Check the plateau length directly at 6.33kHz and 10kHz.
    for(double f:{4186.*1.5*std::exp2(14/1200.),10000.}) {
        ceres::Patch q;q.pwmDepth=0;q.pulseWidth=.5;
        constexpr double fs=1536000;
        const double r=f*q.resetMicroseconds*1e-6,dt=f/fs;
        check(r>.35&&r<.9,"High-pitch probe must exceed the former reset clamp");
        const auto wave=ceres::oscillatorTrace(fs,f,q,std::size_t(fs/f));
        const Coupling coupling(fs,f,r,wave.size());
        std::size_t plateau=0,ramp=0;
        for(std::size_t i=0;i<wave.size();++i) {
            const double phase=i*dt,c=coupling.states[i];
            if(phase>2*dt&&phase<r-2*dt){++plateau;
                check(wave[i].saw==-1&&std::abs(wave[i].sine-shaped(1-c))<1e-12,"Plateau lasts the physical one-shot fraction f*tau");}
            if(phase>r+2*dt&&phase<1-2*dt){++ramp;check(std::abs(wave[i].saw-(2*phase-1))<1e-12,"Ramp after the physical one-shot keeps its fixed slope");
                // At 10kHz (r>.5) the plateau covers the fold: no fold corner.
                if(std::abs(phase-.5)>2*dt)
                    check(std::abs(wave[i].sine-shaped(foldedRamp(phase,r)-c))<1e-12,"Shaped sine follows the folded ramp after the one-shot");
}
            const double edge=std::max(.5,r);
            if(phase>2*dt&&phase<1-2*dt&&std::abs(phase-edge)>2*dt)
                check(wave[i].pulse==(phase<edge?-1.:1.),"Logical pulse falls at max(50% duty, plateau step)");
        }
        check(plateau>10&&ramp>10,"Both segments sampled");
    }
    for(double fs:{32000.,176400.,192000.,384000.,1536000.})
        for(double f:{27.5,440.,3520.,8372.,20000.,100000.}) {
            const auto wave=ceres::oscillatorTrace(fs,f,p,512,.997);
            for(const auto& x:wave)
                check(std::isfinite(x.saw)&&std::isfinite(x.sine)&&std::isfinite(x.pulse)&&
                    std::abs(x.saw)<2&&std::abs(x.sine)<2&&std::abs(x.pulse)<2,"Extreme CV and rate guards must remain finite/bounded");
        }
}

// The shipping WSC sine (owner listening verdict, 2026-10-20): the textbook
// bipolar differential-pair converter at its own optimum. It is neither a fit
// nor a measurement; this pins its definition, the derivation of k and the
// engine's band-limited rendering of it.
void wscSineShaper() {
    // Odd sine-series harmonics of tanh(k*tri)/tanh(k) for an ideal triangle
    // in sine phase, by quarter-wave symmetry b_n=(4/pi)*int_0^{pi/2}
    // g(2*theta/pi)*sin(n*theta) dtheta (Simpson, 2^14 intervals).
    const auto harmonics=[](double k) {
        std::array<double,200> b{};
        constexpr int intervals=1<<14;
        const double h=(pi/2)/intervals;
        for(int i=0;i<=intervals;++i) {
            const double theta=i*h,weight=(i==0||i==intervals) ? 1 : (i%2 ? 4 : 2);
            const double x=shaped(2*theta/pi,k);
            for(int n=1;n<200;n+=2)b[n]+=weight*x*std::sin(n*theta);
        }
        for(auto& value:b)value*=(4/pi)*h/3;
        return b;
    };
    const auto thd=[&harmonics](double k) {
        const auto b=harmonics(k);
        double energy=0;
        for(int n=3;n<200;n+=2)energy+=b[n]*b[n];
        return std::sqrt(energy)/std::abs(b[1]);
    };
    const auto b=harmonics(shaperDrive);
    const auto db=[&b](int n){return 20*std::log10(std::abs(b[n]/b[1]));};
    std::cout<<"WSC shaper k="<<shaperDrive<<": H3 "<<db(3)<<" dB, H5 "<<db(5)<<" dB, H7 "<<db(7)
             <<" dB, THD "<<100*thd(shaperDrive)<<"%\n";
    check(std::abs(db(3)+54.09)<.05&&std::abs(db(5)+39.01)<.05&&std::abs(db(7)+45.13)<.05,
          "Differential-pair shaper keeps its documented odd harmonics");
    const double best=thd(shaperDrive);
    check(std::abs(best-.0135943)<5e-7,"Documented 1.36% THD for the ideal triangle");
    check(best<thd(shaperDrive-.001)&&best<thd(shaperDrive+.001),"k=1.4020 is the topology's own minimum-THD drive");
    // Engine rendering, plateau and coupling included: over a 20480-sample
    // period the BLEP/BLAMP kernels and aliasing move harmonics n<=9 by
    // O((n/N)^2) of each edge term, about 1e-9.
    ceres::Patch p;p.pwmDepth=0;
    constexpr double fs=1024000,f=50;
    const auto length=std::size_t(fs/f);
    const double r=f*p.resetMicroseconds*1e-6;
    const auto wave=ceres::oscillatorTrace(fs,f,p,length);
    const Coupling coupling(fs,f,r,length);
    for(int n=1;n<=9;++n)
        check(std::abs(measured(wave,n,true)-shapedCoefficient(n,coupling))<1e-8,
              "Rendered shaped sine harmonics must match quadrature of the continuous waveform");
}

// Beside each edge the 2-point BLEP and BLAMP make a sample equal the
// continuous waveform convolved with the unit triangle kernel one sample wide
// each side, whose spectrum is the sinc^2 of the saw check above; that is
// exact on straight pieces. The sine's curved pieces stay unsmoothed, at most
// max|w''|/12 from that convolution: 16*1.709/12/N^2 per sample^2, 3.9e-5 at
// the CS-50's top (N=242.6), where the coupling sets STI 29% of a peak below
// the triangle and so moves every edge's jump and corner slopes.
void wscSineEdges() {
    ceres::Patch p;p.pwmDepth=0;
    constexpr double fs=1536000;
    const double f=4186.*1.5*std::exp2(14/1200.); // C6 on 4', PITCH at 6V, +14 cents
    const double r=f*p.resetMicroseconds*1e-6,period=fs/f;
    const auto length=std::size_t(2*period)+2;
    const auto wave=ceres::oscillatorTrace(fs,f,p,length);
    const Coupling coupling(fs,f,r,length);
    std::vector<double> edges;
    for(double k:{0.,1.})for(double at:{k,k+r,k+.5})edges.push_back(at*period);
    // The kernel's own corner at s=0 and each edge cut the integral into
    // smooth pieces; their ends are pulled in by 1e-9 sample so that a jump is
    // read from the correct side, which costs below 1e-8.
    const auto convolved=[&](double n) {
        std::vector<double> cuts{n-1,n,n+1};
        for(double e:edges)if(e>n-1&&e<n+1)cuts.push_back(e);
        std::sort(cuts.begin(),cuts.end());
        double sum=0;
        for(std::size_t j=1;j<cuts.size();++j)
            sum+=simpson([&](double x){return (1-std::abs(x-n))*coupling.sine(x);},
                         cuts[j-1]+1e-9,cuts[j]-1e-9,64).real();
        return sum;
    };
    std::size_t checked=0;
    double worst=0;
    for(double e:edges) {
        if(e<1||e+2>length)continue;
        double correction=0;
        for(double n:{std::floor(e),std::floor(e)+1}) {
            const double smooth=convolved(n);
            worst=std::max(worst,std::abs(wave[std::size_t(n)].sine-smooth));
            correction=std::max(correction,std::abs(coupling.sine(n)-smooth));
            ++checked;
        }
        check(correction>2e-4,"Each checked edge must move a sample well beyond the tolerance");
    }
    std::cout<<"WSC sine edges: "<<checked<<" samples within "<<worst<<" of the kernel-smoothed waveform\n";
    check(checked==10,"Reset, plateau-step and fold edges of the second period and the first period's step and fold");
    check(worst<5e-5,"Sine BLEP/BLAMP corrections must follow the coupled shaper's jump and corner slopes");
}

// Public CS-50 oscillator demonstration: AutomaticGainsay, d2u1QrIYvnw.
// Comparable solo and both-on examples support constructive saw/pulse mixing:
// solo pulse/saw H1 is about2, while both-on H2/H1 is around-16dB rather
// than the old cancellation model's-6dB. These deliberately broad intervals
// encode that independent mixture behavior, not the oscillator implementation
// or an alleged transistor-level polarity proof. Recording-chain/panel
// uncertainty is documented in Docs/research/accuracy/waveform-mixing-20261019/.
void recordedMixtureRelationship() {
    constexpr double fs=1048576,frequency=256;
    constexpr std::size_t count=4096;
    ceres::Patch p;p.pulseWidth=.5;p.pwmDepth=0;
    const auto trace=ceres::oscillatorTrace(fs,frequency,p,count,.137);
    std::array<C,3> saw{},pulse{},mixed{};
    double sawMean=0,mixedMean=0,sawSquares=0,mixedSquares=0;
    for(std::size_t i=0;i<count;++i) {
        const double s=trace[i].saw,q=trace[i].pulse,m=s+q;
        sawMean+=s;mixedMean+=m;sawSquares+=s*s;mixedSquares+=m*m;
        for(unsigned h=0;h<3;++h) {
            const auto basis=std::polar(1.0,-2*pi*(h+1)*i/count);
            saw[h]+=s*basis;pulse[h]+=q*basis;mixed[h]+=m*basis;
        }
    }
    const double soloRatio=std::abs(pulse[0])/std::abs(saw[0]);
    const double secondDb=20*std::log10(std::abs(mixed[1])/std::abs(mixed[0]));
    const double thirdDb=20*std::log10(std::abs(mixed[2])/std::abs(mixed[0]));
    const double sawVariance=sawSquares/count-std::pow(sawMean/count,2);
    const double mixedVariance=mixedSquares/count-std::pow(mixedMean/count,2);
    const double rmsRatio=std::sqrt(mixedVariance/sawVariance);
    std::cout<<"Raw mixture: pulse/saw H1="<<soloRatio<<", H2/H1="<<secondDb
             <<"dB, H3/H1="<<thirdDb<<"dB, centered RMS ratio="<<rmsRatio<<'\n';
    check(soloRatio>1.9&&soloRatio<2.1,"Equal-Vpp solo sources retain the recorded approximate2:1 fundamental ratio");
    check(secondDb>-17&&secondDb<-14,"Both-on second harmonic must match constructive public-recording relationship");
    check(thirdDb>-11&&thirdDb<-8,"Both-on third harmonic retains the independently observed odd-harmonic ratio");
    check(rmsRatio>2.5&&rmsRatio<2.8,"Both-on RMS must increase with constructive equal-level mixing");
}

// Public CS-80 sample set (Freesound "mogigrumbles"; same M board #21233, so
// the same IG00158; Tools/measure_cs80_wsc_recordings.py): under PWM the
// falling audio pulse edge is fixed and the rising one moves (all 12 PWM
// notes), and the saw rises and drops at its reset (all 11 SAW notes). The
// comparator's fixed edge is the reset, so pulse and saw fall together. This
// checks that relationship on the rendered edges themselves, not through the
// comparator formula, and assumes as the recording does that both takes share
// one chain polarity.
std::vector<double> zeroCrossings(const std::vector<ceres::OscillatorFrame>& trace,bool pulse,bool rising) {
    std::vector<double> times;
    for(std::size_t i=1;i<trace.size();++i) {
        const double a=pulse ? trace[i-1].pulse:trace[i-1].saw,b=pulse ? trace[i].pulse:trace[i].saw;
        if(rising ? (a<0&&b>=0) : (a>0&&b<=0))times.push_back(double(i-1)+a/(a-b));
    }
    return times;
}
double intervalSpread(const std::vector<double>& times) {
    double sum=0,squares=0;
    for(std::size_t i=1;i<times.size();++i) {const double d=times[i]-times[i-1];sum+=d;squares+=d*d;}
    const double n=double(times.size()-1);
    return std::sqrt(std::max(0.,squares/n-std::pow(sum/n,2)));
}
void recordedEdgeRelationship() {
    // An 800-sample period puts every reset at the same sub-sample position
    // (mid-sample), so linear interpolation of the band-limited edges has the
    // same small bias each period and the spreads measure the engine alone.
    constexpr double fs=192000,frequency=240;
    ceres::Patch p;p.pulseWidth=.5;p.pwmDepth=.35;p.pwmRate=3;
    const auto trace=ceres::oscillatorTrace(fs,frequency,p,64000,.25+.5/800);
    const auto drops=zeroCrossings(trace,false,false);
    const auto falls=zeroCrossings(trace,true,false),rises=zeroCrossings(trace,true,true);
    check(drops.size()>70&&falls.size()==drops.size()&&rises.size()+1>=falls.size()&&rises.size()<=falls.size()+1,
          "One audio pulse fall and rise per saw reset");
    double worst=0;
    for(std::size_t i=0;i<falls.size();++i)worst=std::max(worst,std::abs(falls[i]-drops[i]));
    const double fixedSpread=intervalSpread(falls),movingSpread=intervalSpread(rises);
    std::cout<<"CS-80 edge relationship: fall-to-reset "<<worst<<" samples, fall spread "<<fixedSpread
             <<", rise spread "<<movingSpread<<'\n';
    check(worst<.05,"The audio pulse falls with the saw's reset drop, as in the CS-80 PWM and SAW takes");
    check(fixedSpread<.05&&movingSpread>5,"PWM moves only the rising audio pulse edge, as in the CS-80 PWM takes");
}

double productionRms(bool saw,bool pulse) {
    ceres::Synth synth(48000,321);
    ceres::Patch p;p.saw=saw;p.pulse=pulse;p.sine=p.noise=0;
    p.pulseWidth=.5;p.pwmDepth=0;p.keyTrack=0;
    p.hpCutoff=5;p.lpCutoff=20000;p.hpResonance=p.lpResonance=0;
    p.filterInitial=p.filterPeak=0;
    p.ampAttack=p.ampDecay=.001;p.ampSustain=1;
    p.drive=.05;p.volume=.1;
    synth.setPatch(p);synth.noteOn(69);
    for(unsigned i=0;i<48000;++i)check(std::isfinite(synth.process()),"Finite open-filter settling audio");
    double sum=0,squares=0;
    // Exactly44 periods of440Hz. All three runs use the same seeded phase,
    // filter settings, envelope and output gain; enabling both sources retains
    // the physical extra mixer load instead of normalizing it away.
    constexpr unsigned frames=4800;
    for(unsigned i=0;i<frames;++i) {
        const double value=synth.process();
        check(std::isfinite(value),"Finite open-filter production mixture");
        sum+=value;squares+=value*value;
    }
    return std::sqrt(squares/frames-std::pow(sum/frames,2));
}
void productionMixtureRelationship() {
    const double saw=productionRms(true,false),pulse=productionRms(false,true),both=productionRms(true,true);
    const double ratio=both/saw;
    std::cout<<"Production mixture: solo saw="<<saw<<", solo pulse="<<pulse
             <<", both="<<both<<", centered both/saw RMS="<<ratio<<'\n';
    check(saw>0&&pulse>saw,"Solo production sources must remain audible at their physical source levels");
    // Public comparable-note examples are around2.35..2.47. A broad bound
    // accommodates the model's uncalibrated analog transfer while rejecting
    // the former approximately0.89 cancellation. This is not an audio match
    // or a fitted recording gain; no peak/RMS normalization is applied.
    check(ratio>2.2&&ratio<2.6,"Production both-on mix must retain the recording-supported level increase after source loading");
}
}
int main() {
    try {nominalTiming();conditionedWaveforms();extrapolationAndRate();wscSineShaper();wscSineEdges();
        bool failed=false;
        for(const auto run:std::array<void(*)(),3>{{recordedMixtureRelationship,recordedEdgeRelationship,productionMixtureRelationship}})
            try {run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';failed=true;}
        if(failed)return 1;
        std::cout<<"PASS nominal VCO timing, conditioned current slope, WSC shared fold, coupling and differential-pair sine, Fourier/DC/edge, public mixture and CS-80 edge relationships and high-CV bounds\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
