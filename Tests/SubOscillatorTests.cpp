// SUB-OSCILLATOR lever laws and source shape (guide printed pp.27, 49, 55;
// SM1 printed p.14 R7; SM2 printed pp.16, 27), and the EXT IN path (guide
// printed p.56; SM2 printed pp.8, 22, 27). Expected values are solved here
// from the schematic as resistor networks, RC circuits and printed
// calibration points, not from the engine's own helper formulas.
#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
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
    // Identical, drift-free boards whatever the defaults, so pitch checks see
    // the SUB alone.
    p.driftCents=p.voiceSpreadCents=0;
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
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

// SUB-EXT, FUNCTION EXT (lfo_waveform 5). The jack signal passes the LEVEL
// ratio, the non-inverting preamp's 1+100k/1k, a 10uF capacitor into EXO's
// 100k (with SUI's 100k in parallel while EXT is selected), and the two
// NJM4558 output swings, datasheet typicals and so an ESTIMATE: +/-14 V into
// the preamp's >=10k load, +/-13 V into the levers' >=2k. The preamp slews at
// the datasheet's typical 1 V/us, an ESTIMATE too. One SUB unit is the
// internal sources' 1.5 V peak at SUI.
constexpr double pi=3.14159265358979323846;
constexpr double subPeakVolts=1.5;
constexpr double extPreampGain=1+100e3/1e3;
constexpr double extTauSelected=10e-6*(100e3*100e3/(100e3+100e3));
constexpr double extTauOpen=10e-6*100e3;
constexpr double extPreampSwing=14/subPeakVolts, extFollowerSwing=13/subPeakVolts;
constexpr double extSlewPerSecond=1e6/subPeakVolts;

ceres::Synth configured(double fs,unsigned factor,const ceres::Patch& p,std::uint64_t seed=5) {
    ceres::Synth s(fs,seed);
    require(s.configure(4,factor),"EXT engine configuration");
    s.setPatch(p);
    return s;
}
ceres::Patch externalPatch() {auto p=quiet();p.lfoWaveform=5;return p;}
// Every SUB route open at once: VCO, VCF and VCA levers, touch vibrato and
// touch wah, with touch BRILLIANCE and LEVEL beside them.
ceres::Patch routedPatch() {
    auto p=quiet();p.lfoRate=7;p.vibratoCents=25;p.filterLfo=.8;p.tremolo=.7;p.ampLevel=.6;
    p.pressureVibrato=20;p.pressureWah=.5;p.pressureFilter=.3;p.pressureVolume=.4;
    p.lpCutoff=1500;p.lpResonance=.5;
    return p;
}
// Audio-rate, overdriven, DC and invalid jack samples.
double wildJack(unsigned n) {
    switch(n%7) {
    case 0: return std::numeric_limits<double>::quiet_NaN();
    case 1: return 1e12;
    case 2: return -std::numeric_limits<double>::infinity();
    case 3: return 3*std::sin(n*.37);
    default: return n%500<250 ? 40. : -.5;
    }
}

void externalSelector() {
    ceres::Patch p;
    require(p.extLevel==1,"EXT LEVEL defaults fully open");
    require(ceres::setParameter(p,"lfo_waveform",5) && p.lfoWaveform==5,"FUNCTION EXT is position 5");
    require(ceres::setParameter(p,"lfo_waveform",4.6) && p.lfoWaveform==5,"FUNCTION rounds up to EXT");
    require(ceres::setParameter(p,"lfo_waveform",4.4) && p.lfoWaveform==4,"FUNCTION rounds down to noise");
    require(ceres::setParameter(p,"lfo_waveform",9) && p.lfoWaveform==5,"FUNCTION has six positions");
    require(ceres::setParameter(p,"ext_level",.3) && p.extLevel==.3,"EXT LEVEL is the wiper ratio");
    require(ceres::setParameter(p,"ext_level",-2) && p.extLevel==0,"EXT LEVEL closes at 0");
    require(ceres::setParameter(p,"ext_level",7) && p.extLevel==1,"EXT LEVEL opens at 1");
    require(!ceres::setParameter(p,"ext_level",std::numeric_limits<double>::quiet_NaN()),
            "Nonfinite EXT LEVEL rejected");
    near(ceres::subExternalUnitVolts*extPreampGain,subPeakVolts,1e-15,
         "One jack unit through the preamp is the SUB peak at SUI");
}

