// Board-to-board tolerance of the four M boards. Expected values are rebuilt
// here from the service documents, not read back from the engine's tables:
//  - Guide printed p.65 (PDF 74) tuning method: every VCO trim point lands
//    within +/-1 cent (OCT-UP C2 and C6 keys; C6 key at NORMAL/-1/-2 oct).
//  - Guide printed pp.28-29 (PDF 37-38): VCO III is linear in the key
//    voltage, so between trim points a board follows f = G*(Kv + Voff).
//  - Yamaha Service News E-46 (January 1978): from 25 to 50C ambient, the
//    IG00153s of one instrument share one 5-cent C6 figure and two
//    neighbouring 5-cent C1 figures. C1 is Kv 125mV, C6 4V.
//  - SM2 printed pp.17-18 (PDF 13-14) note 9: IG00156 rank resistors
//    R6 2.7k/2.2k/1.8k, R7 3.3k/2.7k/2.2k on each filter's Vfc node (CS-60
//    SM printed p.15, the "CS-60,50" M circuit); half a rank step bounds the
//    control-sensitivity residual. Static cutoff and Q are trimmed by ear,
//    each filter with its own lever at the centre (CS-60 SM printed p.38,
//    III-IV), so only a static cutoff at that centre is every board's.
//  - Sibling CS-60 SM printed pp.37-39 (PDF 39-41) alignment checks: VCF EG
//    +5V +/-.5V at TP1; VCA EG 10V +/-.2V and -200 +/-20mV at TP2; VR14 sets
//    the output at full sustain (VI.4); VR6 sets 0 dBm from the sine alone
//    with all keys down (PRA III).
//  - Sibling CS-80 Adjustment Procedure printed pp.7, 13, 15: EG attacks
//    trimmed to 4ms and 125ms "(+/-5%)"; VR11 sine 1.00 +/-.02Vpp and VR12
//    filtered 800 +/-20mVpp at CP4.
//  - The +/-1 cent of the tuning method again, after the warm-up it asks for
//    (Guide p.65: 10 minutes; CS-80 printed pp.3, 5: 30 minutes): the
//    shipping drift_cents (an owner-requested engineering value) keeps each
//    board's free-running wander inside it.
#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
constexpr double rate=48000;
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why) {
    if(!std::isfinite(a)||std::abs(a-b)>tolerance) {
        std::fprintf(stderr,"actual=%.17g expected=%.17g\n",a,b);throw std::runtime_error(why);
    }
}
double cents(double a,double b) {return 1200*std::log2(a/b);}
double keyHz(int key) {return 440*std::exp2((key-69)/12.0);}
// Identical boards and no free-running drift: each contract below enables
// only the tolerance it checks.
ceres::Patch plain() {
    ceres::Patch p;p.saw=1;p.ampAttack=.001;p.ampSustain=1;p.ampRelease=5;
    p.filterInitial=p.filterPeak=0;p.hpCutoff=200;p.lpCutoff=3000;
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=0;
    p.boardEnvelopeTimeTolerance=p.boardVcaTolerance=p.boardTemperatureRise=0;
    p.driftCents=p.voiceSpreadCents=0;
    return p;
}
ceres::Patch everything(ceres::Patch p) {
    p.boardPitchTolerance=p.boardFilterTolerance=p.boardEnvelopeTolerance=1;
    p.boardEnvelopeTimeTolerance=p.boardVcaTolerance=1;p.boardTemperatureRise=25;
    return p;
}
// State of one physical board playing one key: earlier boards take other keys.
ceres::Synth::VoiceState play(std::uint64_t seed,const ceres::Patch& p,unsigned board,int key,
                              unsigned voices=4) {
    ceres::Synth s(rate,seed);require(s.configure(voices,4),"configure");
    s.setPatch(p);
    const int others[]={30,31,32,33,34,35,36,37};
    for(unsigned i=0;i<board;++i)s.noteOn(others[i]);
    s.noteOn(key);
    for(unsigned n=0;n<4;++n)s.process();
    const auto state=s.allVoiceState()[board];
    require(state.note==key && state.held,"the key landed on the intended board");
    return state;
}
double centsVersus(std::uint64_t seed,const ceres::Patch& p,const ceres::Patch& reference,
                   unsigned board,int key) {
    return cents(play(seed,p,board,key).oscillatorFrequency,play(seed,reference,board,key).oscillatorFrequency);
}

