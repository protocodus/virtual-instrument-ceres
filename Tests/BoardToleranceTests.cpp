// Board-to-board tolerance of the four M boards. Expected values are rebuilt
// here from the service documents, not read back from the engine's tables:
//  - Guide printed p.65 (PDF 74) tuning method: every VCO trim point lands
//    within +/-1 cent (OCT-UP C2 and C6 keys; C6 key at NORMAL/-1/-2 oct).
//  - Guide printed pp.28-29 (PDF 37-38): VCO III is linear in the key
//    voltage, so between trim points a board follows f = G*(Kv + Voff).
//  - SM2 printed pp.17-18 (PDF 13-14) note 9: IG00156 rank resistors
//    R6 2.7k/2.2k/1.8k, R7 3.3k/2.7k/2.2k on each filter's Vfc node (CS-60
//    SM printed p.15, the "CS-60,50" M circuit); half a rank step bounds the
//    control-sensitivity residual. Static cutoff and Q are trimmed by ear.
//  - Sibling CS-60 SM printed pp.37-38 (PDF 39-40) alignment checks: VCF EG
//    +5V +/-.5V at TP1, VCA EG 10V +/-.2V at TP2.
#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
double cents(double a,double b) {return 1200*std::log2(a/b);}
ceres::Patch plain() {
    ceres::Patch p;p.saw=1;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=5;
    p.filterInitial=p.filterPeak=0;p.hpCutoff=200;p.lpCutoff=3000;
    return p;
}
// State of one physical board playing one key: earlier boards take other keys.
ceres::Synth::VoiceState play(std::uint64_t seed,const ceres::Patch& p,unsigned board,int key,
                              unsigned voices=4) {
    ceres::Synth s(48000,seed);require(s.configure(voices,4),"configure");
    s.setPatch(p);
    const int others[]={30,31,32,33,34,35,36,37};
    for(unsigned i=0;i<board;++i)s.noteOn(others[i]);
    s.noteOn(key);
    for(unsigned n=0;n<4;++n)s.process();
    const auto state=s.allVoiceState()[board];
    require(state.note==key && state.held,"the key landed on the intended board");
    return state;
}

// The engine's frequency against identical boards, in cents.
double error(std::uint64_t seed,ceres::Patch p,unsigned board,int key,double octave) {
    p.octave=octave;
    auto ideal=p;ideal.boardPitchTolerance=0;
    return cents(play(seed,p,board,key).oscillatorFrequency,play(seed,ideal,board,key).oscillatorFrequency);
}

void defaultsAreIdenticalBoards() {
    const ceres::Patch defaults;
    require(defaults.boardPitchTolerance==0 && defaults.boardFilterTolerance==0 &&
            defaults.boardEnvelopeTolerance==0,
            "variation is off by default: existing patches keep identical boards");
    auto p=plain();
    for(unsigned board=1;board<4;++board) {
        const auto a=play(9,p,0,60),b=play(9,p,board,60);
        require(a.oscillatorFrequency==b.oscillatorFrequency && a.hpFrequency==b.hpFrequency &&
                a.lpFrequency==b.lpFrequency,"tolerance 0 keeps the four boards bit-identical");
    }
}

void trimPointsHoldTheirBand() {
    auto p=plain();p.boardPitchTolerance=1;
    double widest=0;
    for(std::uint64_t seed=1;seed<=40;++seed)for(unsigned board=0;board<4;++board) {
        // Trim points: OCT-UP C2 key (MIDI 48 -> 261.6Hz) and C6 key (MIDI 96),
        // and the C6 key at NORMAL, 1 down and 2 down.
        const double points[]={error(seed,p,board,48,1),error(seed,p,board,96,1),
            error(seed,p,board,96,0),error(seed,p,board,96,-1),error(seed,p,board,96,-2)};
        for(double e:points) {
            require(std::abs(e)<=1+1e-9,"every trim point stays inside the printed +/-1 cent");
            widest=std::max(widest,std::abs(e));
        }
        // Between the C2 and C6 keys the offset term is monotone in Kv and
        // equals eLow-eUp (at most 2 cents) at the C2 key, on top of the
        // +/-1 cent NORMAL gain trim.
        for(int key=48;key<=96;key+=6)
            require(std::abs(error(seed,p,board,key,0))<=3+1e-9,
                    "the playable range stays within the trim band plus a 2-cent offset span");
    }
    require(widest>.9,"residuals are spread across the band, not collapsed to zero");
}

