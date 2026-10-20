// KAS portamento/glissando, same-key channel reuse and the touch-response
// routes (guide printed pp.17-25 and 55-56; SM1 p.14 R7 matrix). Expected
// values are recomputed here from the documented laws, not read back from
// the engine's own state.
#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
constexpr double hpShare=(.7/.65)*(47./150.);
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
ceres::Patch plain() {
    ceres::Patch p;p.saw=1;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=5;
    p.filterInitial=p.filterPeak=0;
    return p;
}
double key(const ceres::Synth& s,unsigned board) {
    return 69+12*std::log2(s.allVoiceState()[board].oscillatorFrequency/440);
}

void offIsUnchanged() {
    auto p=plain();ceres::Synth a(48000,7),b(48000,7);
    auto q=p;q.glideRate=3;
    a.setPatch(p);b.setPatch(q);
    for(unsigned n=0;n<20000;++n) {
        if(n==0){a.noteOn(48);b.noteOn(48);}
        if(n==4000){a.noteOn(72);b.noteOn(72);}
        if(n==9000){a.noteOff(48);b.noteOff(48);a.noteOn(55);b.noteOn(55);}
        require(a.process()==b.process(),"Glide off ignores the rate and keeps the step pitch path");
    }
}

// Glissando: the counter moves one semitone per 1/rate seconds and the
// sample-hold sees each exact key voltage.
void glissandoSteps() {
    constexpr double fs=8000,rate=40;
    ceres::Synth s(fs,3);require(s.configure(1,1),"Configure glissando witness");
    auto p=plain();p.glideMode=2;p.glideRate=rate;s.setPatch(p);
    s.noteOn(60);s.process();near(key(s,0),60,1e-9,"First key starts at the power-on C4 code");
    s.noteOff(60);s.noteOn(67);
    for(unsigned n=1;n<=unsigned(fs*8/rate);++n) {
        s.process();
        const double expected=std::min(67.,60+std::floor(n*rate/fs+1e-9));
        near(key(s,0),expected,1e-9,"Glissando counts one semitone per clock step");
    }
}

// Portamento integrates the stepped key voltage, linear in Hz, with tau equal
// to one counter step.
void portamentoIntegratesHz() {
    constexpr double fs=8000,rate=12;
    ceres::Synth s(fs,3);require(s.configure(1,1),"Configure portamento witness");
    auto p=plain();p.glideMode=1;p.glideRate=rate;s.setPatch(p);
    s.noteOn(48);
    double code=60,steps=0,voltage=1;
    const double c=-std::expm1(-rate/fs);
    double previous=-1;
    for(unsigned n=0;n<unsigned(fs*3);++n) {
        if(code!=48){steps+=rate/fs;while(steps>=1&&code!=48){code-=1;steps-=1;}}else steps=0;
        voltage+=(std::exp2((code-60)/12)-voltage)*c;
        s.process();
        near(key(s,0),60+12*std::log2(voltage),1e-9,"Portamento follows the Hz-domain integrator");
        if(previous>=0)require(key(s,0)<=previous+1e-12,"Downward glide is monotone");
        previous=key(s,0);
    }
    near(key(s,0),48,1e-3,"Portamento settles on the new key");
}

// Sustain I holds the sample-hold at key-off; Sustain II keeps tracking.
void sustainModesAtRelease() {
    for(int mode:{1,2}) {
        constexpr double fs=8000;
        ceres::Synth s(fs,3);require(s.configure(1,1),"Configure sustain witness");
        auto p=plain();p.glideMode=1;p.glideRate=6;p.sustainMode=mode;s.setPatch(p);
        s.noteOn(60);s.process();s.noteOff(60);s.noteOn(72);
        for(unsigned n=0;n<8000;++n)s.process();
        const double atRelease=key(s,0);
        require(atRelease>60.5&&atRelease<71.5,"Release happens mid-glide");
        s.noteOff(72);
        for(unsigned n=0;n<4000;++n)s.process();
        if(mode==1)near(key(s,0),atRelease,1e-12,"Sustain I holds the released pitch");
        else require(key(s,0)>atRelease+.5,"Sustain II keeps gliding while decaying");
    }
}

void freeBoardGlidesFromLastKey() {
    constexpr double fs=8000;
    ceres::Synth s(fs,3);require(s.configure(2,1),"Configure two-board witness");
    auto p=plain();p.glideMode=2;p.glideRate=20;s.setPatch(p);
    s.noteOn(50);for(unsigned n=0;n<8000;++n)s.process();
    s.noteOn(55);s.process();
    near(key(s,1),50,1e-9,"An empty channel carries the last-pressed key before gliding");
}

