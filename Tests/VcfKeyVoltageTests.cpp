// IG00156 key-voltage law (guide printed p.34: fc=125*Kv*2^Vfc; SM1 printed
// p.19 pin sheets). Kv is linear in key Hz and is the board's K terminal, the
// same node that feeds VCO III pin 2 (SM2 p.16), after the KAS glide and the
// PITCH master offset. TRANSPOSITION enters VCO III pin 1 (FT) only. So the
// variable-gm cutoff follows the sounding key exactly, octave for octave,
// except that transposition moves the oscillator and not the filters. The
// factory presets convert their cutoff columns with the same printed law,
// and their Q columns through the pin-7 divider, each column as the voltage
// its T-board row actually holds under R7's load. The PANEL HPF and LPF
// levers reach the same cutoff rows through a follower and a T71 diode.
// Expected values come from those laws, not from the engine's own formulas.
#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
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
    // Nominal boards, no drift: Kv is read back through the design's VCO
    // law.
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    p.driftCents=0;
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

// A T-board row is the bare divider the guide prints, less a 1S1555 drop
// (preset() models the diode as 1N4148-class: Is 2.52nA, N 1.75), with the
// row's 10k to ground and whatever R7 draws from it beside. The column's
// series resistor is recovered from the printed divider value. Solved here by
// bisection on the row's node equation, with R7's draw given as a function of
// the row voltage, not by the engine's Thevenin fixed point.
template<class Draw> double presetRow(double ideal,Draw r7Draw) {
    const double vt=1.75*25.85e-3,is=2.52e-9;
    double low=0,high=10;
    if(ideal<=0) {
        // No matrix path: the row settles where the 10k takes what R7 gives.
        for(int i=0;i<200;++i) {
            const double mid=(low+high)/2;
            (mid/10e3+r7Draw(mid)<0 ? low : high)=mid;
        }
        return (low+high)/2;
    }
    const double rs=10e3*(10/ideal-1);
    for(int i=0;i<200;++i) {
        const double mid=(low+high)/2,current=mid/10e3+r7Draw(mid);
        const bool below=current<=0 || 10-rs*current-vt*std::log(current/is)>mid;
        (below ? low : high)=mid;
    }
    return (low+high)/2;
}
// R7 (SM1 printed p.14) feeds each cutoff bus through 39k into each of the
// four boards' pin-4 nodes. Each node's other paths to ground (SM2 printed
// p.16 and Overall): rank-B shunt R6 2.2k (HP) or R7 2.4k (LP), the 330k
// trim feed behind its centred B-100k (25k), BR 680k/220k, SUB/TBR/VCF
// 150k/47k each, EG 220k/47k; pin 4 itself taken as high impedance, every
// source at 0V.
double cutoffDraw(double bus,double shunt,double br,double sub,double eg) {
    const double g=1/shunt+1/(330e3+100e3/4)+1/br+3/sub+1/eg;
    const double node=(bus/39e3)/(1/39e3+g);
    return 4*(bus-node)/39e3;
}
// Each Q bus feeds four 220k into pin-7 nodes that return through 220k to the
// RES bus, +10V at RESONANCE MIN, and through the pin's current input, taken
// as 1.72k to ground (ceres.cpp, beside qControlTau).
double qDraw(double bus) {
    const double pin=(bus/220e3+10/220e3)/(2/220e3+1/1720.);
    return 4*(bus-pin)/220e3;
}
double fhBus(double g) { return presetRow(g,[](double v){return cutoffDraw(v,2.2e3,680e3,150e3,220e3);}); }
double flBus(double i) { return presetRow(i,[](double v){return cutoffDraw(v,2.4e3,220e3,47e3,47e3);}); }
double qBus(double q) { return presetRow(q,qDraw); }
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
    // fc=125*Kv*2^Vfc with Vfc read on the FH/FL bus, ahead of its 39k into
    // pin 4 (guide printed pp.33-34): 62.5Hz of gm at middle C for 0V and
    // one octave per lever volt, the same for HP and LP. The fixed 4.7M feed
    // adds its floor once. Chosen by ear 2026-10-20 (vcf-printed-law). The
    // T-board rows g and i drive R7 directly (Overall circuit diagram), so a
    // preset's bus is its divider loaded by four 39k branches as well as the
    // row's own 10k.
    for(const auto& row:factoryFilterColumns) {
        const auto p=ceres::preset(row.name);
        near(std::log2((p.hpCutoff-floorHz)/(125*kvMiddleC)),fhBus(row.g),1e-12,
             "Factory HP cutoff follows the printed law on the loaded FH bus");
        near(std::log2((p.lpCutoff-floorHz)/(125*kvMiddleC)),flBus(row.i),1e-12,
             "Factory LP cutoff follows the printed law on the loaded FL bus");
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
    // (SM1 printed p.14; SM2 printed p.16), so its current, read as VQ,
    // settles at their mean. The presets assume RESONANCE at MIN, RES = +10V
    // (chosen by ear 2026-10-20, brilliance-resonance-preset-q-divider). The
    // engine coordinate r is 1 - VQ/10 (r = 1 is VQ 0V, Q 5). Until then
    // presets put the bus straight on the pin, r = 1 - P/10. P is the row as
    // loaded by the four pin-7 networks, so a zero column sits at .012V.
    for(const auto& row:factoryFilterColumns) {
        const auto p=ceres::preset(row.name);
        near(p.hpResonance,1-(qBus(row.h)+10)/2/10,1e-13,"Factory HP Q pin at (P + RES MIN)/2");
        near(p.lpResonance,1-(qBus(row.j)+10)/2/10,1e-13,"Factory LP Q pin at (P + RES MIN)/2");
    }
}

