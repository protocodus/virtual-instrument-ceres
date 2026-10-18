#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
void require(bool condition,const std::string& description) {
    if (!condition) throw std::runtime_error(description);
}
std::vector<float> render(ceres::Synth& synth,std::size_t frames) {
    std::vector<float> output(frames);
    for (auto& sample : output) {
        sample=synth.process();
        require(std::isfinite(sample),"Output must be finite");
    }
    return output;
}
double rms(const std::vector<float>& samples) {
    double sum=0;
    for (float x : samples) sum+=x*x;
    return std::sqrt(sum/std::max<std::size_t>(1,samples.size()));
}
ceres::Patch sinePatch() {
    auto p=ceres::preset("init");
    p.saw=p.pulse=p.noise=0; p.sine=1;
    p.ampAttack=.001; p.ampSustain=1; p.ampRelease=.03;
    return p;
}
std::complex<double> response(const std::vector<double>& impulse,double f,double fs) {
    std::complex<double> result=0;
    for (std::size_t i=0;i<impulse.size();++i)
        result+=impulse[i]*std::polar(1.0,-2*pi*f*static_cast<double>(i)/fs);
    return result;
}
std::complex<double> physicalFilter(double f,double rate,double omega,double damping,
    double sourceConductance,double referenceConductance,bool highPass) {
    // Physical source and feedback branch currents, independent of the DSP's
    // canonical state elimination. The external voltage source enters as 2Ys.
    const std::complex<double> s(0,2*rate*std::tan(pi*f/rate));
    const auto ys=sourceConductance*s*1e-6/(sourceConductance+s*1e-6);
    const double gReference=referenceConductance+1/33e3+1/1.2e6;
    const double gmQ=damping*gReference/2-1/1.2e6;
    const auto d=ys+1/33e3+1/1.2e6+s*180e-12;
    const auto n=gmQ+1/1.2e6+s*180e-12;
    const auto denominator=d*(s*s+omega*omega)+2*omega*n*s;
    const auto numerator=highPass ? s*s : std::complex<double>(omega*omega,0);
    return 2.0*ys*numerator/denominator;
}
void parametersAndConstruction() {
    ceres::Patch p;
    require(!ceres::setParameter(p,"bogus",1),"Unknown parameter rejected");
    require(!ceres::setParameter(p,"volume",std::numeric_limits<double>::quiet_NaN()),"NaN rejected");
    require(ceres::setParameter(p,"lp_resonance",2) && p.lpResonance==1,"Range clamps");
    for (double fs : {0.0,7999.0,500000.0,std::numeric_limits<double>::infinity()}) {
        bool threw=false;
        try { ceres::Synth invalid(fs); } catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid sample rate rejected");
    }
    bool threw=false;
    try { (void)ceres::preset("unknown"); } catch (const std::invalid_argument&) { threw=true; }
    require(threw,"Unknown preset rejected");
}
void circuitResponse() {
    constexpr double fs=48000,fc=1000,amplitude=1e-5;
    const double omega=2*fs*std::tan(pi*fc/fs);
    // Longer impulses capture the ~0.9Hz source-capacitor tail. Covers the
    // LP path and all three enabled-waveform HP source-load cases.
    for (int branches : {1,2,3,4})
        for (double resonance : {0.0,.5,1.0}) for (bool highPass : {false,true}) {
            const double gs=branches/150e3;
            const double referenceGs=(highPass ? 3:1)/150e3;
            const auto impulse=ceres::filterImpulse(fs,fc,resonance,highPass,262144,amplitude,gs,referenceGs);
            for (double f : {.3,1.0,5.0,50.0,250.0,1000.0,4000.0,12000.0}) {
                const auto expected=physicalFilter(f,fs,omega,2-1.8*resonance,gs,referenceGs,highPass);
                const auto observed=response(impulse,f,fs)/amplitude;
                require(std::abs(observed-expected)<2e-6,
                        "Coupled source/Q capacitor response differs from physical branch-current equations");
            }
        }
    const auto lp=ceres::filterImpulse(192000,2000,0,false,262144,amplitude);
    const double slope=20*std::log10(std::abs(response(lp,16000,192000)/response(lp,8000,192000)));
    require(slope < -11 && slope > -13,"LP intermediate-band slope must remain near -12 dB/oct");
    const auto lowLp=ceres::filterImpulse(fs,5,1,false,262144,amplitude);
    const auto lowHp=ceres::filterImpulse(fs,5,1,true,262144,amplitude);
    require(std::abs(response(lowLp,0,fs)/amplitude)<1e-6,
            "Physical1uF source capacitor must remove LP DC transmission");
    require(std::abs(response(lowHp,0,fs)/amplitude)<1e-6,
            "Coupled source and integrator paths must preserve HP zero DC gain");
    const double floor=1/(2*pi*1.5e-9*4.7e6);
    require(std::abs(response(lowLp,floor,fs)/amplitude-
        physicalFilter(floor,fs,2*pi*floor,.2,1/150e3,1/150e3,false))<1e-5,
        "Fixed-resistor floor must retain both physical coupling capacitors without artificial gm");
    const auto highQ=ceres::filterImpulse(192000,10000,1,false,262144,amplitude);
    require(std::abs(response(highQ,8000,192000))>std::abs(response(highQ,10000,192000))*1.4,
            "Feedback RC must shift high-frequency resonance below the integrator frequency");
    for (double gs : {1e-320,1e-300,1.0}) {
        const auto impulse=ceres::filterImpulse(fs,fc,.5,false,1024,.001,gs);
        require(std::all_of(impulse.begin(),impulse.end(),[](double x) { return std::isfinite(x); }),
                "Diagnostic source conductance endpoints must avoid intermediate normalization overflow");
    }
    for (double gs : {0.0,-1.0,2.0,std::numeric_limits<double>::infinity()}) {
        bool threw=false;
        try { (void)ceres::filterImpulse(fs,fc,0,false,16,amplitude,gs); }
        catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid source conductance must be rejected by the circuit diagnostic");
        threw=false;
        try { (void)ceres::filterImpulse(fs,fc,0,false,16,amplitude,1/150e3,gs); }
        catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid reference conductance must be rejected by the circuit diagnostic");
    }
}