// A released board still holding the pressed key is re-keyed even when an
// idle board is available.
void sameKeyReuse() {
    ceres::Synth s(48000,5);auto p=plain();s.setPatch(p);
    s.noteOn(60);s.process();s.noteOff(60);s.process();
    s.noteOn(64);s.process();s.noteOff(64);s.process();
    const auto before=s.allVoiceState();
    s.noteOn(60);
    const auto after=s.allVoiceState();
    require(before[0].note==60&&after[0].held&&!after[1].held&&!after[2].held&&!after[3].held,
            "Repeated key re-keys its own released board");
    s.noteOn(60);
    unsigned held=0;for(const auto& v:s.allVoiceState())held+=v.held;
    require(held==2,"An overlapping press of a held key takes another board");
}

// Keys pressed together are encoded higher note first (guide printed p.16):
// a simultaneous five-key chord keeps its four highest notes in any arrival
// order, the dropped key's note-off releases nothing, and a later fifth key
// is still not stored.
void simultaneousKeysKeepHighest() {
    for(const auto& order:{std::array<int,5>{48,52,55,60,64},std::array<int,5>{64,60,55,52,48},
                           std::array<int,5>{55,64,48,60,52}}) {
        ceres::Synth s(48000,5),reference(48000,5);auto p=plain();s.setPatch(p);reference.setPatch(p);
        for(int note:order)s.noteOn(note,.2+.01*note);
        unsigned held=0;
        std::array<int,4> notes{};
        for(unsigned i=0;i<4;++i){const auto v=s.allVoiceState()[i];held+=v.held;notes[i]=v.note;}
        // Idle boards fill in order, so pressing the stored keys alone, board
        // by board, is the never-stored witness.
        for(int note:notes)reference.noteOn(note,.2+.01*note);
        std::sort(notes.begin(),notes.end());
        require(held==4&&notes==std::array<int,4>{52,55,60,64},"Simultaneous chord keeps its four highest keys");
        s.noteOff(48);held=0;for(const auto& v:s.allVoiceState())held+=v.held;
        require(held==4,"Dropped key's note-off releases nothing");
        for(unsigned n=0;n<64;++n){
            const double a=s.process(),b=reference.process();
            require(a==b,"Replaced boards start as if the dropped key was never stored");
        }
        s.noteOn(72);
        for(const auto& v:s.allVoiceState())require(v.note!=72,"A later key is still not stored");
    }
}

void touchRoutes() {
    ceres::Patch p=plain();p.expressionSource=1;p.pressureWah=1.5;p.pressureFilter=.8;
    p.pressureVibrato=30;p.lfoRate=3;p.keyTrack=0;p.hpCutoff=90;p.lpCutoff=900;
    ceres::Synth s(8000,9);s.setPatch(p);s.noteOn(60,.6);
    for(unsigned n=0;n<500;++n) {
        s.process();
        const double sub=s.modulationState().sub;
        const auto v=s.allVoiceState()[0];
        const double lp=.6*(p.pressureFilter+sub*p.pressureWah);
        near(v.lpFrequency,floorHz+(p.lpCutoff-floorHz)*std::exp2(lp),1e-9,"Touch brilliance and wah reach LP in full");
        near(v.hpFrequency,floorHz+(p.hpCutoff-floorHz)*std::exp2(lp*hpShare),1e-9,"HP gets the 150k/47k share");
        const double cents=.6*p.pressureVibrato*(sub+1./3);
        near(v.oscillatorFrequency,261.6255653005986*std::exp2(cents/1200),1e-9,"Touch vibrato is centred on +.5V");
    }
}

void keys() {
    ceres::Patch p;
    require(ceres::setParameter(p,"glide_mode",1.6)&&p.glideMode==2,"Glide mode rounds");
    require(ceres::setParameter(p,"glide_rate",1000)&&p.glideRate==200,"Glide rate clamps");
    require(ceres::setParameter(p,"pressure_wah",9)&&p.pressureWah==5,"Touch wah clamps");
}
}

int main() {
    try {
        keys();offIsUnchanged();glissandoSteps();portamentoIntegratesHz();sustainModesAtRelease();
        freeBoardGlidesFromLastKey();sameKeyReuse();simultaneousKeysKeepHighest();touchRoutes();
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    std::puts("PASS glide, same-key reuse and touch-route contracts");
}