void linearVcoBetweenTrimPoints() {
    // Rebuild the board as f = a*f_nominal + b from its two OCT-UP trim
    // points, carry the offset (a fixed Kv offset, so half the Hz at NORMAL)
    // to the NORMAL gain trim, and predict an untrimmed key.
    auto p=plain();p.boardPitchTolerance=1;
    for(std::uint64_t seed:{3u,17u,29u})for(unsigned board=0;board<4;++board) {
        auto up=p;up.octave=1;auto normal=p;normal.octave=0;
        const double fl=play(seed,up,board,48).oscillatorFrequency;
        const double fh=play(seed,up,board,96).oscillatorFrequency;
        const double nl=440*std::exp2((48-57)/12.0), nh=440*std::exp2((96-57)/12.0);
        const double a=(fh-fl)/(nh-nl), b=fl-a*nl;
        const double offsetNormalHz=b/a/2; // Kv offset expressed in NORMAL Hz
        const double c6=440*std::exp2((96-69)/12.0);
        const double gain=play(seed,normal,board,96).oscillatorFrequency/(c6+offsetNormalHz);
        for(int key:{50,60,67,79}) {
            const double nominal=440*std::exp2((key-69)/12.0);
            near(play(seed,normal,board,key).oscillatorFrequency,gain*(nominal+offsetNormalHz),
                 nominal*1e-11,"untrimmed keys follow one linear VCO through the trim points");
        }
    }
}

double variableOctaves(const ceres::Synth::VoiceState& a,const ceres::Synth::VoiceState& b,bool high) {
    return high ? std::log2((a.hpFrequency-floorHz)/(b.hpFrequency-floorHz))
                : std::log2((a.lpFrequency-floorHz)/(b.lpFrequency-floorHz));
}

// The board's Vfc response for one filter: octaves a BRILLIANCE step moves
// the cutoff, relative to identical boards (the Vfc law itself cancels).
double sensitivity(std::uint64_t seed,ceres::Patch p,unsigned board,bool high) {
    auto bright=p;bright.brilliance=5;
    auto ideal=p;ideal.boardFilterTolerance=ideal.boardEnvelopeTolerance=0;
    auto idealBright=ideal;idealBright.brilliance=5;
    return variableOctaves(play(seed,bright,board,60),play(seed,p,board,60),high)/
           variableOctaves(play(seed,idealBright,board,60),play(seed,ideal,board,60),high);
}

void filterRankResidual() {
    // Half a rank step in octaves, from the printed rank resistor values:
    // CS-50 NA03574 R6 and R7, and the CS-60/50 schematic's R7 column.
    const double half=std::log2(std::sqrt(2.7/1.8))/2;
    near(half,std::log2(std::sqrt(3.3/2.2))/2,1e-15,"R6 and R7 ranks step by the same ratio");
    near(half,std::log2(std::sqrt(3.0/2.0))/2,1e-15,"the CS-60/50 R7 column steps by the same ratio");
    auto p=plain();p.boardFilterTolerance=1;p.boardEnvelopeTolerance=1;
    auto halfAmount=p;halfAmount.boardFilterTolerance=.5;
    auto ideal=p;ideal.boardFilterTolerance=ideal.boardEnvelopeTolerance=0;
    double widest=0;
    for(std::uint64_t seed=1;seed<=40;++seed)for(unsigned board=0;board<4;++board) {
        // Static cutoff and Q are trimmed by ear to match the other boards.
        const auto s=play(seed,p,board,60),i=play(seed,ideal,board,60);
        require(s.hpFrequency==i.hpFrequency && s.lpFrequency==i.lpFrequency,
                "the trimmed static cutoff stays identical across boards");
        require(s.oscillatorFrequency==i.oscillatorFrequency,"filter tolerance leaves the VCO alone");
        for(bool high:{true,false}) {
            const double scale=std::log2(sensitivity(seed,p,board,high));
            require(std::abs(scale)<=half+1e-9,"control sensitivity within half a rank step");
            near(std::log2(sensitivity(seed,halfAmount,board,high)),scale/2,1e-9,
                 "amount scales the sensitivity residual in octaves");
            widest=std::max(widest,std::abs(scale));
        }
    }
    require(widest>.13,"sensitivity residuals span the rank bin");
}