void physicalCascade() {
    constexpr double fs=96000,hp=300,lp=7000,amplitude=1e-5;
    const double wh=2*fs*std::tan(pi*hp/fs),wl=2*fs*std::tan(pi*lp/fs);
    // Independent expected VCA trim from the conditional broad-pass .8:1
    // target, explicitly downstream of BOTH nonlinear stages.
    const double hpGain=2/(150e3*(3/150e3+1/33e3+1/1.2e6));
    const double lpGain=2/(150e3*(1/150e3+1/33e3+1/1.2e6));
    const double calibration=.8/(hpGain*lpGain);
    for (unsigned branches : {2u,3u,4u}) for (double resonance : {0.0,.5,1.0}) {
        const auto impulse=ceres::filterCascadeImpulse(fs,hp,lp,resonance,.5,
            branches,262144,amplitude,1);
        for (double f : {20.0,100.0,300.0,1000.0,4000.0,8000.0}) {
            const auto expected=physicalFilter(f,fs,wh,2-1.8*resonance,branches/150e3,3/150e3,true)
                *physicalFilter(f,fs,wl,1.1,1/150e3,1/150e3,false)*calibration/double(branches);
            require(std::abs(response(impulse,f,fs)/amplitude-expected)<3e-6,
                "Production cascade must preserve physical mixer gain, fixed Q current and interstage units");
        }
    }
    const auto small=ceres::filterCascadeImpulse(fs,hp,lp,.8,.6,3,8192,.001,1);
    const auto scaled=ceres::filterCascadeImpulse(fs,hp,lp,.8,.6,3,8192,.000125,8);
    for (std::size_t i=0;i<small.size();++i)
        require(std::abs(small[i]-8*scaled[i])<1e-12,
            "Shared voltage scale must preserve equivalent nonlinear trajectories");
    for (unsigned branches : {0u,1u,5u}) {
        bool threw=false;
        try { (void)ceres::filterCascadeImpulse(fs,hp,lp,0,0,branches,16); }
        catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid physical mixer branch count rejected");
    }
    for (double drive : {0.0,std::numeric_limits<double>::denorm_min(),.049,8.01,
                         std::numeric_limits<double>::quiet_NaN()}) {
        bool threw=false;
        try { (void)ceres::filterCascadeImpulse(fs,hp,lp,0,0,3,16,.001,drive); }
        catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid cascade drive rejected without underflowing its shared voltage scale");
    }
}

