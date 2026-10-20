#include "ceres.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
// R7 TBR: 47k LP, 150k HP with the .7/.65 manual slopes (SM1 p.14).
constexpr double hpTouch=(.7/.65)*(47./150.);
// Touch level amplifier I centres the 3Vpp SUB on +.5V (guide p.56).
constexpr double vibratoBias=1./3;
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<"actual="<<a<<" expected="<<b<<'\n';throw std::runtime_error(why);
    }
}
ceres::Patch patch() {
    ceres::Patch p;p.expressionSource=1;p.pressureFilter=1.3;p.pressureVibrato=80;
    p.pressureVolume=.8;p.filterInitial=p.filterPeak=0;p.hpCutoff=90;p.lpCutoff=900;
    p.ampAttack=.001;p.ampSustain=1;p.ampRelease=10;p.lfoRate=7;
    // Nominal boards, no drift: the expected pitch and cutoffs are the
    // design laws.
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    p.driftCents=0;
    return p;
}
void advance(ceres::Synth& s,unsigned n=16) {
    for(unsigned i=0;i<n;++i)require(std::isfinite(s.process()),"Finite expression audio");
}
void checkVoice(const ceres::Synth& s,unsigned board,double velocity,const ceres::Patch& p) {
    const auto v=s.voiceState()[board];
    const double key=(v.keyNote-60)*p.keyTrack/12+p.tuneCents/1200;
    near(v.hpFrequency,floorHz+(p.hpCutoff-floorHz)*std::exp2(key+velocity*p.pressureFilter*hpTouch),1e-10,
         "HP tracks the board's latched expression above the fixed gm floor");
    near(v.lpFrequency,floorHz+(p.lpCutoff-floorHz)*std::exp2(key+velocity*p.pressureFilter),1e-9,
         "LP tracks the board's latched expression above the fixed gm floor");
    const double cents=s.modulationState().sub*(p.vibratoCents+velocity*p.pressureVibrato)
        +velocity*p.pressureVibrato*vibratoBias+p.tuneCents;
    near(v.oscillatorFrequency,440*std::exp2((v.keyNote-69+12*p.octave+cents*.01)/12),1e-10,
         "SUB vibrato uses each board's expression independently");
}
void rangeContract() {
    ceres::Patch p;require(p.expressionSource==0,"Legacy aftertouch remains default");
    require(ceres::setParameter(p,"expression_source",.49)&&p.expressionSource==0,"Selector rounds down");
    require(ceres::setParameter(p,"expression_source",.5)&&p.expressionSource==1,"Selector midpoint rounds up");
    require(ceres::setParameter(p,"expression_source",2)&&p.expressionSource==1,"Selector clamps upper bound");
    require(ceres::setParameter(p,"expression_source",-1)&&p.expressionSource==0,"Selector clamps lower bound");
    require(!ceres::setParameter(p,"expression_source",std::numeric_limits<double>::infinity()),"Reject nonfinite source");
    p.expressionSource=std::numeric_limits<double>::quiet_NaN();
    ceres::Synth a(8000,1),b(8000,1);a.setPatch(p);a.noteOn(60,.3);b.noteOn(60,.3);
    for(unsigned i=0;i<500;++i)require(a.process()==b.process(),"Direct invalid source restores legacy default");
    auto rounded=patch();rounded.expressionSource=.7;a.setPatch(rounded);a.reset();a.noteOn(60,.3);advance(a);
    checkVoice(a,0,.3,rounded);
}
void polyphonyIsolation() {
    const auto p=patch();const std::array<int,4> notes={48,55,60,67};
    const std::array<double,4> velocities={.15,.4,.7,1};
    ceres::Synth a(8000,3),b(8000,3);a.setPatch(p);b.setPatch(p);
    for(unsigned i=0;i<4;++i){a.noteOn(notes[i],velocities[i]);b.noteOn(notes[i],i==1?.9:velocities[i]);}
    a.pressure(.1);b.pressure(.9);
    double audioDifference=0;
    for(unsigned n=0;n<800;++n) {
        const double d=a.process()-b.process();audioDifference+=d*d;
        for(unsigned i=0;i<4;++i) {
            checkVoice(a,i,velocities[i],p);checkVoice(b,i,i==1?.9:velocities[i],p);
            if(i!=1) {
                const auto x=a.voiceState()[i],y=b.voiceState()[i];
                require(x.oscillatorPhase==y.oscillatorPhase && x.hpFrequency==y.hpFrequency &&
                        x.lpFrequency==y.lpFrequency && x.ampEnvelope==y.ampEnvelope,
                        "One note's velocity must not change another board's oscillator/filter/envelope");
            }
        }
    }
    require(audioDifference>1e-5,"Mixed-velocity difference reaches actual output");
}
void lifecycle() {
    auto p=patch();ceres::Synth s(8000,4);s.setPatch(p);
    s.noteOn(60,.2);s.noteOn(60,.8);advance(s);
    s.noteOff(60);advance(s);checkVoice(s,0,.2,p);checkVoice(s,1,.8,p);
    require(!s.voiceState()[0].held && s.voiceState()[1].held,"Repeated-note FIFO release");
    s.sustain(true);s.noteOff(60);advance(s);checkVoice(s,1,.8,p);
    require(s.voiceState()[1].pedalHeld,"MIDI pedal retains note's velocity");
    p.sustainMode=2;s.setPatch(p);s.noteOn(72,.45);advance(s);
    require(s.voiceState()[0].keyNote==72 && s.voiceState()[1].keyNote==60,"II follows only free key voltages");
    checkVoice(s,0,.2,p);checkVoice(s,1,.8,p);checkVoice(s,2,.45,p);
    s.sustain(false);advance(s);checkVoice(s,1,.8,p);
    require(s.voiceState()[1].keyNote==72,"Pedal-up changes key CV without copying new key velocity");
    s.noteOn(72,0);advance(s);checkVoice(s,2,.45,p);
    require(!s.voiceState()[2].held,"Zero-velocity noteOn is noteOff, not replacement velocity");
    // A fifth key while all four are held is not stored. Re-keying an
    // audible released board retargets only its velocity. The consumed CV
    // follows the 5ms digital controller RC; capacitors are not restarted.
    s.reset();s.setPatch(p);
    for(unsigned i=0;i<4;++i)s.noteOn(48+int(i)*4,.2+.15*i);
    advance(s);s.noteOn(80,.95);
    for(unsigned i=0;i<4;++i)require(s.voiceState()[i].note==48+int(i)*4,"Fifth held key is not stored");
    s.noteOff(80);s.noteOff(48);const auto old=s.voiceState();s.noteOn(80,.95);
    const auto assigned=s.voiceState();
    require(assigned[0].note==80 && assigned[0].oscillatorPhase==old[0].oscillatorPhase &&
            assigned[0].ampEnvelope==old[0].ampEnvelope,"Reuse assigns velocity without clearing capacitor/phase state");
    advance(s);checkVoice(s,0,.95+(.2-.95)*std::exp(-16/(8000.*.005)),p);
    for(unsigned i=1;i<4;++i)checkVoice(s,i,.2+.15*i,p);
    s.allNotesOff();advance(s);checkVoice(s,0,.95+(.2-.95)*std::exp(-32/(8000.*.005)),p);
    s.allSoundOff();for(unsigned i=0;i<100;++i)require(s.process()==0,"Panic clears audio with velocity expression");
    s.noteOn(61,.33);advance(s);
    bool found=false;for(unsigned i=0;i<4;++i)if(s.voiceState()[i].held){checkVoice(s,i,.33,p);found=true;}
    require(found,"Fresh assignment after panic has its new velocity");
    ceres::Synth fresh(8000,4);fresh.setPatch(p);s.reset();fresh.noteOn(57,.61);s.noteOn(57,.61);
    for(unsigned i=0;i<500;++i)require(s.process()==fresh.process(),"Reset equivalence with active velocity routes");
}
void pressureIsolationAndLiveSelection() {
    auto p=patch();ceres::Synth a(8000,5),b(8000,5);a.setPatch(p);b.setPatch(p);
    a.noteOn(60,.3);b.noteOn(60,.3);a.noteOn(67,.9);b.noteOn(67,.9);
    for(unsigned n=0;n<1500;++n) {
        if(n%37==0){a.pressure((n%101)/100.);b.pressure(1-(n%101)/100.);}
        if(n==300){a.noteOff(60);b.noteOff(60);}
        if(n==700){a.allNotesOff();b.allNotesOff();}
        require(a.process()==b.process(),"Channel pressure cannot affect velocity mode, including release");
    }
    // Pressure still runs its 10ms touch RC while velocity is selected.
    a.reset();b.reset();a.setPatch(p);p.expressionSource=0;b.setPatch(p);
    a.pressure(.8);b.pressure(.8);advance(a,800);advance(b,800);
    a.noteOn(60,.25);b.noteOn(60,.25);advance(a,100);advance(b,100);
    const auto old=a.voiceState()[0];a.setPatch(p);const auto after=a.voiceState()[0];
    require(old.oscillatorPhase==after.oscillatorPhase && old.ampEnvelope==after.ampEnvelope &&
            old.filterEnvelope==after.filterEnvelope && old.held==after.held,
            "Live source change cannot reset envelopes, gates or oscillator phase");
    a.process();b.process();
    near(a.voiceState()[0].hpFrequency,b.voiceState()[0].hpFrequency,0,
         "Returning to aftertouch uses uninterrupted channel pressure smoothing");
    p.expressionSource=1;a.setPatch(p);a.process();checkVoice(a,0,.25,p);
}
// SM2 p.8 touch pre-amplifier: VR3's wiper reaches IC1-2 through 100k with
// .1uF to the +15V rail, so touch follows a 10ms RC. Velocity keeps 5ms.
void touchRc() {
    auto p=patch();p.expressionSource=0;p.lfoRate=0;p.pressureVibrato=0;
    ceres::Synth s(8000,6);s.setPatch(p);s.noteOn(60,.9);advance(s,40);
    checkVoice(s,0,0,p);s.pressure(.8);
    for(unsigned n=1;n<=400;++n) {
        s.process();
        if(n%25==0)checkVoice(s,0,.8*-std::expm1(-4.*n/(32000*.01)),p);
    }
}
void audioLawsAndZeroDepth() {
    for(unsigned route=0;route<3;++route) {
        auto p=patch();p.pressureFilter=p.pressureVibrato=p.pressureVolume=0;
        p.sine=.15;constexpr double v=.37;
        if(route==0){p.pressureVolume=.8;p.ampLevel=.5;}
        if(route==1)p.pressureFilter=1.3;
        if(route==2)p.pressureVibrato=80;
        auto equivalent=p;equivalent.expressionSource=0;
        // Touch LEVEL adds 1.08 x depth x touch to LEVEL at common VCA2 (R7 LC).
        if(route==0){equivalent.ampLevel=p.ampLevel+(6/150e3)/(10/270e3)*v*p.pressureVolume;equivalent.pressureVolume=0;}
        if(route==1) {
            equivalent.hpCutoff=floorHz+(p.hpCutoff-floorHz)*std::exp2(v*p.pressureFilter*hpTouch);
            equivalent.lpCutoff=floorHz+(p.lpCutoff-floorHz)*std::exp2(v*p.pressureFilter);
            equivalent.pressureFilter=0;
        }
        // The touch vibrato's +.5V centre is a VCO-only offset, as TRANSPOSE is.
        if(route==2){equivalent.vibratoCents=v*p.pressureVibrato;equivalent.pressureVibrato=0;
            equivalent.octave=v*p.pressureVibrato*vibratoBias/1200;}
        ceres::Synth a(8000,6),b(8000,6);a.setPatch(p);b.setPatch(equivalent);a.noteOn(60,v);b.noteOn(60,v);
        double energy=0;
        for(unsigned n=0;n<1600;++n) {
            if(n==500){a.noteOff(60);b.noteOff(60);}
            const double x=a.process(),y=b.process();energy+=x*x;
            near(x,y,2e-7,"Each velocity expression route matches its independent gain/gm/SUB audio equivalent");
        }
        require(energy>1e-5,"Audio route equivalence is not a silence test");
    }
    auto p=patch();p.pressureVolume=p.pressureFilter=p.pressureVibrato=0;
    p.velocitySensitivity=.6;p.ringMix=.4;p.filterLfo=.7;p.expressionSource=0;
    ceres::Synth a(8000,7),b(8000,7);a.setPatch(p);p.expressionSource=1;b.setPatch(p);
    for(unsigned n=0;n<1500;++n) {
        if(n%200==0){a.noteOn(48+n/200,.1+n/2000.);b.noteOn(48+n/200,.1+n/2000.);}
        if(n==700){a.allNotesOff();b.allNotesOff();}
        a.pressure(n/1500.);b.pressure(1-n/1500.);
        require(a.process()==b.process(),"Zero expression depths are byte-exact with independent legacy velocity sensitivity");
    }
    // At one shared velocity, velocity sensitivity multiplies the common VCA
    // gain that touch LEVEL adds to; it does not replace it.
    p=patch();p.pressureFilter=p.pressureVibrato=0;p.pressureVolume=.3;p.velocitySensitivity=.4;p.ampLevel=.5;
    constexpr double v=.4;auto both=p;both.pressureVolume=0;
    both.ampLevel=p.ampLevel+(6/150e3)/(10/270e3)*.3*v;
    ceres::Synth c(8000,8),d(8000,8);c.setPatch(p);d.setPatch(both);c.noteOn(60,v);d.noteOn(60,v);
    for(unsigned n=0;n<500;++n)near(c.process(),d.process(),2e-8,"Velocity sensitivity remains an independent multiplicative gain");
}
}
int main() {
    try {
        rangeContract();std::cout<<"PASS expression enum/default/nonfinite contract\n";
        polyphonyIsolation();std::cout<<"PASS mixed-velocity polyphony oscillator/filter/audio isolation\n";
        lifecycle();std::cout<<"PASS expression release, repeated notes, pedal, Sustain II, steal, panic and reset\n";
        pressureIsolationAndLiveSelection();std::cout<<"PASS pressure isolation and state-preserving live source selection\n";
        touchRc();std::cout<<"PASS touch follows the 10ms pre-amplifier RC\n";
        audioLawsAndZeroDepth();std::cout<<"PASS independent expression audio laws, zero-depth parity and legacy velocity gain\n";
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
