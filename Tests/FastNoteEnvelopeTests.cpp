#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
void require(bool good,const char* what) {if(!good)throw std::runtime_error(what);}
void near(double actual,double expected,double tolerance,const char* what) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
        std::fprintf(stderr,"actual %.17g expected %.17g\n",actual,expected);
        throw std::runtime_error(what);
    }
}
void clock(ceres::Synth& synth,unsigned frames) {
    while(frames--)require(std::isfinite(synth.process()),"Finite rapid-note audio");
}
void sameCharge(const ceres::Synth::VoiceState& a,const ceres::Synth::VoiceState& b) {
    require(a.ampEnvelope==b.ampEnvelope && a.filterEnvelope==b.filterEnvelope &&
            a.oscillatorPhase==b.oscillatorPhase,
            "Note events preserve amp/filter capacitor voltage and oscillator phase");
}
ceres::Patch base() {
    ceres::Patch p;p.saw=.45;p.pulse=.55;p.sine=.84;p.pulseWidth=.58;
    p.filterInitial=-2.5;p.filterPeak=1.4;
    p.filterAttack=1.4;p.filterDecay=3.5;p.filterRelease=2.5;
    p.ampAttack=1;p.ampDecay=2;p.ampSustain=.86;p.ampRelease=2.8;
    p.hpCutoff=28;p.lpCutoff=1750;p.lpResonance=.2;p.volume=.42;
    p.ringRate=1.26;p.ringDepth=8;p.ringAttack=.0087;p.ringDecay=.05;
    p.pwmRate=1.48;p.pwmDepth=.31;p.lfoRate=.22;p.filterLfo=.18;
    p.expressionSource=1;p.pressureVolume=.45;p.pressureFilter=1;
    // Nominal boards: the closed-form EG and touch readouts are design laws.
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    return p;
}

// The independent capacitor solution establishes the real size of a deliberately
// short attack. A 0.1ms attack can advance a long way in one host sample; the
// test must not silently replace it with a minimum fade or a reset-to-zero gate.
void shortEnvelopeLaws() {
    for(double hostFs:{8000.,48000.,96000.})for(unsigned factor:{1u,2u,4u})
        for(double attack:{.0001,.001,1.})for(bool filter:{false,true}) {
            const double fs=hostFs*factor;
            auto p=base();p.ampAttack=p.filterAttack=attack;
            p.ampDecay=p.filterDecay=.01;p.ampRelease=p.filterRelease=.01;
            // The VCA-EG's IL target carries the aligned -.2V VR17 floor
            // (CS-60 Service Manual p.38): -.02 of the 10V peak.
            const double initial=filter?p.filterInitial:-.02, peak=filter?p.filterPeak:1;
            const double headroom=.01,target=peak+headroom*(peak-initial);
            const double a=std::exp(-std::log1p(1/headroom)/(fs*attack));
            const auto first=ceres::envelopeTrace(fs,p,filter,2,{{0,true}});
            near(first[0],std::min(peak,target+(initial-target)*a),2e-14,
                 "Shortest and ordinary attacks retain the specified RC law");
            // A single sample on, a single sample off and immediate re-on is
            // faster than practical keyboard playing. The remaining charge,
            // not initial level, is the next attack's starting condition.
            const auto interrupted=ceres::envelopeTrace(fs,p,filter,3,
                                                       {{0,true},{1,false},{2,true}});
            const double released=initial+(first[0]-initial)*std::exp(-std::log(1000.)/(fs*.01));
            near(interrupted[1],released,2e-14,"Fast key-off retains charge and follows release resistance");
            near(interrupted[2],std::min(peak,target+(released-target)*a),2e-14,
                 "Fast re-gate attacks from retained capacitor charge");
        }
}

