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
    return p;
}
double velocity(const ceres::Synth& s) {
    // The actual production filter CV provides an independent readout of the
    // effective expression coordinate; no test-only DSP mutator is needed.
    return std::log2((s.allVoiceState()[0].hpFrequency-floorHz)/(100-floorHz));
}
unsigned held(const ceres::Synth& s) {
    unsigned count=0;for(const auto& v:s.allVoiceState())if(v.held)++count;return count;
}
void continuousVelocity() {
    for(double fs:{8000.,48000.,96000.})for(unsigned factor:{1u,2u,4u}) {
        ceres::Synth s(fs,50);require(s.configure(1,factor),"Configure reuse quality");
        auto p=patch();s.setPatch(p);s.noteOn(60,.2);advance(s,unsigned(fs*.02));
        near(velocity(s),.2,1e-13,"Fresh note precharges its velocity");
        const auto before=s.allVoiceState()[0];s.noteOn(60,.9);
        const auto assigned=s.allVoiceState()[0];
        require(assigned.oscillatorPhase==before.oscillatorPhase &&
                assigned.ampEnvelope==before.ampEnvelope &&
                assigned.filterEnvelope==before.filterEnvelope,"Reuse preserves oscillator and envelope capacitors");
        s.process();double expected=.9+(.2-.9)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-13,"Audible stolen velocity follows five-ms RC without a step");
        const unsigned frames=unsigned(fs*.003);advance(s,frames);
        expected=.9+(.2-.9)*std::exp(-double(frames+1)/(fs*.005));
        near(velocity(s),expected,2e-12,"Five-ms velocity timing is independent of sample rate and quality");
        // Retarget the actual in-flight value rather than the previous target.
        s.noteOn(60,.4);s.process();expected=.4+(expected-.4)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-12,"Repeated steals retarget the current velocity value");
        // Consume all three physical presses. Only the newest surviving press
        // releases this board; its still audible tail must also reuse smoothly.
        s.noteOff(60);s.noteOff(60);s.noteOff(60);advance(s,unsigned(fs*.002));
        const double previous=velocity(s);require(!s.allVoiceState()[0].held,"Newest FIFO note released");
        require(s.allVoiceState()[0].ampEnvelope>0,"Released reuse witness is audible");
        s.noteOn(60,.1);s.process();expected=.1+(previous-.1)*std::exp(-1/(fs*.005));
        near(velocity(s),expected,2e-12,"Audible released-board reuse also keeps velocity continuous");
        // A silent gate assigned twice before its first sample must not ramp.
        s.reset();s.noteOn(60,.2);s.noteOn(60,.8);s.process();
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
        auto p=patch();p.pressureFilter=0;selected.setPatch(p);
        p.expressionSource=0;p.velocitySensitivity=p.pressureVolume;p.pressureVolume=0;legacy.setPatch(p);
        for(unsigned f=0;f<5000;++f) {
            if(f==0 || f==2000 || f==2100)for(auto* s:{&selected,&legacy})s->noteOn(60,f==0?.2:(f==2000?.9:.4));
            require(selected.process()==legacy.process(),"Legacy velocity gain uses the same continuous coordinate");
        }
        selected.reset();legacy.reset();
        for(auto* s:{&selected,&legacy})s->noteOn(67,.65);
        for(unsigned f=0;f<1000;++f)require(selected.process()==legacy.process(),"Reset precharges both velocity routes identically");
    }
}
void stolenPressFifo() {
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}) {
        ceres::Synth s(48000,52);s.configure(voices,factor);s.setPatch(patch());
        for(unsigned n=0;n<voices+2;++n)s.noteOn(60,.2+.5*n/(voices+2));
        require(held(s)==voices,"Repeated presses fill and steal configured voices");
        for(unsigned n=0;n<2;++n) {
            s.noteOff(60);
            require(held(s)==voices,"Off for a stolen oldest press cannot release its newer replacement");
        }
        for(unsigned n=0;n<voices;++n) {
            s.noteOff(60);require(held(s)==voices-n-1,"Surviving same-pitch presses release in FIFO order");
        }
        require(s.modulationState().physicalKeyPresses==0,"Every stolen and surviving physical press is consumed");
        s.noteOff(60);require(held(s)==0,"Unmatched note-off has no owner");
        s.reset();s.noteOn(60,.2);s.noteOn(60,.8);s.sustain(true);
        if(voices==1) {
            s.noteOff(60);require(held(s)==1&&!s.allVoiceState()[0].pedalHeld,"Stolen press off cannot pedal-latch its replacement");
            s.noteOff(60);require(held(s)==0&&s.allVoiceState()[0].pedalHeld,"Matching press off still honors pedal");
            s.sustain(false);require(!s.allVoiceState()[0].pedalHeld,"Pedal-up releases the correct replacement");
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
        stolenPressFifo();std::puts("PASS repeated-note ownership after stealing, FIFO/pedal/panic/reset across 1/4/16 voices");
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
