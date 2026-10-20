// IG00156 key-voltage law (guide printed p.34: fc=125*Kv*2^Vfc; SM1 printed
// p.19 pin sheets). Kv is linear in key Hz and is the board's K terminal, the
// same node that feeds VCO III pin 2 (SM2 p.16), after the KAS glide and the
// PITCH master offset. TRANSPOSITION enters VCO III pin 1 (FT) only. So the
// variable-gm cutoff follows the sounding key exactly, octave for octave,
// except that transposition moves the oscillator and not the filters. The
// factory presets convert their cutoff columns with the same printed law,
// and their Q columns through the pin-7 divider.
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

// A T-board pin sits a 1S1555 drop below the printed ideal divider value
// (preset() models the diode as 1N4148-class: Is 2.52nA, N 1.75, 10k load
// from a 10V source). Solved here by bisection on the node equation, not by
// the engine's fixed-point pass.
double presetPin(double ideal) {
    if(ideal<=0)return 0;
    const double vt=1.75*25.85e-3;
    double low=0,high=ideal;
    for(int i=0;i<200;++i) {
        const double mid=(low+high)/2;
        ((10-vt*std::log(mid/(10e3*2.52e-9)))*ideal/10>mid ? low : high)=mid;
    }
    return (low+high)/2;
}
// Guide printed p.64 (PDF 73), CS-50 tone-preset table, columns g (HP
// cutoff), h (HP Q), i (LP cutoff) and j (LP Q) in volts, with the two
// T-board errata that preset() applies (string1 i 8.70, brass2 i 3.85).
struct FactoryFilterColumns { const char* name; double g,h,i,j; };
const FactoryFilterColumns factoryFilterColumns[]={
    {"string1",5,6.17,8.70,10},{"string2",2.33,10,5.95,10},{"brass1",0,10,5.71,5.71},
    {"brass2",2.17,3.85,3.85,7.87},{"flute",1.18,3.57,5.24,2.33},
    {"electric_piano",1.28,6.41,1.52,3.33},{"clavichord",5.24,5.95,5.71,4.55},
    {"harpsichord",6.41,10,7.87,8.47},{"guitar1",3.85,5.49,5.95,2.50},
    {"guitar2",2.50,0,2.70,0},{"funky1",5,6.17,5.95,0},{"funky2",5.71,0,7.35,0},
    {"funky3",4,2.50,4.76,3.13}};

void factoryCutoffsFollowPrintedLaw() {
    // fc=125*Kv*2^Vfc with Vfc read at the lever, ahead of its 39k into
    // pin 4 (guide printed pp.33-34): 62.5Hz of gm at middle C for 0V and
    // one octave per lever volt, the same for HP and LP. The fixed 4.7M feed
    // adds its floor once. Chosen by ear 2026-10-20 (vcf-printed-law).
    for(const auto& row:factoryFilterColumns) {
        const auto p=ceres::preset(row.name);
        near(std::log2((p.hpCutoff-floorHz)/(125*kvMiddleC)),presetPin(row.g),1e-12,
             "Factory HP cutoff follows the printed law at 1 oct/V");
        near(std::log2((p.lpCutoff-floorHz)/(125*kvMiddleC)),presetPin(row.i),1e-12,
             "Factory LP cutoff follows the printed law at 1 oct/V");
        // The filter EG enters through 220k (HP) and 47k (LP) beside the
        // 39k lever (Overall circuit diagram), so 39/220 and 39/47 oct/V.
        near(p.hpEnvelopeScale,39./220,1e-15,"Factory HP EG at 39k/220k of 1 oct/V");
        near(p.lpEnvelopeScale,39./47,1e-15,"Factory LP EG at 39k/47k of 1 oct/V");
    }
    // The pin sheet's own check point (SM1 printed p.19): VK .25V (C3) with
    // Vf 5V gives 1kHz of gm, here through the engine's key tracking.
    ceres::Synth s(48000,3);auto p=plain();
    p.hpCutoff=p.lpCutoff=floorHz+125*kvMiddleC*std::exp2(5.);
    s.setPatch(p);s.noteOn(48);
    for(int n=0;n<64;++n)s.process();
    const auto v=s.allVoiceState()[0];
    near(v.lpFrequency-floorHz,1000,1e-9,"VK .25V, Vf 5V gives 1kHz (LP)");
    near(v.hpFrequency-floorHz,1000,1e-9,"VK .25V, Vf 5V gives 1kHz (HP)");
}

void factoryQThroughDivider() {
    // Each pin 7 takes its preset Q bus and the RES bus through 220k each
    // (SM1 printed p.14; SM2 printed p.16), so it settles at their mean. The
    // presets assume RESONANCE at MIN, RES = +10V (chosen by ear 2026-10-20,
    // brilliance-resonance-preset-q-divider). The engine coordinate r is
    // 1 - VQ/10 (r = 1 is VQ 0V, Q 5). Until then presets put the bus
    // straight on the pin, r = 1 - P/10.
    for(const auto& row:factoryFilterColumns) {
        const auto p=ceres::preset(row.name);
        near(p.hpResonance,1-(presetPin(row.h)+10)/2/10,1e-13,"Factory HP Q pin at (P + RES MIN)/2");
        near(p.lpResonance,1-(presetPin(row.j)+10)/2/10,1e-13,"Factory LP Q pin at (P + RES MIN)/2");
    }
}
}

int main() {
    try {
        tracksKeyVoltage();transpositionLeavesFilters();pitchMovesFilters();glideMovesFilters();
        factoryCutoffsFollowPrintedLaw();factoryQThroughDivider();
        std::puts("VCF key-voltage contracts passed");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