void fixedConductanceModulation() {
    // Independent electrical identity: gm scales with exp(CV), while the
    // parallel 4.7Mohm input path is fixed. For the same note/oscillator,
    // key modulation must equal a manual change of gm at middle C, even
    // during an envelope sweep. The comparison exercises actual voices.
    constexpr double capacitance=1.5e-9, resistance=4.7e6;
    const double fixedHz=1/(2*pi*capacitance*resistance);
    for (int note : {36,48,60,72,84}) for (bool zeroGm : {false,true}) {
        auto tracking=ceres::preset("init");
        tracking.hpCutoff=zeroGm ? 5 : 30;
        tracking.lpCutoff=zeroGm ? 10 : 100;
        tracking.hpResonance=.4; tracking.lpResonance=.7;
        tracking.filterInitial=-2; tracking.filterPeak=3;
        tracking.filterAttack=.015; tracking.filterDecay=.12;
        tracking.hpEnvelopeScale=.2; tracking.lpEnvelopeScale=.8;
        tracking.drive=.05;
        auto fixed=tracking; fixed.keyTrack=0;
        const double keyRatio=std::pow(2.0,(note-60)/12.0);
        const auto electrical=[&](double totalHz) {
            const double gmAtC=std::max(0.0,2*pi*capacitance*totalHz-1/resistance);
            return (gmAtC*keyRatio+1/resistance)/(2*pi*capacitance);
        };
        fixed.hpCutoff=electrical(tracking.hpCutoff);
        fixed.lpCutoff=electrical(tracking.lpCutoff);
        ceres::Synth modulated(48000,51), equivalent(48000,51);
        modulated.setPatch(tracking); equivalent.setPatch(fixed);
        modulated.noteOn(note); equivalent.noteOn(note);
        const auto a=render(modulated,4096),b=render(equivalent,4096);
        double difference=0;
        for (std::size_t i=0;i<a.size();++i)
            difference=std::max(difference,std::abs(double(a[i])-b[i]));
        require(difference<2e-8,
                "Keyboard and envelope CV must scale gm without modulating the fixed feed resistance");
        if (zeroGm)
            require(std::abs(fixed.hpCutoff-fixedHz)<1e-12,
                    "Zero variable gm must retain the fixed integrator rate at every key CV");
    }
    // An EG voltage shift must be absorbed by the opposite gm calibration
    // shift. Both envelopes have identical span/duration and differ by one
    // volt throughout the attack. This isolates EG from keyboard tracking.
    auto a=ceres::preset("init");
    a.keyTrack=0; a.hpCutoff=30; a.lpCutoff=100;
    a.hpResonance=.4; a.lpResonance=.7; a.drive=.05;
    a.filterInitial=-2; a.filterPeak=2; a.filterAttack=.3;
    a.hpEnvelopeScale=.2; a.lpEnvelopeScale=.8;
    auto b=a; b.filterInitial+=1; b.filterPeak+=1;
    b.hpCutoff=fixedHz+(a.hpCutoff-fixedHz)*std::pow(2.0,-a.hpEnvelopeScale);
    b.lpCutoff=fixedHz+(a.lpCutoff-fixedHz)*std::pow(2.0,-a.lpEnvelopeScale);
    ceres::Synth first(48000,71), shifted(48000,71);
    first.setPatch(a); shifted.setPatch(b);
    first.noteOn(60); shifted.noteOn(60);
    const auto x=render(first,4096), y=render(shifted,4096);
    double difference=0;
    for (std::size_t i=0;i<x.size();++i)
        difference=std::max(difference,std::abs(double(x[i])-y[i]));
    require(difference<2e-8,
            "An EG voltage offset must change only gm, preserving the fixed feed-current path");
}
void deterministicAndReset() {
    ceres::Synth a(48000,123),b(48000,123),different(48000,124);
    auto p=ceres::preset("brass1"); p.ampAttack=.001;
    a.setPatch(p); b.setPatch(p); different.setPatch(p);
    for (int n : {48,55,60,64}) { a.noteOn(n); b.noteOn(n); different.noteOn(n); }
    const auto original=render(a,4096);
    require(original==render(b,4096),"Same seed and events must reproduce exactly");
    require(original!=render(different,4096),"Different free-running phases must affect audio");
    a.reset();
    for (int n : {48,55,60,64}) a.noteOn(n);
    require(original==render(a,4096),"Reset must restore deterministic initial conditions");
}
void masterPitchRouting() {
    auto ranged=ceres::preset("init");
    require(ceres::setParameter(ranged,"tune_cents",-501) && ranged.tuneCents==-500,
            "Master PITCH lower nominal endpoint must clamp to -500 cents");
    require(ceres::setParameter(ranged,"tune_cents",701) && ranged.tuneCents==700,
            "Master PITCH upper nominal endpoint must clamp to +700 cents");
    require(!ceres::setParameter(ranged,"tune_cents",std::numeric_limits<double>::infinity()) &&
            ranged.tuneCents==700,"Nonfinite master tuning must be rejected without mutation");
    constexpr double capacitance=1.5e-9, resistance=4.7e6;
    const double floor=1/(2*pi*capacitance*resistance);
    for (double cents : {-500.0,-100.0,100.0,700.0})
        for (double tracking : {0.0,.5,1.0}) for (int note : {48,60,72})
            for (bool zeroGm : {false,true}) {
                auto tuned=ceres::preset("init");
                tuned.tuneCents=cents; tuned.keyTrack=tracking;
                tuned.hpCutoff=zeroGm ? 5 : 180; tuned.lpCutoff=zeroGm ? 5 : 1600;
                tuned.hpResonance=.4; tuned.lpResonance=.7; tuned.drive=.05;
                tuned.filterInitial=-2; tuned.filterPeak=3; tuned.filterAttack=.015;
                tuned.filterDecay=.12; tuned.hpEnvelopeScale=.2; tuned.lpEnvelopeScale=.8;
                auto equivalent=tuned;
                equivalent.tuneCents=0;
                equivalent.octave=cents/1200; // Separate FT path gives the same VCO pitch.
                const auto electrical=[&](double totalHz) {
                    const double variableGm=std::max(0.0,2*pi*capacitance*totalHz-1/resistance);
                    return (variableGm*std::exp2(cents/1200)+1/resistance)/(2*pi*capacitance);
                };
                equivalent.hpCutoff=electrical(tuned.hpCutoff);
                equivalent.lpCutoff=electrical(tuned.lpCutoff);
                ceres::Synth actual(48000,73),expected(48000,73);
                actual.setPatch(tuned); expected.setPatch(equivalent);
                actual.noteOn(note); expected.noteOn(note);
                const auto a=render(actual,4096),b=render(expected,4096);
                double difference=0;
                for (std::size_t i=0;i<a.size();++i)
                    difference=std::max(difference,std::abs(double(a[i])-b[i]));
                require(difference<2e-8,
                        "Master PITCH must scale shared variable gm independently of engineering keyboard slope");
                if (zeroGm)
                    require(std::abs(equivalent.hpCutoff-floor)<1e-12,
                            "Master PITCH must not change the fixed-resistor integrator floor");
            }
    // The shared noise source removes the oscillator from the signal path,
    // allowing direct observation of which pitch controls enter the filters.
    auto plain=ceres::preset("init");
    plain.saw=plain.pulse=plain.sine=0; plain.noise=.8;
    plain.hpCutoff=200; plain.lpCutoff=1800;
    plain.hpResonance=.7; plain.lpResonance=.5; plain.ampAttack=.001;
    auto local=plain;
    local.octave=1; local.vibratoCents=100; local.pressureVibrato=100;
    local.voiceSpreadCents=30; local.driftCents=20;
    auto master=plain; master.tuneCents=300;
    ceres::Synth reference(48000,90),oscillatorOnly(48000,90),sharedPitch(48000,90);
    reference.setPatch(plain); oscillatorOnly.setPatch(local); sharedPitch.setPatch(master);
    for (auto* synth : {&reference,&oscillatorOnly,&sharedPitch}) {
        synth->pressure(.7); synth->noteOn(60);
    }
    const auto a=render(reference,8192),b=render(oscillatorOnly,8192),c=render(sharedPitch,8192);
    require(a==b,"Transpose, vibrato, drift and spread must not retune the filter path");
    double difference=0;
    for (std::size_t i=0;i<a.size();++i) difference=std::max(difference,std::abs(double(a[i])-c[i]));
    require(difference>1e-4,"Master PITCH must audibly retune the filters even with a noise-only source");
}
void outputCouplingResponse() {
    // Independent bilinear RC impulse solution: h[0]=1/(1+g), then a
    // geometric negative tail. Component values come from the M/PRA sheets.
    const double taus[]={100000*1e-6,127000*1e-6,
                         (1000+4*47000)*4.7e-6,5260*33e-6};
    for (double fs : {100.0,32000.0,192000.0,1536000.0})
        for (unsigned stage=0;stage<4;++stage) {
            const double g=1/(2*fs*taus[stage]),a=(1-g)/(1+g);
            const auto impulse=ceres::outputCouplingImpulse(stage,fs,4096);
            require(std::abs(impulse[0]-1/(1+g))<1e-14,
                    "Physical output coupling must retain positive instantaneous feedthrough");
            for (std::size_t n=1;n<impulse.size();++n) {
                const double expected=-2*g/((1+g)*(1+g))*std::pow(a,double(n-1));
                require(std::abs(impulse[n]-expected)<1e-13,
                        "Output capacitor impulse must match independent RC solution");
            }
        }
    // Long observation at a diagnostic rate covers all four physical decays
    // without assuming a fixed number of host frames is sufficient silence.
    constexpr double fs=1000;
    for (unsigned stage=0;stage<4;++stage) {
        const auto frames=static_cast<std::size_t>(std::ceil(12*taus[stage]*fs));
        const auto impulse=ceres::outputCouplingImpulse(stage,fs,frames);
        double dc=0;
        for (double sample : impulse) dc+=sample;
        require(std::abs(dc)<7e-6,"Each output coupling must reject DC after twelve time constants");
        for (double f : {.1,1.0,10.0,100.0}) {
            const std::complex<double> s(0,2*fs*std::tan(pi*f/fs));
            const auto expected=s*taus[stage]/(1.0+s*taus[stage]);
            require(std::abs(response(impulse,f,fs)-expected)<7e-6,
                    "Output coupling AC gain/phase must follow its physical RC values");
        }
    }
    for (double rate : {0.0,99.0,1536001.0,std::numeric_limits<double>::infinity()}) {
        bool threw=false;
        try { (void)ceres::outputCouplingImpulse(0,rate,16); }
        catch (const std::invalid_argument&) { threw=true; }
        require(threw,"Invalid output coupling probe rate rejected");
    }
    bool threw=false;
    try { (void)ceres::outputCouplingImpulse(4,48000,16); }
    catch (const std::invalid_argument&) { threw=true; }
    require(threw,"Unknown output coupling stage rejected");
}
void outputLifecycleAndPolarity() {
    auto p=sinePatch(); p.saw=1; p.drive=.05; p.volume=.1;
    ceres::Synth held(48000,1),stolen(48000,1);
    for (auto* synth : {&held,&stolen}) {
        synth->setPatch(p);
        for (int i=0;i<4;++i) synth->noteOn(60);
        render(*synth,12000);
    }
    // Retask the oldest physical board to the SAME note, with its amp already
    // at sustain=peak and VCF EG fixed at zero. Only allocation changes.
    stolen.noteOn(60);
    require(render(held,1024)==render(stolen,1024),
            "Voice stealing must preserve physical filter and output capacitor histories");

    p=sinePatch();
    ceres::Synth warm(48000,1),cleared(48000,1);
    for (auto* synth : {&warm,&cleared}) { synth->setPatch(p); render(*synth,24000); }
    cleared.allSoundOff(); // Retains the same oscillator phase, clears capacitors.
    warm.noteOn(60); cleared.noteOn(60);
    const auto a=render(warm,1024),b=render(cleared,1024);
    double difference=0;
    for (std::size_t i=0;i<a.size();++i) difference=std::max(difference,std::abs(double(a[i])-b[i]));
    require(difference>1e-5,"Idle physical boards must continue charging their pre-VCA coupling capacitors");
    warm.allSoundOff();
    require(rms(render(warm,1024))==0,"Panic must clear every output coupling and FIR history exactly");

    // Independent absolute phase reference: first SplitMix64 unit value for
    // seed 1, a cosine WSC sine, and the symmetric FIR's 124-sample delay.
    // The final-of-four output phase is 3, hence 121 internal samples delay.
    // Physical RC phase at 440Hz is small; an omitted line inversion changes
    // this correlation's sign. No old renderer is needed by this native test.
    ceres::Synth sign(48000,1); p.volume=.01; sign.setPatch(p); sign.noteOn(69);
    constexpr std::size_t start=96000;
    render(sign,start);
    const auto audio=render(sign,4800);
    double cross=0,energy=0,referenceEnergy=0;
    for (std::size_t i=0;i<audio.size();++i) {
        const double phase=.5665615751722809+440*(double(start+i)-121.0/4)/48000;
        const double reference=std::cos(2*pi*phase);
        cross+=audio[i]*reference; energy+=audio[i]*audio[i]; referenceEnergy+=reference*reference;
    }
    require(cross/std::sqrt(energy*referenceEnergy)<-.9999,
            "Documented final dry-line inversion must reverse the WSC sine polarity");
}
void allocationAndTails() {
    ceres::Synth s;
    auto p=sinePatch(); s.setPatch(p);
    for (int n : {48,52,55,60,64}) { s.noteOn(n); render(s,64); }
    require(s.activeVoices()==4,"Exactly four voices after fifth note");
    s.noteOff(48); render(s,2000);
    require(s.activeVoices()==4,"Note-off for stolen voice must not release other pitch");
    for (int n : {52,55,60,64}) s.noteOff(n);
    const auto beginning=render(s,1000);
    const auto early=render(s,15000);
    require(rms(beginning)>1e-4,"Release has an audible tail");
    require(s.activeVoices()==0,"Release eventually frees all four voices");
    require(rms(std::vector<float>(early.end()-1000,early.end()))>1e-5,
            "Zero active envelopes must not truncate the physical output bus tail");
    // Observe twelve .8883s bus time constants, preserving the original
    // silence threshold. The old .333s window is insufficient for this RC.
    const auto end=render(s,static_cast<std::size_t>(48000*12*4.7e-6*(1000+4*47000))-15000);
    require(rms(std::vector<float>(end.end()-1000,end.end()))<1e-5,"Output capacitor tail eventually becomes silent");
    s.noteOn(60); s.noteOn(60); s.noteOff(60); render(s,6000);
    require(s.activeVoices()==1,"Repeated note-off releases one matching voice");
    s.noteOff(60); render(s,6000);
    require(s.activeVoices()==0,"Second repeated note-off releases remaining voice");
}
void sustainAndPanic() {
    ceres::Synth a(48000,12),b(48000,12);
    auto p=sinePatch(); p.pressureVolume=.7;
    for (auto* s : {&a,&b}) {
        s->setPatch(p); s->pressure(.65); s->pitchBend(3); s->sustain(true);
        s->noteOn(60); render(*s,2000); s->allNotesOff(); render(*s,6000);
        require(s->activeVoices()==1,"All Notes Off honors pedal");
        s->allSoundOff();
        require(s->activeVoices()==0 && rms(render(*s,128))==0,"All Sound Off immediately silences FIR and voices");
    }
    // Explicitly restore expected controller values only on the reference.
    b.pressure(.65); b.pitchBend(3); b.sustain(true);
    a.noteOn(62); b.noteOn(62);
    require(render(a,2000)==render(b,2000),"All Sound Off preserves pitch and pressure controllers");
    a.noteOff(62); b.noteOff(62);
    render(a,6000); render(b,6000);
    require(a.activeVoices()==1,"All Sound Off preserves pedal controller");
    a.sustain(false); render(a,6000);
    require(a.activeVoices()==0,"Pedal up releases latched voice");
}
void globalReleaseEnvelope() {
    ceres::Patch parameter;
    require(parameter.globalRelease==0,"Global release defaults to no extension");
    for (const auto& name : ceres::presetNames())
        require(ceres::preset(name).globalRelease==0,"Factory presets retain zero global override");
    require(ceres::setParameter(parameter,"global_release",-1) && parameter.globalRelease==0,
            "Global release clamps negative values to bypass");
    require(ceres::setParameter(parameter,"global_release",31) && parameter.globalRelease==30,
            "Global release shares the engineering 30s time bound");
    require(!ceres::setParameter(parameter,"global_release",std::numeric_limits<double>::infinity()) &&
            parameter.globalRelease==30,"Nonfinite global release setter leaves the patch unchanged");
    require(!ceres::setParameter(parameter,"global_release",std::numeric_limits<double>::quiet_NaN()),
            "NaN global release is rejected");
    for (double fs : {48000.,192000.}) for (bool swap : {false,true}) {
        auto base=ceres::preset("init");
        base.ampAttack=base.filterAttack=.01;
        base.ampDecay=.1; base.filterDecay=.13; base.ampSustain=.63;
        base.filterInitial=-1.5; base.filterPeak=2;
        base.ampRelease=swap ? .4 : .04;
        base.filterRelease=swap ? .04 : .4;
        for (double global : {0.,.01,.2,.7}) for (bool filter : {false,true}) {
            auto p=base; p.globalRelease=global;
            const double selected=filter ? p.filterRelease : p.ampRelease;
            const double expectedTime=global>selected ? global : selected;
            const double target=filter ? p.filterInitial : 0;
            const auto off=static_cast<std::size_t>(std::lround(.03*fs));
            const auto n=static_cast<std::size_t>(std::lround(.15*expectedTime*fs));
            const std::vector<ceres::EnvelopeGate> gates={{0,true},{off,false}};
            const auto observed=ceres::envelopeTrace(fs,p,filter,off+n,gates);
            const auto unchanged=ceres::envelopeTrace(fs,base,filter,off+n,gates);
            require(std::equal(observed.begin(),observed.begin()+off,unchanged.begin()),
                    "Global release must not change held-note attack/decay");
            const double expected=target+(observed[off-1]-target)*
                std::exp(-std::log(1000.0)*n/(fs*expectedTime));
            require(std::abs(observed.back()-expected)<2e-11,
                    "Amp/filter release must obey independent RC decay to zero/IL with the selected slower time");
            if (global<=selected)
                require(observed==unchanged,"Shorter global release cannot shorten the selected release");
        }
        auto invalid=base; invalid.globalRelease=std::numeric_limits<double>::quiet_NaN();
        require(ceres::envelopeTrace(fs,invalid,false,4000,{{0,true},{1000,false}})==
                ceres::envelopeTrace(fs,base,false,4000,{{0,true},{1000,false}}),
                "Direct nonfinite Patch global release falls back to zero");
    }
}
void globalReleaseProduction() {
    for (double fs : {48000.,96000.}) {
        auto selected=ceres::preset("flute");
        selected.ampRelease=.03; selected.filterRelease=.3;
        auto global=selected; global.globalRelease=.12;
        auto manual=selected; manual.ampRelease=.12;
        ceres::Synth a(fs,53),b(fs,53),held(fs,53);
        a.setPatch(global); b.setPatch(manual); held.setPatch(selected);
        for (int note : {48,55,60,64}) {
            a.noteOn(note); b.noteOn(note); held.noteOn(note);
            const auto x=render(a,1000);
            require(x==render(b,1000) && x==render(held,1000),
                    "All four held voices must ignore the global release override");
        }
        a.noteOff(48); b.noteOff(48); a.noteOff(60); b.noteOff(60);
        require(render(a,2000)==render(b,2000),
                "Global release audio equals independently extended amp/filter controls");
        // The physical selector can change while a capacitor is discharging.
        // Compare the live global setting with the same piecewise explicit
        // release values, retaining the already charged states in both synths.
        global.globalRelease=.6; manual.ampRelease=manual.filterRelease=.6;
        a.setPatch(global); b.setPatch(manual);
        require(render(a,3000)==render(b,3000),
                "Live extension must preserve active voice/capacitor histories");
        global.globalRelease=.08; manual.ampRelease=.08; manual.filterRelease=.3;
        a.setPatch(global); b.setPatch(manual);
        a.noteOff(55); b.noteOff(55); a.noteOff(64); b.noteOff(64);
        require(render(a,5000)==render(b,5000),
                "Live global shortening restores each selected release independently");
    }
}
void globalReleasePedalLifecycle() {
    auto p=sinePatch(); p.ampRelease=.01; p.globalRelease=.1;
    ceres::Synth pedal(48000,31),reference(48000,31);
    pedal.setPatch(p); reference.setPatch(p);
    pedal.noteOn(60); reference.noteOn(60);
    require(render(pedal,2000)==render(reference,2000),"Pedal test starts from identical state");
    pedal.sustain(true); pedal.noteOff(60); pedal.allNotesOff();
    require(render(pedal,3000)==render(reference,3000),
            "MIDI pedal still holds gates; global release is not pedal hold");
    pedal.sustain(false); reference.noteOff(60);
    require(render(pedal,2000)==render(reference,2000) && pedal.activeVoices()==1,
            "Pedal-up starts the extended release rather than the short selected release");
    render(pedal,16000); render(reference,16000);
    require(pedal.activeVoices()==0 && reference.activeVoices()==0,
            "Global release eventually retires amplitude envelopes without a gate latch");
    p.globalRelease=.2; pedal.setPatch(p); render(pedal,128);
    require(pedal.activeVoices()==0,"Extending global release must not resurrect an Idle envelope");
    pedal.allSoundOff();
    require(rms(render(pedal,128))==0,"Panic clears the extended release and output histories exactly");
    pedal.reset();
    ceres::Synth fresh(48000,31); fresh.setPatch(p);
    pedal.noteOn(62); fresh.noteOn(62);
    require(render(pedal,2000)==render(fresh,2000),"Reset retains patch global release and restores seeded state");
    pedal.noteOff(62); fresh.noteOff(62);
    require(render(pedal,2000)==render(fresh,2000) && pedal.activeVoices()==1,
            "Global patch release persists across panic/reset");
}
void velocityAndRates() {
    auto p=sinePatch();
    ceres::Synth soft(48000,1),hard(48000,1);
    soft.setPatch(p); hard.setPatch(p);
    soft.noteOn(69,.1); hard.noteOn(69,1);
    require(render(soft,2048)==render(hard,2048),"Hardware-default response must ignore velocity");
    for (double fs : {8000.0,44100.0,48000.0,96000.0}) {
        ceres::Synth s(fs,1); s.setPatch(p); s.noteOn(69);
        render(s,static_cast<std::size_t>(fs*.05));
        const auto audio=render(s,static_cast<std::size_t>(fs*.05));
        std::vector<double> crossings;
        for (std::size_t i=1;i<audio.size();++i)
            if (audio[i-1]<=0 && audio[i]>0)
                crossings.push_back(static_cast<double>(i)-audio[i]/(audio[i]-audio[i-1]));
        require(crossings.size()>10,"Sine must contain expected periodic signal");
        const double frequency=fs*(crossings.size()-1)/(crossings.back()-crossings.front());
        require(std::abs(frequency-440)<.15,"Pitch must be independent of host sample rate");
    }
}
void waveformRelationships() {
    // Actual rendered voices, same free-running phase and envelope. Opening
    // the filters makes their remaining 440 Hz phase rotation small. This
    // catches a wrong WSC saw inversion or an unrelated sine phase clock.
    const auto fundamental=[](int wave) {
        ceres::Synth s(48000,321);
        auto p=sinePatch(); p.saw=wave==0; p.pulse=wave==1; p.sine=wave==2;
        p.hpCutoff=5; p.lpCutoff=20000; p.drive=.05; p.volume=.1;
        s.setPatch(p); s.noteOn(69); render(s,4096);
        const auto audio=render(s,4800); // exactly 44 cycles at 440 Hz
        std::complex<double> value=0;
        for (std::size_t i=0;i<audio.size();++i)
            value+=static_cast<double>(audio[i])*std::polar(1.0,-2*pi*440*i/48000);
        return value;
    };
    const auto saw=fundamental(0),pulse=fundamental(1),sine=fundamental(2);
    const auto relativePulse=saw*std::conj(pulse)/(std::abs(saw)*std::abs(pulse));
    const auto relativeSine=saw*std::conj(sine)/(std::abs(saw)*std::abs(sine));
    require(relativePulse.real()<-.99,"WSC inverted saw and comparator pulse fundamentals have opposite polarity");
    require(std::abs(relativeSine.real())<.15 && std::abs(relativeSine.imag())>.98,
            "Folded-triangle sine is approximately in quadrature with saw fundamental");
}
void envelopeAttackShape() {
    constexpr double fs=48000,attack=.12;
    constexpr std::size_t attackFrames=5760;
    for (double headroom : {.0001,.01,.4,1.0,100000.0}) {
        auto p=sinePatch(); p.ampAttack=p.filterAttack=attack;
        p.ampAttackHeadroom=p.filterAttackHeadroom=headroom;
        p.ampSustain=.3; p.filterInitial=-2.5; p.filterPeak=3;
        for (bool filter : {false,true}) {
            const auto trace=ceres::envelopeTrace(fs,p,filter,attackFrames+8,{{0,true}});
            const double initial=filter ? p.filterInitial : 0;
            const double peak=filter ? p.filterPeak : 1;
            // Closed-form circuit solution at elapsed attack fractions,
            // independent of the sample-by-sample implementation.
            for (double fraction : {.25,.5,.75}) {
                const auto frame=static_cast<std::size_t>(attackFrames*fraction)-1;
                const double progress=(1+headroom)*(-std::expm1(-std::log1p(1/headroom)*fraction));
                const double expected=initial+(peak-initial)*progress;
                require(std::abs(trace[frame]-expected)<2e-9,"RC attack must match normalized closed-form shape");
            }
            require(trace[attackFrames-2]<peak,"Attack must not finish before its specified duration");
            require(std::abs(trace[attackFrames-1]-peak)<2e-9,"Headroom must preserve nominal attack duration");
            if (headroom==100000) {
                const double midpoint=(trace[attackFrames/2-1]-initial)/(peak-initial);
                require(std::abs(midpoint-.5)<2e-6,"Large headroom must converge stably to linear attack");
            }
        }
    }
    auto p=sinePatch(); p.ampAttack=.12; p.ampAttackHeadroom=.4;
    p.ampRelease=.2;
    constexpr std::size_t offFrame=1440,retriggerFrame=2880;
    const auto retriggered=ceres::envelopeTrace(fs,p,false,9000,
        {{0,true},{offFrame,false},{retriggerFrame,true}});
    const double heldCharge=retriggered[retriggerFrame-1];
    require(heldCharge>.05,"Retrigger test must retain nontrivial capacitor charge");
    require(retriggered[retriggerFrame]>heldCharge && retriggered[retriggerFrame]-heldCharge<.001,
            "Retrigger must continue from capacitor charge without resetting it");
    const double target=1+p.ampAttackHeadroom;
    const double rate=std::log1p(1/p.ampAttackHeadroom)/p.ampAttack;
    const double expected=target-(target-heldCharge)*std::exp(-rate/fs);
    require(std::abs(retriggered[retriggerFrame]-expected)<1e-12,"Retrigger follows same RC target from stored voltage");
    p.filterInitial=-3;
    const auto initial=ceres::envelopeTrace(fs,p,true,2000,{{1000,true}});
    require(initial[0]==-3 && initial[999]==-3,"Untriggered filter envelope must remain at IL");
    require(initial[1000]>-3 && initial[1000]<-2.9,"Filter first attack must start continuously from IL");
}
void envelopeCurrentLimit() {
    constexpr double fs=48000,attack=.12,headroom=.01;
    constexpr std::size_t attackFrames=5760;
    auto p=sinePatch(); p.ampAttack=p.filterAttack=attack;
    p.ampAttackHeadroom=p.filterAttackHeadroom=headroom;
    p.filterInitial=-2; p.filterPeak=3;
    for (double saturation : {.001,.05,.4,10.0}) {
        p.ampAttackSaturation=p.filterAttackSaturation=saturation;
        for (bool filter : {false,true}) {
            const auto trace=ceres::envelopeTrace(fs,p,filter,attackFrames+8,{{0,true}});
            const double initial=filter ? p.filterInitial : 0;
            const double peak=filter ? p.filterPeak : 1;
            const double span=peak-initial,vs=saturation*span,target=peak+headroom*span;
            require(trace[attackFrames-2]<peak,"Current-limited attack must not finish early");
            require(std::abs(trace[attackFrames-1]-peak)<2e-9,"Current-limit mode must preserve attack duration");
            for (double fraction : {.25,.5,.75}) {
                const auto frame=static_cast<std::size_t>(attackFrames*fraction)-1;
                if (saturation>=.05) {
                    // Direct finite-argument closed form independent of the
                    // stable log-domain recurrence used in the audio core.
                    const double startY=(1+headroom)/saturation;
                    const double endY=headroom/saturation;
                    const double k=std::log(std::sinh(startY)/std::sinh(endY))/attack;
                    const double expected=target-vs*std::asinh(std::sinh(startY)*std::exp(-k*attack*fraction));
                    require(std::abs(trace[frame]-expected)<3e-9,"Current-limit trace must match exact closed form");
                    const double numericalSlope=(trace[frame+1]-trace[frame-1])*fs*.5;
                    const double circuitSlope=k*vs*std::tanh((target-trace[frame])/vs);
                    require(std::abs(numericalSlope-circuitSlope)<2e-5*std::max(1.0,std::abs(circuitSlope)),
                            "Current-limit trace must satisfy differential-pair ODE");
                } else {
                    const double progress=(trace[frame]-initial)/span;
                    require(std::abs(progress-fraction)<1e-6,"Strong current limit must approach linear ramp");
                }
            }
        }
    }
    p.ampAttackSaturation=10;
    const auto weak=ceres::envelopeTrace(fs,p,false,attackFrames,{{0,true}});
    p.ampAttackSaturation=0;
    const auto rc=ceres::envelopeTrace(fs,p,false,attackFrames,{{0,true}});
    double maxDifference=0;
    for (std::size_t i=0;i<rc.size();++i) maxDifference=std::max(maxDifference,std::abs(rc[i]-weak[i]));
    require(maxDifference<.0006,"Weak current saturation must converge toward existing RC law");
    p.ampAttackSaturation=.4; p.ampRelease=.2;
    constexpr std::size_t retriggerFrame=2880;
    const auto retrigger=ceres::envelopeTrace(fs,p,false,9000,{{0,true},{1440,false},{retriggerFrame,true}});
    const double charge=retrigger[retriggerFrame-1];
    const double target=1+headroom,vs=.4;
    const double k=std::log(std::sinh(target/vs)/std::sinh(headroom/vs))/attack;
    const double expected=target-vs*std::asinh(std::sinh((target-charge)/vs)*std::exp(-k/fs));
    require(charge>.05 && std::abs(retrigger[retriggerFrame]-expected)<1e-12,
            "Current-limited retrigger must retain capacitor charge");
    p.ampAttackHeadroom=100000; p.ampAttackSaturation=.001;
    const auto extreme=ceres::envelopeTrace(fs,p,false,attackFrames,{{0,true}});
    require(std::abs(extreme[attackFrames/2-1]-.5)<2e-9 && std::abs(extreme.back()-1)<2e-9,
            "Large target and strong current limit must avoid cancellation");
    require(ceres::setParameter(p,"amp_attack_saturation",.000001) && p.ampAttackSaturation==.001,
            "Positive saturation must clamp to supported minimum");
    require(ceres::setParameter(p,"amp_attack_saturation",0) && p.ampAttackSaturation==0,
            "Zero saturation must retain exact legacy RC mode");
}
void factoryEnvelopeTiming() {
    // Flute CVs independently transcribed from Yamaha's preset table.
    // Verify capacitor trajectories, not only the parameter conversion.
    constexpr double fs=48000;
    const auto p=ceres::preset("flute");
    for (bool filter : {false,true}) {
        const double nominalA=std::pow(10,-.3*(filter ? 3.57 : 3.13));
        const double nominalD=10*std::pow(10,-.3*(filter ? 1.64 : 0.0));
        const double nominalR=10*std::pow(10,-.3*(filter ? 7.35 : 7.19));
        const double initial=filter ? p.filterInitial : 0;
        const double peak=filter ? p.filterPeak : 1;
        const double sustained=filter ? 0 : p.ampSustain;
        const auto attackFrames=static_cast<std::size_t>(std::ceil(nominalA*fs));
        const auto decayFrames=static_cast<std::size_t>(std::lround(nominalD*fs));
        const auto off=attackFrames+decayFrames;
        const auto releaseFrames=static_cast<std::size_t>(std::lround(nominalR*fs));
        const auto trace=ceres::envelopeTrace(fs,p,filter,off+releaseFrames,{{0,true},{off,false}});
        const auto quarter=static_cast<std::size_t>(nominalA*fs*.25);
        const double expected=(1-std::exp(-5.0*quarter/(nominalA*fs)))/(1-std::exp(-5.0));
        require(std::abs((trace[quarter-1]-initial)/(peak-initial)-expected)<1e-10,
                "Factory attack must follow the normalized five-RC trajectory");
        require(trace[attackFrames-2]<peak && std::abs(trace[attackFrames-1]-peak)<1e-10,
                "Factory attack must retain the documented peak time");
        const double decayRatio=(trace[off-1]-sustained)/(peak-sustained);
        require(std::abs(decayRatio-std::exp(-5.0*decayFrames/(nominalD*fs)))<1e-10,
                "Factory decay must leave exp(-5) of its deviation at nominal duration");
        const double releaseRatio=(trace.back()-initial)/(trace[off-1]-initial);
        require(std::abs(releaseRatio-std::exp(-5.0*releaseFrames/(nominalR*fs)))<1e-10,
                "Factory release must follow the same five-RC convention");
    }
    auto manual=ceres::preset("init");
    manual.ampAttack=.001; manual.ampSustain=1; manual.ampRelease=.1;
    require(manual.ampAttackHeadroom==.01 && manual.filterAttackHeadroom==.01,
            "Engineering attack defaults must remain unchanged");
    const auto manualTrace=ceres::envelopeTrace(fs,manual,false,5760,{{0,true},{960,false}});
    require(std::abs(manualTrace.back()/manualTrace[959]-.001)<1e-12,
            "Explicit engineering release must retain its T60 contract");
}
void filterEnvelopeCoupling() {
    const ceres::Patch manual;
    require(manual.hpEnvelopeScale==1 && manual.lpEnvelopeScale==1,
            "Engineering patches must retain direct-octave envelope defaults");
    for (const auto& name : ceres::presetNames()) {
        if (name=="init") continue;
        const auto p=ceres::preset(name);
        const double fixedHz=1/(2*pi*1.5e-9*4.7e6);
        require(p.hpCutoff>fixedHz && p.lpCutoff>fixedHz,
                "Finite factory cutoff CV must retain nonzero gm and respond to envelope modulation");
        // Independent documented resistor ratio relative to the existing
        // provisional manual slope; no claim of absolute cutoff calibration.
        require(std::abs(p.hpEnvelopeScale/.7-39000.0/220000.0)<1e-14,
                "Factory HP envelope must use 39k/220k relative coupling");
        require(std::abs(p.lpEnvelopeScale/.65-39000.0/47000.0)<1e-14,
                "Factory LP envelope must use 39k/47k relative coupling");
    }
    auto p=ceres::preset("init"); p.filterInitial=-2; p.filterPeak=3;
    p.filterAttack=.03; p.pressureFilter=.4; p.filterLfo=.2;
    ceres::Synth inherited(48000,55),explicitScales(48000,55);
    inherited.setPatch(p);
    require(ceres::setParameter(p,"hp_envelope_scale",1) &&
            ceres::setParameter(p,"lp_envelope_scale",1),"Envelope scale parameter names accepted");
    explicitScales.setPatch(p);
    for (auto* s : {&inherited,&explicitScales}) { s->pressure(.6); s->noteOn(57); }
    require(render(inherited,2048)==render(explicitScales,2048),"Explicit unity scales preserve legacy patch behavior");
    p.hpEnvelopeScale=p.lpEnvelopeScale=0;
    auto flat=p; flat.filterInitial=flat.filterPeak=0;
    flat.hpEnvelopeScale=flat.lpEnvelopeScale=1;
    ceres::Synth disconnected(48000,55),zeroEnvelope(48000,55);
    disconnected.setPatch(p); zeroEnvelope.setPatch(flat);
    for (auto* s : {&disconnected,&zeroEnvelope}) { s->pressure(.6); s->noteOn(57); }
    require(render(disconnected,2048)==render(zeroEnvelope,2048),
            "Zero coupling must remove only EG modulation while preserving key/pressure/LFO paths");
}
void pwmSourceBehavior() {
    constexpr double fs=192000,frequency=1000;
    constexpr std::size_t period=192,frames=period*32;
    auto p=ceres::preset("init"); p.pwmDepth=0;
    for (double duty : {.5,.7,.9}) {
        p.pulseWidth=duty; p.resetMicroseconds=.1;
        const auto shortReset=ceres::oscillatorTrace(fs,frequency,p,period*4);
        p.resetMicroseconds=50;
        const auto longReset=ceres::oscillatorTrace(fs,frequency,p,period*4);
        double average=0;
        for (std::size_t i=0;i<period*4;++i) {
            require(shortReset[i].pulse==longReset[i].pulse,
                    "Conditioned WSC pulse edges must not track internal capacitor recharge slope");
            average+=longReset[i].pulse;
        }
        require(std::abs(average/(period*4)-(2*duty-1))<1e-10,
                "Static comparator duty must preserve 50..90 percent range");
        require(shortReset[period-3].saw!=longReset[period-3].saw,
                "Reset-duration stress must actually alter capacitor saw trajectory");
    }
    p.pulseWidth=.6; p.pwmDepth=.35; p.pwmRate=13; p.pwmPhase=.12;
    p.resetMicroseconds=50; // Magnifies the old moving-rising-edge defect.
    const auto modulated=ceres::oscillatorTrace(fs,frequency,p,frames);
    std::vector<double> rising,falling;
    for (std::size_t i=1;i<frames;++i) {
        const double before=modulated[i-1].pulse,after=modulated[i].pulse;
        if (before<=0 && after>0)
            rising.push_back(i-after/(after-before));
        if (before>=0 && after<0)
            falling.push_back(i-after/(after-before));
    }
    require(rising.size()>=31 && falling.size()>=31,"PWM probe must contain repeated transitions");
    for (double edge : rising)
        require(std::abs(edge-std::round(edge/period)*period)<1e-8,
                "Positive pulse edge must remain fixed to VCO period throughout PWM");
    double smallest=period,largest=0;
    for (double edge : falling) {
        const double withinCycle=std::fmod(edge,period);
        smallest=std::min(smallest,withinCycle); largest=std::max(largest,withinCycle);
    }
    require(largest-smallest>30,"PWM must move the falling edge substantially");
    p.pulseWidth=.6; p.pwmDepth=.2; p.pwmRate=.05; p.pwmPhase=0;
    const auto zeroPhase=ceres::oscillatorTrace(fs,frequency,p,period);
    p.pwmPhase=.25;
    const auto quarterPhase=ceres::oscillatorTrace(fs,frequency,p,period);
    require(zeroPhase[125].pulse<-.9 && quarterPhase[125].pulse>.9,
            "PWM phase offset must apply immediately to the sine modulation");
    p.pwmPhase=1;
    const auto wholePhase=ceres::oscillatorTrace(fs,frequency,p,period);
    for (std::size_t i=0;i<period;++i)
        require(std::abs(zeroPhase[i].pulse-wholePhase[i].pulse)<1e-10,
                "PWM phase offset is periodic in one cycle");
    require(ceres::setParameter(p,"pwm_phase",.5090477) && p.pwmPhase==.5090477,
            "Recorded PWM phase alignment must be an accepted patch parameter");
    require(ceres::setParameter(p,"pwm_rate",222) && p.pwmRate==222,
            "PWM must support documented 222Hz endpoint");
    const auto funky=ceres::preset("funky1");
    const double expected=.2*std::exp(std::log(222/.2)*2.33/10);
    require(std::abs(funky.pwmRate-expected)<1e-12,
            "Factory PWM rate must use sourced exponential VC1 calibration");
    require(std::abs(ceres::preset("electric_piano").pwmRate-.2)<1e-14,
            "Zero PWM speed CV must map to 0.2Hz");
}
void finiteAtExtremes() {
    ceres::Synth s(8000,987);
    auto p=ceres::preset("init");
    p.hpResonance=p.lpResonance=1; p.hpCutoff=19000; p.lpCutoff=20000;
    p.filterInitial=-12; p.filterPeak=12; p.filterAttack=.0001;
    p.ampAttack=.0001; p.drive=8; p.saw=p.pulse=p.sine=p.noise=1;
    p.pulseWidth=.9; p.pwmDepth=.4; p.pwmRate=50;
    s.setPatch(p); s.pitchBend(48);
    double peak=0;
    for (int i=0;i<16000;++i) {
        if (i%317==0) s.noteOn((i/317*13)%128);
        const double sample=s.process();
        require(std::isfinite(sample),"Extreme modulation must stay finite");
        peak=std::max(peak,std::abs(sample));
    }
    require(peak<2,"Physical output rail remains bounded");
    for (const auto& name : ceres::presetNames()) {
        s.setPatch(ceres::preset(name)); s.allSoundOff(); s.noteOn(60);
        render(s,256);
    }
}
}