// The engine's frequency against identical boards, in cents.
double error(std::uint64_t seed,ceres::Patch p,unsigned board,int key,double octave) {
    p.octave=octave;
    auto ideal=p;ideal.boardPitchTolerance=0;
    return centsVersus(seed,p,ideal,board,key);
}

// One board's EG voltages, one EG step per sample (oversampling 1). The key
// is released at frame `release`.
struct Trace {std::vector<double> amp,filter;};
Trace trace(std::uint64_t seed,const ceres::Patch& p,unsigned board,unsigned frames,unsigned release) {
    ceres::Synth s(rate,seed);require(s.configure(4,1),"configure");
    s.setPatch(p);
    const int others[]={30,31,32};
    for(unsigned i=0;i<board;++i)s.noteOn(others[i]);
    s.noteOn(60);
    Trace t;
    for(unsigned n=0;n<frames;++n) {
        if(n==release)s.noteOff(60);
        s.process();
        const auto v=s.allVoiceState()[board];
        if(n==0)require(v.note==60 && v.held,"the key landed on the intended board");
        t.amp.push_back(v.ampEnvelope);t.filter.push_back(v.filterEnvelope);
    }
    return t;
}

// Output of one board playing middle C alone: the boards before it are keyed
// and released before the first sample, so their VCA EGs never leave the
// floor and their VCAs pass nothing.
std::vector<double> solo(std::uint64_t seed,const ceres::Patch& p,unsigned board,unsigned frames) {
    ceres::Synth s(rate,seed);require(s.configure(4,1),"configure");
    s.setPatch(p);
    for(unsigned i=0;i<board;++i){s.noteOn(30+int(i));s.noteOff(30+int(i));}
    s.noteOn(60);
    std::vector<double> out(frames);
    for(unsigned n=0;n<frames;++n) {
        out[n]=s.process();
        if(n==0)require(s.allVoiceState()[board].note==60,"the key landed on the intended board");
    }
    return out;
}
// Least-squares gain of x over y on [from,size), with the relative residual.
struct Gain {double gain,residual;};
Gain gainOf(const std::vector<double>& x,const std::vector<double>& y,std::size_t from) {
    double xy=0,yy=0;
    for(std::size_t n=from;n<x.size();++n){xy+=x[n]*y[n];yy+=y[n]*y[n];}
    const double g=xy/yy;double rr=0;
    for(std::size_t n=from;n<x.size();++n)rr+=(x[n]-g*y[n])*(x[n]-g*y[n]);
    require(yy>0,"the reference board sounds");
    return {g,std::sqrt(rr/yy)};
}

