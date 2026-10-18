#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*4.7e6*1.5e-9);
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char* message) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<message<<": "<<std::setprecision(17)<<a<<" != "<<b<<'\n';
        throw std::runtime_error(message);
    }
}
void printStage(const ceres::CalibrationStageState& s) {
    std::cout<<s.busVoltage<<' '<<s.kappa<<' '<<s.conductance<<' '
        <<s.selectedCurrent<<' '<<s.manualOffMargin<<' '<<s.normalizedKclResidual<<' '
        <<s.selectedPresent<<' '<<s.diodeStatesValid;
    for(auto* values:{&s.pinVoltage,&s.exponent,&s.referenceFrequency})
        for(double x:*values) std::cout<<' '<<x;
}
void probe() {
    ceres::Patch p;std::array<double,4> e{};double sub;
    std::cout<<std::setprecision(17);
    while(std::cin>>p.calibrationNetwork>>p.brilliance>>p.calibrationHpBias
          >>p.calibrationLpBias>>p.calibrationAlpha>>p.filterLfo>>sub
          >>e[0]>>e[1]>>e[2]>>e[3]) {
        auto state=ceres::calibrationNetworkState(p,e,sub);
        std::cout<<state.subVoltage<<' ';printStage(state.hp);
        std::cout<<' ';printStage(state.lp);std::cout<<'\n';
    }
}
void parameterAndValidity() {
    ceres::Patch p;
    require(p.calibrationNetwork==0 && p.calibrationAlpha==1,"research defaults disabled");
    require(ceres::setParameter(p,"calibration_network",1.8)&&p.calibrationNetwork==2,"mode rounded");
    require(ceres::setParameter(p,"calibration_alpha",0)&&p.calibrationAlpha==.01,"strictly positive alpha");
    require(!ceres::setParameter(p,"calibration_alpha",std::numeric_limits<double>::infinity()),"nonfinite rejected");
    p.pressureFilter=1;ceres::Synth s(8000);
    bool rejected=false;try{s.setPatch(p);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"unsupported pressure cutoff must not be silently ignored");
    p.pressureFilter=0;p.calibrationAlpha=1;p.filterLfo=5;
    // Deliberately exceed the actual oscillator's bounded value. The static
    // probe exposes invalid conduction, rather than silently clamping nodes.
    const auto invalid=ceres::calibrationNetworkState(p,{{-12,-12,-12,-12}},-100);
    require(!invalid.hp.diodeStatesValid && !invalid.lp.diodeStatesValid,"diode region diagnostic");
}
void alphaAndAllTerms() {
    for(double mode:{1.,2.})for(double alpha:{.2,1.,2.7}) {
        ceres::Patch p;p.calibrationNetwork=mode;p.calibrationAlpha=1;
        p.brilliance=-11;p.filterLfo=.3;p.calibrationHpBias=7;p.calibrationLpBias=-9;
        const std::array<double,4> e{{-2.62,.315,-.1,1.785}};
        const auto base=ceres::calibrationNetworkState(p,e,.71);
        p.calibrationAlpha=alpha;const auto changed=ceres::calibrationNetworkState(p,e,.71);
        for(bool hp:{true,false}) {
            const auto& a=hp?base.hp:base.lp;const auto& b=hp?changed.hp:changed.lp;
            near(a.busVoltage,b.busVoltage,0,"alpha must not change control-node voltage");
            require(b.diodeStatesValid,"declared example diode states valid");
            for(unsigned i=0;i<4;++i) {
                near(b.exponent[i],alpha*a.exponent[i],2e-14,"alpha applies to entire exponent");
                near(b.referenceFrequency[i],floorHz+(hp?12.:180.)*std::exp2(alpha*a.exponent[i]),1e-9,"floor outside alpha");
            }
        }
        auto moved=e;moved[3]+=.2;
        const auto cross=ceres::calibrationNetworkState(p,moved,.71);
        require(cross.hp.exponent[0]>changed.hp.exponent[0] && cross.lp.exponent[0]>changed.lp.exponent[0],
            "other physical board changes shared preset bus");
    }
}
void productionStates() {
    for(int mode:{1,2}) {
        auto p=ceres::preset(mode==1?"flute":"brass1");
        p.calibrationNetwork=mode;p.calibrationAlpha=1.7;
        p.calibrationHpBias=5;p.calibrationLpBias=-3;p.brilliance=mode==1?0:-15;
        p.filterLfo=.15;p.lfoRate=6.7;p.keyTrack=.4;p.tuneCents=237;
        auto legacy=p;legacy.calibrationNetwork=0;
        auto ignored=p;ignored.hpCutoff=19999;ignored.lpCutoff=5;
        ignored.hpEnvelopeScale=3.7;ignored.lpEnvelopeScale=0;
        ceres::Synth a(8000,50),b(8000,50),c(8000,50);
        a.setPatch(p);b.setPatch(legacy);c.setPatch(ignored);
        for(unsigned frame=0;frame<1800;++frame) {
            for(auto* s:{&a,&b,&c}) {
                if(frame==0){s->noteOn(72,.8);s->pitchBend(.23);}
                if(frame==37)s->noteOn(76,.5);
                if(frame==128)s->noteOff(72);
                if(frame==170)s->noteOn(81,1);
                if(frame==240)s->noteOff(76);
            }
            const float x=a.process(),y=c.process();b.process();
            require(std::isfinite(x)&&x==y,"research must replace old cutoff and own-EG controls entirely");
            const auto va=a.voiceState(),vb=b.voiceState();const auto state=a.calibrationState();
            for(unsigned i=0;i<4;++i) {
                near(va[i].filterEnvelope,vb[i].filterEnvelope,0,"all original EG timing unchanged");
                near(state.envelope[i],va[i].filterEnvelope,0,"network uses current EG once per internal frame");
                const double key=(va[i].keyNote-60+.23)*p.keyTrack/12+p.tuneCents/1200;
                near(va[i].hpFrequency,floorHz+12*std::exp2(key+state.hp.exponent[i]),1e-10,"HP production mapping");
                near(va[i].lpFrequency,floorHz+180*std::exp2(key+state.lp.exponent[i]),1e-8,"LP key factor outside alpha");
            }
            near(state.envelope[3],p.filterInitial,0,"fourth idle board participates at IL");
            require(state.hp.normalizedKclResidual<1e-13&&state.lp.normalizedKclResidual<1e-13,"production node KCL");
        }
    }
}
void disabledControls() {
    ceres::Patch a;a.filterLfo=.7;a.tremolo=.5;a.noise=.1;a.ringMix=.2;
    auto b=a;b.calibrationHpBias=15;b.calibrationLpBias=-15;b.calibrationAlpha=8;
    ceres::Synth x(8000,91),y(8000,91);x.setPatch(a);y.setPatch(b);
    for(unsigned frame=0;frame<4000;++frame) {
        for(auto* s:{&x,&y}) {
            if(frame%750==0)s->noteOn(48+int(frame/750)*5,.7);
            if(frame==1800)s->allNotesOff();
            if(frame==2400)s->reset();
        }
        require(x.process()==y.process(),"disabled research controls must be byte inert");
    }
}
}
int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--probe"){probe();return 0;}
    try {
        parameterAndValidity();alphaAndAllTerms();productionStates();disabledControls();
        std::cout<<"PASS research calibration: parameter/diode, full-alpha, four-board production, disabled contracts\n";
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