void externalPassband() {
    // A 200 Hz sine from t=0 into an uncharged capacitor: the RC high-pass
    // solution, transient included, at every host instant and quality.
    struct Case {double fs;unsigned factor;};
    for(const Case c:{Case{48000,1},Case{48000,2},Case{48000,4},Case{44100,1},Case{96000,4}}) {
        auto p=externalPatch();p.extLevel=.75;
        auto s=configured(c.fs,c.factor,p);
        const double amplitude=.8,w=2*pi*200,wt=w*extTauSelected;
        for(unsigned n=0;n<static_cast<unsigned>(c.fs/2);++n) {
            const double t=n/c.fs;
            s.externalInput(amplitude*std::sin(w*t));s.process();
            near(s.modulationState().sub,p.extLevel*amplitude*wt/(1+wt*wt)
                 *(wt*std::sin(w*t)+std::cos(w*t)-std::exp(-t/extTauSelected)),1e-6,
                 "EXT passband: LEVEL times the jack in SUB units, after the .318 Hz coupling");
        }
    }
}

void externalCoupling() {
    for(unsigned factor:{1u,4u}) {
        // A held DC step, fed once. The host step is centred half a host
        // period before the sample it lands on, so the charge decays from there.
        const double fs=48000,T=1/fs;
        auto s=configured(fs,factor,externalPatch());s.externalInput(.5);
        for(unsigned n=0;n<48000;++n) {
            s.process();
            near(s.modulationState().sub,.5*std::exp(-(n+.5)*T/extTauSelected),1e-9,
                 "EXT selected: 10uF into 100k||100k");
        }
        // FUNCTION elsewhere: EXO's own 100k charges the capacitor; selecting
        // EXT carries that charge on, with SUI's 100k added. The switch lands
        // between two host samples: either end moves the result under 2e-6.
        auto p=quiet();p.lfoWaveform=0;
        auto t=configured(fs,factor,p);t.externalInput(.5);
        constexpr unsigned open=24000;
        for(unsigned n=0;n<open;++n)t.process();
        p.lfoWaveform=5;t.setPatch(p);
        const double switched=(open-.5)*T,atSwitch=.5*std::exp(-(switched+T/2)/extTauOpen);
        for(unsigned n=open;n<open+24000;++n) {
            t.process();
            near(t.modulationState().sub,atSwitch*std::exp(-(n*T-switched)/extTauSelected),1e-5,
                 "FUNCTION carries the EXT capacitor charge across the switch");
        }
    }
}

void externalSwings() {
    // Overdrive: the preamp clips at its swing, the capacitor passes that
    // step, and the IC1-1 follower clips it again until it decays below its
    // own swing. LEVEL is ahead of the preamp, so it sets the headroom.
    struct Case {double level,input,start;};
    for(const double fs:{44100.,48000.})
    for(const Case c:{Case{1,20,extPreampSwing},Case{1,-20,-extPreampSwing},
                      Case{.25,40,extPreampSwing},Case{.2,40,8}}) {
        auto p=externalPatch();p.extLevel=c.level;
        auto s=configured(fs,1,p);s.externalInput(c.input);
        for(unsigned n=0;n<static_cast<unsigned>(fs/4);++n) {
            s.process();
            const double decayed=c.start*std::exp(-(n+.5)/fs/extTauSelected);
            near(s.modulationState().sub,std::max(-extFollowerSwing,std::min(extFollowerSwing,decayed)),1e-8,
                 "EXT preamp and follower swings");
        }
    }
    // At 4x the interpolated host step clips the same way.
    auto s=configured(48000,4,externalPatch());s.externalInput(20);
    for(unsigned n=0;n<2000;++n) {
        s.process();const double sub=s.modulationState().sub;
        if(n<1700)require(sub==extFollowerSwing,"4x EXT overdrive holds the follower swing");
        if(n>=1800)require(sub<extFollowerSwing,"4x EXT overdrive decays below the follower swing");
    }
}