void defaultsShipTheVerdict() {
    // Listening verdict B (2026-10-20) ships the pitch, filter and envelope
    // residuals; the owner took the time and VCA candidates unheard
    // (2026-10-21). Temperature has no defensible default.
    const ceres::Patch defaults;
    require(defaults.boardPitchTolerance==1 && defaults.boardFilterTolerance==1 &&
            defaults.boardEnvelopeTolerance==1 && defaults.boardEnvelopeTimeTolerance==1 &&
            defaults.boardVcaTolerance==1 && defaults.boardTemperatureRise==0,
            "every board residual ships; temperature does not");
    for(const char* name:{"init","brass1","string1"}) {
        const auto preset=ceres::preset(name);
        require(preset.boardPitchTolerance==1 && preset.boardFilterTolerance==1 &&
                preset.boardEnvelopeTolerance==1 && preset.boardEnvelopeTimeTolerance==1 &&
                preset.boardVcaTolerance==1,"factory presets play the shipping boards");
    }
    auto shipping=plain();
    shipping.boardPitchTolerance=shipping.boardFilterTolerance=shipping.boardEnvelopeTolerance=1;
    require(play(9,shipping,0,60).oscillatorFrequency!=play(9,shipping,1,60).oscillatorFrequency,
            "the shipping boards differ");
    auto p=plain();
    for(unsigned board=1;board<4;++board) {
        const auto a=play(9,p,0,60),b=play(9,p,board,60);
        require(a.oscillatorFrequency==b.oscillatorFrequency && a.hpFrequency==b.hpFrequency &&
                a.lpFrequency==b.lpFrequency && a.ampEnvelope==b.ampEnvelope &&
                a.filterEnvelope==b.filterEnvelope,"tolerance 0 keeps the four boards bit-identical");
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

void thermalDriftWithinTheRanks() {
    // E-46 ranks each IG00153 by its pitch change at C1 and C6 from 25 to
    // 50C, one figure per 5 cents. One instrument mixes one C6 figure and two
    // neighbouring C1 figures: about the figures' centres (the drift the
    // boards share, not modelled) each board moves within +/-2.5 cents at C6
    // and +/-5 at C1. C1 and C6 are MIDI 36 and 96 at A=440, x1.000002 of
    // the Kv 125mV and 4V points: hence the small margins on the bands.
    const auto tuned=plain();
    auto warm=tuned;warm.boardTemperatureRise=25;
    const double f1=keyHz(36),f6=keyHz(96);
    double widestHigh=0,widestLow=0;
    for(std::uint64_t seed=1;seed<=40;++seed) {
        double lowMin=1e9,lowMax=-1e9,highMin=1e9,highMax=-1e9;
        for(unsigned board=0;board<4;++board) {
            const double low=centsVersus(seed,warm,tuned,board,36);
            const double high=centsVersus(seed,warm,tuned,board,96);
            require(std::abs(high)<=2.5+1e-6,"C6 drifts within one 5-cent figure");
            require(std::abs(low)<=5+1e-3,"C1 drifts within two neighbouring 5-cent figures");
            lowMin=std::min(lowMin,low);lowMax=std::max(lowMax,low);
            highMin=std::min(highMin,high);highMax=std::max(highMax,high);
            widestHigh=std::max(widestHigh,std::abs(high));widestLow=std::max(widestLow,std::abs(low));
            // One linear VCO drifts in gain and Kv offset: rebuild both from
            // C1 and C6 and predict the keys between and above.
            const double ratio=std::exp2((low-high)/1200);
            const double offsetHz=(ratio-1)/(1/f1-ratio/f6);
            const double gainCents=high-1200*std::log2(1+offsetHz/f6);
            for(int key:{48,55,60,72,84})
                near(centsVersus(seed,warm,tuned,board,key),gainCents+1200*std::log2(1+offsetHz/keyHz(key)),
                     1e-7,"thermal drift is one linear VCO's gain and offset drift");
        }
        require(highMax-highMin<5 && lowMax-lowMin<10,
                "one instrument's boards differ by less than one C6 figure and two C1 figures");
    }
    require(widestHigh>2.2 && widestLow>4.4,"thermal residuals span the rank windows");
    // Linear in the temperature change (E-46 gives the 25K step only), the
    // same at every TRANSPOSE position (the IC is ranked, not the Rn
    // resistors), and on top of the trim residual.
    auto mild=tuned;mild.boardTemperatureRise=10;
    auto cool=tuned;cool.boardTemperatureRise=-25;
    auto trimmed=tuned;trimmed.boardPitchTolerance=1;
    auto both=warm;both.boardPitchTolerance=1;
    for(std::uint64_t seed:{2u,23u})for(unsigned board=0;board<4;++board)for(int key:{36,60,96}) {
        const double drift=centsVersus(seed,warm,tuned,board,key);
        near(centsVersus(seed,mild,tuned,board,key),.4*drift,1e-9,"drift scales with the temperature change");
        near(centsVersus(seed,cool,tuned,board,key),-drift,1e-9,"cooling reverses the drift");
        for(double octave:{-2.,1.}) {
            auto a=warm,b=tuned;a.octave=b.octave=octave;
            near(centsVersus(seed,a,b,board,key),drift,1e-9,"thermal drift ignores TRANSPOSE");
        }
        near(centsVersus(seed,both,tuned,board,key),centsVersus(seed,trimmed,tuned,board,key)+drift,1e-9,
             "thermal drift adds to the trim residual");
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
        const auto s=play(seed,p,board,60),i=play(seed,ideal,board,60);
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

// The trims are offsets set with each filter's lever at its centre (CS-60 SM
// printed p.38: "HPF lever to the center position", "LPF to center
// position"). A static cutoff there is every board's; away from it the
// board's sensitivity, measured above through BRILLIANCE, stretches the gm
// octaves from that point, which the printed law makes the bus volts.
void staticCutoffFromTheLeverCentre() {
    const double hpCentre=ceres::hpfLeverHz(.5)-floorHz, lpCentre=ceres::lpfLeverHz(.5)-floorHz;
    auto centred=plain();centred.boardFilterTolerance=1;
    centred.hpCutoff=hpCentre+floorHz;centred.lpCutoff=lpCentre+floorHz;
    auto atFloor=centred;atFloor.hpCutoff=floorHz-1;atFloor.lpCutoff=floorHz;
    const auto ideal=[](ceres::Patch q){q.boardFilterTolerance=0;return q;};
    double widest=0;
    for(std::uint64_t seed=1;seed<=40;++seed)for(unsigned board=0;board<4;++board) {
        const auto c=play(seed,centred,board,60),ci=play(seed,ideal(centred),board,60);
        near(variableOctaves(c,ci,true),0,1e-12,"HP static cutoff is every board's at the HPF lever centre");
        near(variableOctaves(c,ci,false),0,1e-12,"LP static cutoff is every board's at the LPF lever centre");
        const auto f=play(seed,atFloor,board,60),fi=play(seed,ideal(atFloor),board,60);
        require(f.hpFrequency==fi.hpFrequency && f.lpFrequency==fi.lpFrequency,
                "a cutoff at the 4.7M floor has no gm to spread");
        for(double amount:{1.,.5}) {
            auto q=centred;q.boardFilterTolerance=amount;
            const double hpScale=sensitivity(seed,q,board,true), lpScale=sensitivity(seed,q,board,false);
            for(double hp:{40.,200.,6000.})for(double lp:{150.,3000.,12000.}) {
                auto p=q;p.hpCutoff=hp;p.lpCutoff=lp;
                const auto s=play(seed,p,board,60),i=play(seed,ideal(p),board,60);
                const double hpOctaves=(hpScale-1)*std::log2((hp-floorHz)/hpCentre);
                const double lpOctaves=(lpScale-1)*std::log2((lp-floorHz)/lpCentre);
                near(variableOctaves(s,i,true),hpOctaves,1e-9,"HP static cutoff spreads from the lever centre");
                near(variableOctaves(s,i,false),lpOctaves,1e-9,"LP static cutoff spreads from the lever centre");
                if(amount==1)widest=std::max(widest,std::abs(hpOctaves));
            }
            // Kv scales gm at pin 2, outside the rank network, so every key
            // keeps the board's spread.
            if(amount==1)for(int key:{36,84}) {
                auto p=q;p.hpCutoff=200;p.lpCutoff=3000;
                const auto s=play(seed,p,board,key),i=play(seed,ideal(p),board,key);
                near(variableOctaves(s,i,true),(hpScale-1)*std::log2((200-floorHz)/hpCentre),1e-9,
                     "Kv leaves the HP spread alone");
                near(variableOctaves(s,i,false),(lpScale-1)*std::log2((3000-floorHz)/lpCentre),1e-9,
                     "Kv leaves the LP spread alone");
            }
        }
    }
    require(widest>.25,"three octaves below the centre, boards spread by a quarter octave of gm");
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

void ampEnvelopeTrims() {
    // VCA EG (CS-60 p.38, EG-VCA 1-2; CS-80 p.13 for the floor): the attack
    // ends where the comparator trips, TP2 10V +/-.2V, a peak of the board's
    // own; held and released levels sit VR17's -200 +/-20mV below the
    // panel's. Read off the EG at full LEVEL with SUSTAIN at 0.
    auto p=plain();p.ampAttack=.001;p.ampDecay=.002;p.ampSustain=0;p.boardEnvelopeTolerance=1;
    auto ideal=p;ideal.boardEnvelopeTolerance=0;
    double widestPeak=0,widestFloor=0;
    for(std::uint64_t seed=1;seed<=20;++seed)for(unsigned board=0;board<4;++board) {
        const auto t=trace(seed,p,board,2400,~0u),i=trace(seed,ideal,board,2400,~0u);
        const double peak=*std::max_element(t.amp.begin(),t.amp.end());
        const double held=t.amp.back();
        require(*std::max_element(i.amp.begin(),i.amp.end())==1 && i.amp.back()==-.2/10,
                "identical boards peak at 10V and hold at -200mV");
        require(std::abs(peak-1)<=.2/10+1e-12,"VCA EG peak within 10V +/-.2V");
        require(std::abs(held+.2/10)<=.02/10+1e-12,"VCA EG floor within -200 +/-20mV");
        widestPeak=std::max(widestPeak,std::abs(peak-1));
        widestFloor=std::max(widestFloor,std::abs(held+.2/10));
    }
    require(widestPeak>.015 && widestFloor>.0015,"VCA EG residuals span their bands");
}

void outputTrimAtFullSustain() {
    // VR14 is trimmed after the EG checks with A, D, R short and S at 10
    // (CS-60 VI.4; "CS-50 is 0.6V"): at full sustain every board gives the
    // nominal output; below it the board's own floor shows. The output after
    // the VCA is linear at this volume: one gain per board, once the
    // attack's difference has left the output couplings.
    for(double sustain:{1.,.3}) {
        auto p=plain();p.volume=1e-4;p.sine=0;p.ampAttack=.001;p.ampDecay=.002;p.ampSustain=sustain;
        p.boardEnvelopeTolerance=1;
        auto ideal=p;ideal.boardEnvelopeTolerance=0;
        for(std::uint64_t seed=1;seed<=12;++seed) {
            const auto x=solo(seed,p,0,72000),y=solo(seed,ideal,0,72000);
            const double held=trace(seed,p,0,2400,~0u).amp.back(); // SL + this board's floor
            const double floor=held-sustain, nominal=-.2/10;
            const auto g=gainOf(x,y,48000);
            near(g.gain,(sustain+floor)/(1+floor)*(1+nominal)/(sustain+nominal),2e-7,
                 "VR14 restores each board's full-sustain output");
            require(g.residual<1e-4,"a board's held output differs by one gain");
        }
    }
}

// Duration of an attack from its per-sample increment over k samples: the
// RC charge aims headroom*span past the peak, so it spans ln(1+1/h) RC.
double attackSeconds(const std::vector<double>& v,unsigned from,unsigned k,double headroom) {
    const double ratio=(v[from+2*k]-v[from+k])/(v[from+k]-v[from]);
    return std::log1p(1/headroom)*k/(-rate*std::log(ratio));
}
double t60Seconds(const std::vector<double>& v,unsigned from,unsigned k) {
    const double ratio=(v[from+2*k]-v[from+k])/(v[from+k]-v[from]);
    return std::log(1000.0)*k/(-rate*std::log(ratio));
}
void envelopeTimesHoldTheirTrims() {
    // CS-80 printed pp.7 and 13 trim each EG's attack to 4ms at 8V and 125ms
    // at 3V, "(+/-5%)" each; the CS-60 (pp.37-38) uses the same trimmers and
    // targets. One exponential converter per EG: the log-time error is
    // linear in its control voltage, so linear in log time through and past
    // the two trims. Decay and release run on the same converter: the
    // factory conversion (preset()) makes an attack 5 RC and a D/R time a
    // T60 of ln(1000) RC, so a T60 shares the RC of a 5*T60/ln(1000) attack.
    const double headroom=1/std::expm1(5.0); // the factory 5-RC attack
    auto timed=[&](double attack) {
        auto p=plain();p.boardEnvelopeTimeTolerance=1;
        p.ampAttack=p.filterAttack=attack;p.ampAttackHeadroom=p.filterAttackHeadroom=headroom;
        p.ampDecay=p.filterDecay=.35;p.ampSustain=.5;p.ampRelease=p.filterRelease=2;
        p.filterInitial=-1;p.filterPeak=1;
        return p;
    };
    auto prediction=[](double fast,double slow,double attack) {
        const double span=std::log(attack/.004)/std::log(.125/.004);
        return std::exp(std::log(fast)+(std::log(slow)-std::log(fast))*span);
    };
    const double decayAttack=5*.35/std::log(1000.0),releaseAttack=5*2/std::log(1000.0);
    double widest=0;bool independent=false;
    for(std::uint64_t seed=1;seed<=10;++seed)for(unsigned board=0;board<4;++board) {
        std::array<double,2> fast{},slow{};
        for(unsigned eg=0;eg<2;++eg) {
            auto pick=[&](const Trace& t) -> const std::vector<double>& {return eg ? t.filter : t.amp;};
            const auto f=trace(seed,timed(.004),board,1400,1200);
            const auto s=trace(seed,timed(.125),board,200,~0u);
            const auto l=trace(seed,timed(1),board,200,~0u);
            fast[eg]=attackSeconds(pick(f),1,48,headroom)/.004;
            slow[eg]=attackSeconds(pick(s),1,48,headroom)/.125;
            for(double factor:{fast[eg],slow[eg]}) {
                require(std::abs(factor-1)<=.05+1e-7,"each attack trim within its +/-5%");
                widest=std::max(widest,std::abs(factor-1));
            }
            near(attackSeconds(pick(l),1,48,headroom)/prediction(fast[eg],slow[eg],1),1,1e-7,
                 "the converter's log-time error is linear past the trims");
            const auto& v=pick(f);
            const auto top=static_cast<unsigned>(std::max_element(v.begin(),v.begin()+1200)-v.begin());
            near(t60Seconds(v,top+2,48)/.35/prediction(fast[eg],slow[eg],decayAttack),1,1e-6,
                 "decay runs on the same converter");
            near(t60Seconds(v,1201,48)/2/prediction(fast[eg],slow[eg],releaseAttack),1,1e-6,
                 "release runs on the same converter");
        }
        independent=independent || fast[0]!=fast[1];
        // Half the amount is half the log-time error.
        auto half=timed(.004);half.boardEnvelopeTimeTolerance=.5;
        near(std::log(attackSeconds(trace(seed,half,board,200,~0u).amp,1,48,headroom)/.004),
             std::log(fast[0])/2,1e-7,"amount scales the log-time error");
    }
    require(widest>.045,"time residuals span the band");
    require(independent,"the VCF and VCA EGs are trimmed separately");
    // Identical boards keep the patch's times exactly (the measurement's own check).
    auto p=timed(.004);p.boardEnvelopeTimeTolerance=0;
    const auto t=trace(1,p,0,1400,1200);
    near(attackSeconds(t.amp,1,48,headroom),.004,1e-12,"nominal attack");
    near(t60Seconds(t.amp,1201,48),2,1e-6,"nominal release");
}

void vcaBalance() {
    // CS-80 p.15: VR11 sets the sine to 1.00 +/-.02Vpp and VR12 the filtered
    // pulse to 800 +/-20mVpp at CP4; VR13/VR14 then restore the filtered
    // path (CS-60 VI.3-4), so a board's sine keeps s = (1+e11)/(1+e12) of
    // its level. VR6, trimmed last for 0 dBm from four sines (PRA III),
    // scales every path by V with V^2*sum(s^2) = 4 (RMS meter, incoherent
    // notes, as the engine's output reference assumes).
    auto sine=plain();sine.saw=sine.pulse=sine.noise=0;sine.sine=1;sine.volume=1e-4;sine.boardVcaTolerance=1;
    auto filtered=plain();filtered.sine=0;filtered.volume=1e-4;filtered.boardVcaTolerance=1;
    auto idealSine=sine,idealFiltered=filtered;idealSine.boardVcaTolerance=idealFiltered.boardVcaTolerance=0;
    double widest=0;
    for(std::uint64_t seed=1;seed<=10;++seed) {
        double power=0,common=0;
        for(unsigned board=0;board<4;++board) {
            const auto gs=gainOf(solo(seed,sine,board,6000),solo(seed,idealSine,board,6000),0);
            const auto gf=gainOf(solo(seed,filtered,board,6000),solo(seed,idealFiltered,board,6000),0);
            require(gs.residual<1e-6 && gf.residual<1e-6,"each path changes by one gain");
            if(board==0)common=gf.gain;
            near(gf.gain,common,1e-7,"the filtered path keeps its level up to the common VR6 gain");
            const double s=gs.gain/gf.gain;
            require(s>=(1-.02)/(1+.02/.8)-1e-7 && s<=(1+.02)/(1-.02/.8)+1e-7,
                    "the sine keeps its trimmed balance to the filtered path");
            power+=s*s;widest=std::max(widest,std::abs(s-1));
        }
        near(common*common*power,4,1e-6,"VR6 holds the four-sine reference");
    }
    require(widest>.03,"balance residuals span the trims' bands");
}

void outputReferenceHolds() {
    // The output stage's 0 dBm anchor (Guide p.7; CS-60 PRA III: sine,
    // SUSTAIN and LEVEL at maximum, four keys down; four CP4 1Vpp sines pass
    // .98 each at full SUSTAIN and .18 at full VOLUME, Tests/OutputStageTests)
    // with every tolerance at full spread. VR14 and VR6 are trimmed with
    // these boards in place, so the instrument stays on its reference within
    // that contract's 0.1 dB, as identical boards do.
    // The WSC sine is the differential-pair shaper tanh(k*tri)/tanh(k),
    // k=1.4020, whose mean square is (1-tanh(k)/k)/tanh(k)^2 (OutputStageTests
    // checks it by quadrature).
    const double k=1.4020;
    const double referenceRms=.18*std::sqrt(4*(1-std::tanh(k)/k)/std::pow(std::tanh(k),2))*.98;
    auto p=plain();p.saw=p.pulse=p.noise=0;p.sine=1;p.filterLevel=0;p.ampLevel=1;p.volume=1;
    const auto tolerant=everything(p);
    for(std::uint64_t seed:{3u,8u,21u}) {
        double rms[2];
        for(unsigned which=0;which<2;++which) {
            ceres::Synth s(rate,seed);s.setPatch(which ? tolerant : p);
            for(int n:{48,52,55,60})s.noteOn(n);
            double energy=0;
            for(int i=0;i<3*48000;++i){const double x=s.process();if(i>=48000)energy+=x*x;}
            rms[which]=std::sqrt(energy/(2*48000));
        }
        require(std::abs(20*std::log10(rms[0]/referenceRms))<.1,"identical boards keep the 0 dBm reference");
        require(std::abs(20*std::log10(rms[1]/referenceRms))<.1,"the boards keep the 0 dBm reference");
    }
}

void sharedStreamUntouched() {
    // SUB noise is the shared random stream; oscillator start phases are its
    // first draws. Neither may move when board tolerances are enabled.
    auto p=plain();p.lfoWaveform=4;
    const auto q=everything(p);
    ceres::Synth a(rate,11),b(rate,11);a.setPatch(p);b.setPatch(q);
    for(unsigned i=0;i<4;++i)require(a.allVoiceState()[i].oscillatorPhase==b.allVoiceState()[i].oscillatorPhase,
                                     "seeded start phases unchanged");
    a.noteOn(60);b.noteOn(60);
    for(unsigned n=0;n<4000;++n) {
        a.process();b.process();
        require(a.modulationState().sub==b.modulationState().sub,"shared noise stream unchanged");
    }
}

void boardsAreFixedUnits() {
    auto p=everything(plain());p.filterInitial=-1;p.filterPeak=1;
    for(unsigned board=0;board<4;++board) {
        const auto four=play(5,p,board,60,4),sixteen=play(5,p,board,60,16);
        require(four.oscillatorFrequency==sixteen.oscillatorFrequency && four.hpFrequency==sixteen.hpFrequency &&
                four.lpFrequency==sixteen.lpFrequency && four.ampEnvelope==sixteen.ampEnvelope &&
                four.filterEnvelope==sixteen.filterEnvelope,"a board keeps its tolerances at any polyphony");
    }
    // Same seed, same instrument; reset does not redraw a different unit.
    ceres::Synth s(rate,5);s.setPatch(p);s.noteOn(60);s.process();
    const double before=s.allVoiceState()[0].oscillatorFrequency;
    s.reset();s.setPatch(p);s.noteOn(60);s.process();
    require(s.allVoiceState()[0].oscillatorFrequency==before,"reset keeps the same physical boards");
    const auto other=play(6,p,0,60),mine=play(5,p,0,60);
    require(other.oscillatorFrequency!=mine.oscillatorFrequency,"another seed is another instrument");
}

// drift_cents ships at 1, an owner-requested engineering value: the wander
// of each board peaks at the +/-1 cent the tuning method holds a warmed-up
// board to. Its time course has no source and is not checked here.
void driftStaysInTheTuningBand() {
    const ceres::Patch defaults;
    require(defaults.driftCents==1 && ceres::preset("brass1").driftCents==1,
            "drift ships at the tuning tolerance, factory presets included");
    auto p=plain();p.driftCents=defaults.driftCents;p.vibratoCents=0;
    auto twice=p;twice.driftCents=2;
    ceres::Synth s(8000,50),d(8000,50);
    require(s.configure(4,1) && d.configure(4,1),"configure");
    s.setPatch(p);d.setPatch(twice);
    for(unsigned board=0;board<4;++board){s.noteOn(60);d.noteOn(60);}
    std::array<double,4> peak{},power{};
    double apart=0;unsigned samples=0;
    for(unsigned n=0;n<30*8000;++n) {
        s.process();d.process();
        if(n%80)continue;
        const auto a=s.voiceState(),b=d.voiceState();
        for(unsigned k=0;k<4;++k) {
            require(a[k].note==60 && a[k].held,"four boards hold one key");
            const double c=cents(a[k].oscillatorFrequency,keyHz(60));
            near(cents(b[k].oscillatorFrequency,keyHz(60)),2*c,1e-9,"drift_cents is the wander's peak depth");
            peak[k]=std::max(peak[k],std::abs(c));power[k]+=c*c;
        }
        apart+=std::pow(cents(a[0].oscillatorFrequency,a[1].oscillatorFrequency),2);++samples;
    }
    for(unsigned k=0;k<4;++k) {
        require(peak[k]<=1,"each board's wander stays within +/-1 cent");
        require(peak[k]>.5 && std::sqrt(power[k]/samples)>.2,"each board wanders");
    }
    require(std::sqrt(apart/samples)>.2,"the boards wander independently");
}
}

int main() {
    try {
        defaultsShipTheVerdict();
        driftStaysInTheTuningBand();
        trimPointsHoldTheirBand();
        linearVcoBetweenTrimPoints();
        thermalDriftWithinTheRanks();
        filterRankResidual();
        staticCutoffFromTheLeverCentre();
        envelopeLevelResidual();
        ampEnvelopeTrims();
        outputTrimAtFullSustain();
        envelopeTimesHoldTheirTrims();
        vcaBalance();
        outputReferenceHolds();
        sharedStreamUntouched();
        boardsAreFixedUnits();
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    std::puts("PASS board tolerance contracts");
    return 0;
}