void envelopeLevelResidual() {
    // VCF EG: TP1 +5V +/-.5V and -4V +/-.5V (CS-60 SM printed p.37), one
    // buffer for both filters. Measured as the octaves the initial level
    // moves each cutoff, over the board's own Vfc sensitivity.
    auto p=plain();p.boardFilterTolerance=1;p.boardEnvelopeTolerance=1;p.filterAttack=30;
    auto ideal=p;ideal.boardFilterTolerance=ideal.boardEnvelopeTolerance=0;
    auto low=p;low.filterInitial=-2;auto idealLow=ideal;idealLow.filterInitial=-2;
    double widest=0;
    for(std::uint64_t seed=1;seed<=40;++seed)for(unsigned board=0;board<4;++board) {
        double gains[2];
        for(bool high:{true,false}) {
            const double tolerant=variableOctaves(play(seed,low,board,60),play(seed,p,board,60),high);
            const double nominal=variableOctaves(play(seed,idealLow,board,60),play(seed,ideal,board,60),high);
            gains[high]=tolerant/nominal/sensitivity(seed,p,board,high);
        }
        near(gains[0],gains[1],1e-9,"one VCF EG buffer feeds both filters");
        require(std::abs(gains[0]-1)<=.5/5+1e-9,"VCF EG level within the +/-.5V of 5V check band");
        widest=std::max(widest,std::abs(gains[0]-1));
    }
    require(widest>.09,"VCF EG residuals span the band");
}

void ampEnvelopeResidual() {
    // VCA EG: TP2 10V +/-.2V (CS-60 SM printed p.38). The VCA is linear in
    // the EG and everything after the voices is linear at a tiny master
    // volume, so a lone board's output scales by its residual. Board 0 of
    // 40 instruments samples the same per-board draw.
    auto p=plain();p.volume=1e-4;p.boardEnvelopeTolerance=1;
    auto ideal=p;ideal.boardEnvelopeTolerance=0;
    double widest=0;
    for(std::uint64_t seed=1;seed<=40;++seed) {
        ceres::Synth a(48000,seed),b(48000,seed);a.setPatch(p);b.setPatch(ideal);
        a.noteOn(60);b.noteOn(60);
        double xy=0,yy=0;std::array<double,2400> x{},y{};
        for(unsigned n=0;n<x.size();++n){x[n]=a.process();y[n]=b.process();xy+=x[n]*y[n];yy+=y[n]*y[n];}
        const double gain=xy/yy;
        for(unsigned n=0;n<x.size();++n)near(x[n],gain*y[n],1e-6*std::sqrt(yy/x.size()),"a lone board scales by one gain");
        require(std::abs(gain-1)<=.2/10+1e-6,"VCA EG level within the +/-.2V of 10V check band");
        widest=std::max(widest,std::abs(gain-1));
    }
    require(widest>.015,"VCA EG residuals span the band");
}
void sharedStreamUntouched() {
    // SUB noise is the shared random stream; oscillator start phases are its
    // first draws. Neither may move when board tolerances are enabled.
    auto p=plain();p.lfoWaveform=4;
    auto q=p;q.boardPitchTolerance=1;q.boardFilterTolerance=1;q.boardEnvelopeTolerance=1;
    ceres::Synth a(48000,11),b(48000,11);a.setPatch(p);b.setPatch(q);
    for(unsigned i=0;i<4;++i)require(a.allVoiceState()[i].oscillatorPhase==b.allVoiceState()[i].oscillatorPhase,
                                     "seeded start phases unchanged");
    a.noteOn(60);b.noteOn(60);
    for(unsigned n=0;n<4000;++n) {
        a.process();b.process();
        require(a.modulationState().sub==b.modulationState().sub,"shared noise stream unchanged");
    }
}

void boardsAreFixedUnits() {
    auto p=plain();p.boardPitchTolerance=1;p.boardFilterTolerance=1;p.boardEnvelopeTolerance=1;
    for(unsigned board=0;board<4;++board) {
        const auto four=play(5,p,board,60,4),sixteen=play(5,p,board,60,16);
        require(four.oscillatorFrequency==sixteen.oscillatorFrequency && four.hpFrequency==sixteen.hpFrequency &&
                four.lpFrequency==sixteen.lpFrequency,"a board keeps its tolerances at any polyphony");
    }
    // Same seed, same instrument; reset does not redraw a different unit.
    ceres::Synth s(48000,5);s.setPatch(p);s.noteOn(60);s.process();
    const double before=s.allVoiceState()[0].oscillatorFrequency;
    s.reset();s.setPatch(p);s.noteOn(60);s.process();
    require(s.allVoiceState()[0].oscillatorFrequency==before,"reset keeps the same physical boards");
    const auto other=play(6,p,0,60),mine=play(5,p,0,60);
    require(other.oscillatorFrequency!=mine.oscillatorFrequency,"another seed is another instrument");
}
}

int main() {
    try {
        defaultsAreIdenticalBoards();
        trimPointsHoldTheirBand();
        linearVcoBetweenTrimPoints();
        filterRankResidual();
        envelopeLevelResidual();
        ampEnvelopeResidual();
        sharedStreamUntouched();
        boardsAreFixedUnits();
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    std::puts("PASS board tolerance contracts");
    return 0;
}