// PANEL HPF/LPF lever: a B-10K wiper, 0..10V (guide printed p.35), buffered
// by a PRA follower (SM2 printed p.22, IC4) that drives T71 input 7 or 9 and
// its 1S1555 onto the cutoff row. Solved here as a current balance at the
// row: the diode's Shockley current at (lever - V) against what the row's
// 10k and R7 draw at V.
template<class Draw> double panelRow(double travel,Draw r7Draw) {
    const double vt=1.75*25.85e-3,is=2.52e-9,lever=10*travel;
    double low=0,high=lever;
    for(int i=0;i<200;++i) {
        const double mid=(low+high)/2;
        (is*std::expm1((lever-mid)/vt)>mid/10e3+r7Draw(mid) ? low : high)=mid;
    }
    return (low+high)/2;
}

void panelLeversFollowPrintedLaw() {
    const auto hpDraw=[](double v){return cutoffDraw(v,2.2e3,680e3,150e3,220e3);};
    const auto lpDraw=[](double v){return cutoffDraw(v,2.4e3,220e3,47e3,47e3);};
    double lastHp=0,lastLp=0;
    for(double t:{0.,.001,.02,.05,.1,.25,.5,.75,.9,1.}) {
        const double hp=ceres::hpfLeverHz(t),lp=ceres::lpfLeverHz(t);
        near(std::log2((hp-floorHz)/(125*kvMiddleC)),panelRow(t,hpDraw),1e-12,
             "PANEL HPF lever follows the printed law through its T71 diode");
        near(std::log2((lp-floorHz)/(125*kvMiddleC)),panelRow(t,lpDraw),1e-12,
             "PANEL LPF lever follows the printed law through its T71 diode");
        if(t>0&&!(hp>lastHp&&lp>lastLp)) throw std::runtime_error("PANEL cutoff levers rise with travel");
        lastHp=hp;lastLp=lp;
    }
    near(ceres::hpfLeverHz(0),floorHz+125*kvMiddleC,1e-12,"HPF lever at 0: 62.5Hz of gm at middle C");
    near(ceres::hpfLeverHz(-1),ceres::hpfLeverHz(0),0,"Lever travel clamps at 0");
    near(ceres::lpfLeverHz(2),ceres::lpfLeverHz(1),0,"Lever travel clamps at 1");
    if(!std::isnan(ceres::hpfLeverHz(std::numeric_limits<double>::quiet_NaN()))||
       !std::isnan(ceres::lpfLeverHz(std::numeric_limits<double>::infinity())))
        throw std::runtime_error("A nonfinite lever travel returns NaN");
}
}

int main() {
    try {
        tracksKeyVoltage();transpositionLeavesFilters();pitchMovesFilters();glideMovesFilters();
        factoryCutoffsFollowPrintedLaw();factoryQThroughDivider();
        panelLeversFollowPrintedLaw();
        std::puts("VCF key-voltage contracts passed");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