void externalSlew() {
    // An overdriven square: the preamp sits on one swing, then the jack flips.
    // A full swing (28 V) takes 28 us, longer than a 48 kHz host period, so the
    // edge reaches SUI as a ramp. Held 10 ms first, the capacitor has decayed
    // little and the follower clips the held level, not the ramp.
    const double fs=48000,T=1/fs;constexpr unsigned held=480;
    {
        auto s=configured(fs,1,externalPatch());s.externalInput(50);
        for(unsigned n=0;n<held;++n)s.process();
        s.externalInput(-50);s.process();
        // One host period of slew from +14 V, then the RC step response.
        const double ramp=extSlewPerSecond*T;
        near(s.modulationState().sub,extPreampSwing*std::exp(-(held+.5)*T/extTauSelected)
             -ramp*std::exp(-.5*T/extTauSelected),1e-8,"EXT preamp slews 1 V/us at 1x");
        s.process();
        near(s.modulationState().sub,-extFollowerSwing,0,"The second host period completes the swing");
    }
    {
        // At 4x the host step is interpolated across four internal steps: the
        // targets are 25, 0, -25 and -50, so the preamp leaves its swing at the
        // second step and slews for three. The capacitor decays 4e-4 in that time.
        auto s=configured(fs,4,externalPatch());s.externalInput(50);
        for(unsigned n=0;n<held;++n)s.process();
        const double before=s.modulationState().sub;
        s.externalInput(-50);s.process();
        const double unclipped=extPreampSwing*std::exp(-(held-.5)*T/extTauSelected);
        require(before==extFollowerSwing && unclipped>extFollowerSwing,"The held swing clips at the follower");
        near(s.modulationState().sub-unclipped,-3*extSlewPerSecond*T/4,5e-4,"EXT preamp slews 1 V/us at 4x");
    }
}

void externalInert() {
    // The internal FUNCTION positions never hear EXT IN, however wild, on any
    // route: fed and unfed engines stay bit-identical.
    for(unsigned factor:{1u,4u})for(unsigned w=0;w<5;++w) {
        auto p=routedPatch();p.lfoWaveform=w;
        auto fed=configured(48000,factor,p,9),plain=configured(48000,factor,p,9);
        fed.pressure(.6);plain.pressure(.6);fed.noteOn(57,.8);plain.noteOn(57,.8);
        for(unsigned n=0;n<4000;++n) {
            fed.externalInput(wildJack(n));
            const float a=fed.process(),b=plain.process();
            require(a==b,"Internal SUB sources ignore EXT IN (audio)");
            require(fed.modulationState().sub==plain.modulationState().sub,"Internal SUB sources ignore EXT IN");
        }
    }
    // EXT selected with nothing at the jack: SUI is exactly 0, so every route
    // adds nothing, exactly as if its depth were 0.
    for(unsigned factor:{1u,4u}) {
        auto p=routedPatch();p.lfoWaveform=5;
        auto depthless=p;depthless.vibratoCents=depthless.filterLfo=depthless.tremolo=depthless.pressureWah=0;
        auto a=configured(48000,factor,p,9),b=configured(48000,factor,depthless,9);
        a.pressure(.6);b.pressure(.6);a.noteOn(57,.8);b.noteOn(57,.8);
        for(unsigned n=0;n<4000;++n) {
            const float x=a.process(),y=b.process();
            require(a.modulationState().sub==0,"Unfed EXT is silent");
            require(x==y,"Unfed EXT moves no route");
        }
    }
}

