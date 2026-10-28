#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void near(double value,double expected,double tolerance,const char* message) {
    if(!std::isfinite(value)||std::abs(value-expected)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",value,expected);
        throw std::runtime_error(message);
    }
}
void advance(ceres::Synth& s,unsigned frames) {
    while(frames--)require(std::isfinite(s.process()),"Finite note-reuse audio");
}
ceres::Patch patch() {
    ceres::Patch p;p.saw=0;p.sine=1;p.keyTrack=0;p.hpCutoff=100;p.lpCutoff=1000;
    p.filterInitial=p.filterPeak=0;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=10;
    p.expressionSource=1;p.pressureVolume=.7;p.pressureFilter=1;p.volume=.3;
    // Nominal boards: the LP readout below is the design's touch depth.
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    return p;
}
double velocity(const ceres::Synth& s) {
    // The actual production filter CV provides an independent readout of the
    // effective expression coordinate; no test-only DSP mutator is needed.
    // LP receives the full touch-brilliance depth; HP gets the R7 47k/150k share.
    // The shared row's rise under that touch (sound path contracts) is removed.
    return std::log2((s.allVoiceState()[0].lpFrequency-floorHz)/(1000-floorHz))-s.modulationState().lpBus;
}
unsigned held(const ceres::Synth& s) {
    unsigned count=0;for(const auto& v:s.allVoiceState())if(v.held)++count;return count;
}
void continuousVelocity() {
    for(double fs:{8000.,48000.,96000.})for(unsigned factor:{1u,2u,4u}) {
        ceres::Synth s(fs,50);require(s.configure(1,factor),"Configure reuse quality");
        auto p=patch();s.setPatch(p);s.noteOn(60,.2);advance(s,unsigned(fs*.02));
        near(velocity(s),.2,1e-13,"Fresh note precharges its velocity");
        // One board: a held key takes no second board, so each re-press
        // follows its release and re-keys the board that held that key.
        s.noteOff(60);const auto before=s.allVoiceState()[0];s.noteOn(60,.9);
        const auto assigned=s.allVoiceState()[0];
        require(assigned.oscillatorPhase==before.oscillatorPhase &&
                assigned.ampEnvelope==before.ampEnvelope &&
                assigned.filterEnvelope==before.filterEnvelope,"Reuse preserves oscillator and envelope capacitors");
        s.process();double expected=.9+(.2-.9)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-13,"Audible reused velocity follows five-ms RC without a step");
        const unsigned frames=unsigned(fs*.003);advance(s,frames);
        expected=.9+(.2-.9)*std::exp(-double(frames+1)/(fs*.005));
        near(velocity(s),expected,2e-12,"Five-ms velocity timing is independent of sample rate and quality");
        // Retarget the actual in-flight value rather than the previous target.
        s.noteOff(60);s.noteOn(60,.4);s.process();expected=.4+(expected-.4)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-12,"Repeated reuse retargets the current velocity value");
        // An extra press while the only board is held is not stored; its
        // note-off is consumed after the owner's. The audible tail reuses smoothly.
        s.noteOn(60,.95);s.noteOff(60);s.noteOff(60);advance(s,unsigned(fs*.002));
        const double previous=velocity(s);require(!s.allVoiceState()[0].held,"Owner press released");
        require(s.allVoiceState()[0].ampEnvelope>0,"Released reuse witness is audible");
        s.noteOn(60,.1);s.process();expected=.1+(previous-.1)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-12,"Audible released-board reuse also keeps velocity continuous");
        // A silent gate assigned twice before its first sample must not ramp.
        s.reset();s.noteOn(60,.2);s.noteOff(60);s.noteOn(60,.8);s.process();
        near(velocity(s),.8,1e-13,"Zero-amplitude assignment precharges immediately");
        s.allSoundOff();s.noteOn(60,.3);s.process();
        near(velocity(s),.3,1e-13,"Panic's silent board starts at new velocity");
        p.ampRelease=.001;s.setPatch(p);s.noteOff(60);advance(s,unsigned(fs*.05));
        require(s.activeVoices()==0,"Release reaches an idle board");
        s.noteOn(60,.7);s.process();near(velocity(s),.7,1e-13,"Idle board precharges immediately");
    }
}
void legacyGainAndReset() {
    for(unsigned factor:{1u,2u,4u}) {
        ceres::Synth selected(48000,51),legacy(48000,51);
        selected.configure(1,factor);legacy.configure(1,factor);
        // Touch LEVEL a+c*e equals legacy (a+c)*(1-s*(1-e)) with s=c/(a+c).
        auto p=patch();p.pressureFilter=0;p.ampLevel=.2;selected.setPatch(p);
        const double c=(6/150e3)/(10/270e3)*p.pressureVolume;
        p.expressionSource=0;p.ampLevel=.2+c;p.velocitySensitivity=c/p.ampLevel;p.pressureVolume=0;legacy.setPatch(p);
        for(unsigned f=0;f<5000;++f) {
            if(f==2000 || f==2100)for(auto* s:{&selected,&legacy})s->noteOff(60);
            if(f==0 || f==2000 || f==2100)for(auto* s:{&selected,&legacy})s->noteOn(60,f==0?.2:(f==2000?.9:.4));
            near(selected.process(),legacy.process(),1e-12,"Legacy velocity gain uses the same continuous coordinate");
        }
        selected.reset();legacy.reset();
        for(auto* s:{&selected,&legacy})s->noteOn(67,.65);
        for(unsigned f=0;f<1000;++f)near(selected.process(),legacy.process(),1e-12,"Reset precharges both velocity routes identically");
    }
}
void stolenPressFifo() {
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}) {
        ceres::Synth s(48000,52);s.configure(voices,factor);s.setPatch(patch());
        // Presses beyond held polyphony are not stored (guide printed p.19).
        for(unsigned n=0;n<voices+2;++n)s.noteOn(60,.2+.5*n/(voices+2));
        require(held(s)==voices,"Repeated presses fill configured voices and no more");
        for(unsigned n=0;n<voices;++n) {
            s.noteOff(60);require(held(s)==voices-n-1,"Owning same-pitch presses release in FIFO order");
        }
        for(unsigned n=0;n<2;++n) {
            const auto before=s.allVoiceState();s.noteOff(60);
            for(unsigned i=0;i<voices;++i)require(s.allVoiceState()[i].ampEnvelope==before[i].ampEnvelope &&
                !s.allVoiceState()[i].held,"Off for an unstored press has no owner");
        }
        require(s.modulationState().physicalKeyPresses==0,"Every stored and unstored physical press is consumed");
        s.noteOff(60);require(held(s)==0,"Unmatched note-off has no owner");
        s.reset();s.noteOn(60,.2);s.noteOn(60,.8);s.sustain(true);
        if(voices==1) {
            s.noteOff(60);require(held(s)==0&&s.allVoiceState()[0].pedalHeld,"Owner press off honors pedal");
            s.noteOff(60);require(s.allVoiceState()[0].pedalHeld,"Unstored press off cannot touch the latched board");
            s.sustain(false);require(!s.allVoiceState()[0].pedalHeld,"Pedal-up releases the board");
        }
        s.allNotesOff();s.noteOn(64,.6);s.noteOff(64);require(held(s)==0,"All Notes Off clears FIFO generations");
        s.allSoundOff();s.noteOn(65,.4);s.noteOff(65);require(held(s)==0,"Panic clears FIFO generations");
        s.reset();s.noteOn(67,.5);s.noteOff(67);require(held(s)==0,"Reset clears FIFO generations");
    }
}
}
int main() {
    try {
        continuousVelocity();std::puts("PASS audible held/released velocity reuse, retarget, fresh/zero/idle gates and all quality clocks");
        legacyGainAndReset();std::puts("PASS legacy/selected velocity audio equivalence and reset");
        stolenPressFifo();std::puts("PASS repeated-note ownership beyond polyphony, FIFO/pedal/panic/reset across 1/4/16 voices");
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