void rapidVelocityAndGates() {
    for(double fs:{8000.,48000.,96000.})for(unsigned factor:{1u,2u,4u})
        for(bool fast:{false,true})for(bool ring:{false,true})for(bool same:{false,true}) {
            ceres::Synth synth(fs,50);require(synth.configure(1,factor),"Configure rapid CV witness");
            auto p=base();p.hpCutoff=100;p.lpCutoff=1000;p.keyTrack=0;p.filterInitial=p.filterPeak=0;
            p.filterLfo=0;p.pressureVolume=.7;p.ringMix=ring?.73:0;
            // Keep a nonzero sustain between rapid re-gates. Zero-sustain
            // retirement is separately exercised by the burst matrix below.
            if(fast){p.ampAttack=.0001;p.ampDecay=.0001;p.ampSustain=.8;p.ampRelease=.01;}
            synth.setPatch(p);synth.noteOn(60,.2);clock(synth,unsigned(fs*.02));
            double expected=.2;
            for(unsigned n=0;n<32;++n) {
                const int note=same?60:48+int(n%25);
                const double target=n%2?.9:.1;
                // The only board is released, then re-keyed at once.
                synth.noteOff(n==0?60:(same?60:48+int((n-1)%25)));
                const auto before=synth.allVoiceState()[0];synth.noteOn(note,target);
                sameCharge(before,synth.allVoiceState()[0]);
                require(synth.allVoiceState()[0].held,"Fast re-press re-keys the released board");
                for(unsigned f=0;f<13;++f) {
                    expected=target+(expected-target)*std::exp(-1/(fs*.005));
                    require(std::isfinite(synth.process()),"Finite rapid CV sample");
                    // LP carries the full touch-brilliance depth (R7 TBR 47k), less
                    // the shared row's rise under it (sound path contracts).
                    const double actual=std::log2((synth.allVoiceState()[0].lpFrequency-floorHz)/(1000-floorHz))
                        -synth.modulationState().lpBus;
                    near(actual,expected,4e-12,"Rapid velocity retarget follows elapsed-time five-ms RC");
                }
            }
            synth.allNotesOff();require(!synth.allVoiceState()[0].held,"Fast run releases final board");
            require(synth.modulationState().physicalKeyPresses==0,"Fast physical-key ledger is empty");
        }
}

void burstMatrix() {
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u})
        for(unsigned shape=0;shape<3;++shape)for(bool ring:{false,true})
        for(unsigned notes:{1u,4u})for(bool varyVelocity:{false,true})for(bool same:{false,true}) {
            ceres::Synth synth(48000,50);require(synth.configure(voices,factor),"Configure rapid burst");
            auto p=base();p.ringMix=ring?.73:0;
            if(shape==1){p.ampAttack=p.filterAttack=.001;p.ampDecay=p.filterDecay=.01;
                        p.ampRelease=p.filterRelease=.01;}
            if(shape==2){p.ampAttack=p.filterAttack=.0001;p.ampDecay=p.filterDecay=.0001;
                        p.ampRelease=p.filterRelease=.0001;p.ampSustain=0;}
            synth.setPatch(p);
            std::array<int,4> previous{};
            for(unsigned event=0;event<24;++event) {
                if(event)for(unsigned k=0;k<notes;++k) {
                    const auto before=synth.allVoiceState();synth.noteOff(previous[k]);
                    const auto after=synth.allVoiceState();
                    for(unsigned i=0;i<voices;++i)sameCharge(before[i],after[i]);
                }
                for(unsigned k=0;k<notes;++k) {
                    const int note=(same?60:36+int((event*7)%48))+int(k)*3;
                    const auto before=synth.allVoiceState();
                    synth.noteOn(note,varyVelocity?(.2+.2*((event+k)%4)):.7);
                    const auto after=synth.allVoiceState();
                    for(unsigned i=0;i<voices;++i)sameCharge(before[i],after[i]);
                    previous[k]=note;
                }
                // Includes free boards, audible released-board reuse and
                // unstored presses for rapid chords beyond the selected polyphony.
                clock(synth,shape==2?8:96);
                const auto sounding=synth.allVoiceState();
                for(unsigned i=0;i<voices;++i) {
                    const auto& v=sounding[i];
                    // Physical VCA-EG endpoints: the aligned -.2V VR17 floor
                    // (CS-60 p.38, -.02 of peak) up to the peak.
                    require(std::isfinite(v.ampEnvelope)&&v.ampEnvelope>=-.02&&v.ampEnvelope<=1,
                            "Fast amplitude charge stays inside physical endpoints");
                    require(std::isfinite(v.filterEnvelope)&&v.filterEnvelope>=p.filterInitial&&v.filterEnvelope<=p.filterPeak,
                            "Fast filter charge stays inside physical endpoints");
                }
            }
            synth.allNotesOff();clock(synth,128);
            require(synth.modulationState().physicalKeyPresses==0,"Burst physical ledger clears");
            for(unsigned i=0;i<voices;++i)require(!synth.allVoiceState()[i].held,"Burst held owners clear");
        }
}
}
int main() {
    try {
        shortEnvelopeLaws();std::puts("PASS 54 short/ordinary RC-envelope cases, 162 closed-form points and one-sample interrupted re-gates");
        rapidVelocityAndGates();std::puts("PASS 72 rapid same/different-pitch velocity/re-gate cases across rates/quality/ring");
        burstMatrix();std::puts("PASS 432 rapid single/chord cases across 1/4/16 voices, all qualities, envelope ranges, ring and velocities");
    } catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