void externalRoutes() {
    // The same SUI signal from EXT and from an internal source moves every
    // route identically. The internal engine's SUB is turned into the jack
    // signal that reproduces it after the selected coupling, by inverting the
    // trapezoidal RC high-pass at 1x (internal rate = host rate).
    const double fs=48000,g=1/(2*fs*extTauSelected);
    for(unsigned route=0;route<5;++route) {
        auto p=quiet();p.lfoRate=7;p.lfoWaveform=route==1 ? 1:0;
        p.lpCutoff=1000;
        if(route==0)p.vibratoCents=25;
        if(route==1)p.filterLfo=1;
        if(route==2){p.tremolo=1;p.ampLevel=.6;}
        if(route==3)p.pressureVibrato=30;
        if(route==4)p.pressureWah=.8;
        auto q=p;q.lfoWaveform=5;q.extLevel=.8;
        auto internal=configured(fs,1,p,13),external=configured(fs,1,q,13);
        internal.pressure(.7);external.pressure(.7);internal.noteOn(57,.9);external.noteOn(57,.9);
        double memory=0,low=1e9,high=-1e9;
        for(unsigned n=0;n<9600;++n) {
            const float a=internal.process();
            const double target=internal.modulationState().sub;
            const double jack=target*(1+g)+memory;memory+=2*g*target;
            external.externalInput(jack/q.extLevel);
            const float b=external.process();
            near(external.modulationState().sub,target,1e-12,"EXT reproduces the internal SUI signal");
            const auto va=internal.voiceState(),vb=external.voiceState();
            for(unsigned v=0;v<va.size();++v) {
                near(vb[v].oscillatorFrequency/va[v].oscillatorFrequency,1,1e-12,"EXT VCO route");
                near(vb[v].lpFrequency/va[v].lpFrequency,1,1e-12,"EXT VCF route (LP)");
                near(vb[v].hpFrequency/va[v].hpFrequency,1,1e-12,"EXT VCF route (HP)");
            }
            near(external.soundPathState().commonVcaGain,internal.soundPathState().commonVcaGain,1e-12,
                 "EXT VCA route");
            near(b,a,1e-6,"EXT audio matches the internal source");
            const auto& voice=vb[0];
            const double measure=route==2 ? external.soundPathState().commonVcaGain
                : route==1 || route==4 ? std::log2(voice.lpFrequency) : 1200*std::log2(voice.oscillatorFrequency);
            low=std::min(low,measure);high=std::max(high,measure);
        }
        const double spread[]={40,1.5,.5,30,.8}; // cents, octaves, gain, cents, octaves
        require(high-low>spread[route],"Each route is driven through EXT");
    }
}

void externalHousekeeping() {
    for(unsigned factor:{1u,4u}) {
        auto p=routedPatch();p.lfoWaveform=5;
        // Nonfinite jack samples read as 0, huge finite ones as the 1e9
        // guard, and a nonfinite LEVEL in a direct Patch as its default.
        auto bad=p;bad.extLevel=std::numeric_limits<double>::quiet_NaN();
        auto a=configured(48000,factor,bad,21),b=configured(48000,factor,p,21);
        a.noteOn(60);b.noteOn(60);
        for(unsigned n=0;n<3000;++n) {
            const double x=2*std::sin(n*.01);
            switch(n%11) {
            case 3: a.externalInput(std::numeric_limits<double>::quiet_NaN());b.externalInput(0);break;
            case 5: a.externalInput(std::numeric_limits<double>::infinity());b.externalInput(0);break;
            case 7: a.externalInput(1e300);b.externalInput(1e9);break;
            case 9: a.externalInput(-1e300);b.externalInput(-1e9);break;
            default: a.externalInput(x);b.externalInput(x);
            }
            const float y=a.process(),z=b.process();
            require(y==z && a.modulationState().sub==b.modulationState().sub,
                    "Invalid and huge EXT IN samples are guarded");
        }
        // reset() clears the jack and the capacitor; allSoundOff() keeps them,
        // as it keeps the SUB clock: the modulation runs on through a panic.
        auto fresh=configured(48000,factor,p,21),kept=configured(48000,factor,p,21);
        b.reset();b.process();fresh.process();kept.process();
        require(b.modulationState().sub==0,"reset() clears EXT IN and its capacitor");
        for(unsigned n=0;n<3000;++n) {
            const double x=1.5*std::sin(n*.02)+(n>1000 ? .4:0);
            if(n==1500)kept.allSoundOff();
            b.externalInput(x);fresh.externalInput(x);kept.externalInput(x);
            b.process();fresh.process();kept.process();
            require(b.modulationState().sub==fresh.modulationState().sub,"reset() restores the EXT chain");
            require(kept.modulationState().sub==fresh.modulationState().sub,"allSoundOff() keeps the EXT chain");
        }
    }
}
} // namespace

int main() {
    try {
        speedCalibration();depthLevers();sourceShapes();vibratoThroughEngine();
        externalSelector();externalPassband();externalCoupling();externalSwings();externalSlew();
        externalInert();externalRoutes();externalHousekeeping();
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
    std::puts("SUB oscillator contracts passed");
    return 0;
}
