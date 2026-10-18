#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Route=ceres::ResearchPitchRoute;
constexpr double floorHz=1/(2*3.14159265358979323846*4.7e6*1.5e-9);
void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
void near(double a,double b,double rel,const char* message) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>rel*std::max(1.,std::abs(b))) {
        std::cerr<<message<<": "<<a<<" != "<<b<<'\n';
        throw std::runtime_error(message);
    }
}
void unchanged(const ceres::Synth& a,const ceres::Synth& b) {
    const auto va=a.voiceState(),vb=b.voiceState();
    for(unsigned i=0;i<4;++i) {
        const auto& x=va[i];const auto& y=vb[i];
        require(x.note==y.note && x.keyNote==y.keyNote &&
            x.held==y.held && x.pedalHeld==y.pedalHeld,"note ownership/gates changed");
        near(x.oscillatorFrequency,y.oscillatorFrequency,0,"oscillator F0 changed");
        near(x.oscillatorPhase,y.oscillatorPhase,0,"oscillator phase changed");
        near(x.filterEnvelope,y.filterEnvelope,0,"VCF EG changed");
        near(x.ampEnvelope,y.ampEnvelope,0,"VCA EG changed");
    }
    const auto ma=a.modulationState(),mb=b.modulationState();
    near(ma.sub,mb.sub,0,"SUB changed");
    near(ma.ringEnvelope,mb.ringEnvelope,0,"ring EG changed");
    near(ma.ringFrequency,mb.ringFrequency,0,"ring F0 changed");
    near(ma.ringCarrier,mb.ringCarrier,0,"ring phase changed");
    require(ma.physicalKeyPresses==mb.physicalKeyPresses,"physical ledger changed");
    const auto sa=a.soundPathState(),sb=b.soundPathState();
    near(sa.commonVcaGain,sb.commonVcaGain,0,"common VCA gain changed");
    near(sa.resonanceVoltageOffset,sb.resonanceVoltageOffset,0,"Q capacitor changed");
    near(sa.hpResonance,sb.hpResonance,0,"HP Q changed");
    near(sa.lpResonance,sb.lpResonance,0,"LP Q changed");
    require(a.activeVoices()==b.activeVoices(),"voice count changed");
    const auto ca=a.calibrationState(),cb=b.calibrationState();
    require(ca.enabled==cb.enabled,"calibration mode changed");
    near(ca.subVoltage,cb.subVoltage,0,"network SUB changed");
    for(unsigned i=0;i<4;++i) {
        near(ca.envelope[i],cb.envelope[i],0,"network EG changed");
        near(ca.hp.pinVoltage[i],cb.hp.pinVoltage[i],0,"HP CV network changed");
        near(ca.lp.pinVoltage[i],cb.lp.pinVoltage[i],0,"LP CV network changed");
        near(ca.hp.exponent[i],cb.hp.exponent[i],0,"HP alpha exponent changed");
        near(ca.lp.exponent[i],cb.lp.exponent[i],0,"LP alpha exponent changed");
    }
}
void events(ceres::Synth& s,unsigned f) {
    if(f==0){s.noteOn(64,.3);s.pitchBend(-.41);s.pressure(.8);}
    if(f==90)s.noteOn(72,.7);
    if(f==140)s.pitchBend(.73);
    if(f==210)s.sustain(true);
    if(f==260)s.noteOff(64);
    if(f==390){s.noteOn(76,.9);s.pressure(.2);}
    if(f==440)s.noteOff(72);
    if(f==500)s.sustain(false);
    if(f==600){s.noteOn(81,.6);s.noteOn(60,.4);s.noteOn(60,.2);}
    if(f==690)s.allNotesOff();
    if(f==740)s.allSoundOff();
    if(f==770)s.noteOn(55,.9);
    if(f==850)s.reset();
    if(f==860){s.noteOn(81,.5);s.pitchBend(-.4);}
    if(f==1050)s.noteOff(81);
}
void mappingsAndState() {
    for(int mode:{0,1,2}) for(double kt:{0.,.5,1.}) for(double tune:{-321.,237.}) {
        auto p=ceres::preset(mode==1?"flute":"brass1");
        p.calibrationNetwork=mode;p.calibrationAlpha=1.3;
        p.keyTrack=kt;p.tuneCents=tune;p.sustainMode=2;
        p.filterAttack=.006;p.filterDecay=.09;p.filterRelease=.08;
        p.ampAttack=.003;p.ampDecay=.08;p.ampRelease=.09;
        p.filterLfo=.1;p.tremolo=.2;p.ringMix=.15;p.noise=.03;
        p.pressureVolume=.4;p.pressureVibrato=13;p.pressureFilter=mode==0?.2:0;
        p.resonanceOffset=.2;p.driftCents=.5;p.voiceSpreadCents=.7;
        ceres::Synth legacy(8000,71),shared(8000,71),vco(8000,71),master(8000,71);
        for(auto* s:{&legacy,&shared,&vco,&master})s->setPatch(p);
        shared.setResearchPitchRoute(Route::Shared);
        vco.setResearchPitchRoute(Route::OscillatorOnly);
        constexpr double offset=.83;
        master.setResearchPitchRoute(Route::StaticMaster,offset);
        double bend=-.41;
        for(unsigned f=0;f<1400;++f) {
            if(f==140)bend=.73;
            if(f==850)bend=0;
            if(f==860)bend=-.4;
            for(auto* s:{&legacy,&shared,&vco,&master})events(*s,f);
            const auto x=legacy.process(),y=shared.process(),z=vco.process(),w=master.process();
            require(x==y,"explicit Shared differs from default audio");
            require(std::isfinite(z)&&std::isfinite(w),"nonfinite route audio");
            unchanged(shared,vco);unchanged(shared,master);
            const auto a=shared.voiceState(),b=vco.voiceState(),c=master.voiceState();
            for(unsigned i=0;i<4;++i) {
                for(bool hp:{true,false}) {
                    const double fa=hp?a[i].hpFrequency:a[i].lpFrequency;
                    const double fb=hp?b[i].hpFrequency:b[i].lpFrequency;
                    const double fc=hp?c[i].hpFrequency:c[i].lpFrequency;
                    near(fb,floorHz+(fa-floorHz)*std::exp2(-bend*kt/12),3e-14,"VCO-only filter factor/floor");
                    near(fc,floorHz+(fb-floorHz)*std::exp2(offset/12),3e-14,"static master must be outside keyTrack");
                }
            }
        }
    }
}
void nullsAndLiveConfiguration() {
    for(int mode:{0,1,2}) {
        auto p=ceres::preset(mode==1?"flute":"brass1");p.calibrationNetwork=mode;
        ceres::Synth a(8000,9),b(8000,9),c(8000,9);
        for(auto* s:{&a,&b,&c}){s->setPatch(p);s->noteOn(69,.8);}
        b.setResearchPitchRoute(Route::OscillatorOnly);
        c.setResearchPitchRoute(Route::StaticMaster,0);
        for(unsigned f=0;f<1000;++f) {
            if(f==300) {
                const auto before=c.voiceState();
                c.setResearchPitchRoute(Route::StaticMaster,2.1);
                const auto after=c.voiceState();
                for(unsigned i=0;i<4;++i) {
                    near(before[i].oscillatorPhase,after[i].oscillatorPhase,0,"setter reset oscillator");
                    near(before[i].filterEnvelope,after[i].filterEnvelope,0,"setter changed EG");
                    near(before[i].hpFrequency,after[i].hpFrequency,0,"setter processed a sample");
                }
                // Return before processing: even the analog filter state must
                // remain untouched by configuration alone.
                c.setResearchPitchRoute(Route::StaticMaster,0);
            }
            const auto x=a.process(),y=b.process(),z=c.process();
            require(x==y&&y==z,"zero-bend route null not exact");
        }
        b.setResearchPitchRoute(Route::StaticMaster,-1.7);
        c.setResearchPitchRoute(Route::StaticMaster,-1.7);
        b.reset();c.reset();b.setPatch(p);c.setPatch(p);
        for(auto* s:{&b,&c}){s->noteOn(74);s->pitchBend(.6);}
        for(unsigned f=0;f<500;++f) {
            if(f==100){b.allSoundOff();c.allSoundOff();b.noteOn(64);c.noteOn(64);}
            require(b.process()==c.process(),"reset/panic/configuration determinism");
            const auto state=b.voiceState();const auto cv=b.calibrationState();
            if(mode!=0) for(unsigned i=0;i<4;++i) {
                const double q=(state[i].keyNote-60)*p.keyTrack/12+p.tuneCents/1200-1.7/12;
                near(state[i].hpFrequency,floorHz+12*std::exp2(q+cv.hp.exponent[i]),3e-14,"reset/panic lost route config");
            }
        }
    }
}
void invalidInputs() {
    ceres::Synth a(8000,8),b(8000,8);
    a.setResearchPitchRoute(Route::StaticMaster,1.2);b.setResearchPitchRoute(Route::StaticMaster,1.2);
    const double nan=std::numeric_limits<double>::quiet_NaN();
    for(auto route:{Route::Shared,Route::OscillatorOnly,Route::StaticMaster,static_cast<Route>(-1),static_cast<Route>(3)})
        for(double offset:{nan,std::numeric_limits<double>::infinity(),-48.01,48.01,1.}) {
            if(route==Route::StaticMaster && offset==1.)continue;
            bool threw=false;try{a.setResearchPitchRoute(route,offset);}catch(const std::invalid_argument&){threw=true;}
            require(threw,"invalid research route input accepted");
        }
    a.noteOn(60);b.noteOn(60);
    for(unsigned f=0;f<100;++f)require(a.process()==b.process(),"invalid setter mutated configuration");
    for(double endpoint:{-48.,48.})a.setResearchPitchRoute(Route::StaticMaster,endpoint);
}
}
int main() {
    try {mappingsAndState();nullsAndLiveConfiguration();invalidInputs();
        std::cout<<"PASS pitch routes: mappings/floor, oscillator/EG/controller isolation, lifecycle/nulls, invalid inputs\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
