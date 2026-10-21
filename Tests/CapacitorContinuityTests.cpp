#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<why<<": "<<a<<" versus "<<b<<'\n';throw std::runtime_error(why);
    }
}
void advance(ceres::Synth& s,unsigned n) {
    while(n--)require(std::isfinite(s.process()),"Finite production sample");
}
double rc(double initial,double target,double seconds,double tau) {
    return target+(initial-target)*std::exp(-seconds/tau);
}
void qStepsAndSuperposition() {
    // KCL C*dq/dt=(P-q)/220k+(RES-q)/220k. The unchanged anchor
    // coordinate implies P=20*(1-r); q=10*(1-anchor)+offset. These are
    // affine compatibility coordinates, not calibrated preset/pin volts.
    // Solve their combined KCL independently of the implementation split.
    constexpr double resistance=220000,capacitance=10e-6;
    constexpr double tau=capacitance/(2/resistance);
    for(double fs:{8000.,48000.,192000.}) {
        ceres::Patch p;p.hpResonance=.2;p.lpResonance=.7;
        ceres::Synth s(fs,72);s.setPatch(p);
        require(!s.soundPathState().hasProcessed,"Initial patch is unclocked");
        advance(s,1); // Silent processing must close the setup exception.
        require(s.soundPathState().hasProcessed,"Silence starts the clock");
        double hpVoltage=10*(1-p.hpResonance),lpVoltage=10*(1-p.lpResonance),offset=0;
        const std::array<std::array<double,4>,3> edits={{{.8,.3,4,.12},{.1,.9,-2,.07},{.55,.45,0,.09}}};
        for(const auto& edit:edits) {
            const auto before=s.soundPathState();
            p.hpResonance=edit[0];p.lpResonance=edit[1];p.resonanceOffset=edit[2];
            s.setPatch(p);const auto after=s.soundPathState();
            near(after.hpResonanceAnchor,before.hpResonanceAnchor,0,"HP edit preserves charge");
            near(after.lpResonanceAnchor,before.lpResonanceAnchor,0,"LP edit preserves charge");
            near(after.resonanceVoltageOffset,before.resonanceVoltageOffset,0,"Shared edit preserves charge");
            const unsigned count=static_cast<unsigned>(fs*edit[3]);
            const double seconds=count/fs;
            advance(s,count);const auto state=s.soundPathState();
            const double hpTarget=(20*(1-p.hpResonance)/resistance+p.resonanceOffset/resistance)/(2/resistance);
            const double lpTarget=(20*(1-p.lpResonance)/resistance+p.resonanceOffset/resistance)/(2/resistance);
            hpVoltage=rc(hpVoltage,hpTarget,seconds,tau);
            lpVoltage=rc(lpVoltage,lpTarget,seconds,tau);
            offset=rc(offset,.5*p.resonanceOffset,seconds,tau);
            near(10*(1-state.hpResonanceAnchor)+state.resonanceVoltageOffset,hpVoltage,2e-10,"HP absolute KCL step/reversal");
            near(10*(1-state.lpResonanceAnchor)+state.resonanceVoltageOffset,lpVoltage,2e-10,"LP absolute KCL step/reversal");
            near(state.resonanceVoltageOffset,offset,2e-11,"Existing common delta remains its single RC");
            near(state.hpResonance,std::clamp(1-hpVoltage/10,0.,1.),2e-11,"HP affine-coordinate decomposition feeds damping");
            near(state.lpResonance,std::clamp(1-lpVoltage/10,0.,1.),2e-11,"LP affine-coordinate decomposition feeds damping");
        }
        const auto before=s.soundPathState();
        s.noteOn(60);s.noteOn(64);s.noteOff(60);s.sustain(true);s.noteOff(64);
        s.allNotesOff();s.allSoundOff();const auto panic=s.soundPathState();
        require(panic.hasProcessed,"Panic cannot reopen initialization");
        near(panic.hpResonanceAnchor,before.hpResonanceAnchor,0,"Panic preserves HP charge");
        near(panic.lpResonanceAnchor,before.lpResonanceAnchor,0,"Panic preserves LP charge");
        near(panic.resonanceVoltageOffset,before.resonanceVoltageOffset,0,"Panic preserves global charge");
        p.hpResonance=0;p.lpResonance=1;s.setPatch(p);
        near(s.soundPathState().hpResonanceAnchor,before.hpResonanceAnchor,0,"Post-panic edit stays live");
        s.reset();const auto reset=s.soundPathState();
        require(!reset.hasProcessed,"Reset reopens explicit configuration");
        near(reset.hpResonanceAnchor,0,0,"Reset precharges configured HP");
        near(reset.lpResonanceAnchor,1,0,"Reset precharges configured LP");
        near(reset.resonanceVoltageOffset,0,0,"Reset retains old common-offset convention");
        p.hpResonance=.3;p.lpResonance=.6;s.setPatch(p);
        near(s.soundPathState().hpResonanceAnchor,.3,0,"Reset then patch configures HP");
        near(s.soundPathState().lpResonanceAnchor,.6,0,"Reset then patch configures LP");
    }
}
void independentStageAndAudio() {
    ceres::Patch p;p.hpResonance=.2;p.lpResonance=.3;p.hpCutoff=100;p.lpCutoff=700;
    p.filterInitial=p.filterPeak=0;p.keyTrack=0;p.sine=0;p.drive=.05;p.ampSustain=1;
    ceres::Synth changed(8000,91),fixed(8000,91);changed.setPatch(p);fixed.setPatch(p);
    changed.noteOn(60);fixed.noteOn(60);
    for(unsigned i=0;i<800;++i)require(changed.process()==fixed.process(),"Identical pre-edit signal");
    p.lpResonance=.9;changed.setPatch(p);double energy=0;
    for(unsigned i=0;i<1600;++i) {
        const double difference=changed.process()-fixed.process();energy+=difference*difference;
        near(changed.soundPathState().hpResonanceAnchor,.2,0,"LP edit cannot alter HP anchor");
    }
    near(changed.soundPathState().lpResonanceAnchor,rc(.3,.9,.2,1.1),2e-12,"Independent LP pole");
    require(energy>1e-7,"Smoothed Q must reach actual filtered audio");
    const auto before=changed.soundPathState();
    auto recalled=ceres::preset("flute");changed.setPatch(recalled);
    near(changed.soundPathState().hpResonanceAnchor,before.hpResonanceAnchor,0,"Preset recall keeps HP charge");
    near(changed.soundPathState().lpResonanceAnchor,before.lpResonanceAnchor,0,"Preset recall keeps LP charge");
}
void idleIlAndRetrigger() {
    constexpr double ln1000=6.907755278982137052;
    for(double fs:{8000.,48000.,192000.}) {
        ceres::Patch p;p.filterInitial=-1.25;p.filterPeak=2;p.filterAttack=.2;
        p.filterAttackHeadroom=.01;p.filterRelease=.2;p.globalRelease=.7;
        // Nominal EG times on every board: this checks the RC law itself.
        // Each board's trimmed times are BoardToleranceTests' contract.
        p.boardEnvelopeTimeTolerance=0;
        ceres::Synth s(fs,4);s.setPatch(p);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,-1.25,0,"Setup precharges IL");
        advance(s,1);p.filterInitial=-4;s.setPatch(p);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,-1.25,0,"Live idle IL edit cannot jump");
        const unsigned count=static_cast<unsigned>(fs*.05);
        advance(s,count);double expected=rc(-1.25,-4,count/fs,.7/ln1000);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,expected,2e-11,"All four idle capacitors follow finite RT/global law");
        require(s.activeVoices()==0,"Idle voltage return does not activate an amp voice");
        p.filterInitial=-.25;s.setPatch(p);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,expected,2e-11,"Reverse target edit preserves voltage");
        advance(s,count);expected=rc(expected,-.25,count/fs,.7/ln1000);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,expected,2e-11,"Idle RC reversal");
        s.noteOn(60);
        near(s.voiceState()[0].filterEnvelope,expected,2e-11,"Note-on preserves idle capacitor voltage");
        const double target=p.filterPeak+p.filterAttackHeadroom*(p.filterPeak-p.filterInitial);
        const double attackTau=p.filterAttack/std::log1p(1/p.filterAttackHeadroom);
        expected=rc(expected,target,1/fs,attackTau);s.process();
        near(s.voiceState()[0].filterEnvelope,expected,2e-11,"Attack starts from retained idle charge");
        s.allSoundOff();
        for(const auto& v:s.voiceState())near(v.filterEnvelope,p.filterInitial,0,"Explicit audio panic still clears EG to configured IL");
        p.filterInitial=-2;s.setPatch(p);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,-.25,0,"Panic must not enable later IL snap");
        s.reset();for(const auto& v:s.voiceState())near(v.filterEnvelope,-2,0,"Reset configures IL");
        p.filterInitial=-3;s.setPatch(p);
        for(const auto& v:s.voiceState())near(v.filterEnvelope,-3,0,"Reset reopens pre-clock IL setup");
    }
}
void releasedIdleReturn() {
    ceres::Patch p;p.filterInitial=-1;p.filterPeak=2;p.filterAttack=.001;
    p.filterRelease=.005;p.ampRelease=.005;p.ampAttack=.001;p.globalRelease=0;
    p.boardEnvelopeTimeTolerance=0; // nominal times, as in idleIlAndRetrigger
    ceres::Synth s(8000,7);s.setPatch(p);s.noteOn(60);advance(s,80);s.noteOff(60);advance(s,200);
    require(s.activeVoices()==0,"Amplitude release retired before edit");
    for(const auto& v:s.voiceState())near(v.filterEnvelope,-1,0,"Filter release reached existing snap endpoint");
    p.filterInitial=-5;p.filterRelease=1;s.setPatch(p);
    for(const auto& v:s.voiceState())near(v.filterEnvelope,-1,0,"Retired filter capacitor survives IL edit");
    advance(s,400);const double expected=rc(-1,-5,.05,1/std::log(1000.));
    for(const auto& v:s.voiceState())near(v.filterEnvelope,expected,2e-12,"Retired and never-used boards obey same circuit");
    s.noteOn(64);
    for(const auto& v:s.voiceState())near(v.filterEnvelope,expected,2e-12,"Allocation does not precharge the next board");
}
}
int main() {
    try {
        qStepsAndSuperposition();std::cout<<"PASS Q steps/reversal/absolute KCL and lifecycle\n";
        independentStageAndAudio();std::cout<<"PASS stage independence, preset continuity and audio connection\n";
        idleIlAndRetrigger();std::cout<<"PASS idle IL RC/reversal, retrigger and initialization\n";
        releasedIdleReturn();std::cout<<"PASS retired-board capacitor return\n";
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