int main() {
    try {
        parametersAndConstruction(); std::cout<<"PASS parameter ranges and constructor validation\n";
        circuitResponse(); std::cout<<"PASS physical source/Q capacitor transfer, four source loads, DC rejection, Q and slope\n";
        physicalCascade(); std::cout<<"PASS production cascade physical voltages, passive mixer, fixed Q current and VCA calibration\n";
        fixedConductanceModulation(); std::cout<<"PASS keyboard/envelope gm scaling with fixed feed conductance\n";
        masterPitchRouting(); std::cout<<"PASS shared master PITCH, independent oscillator routes, fixed feed floor and nominal range\n";
        deterministicAndReset(); std::cout<<"PASS deterministic seed and reset\n";
        outputCouplingResponse(); std::cout<<"PASS four output coupling RC transfers, decay and diagnostic validation\n";
        outputLifecycleAndPolarity(); std::cout<<"PASS output capacitor allocation/idle/panic lifecycle and dry-line polarity\n";
        allocationAndTails(); std::cout<<"PASS four voices, stealing, repeated notes and release tails\n";
        sustainAndPanic(); std::cout<<"PASS sustain, CC123 behavior and controller-preserving panic\n";
        globalReleaseEnvelope(); std::cout<<"PASS shared global release bounds, independent RC trajectories and held-note invariance\n";
        globalReleaseProduction(); std::cout<<"PASS global release production equivalence and live capacitor-state continuity\n";
        globalReleasePedalLifecycle(); std::cout<<"PASS global release versus MIDI pedal, panic and reset lifecycle\n";
        velocityAndRates(); std::cout<<"PASS hardware velocity behavior and sample-rate pitch invariance\n";
        waveformRelationships(); std::cout<<"PASS WSC saw/pulse polarity and folded-sine phase\n";
        envelopeAttackShape(); std::cout<<"PASS RC attack curvature, fixed duration, linear limit and retrigger charge\n";
        envelopeCurrentLimit(); std::cout<<"PASS optional saturating-current attack ODE, exact solution, limits and retrigger\n";
        factoryEnvelopeTiming(); std::cout<<"PASS factory five-RC envelope trajectories and engineering T60 compatibility\n";
        filterEnvelopeCoupling(); std::cout<<"PASS factory HP/LP envelope resistor ratios and engineering-patch compatibility\n";
        pwmSourceBehavior(); std::cout<<"PASS fixed PWM reset edge, duty, phase offset and sourced speed calibration\n";
        finiteAtExtremes(); std::cout<<"PASS extreme finite output and all factory presets\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;
    }
}
