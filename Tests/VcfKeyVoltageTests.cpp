// IG00156 key-voltage law (guide printed p.34: fc=125*Kv*2^Vfc; SM1 printed
// p.19 pin sheets). Kv is linear in key Hz and is the board's K terminal, the
// same node that feeds VCO III pin 2 (SM2 p.16), after the KAS glide and the
// PITCH master offset. TRANSPOSITION enters VCO III pin 1 (FT) only. So the
// variable-gm cutoff follows the sounding key exactly, octave for octave,
// except that transposition moves the oscillator and not the filters.
// Expected values come from those laws, not from the engine's own formulas.
#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
// Kv at middle C: .25V at 130.8Hz (Yamaha's C2), doubling per octave.
constexpr double kvMiddleC=.5;
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
ceres::Patch plain() {
    ceres::Patch p;p.saw=1;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=5;
    p.filterInitial=p.filterPeak=0;p.hpCutoff=80;p.lpCutoff=900;
    return p;
}
// Variable-gm part of each stage relative to its middle-C reference, in
// octaves: log2(Kv/Kv(middle C)) by the printed law.
double gmOctaves(double total,double reference) {
    return std::log2((total-floorHz)/(reference-floorHz));
}
double keyVoltage(double oscillatorHz,double transposeOctaves) {
    // Undo transposition (FT) to recover the K terminal's own pitch, then
    // apply the VCO III table: .25V per 130.8128Hz.
    return .25*oscillatorHz*std::exp2(-transposeOctaves)/130.81278265029931;
}

void tracksKeyVoltage() {
    for(int note:{48,55,60,67,72,84,96}) {
        ceres::Synth s(48000,3);auto p=plain();s.setPatch(p);s.noteOn(note);
        for(int n=0;n<64;++n)s.process();
        const auto v=s.allVoiceState()[0];
        const double kv=keyVoltage(v.oscillatorFrequency,0);
        near(kv,.25*std::exp2((note-48)/12.),1e-12,"Kv follows the VCO III table");
        near(gmOctaves(v.hpFrequency,p.hpCutoff),std::log2(kv/kvMiddleC),1e-9,"HP gm is linear in Kv");
        near(gmOctaves(v.lpFrequency,p.lpCutoff),std::log2(kv/kvMiddleC),1e-9,"LP gm is linear in Kv");
    }
}

void transpositionLeavesFilters() {
    ceres::Synth a(48000,3),b(48000,3);auto p=plain();auto q=p;q.octave=1;
    a.setPatch(p);b.setPatch(q);a.noteOn(60);b.noteOn(60);
    for(int n=0;n<64;++n){a.process();b.process();}
    const auto x=a.allVoiceState()[0],y=b.allVoiceState()[0];
    near(y.oscillatorFrequency/x.oscillatorFrequency,2,1e-12,"OCT UP doubles VCO III");
    near(y.hpFrequency,x.hpFrequency,0,"Transposition is VCO III FT only: HP Kv unchanged");
    near(y.lpFrequency,x.lpFrequency,0,"Transposition is VCO III FT only: LP Kv unchanged");
}

void pitchMovesFilters() {
    // PITCH shifts the octave-ladder reference, so it scales K itself.
    ceres::Synth s(48000,3);auto p=plain();p.tuneCents=-500;s.setPatch(p);s.noteOn(60);
    for(int n=0;n<64;++n)s.process();
    const auto v=s.allVoiceState()[0];
    const double kv=keyVoltage(v.oscillatorFrequency,0);
    near(gmOctaves(v.lpFrequency,p.lpCutoff),std::log2(kv/kvMiddleC),1e-9,"PITCH moves the filters with K");
    near(gmOctaves(v.lpFrequency,p.lpCutoff),-500/1200.,1e-9,"PITCH -500 cents reaches the filters");
}

void glideMovesFilters() {
    // Portamento integrates the key voltage that both chips receive, so the
    // cutoff glides with the oscillator at every sample.
    ceres::Synth s(48000,3);auto p=plain();p.glideMode=1;p.glideRate=12;
    s.setPatch(p);s.noteOn(48);
    for(int n=0;n<2000;++n)s.process();
    s.noteOn(72);
    bool moved=false;
    for(int n=0;n<48000;++n) {
        s.process();
        for(const auto& v:s.allVoiceState()) {
            if(!v.held)continue;
            const double kv=keyVoltage(v.oscillatorFrequency,0);
            near(gmOctaves(v.hpFrequency,p.hpCutoff),std::log2(kv/kvMiddleC),1e-9,"HP Kv glides with K");
            near(gmOctaves(v.lpFrequency,p.lpCutoff),std::log2(kv/kvMiddleC),1e-9,"LP Kv glides with K");
            if(v.keyNote==72 && kv>.3 && kv<.95)moved=true;
        }
    }
    if(!moved)throw std::runtime_error("Glide was observed mid-travel");
}
}

int main() {
    try {
        tracksKeyVoltage();transpositionLeavesFilters();pitchMovesFilters();glideMovesFilters();
        std::puts("VCF key-voltage contracts passed");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
