#include "ceres.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <cstring>
#if !defined(CERES_EMBEDDED_TARGET)
#include <stdexcept>
#endif
#include <utility>

namespace ceres {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
// The Rack's toolchain compiles this file with -fno-builtin against the
// SDK's own libc++, where std::abs, std::isfinite, std::copysign and
// std::sqrt become out-of-line library calls (and sqrt a bit-serial
// software one); Acustra measured about a fifth of its batch in them. The
// __builtin_ forms are the same IEEE operations, inlined whatever the
// flags, so the results are bit for bit the same. MSVC has no builtins of
// these names and inlines the std:: forms itself.
#if defined(__GNUC__) || defined(__clang__)
inline double inlineAbs(double x) { return __builtin_fabs(x); }
inline double inlineCopysign(double x, double sign) { return __builtin_copysign(x, sign); }
inline double inlineSqrt(double x) { return __builtin_sqrt(x); }
inline bool inlineFinite(double x) { return __builtin_isfinite(x); }
#else
inline double inlineAbs(double x) { return std::abs(x); }
inline double inlineCopysign(double x, double sign) { return std::copysign(x, sign); }
inline double inlineSqrt(double x) { return std::sqrt(x); }
inline bool inlineFinite(double x) { return std::isfinite(x); }
#endif
// tanh is the engine's most frequent transcendental: the OTA filters solve
// one per Newton iteration per stage, the WSC shaper one per sample and the
// ring carrier's steering one per sample. glibc's tanh reaches expm1 and
// costs about 2.6 times this rational form in a dependent chain (measured
// here; see Docs/benchmarks/cpu-approximations-2026-10-23.md). Cephes
// tanh.c (Moshier): for |x| < .625, x + x^3 P(x^2)/Q(x^2); beyond, the
// identity 1 - 2/(exp(2|x|)+1). Swept against glibc over 1e-9..30 at 14
// million points: at most 2 ulp (4.2e-16 relative). Not bit-identical to
// std::tanh, so this and everything after it differ from 1.0.3 at the last
// bit.
double fastTanh(double x) {
    const double ax=inlineAbs(x);
    if (ax<.625) {
        const double s=x*x;
        const double p=(-9.64399179425052238628e-1*s-9.92877231001918586564e1)*s
            -1.61468768441708447952e3;
        const double q=((s+1.12811678491632931402e2)*s+2.23548839060100448583e3)*s
            +4.84406305325125486048e3;
        return x+x*s*(p/q);
    }
    const double y=1-2/(std::exp(2*ax)+1);
    return x<0 ? -y : y;
}
// The filters' TPT prewarp, tan(pi*fc/fs), is evaluated whenever a cutoff
// moves (every sample under envelope, SUB or glide). The argument is at most
// .22 pi (the cutoff clamp), inside Cephes tan.c's |x| <= pi/4 rational,
// x + x^3 P(x^2)/Q(x^2): at most 1 ulp (2.2e-16 relative) against glibc over
// 1e-7..0.22 pi, at about a third of glibc's instructions.
double prewarpTan(double x) {
    const double z=x*x;
    const double p=(-1.30936939181383777646e4*z+1.15351664838587416140e6)*z
        -1.79565251976484877988e7;
    const double q=(((z+1.36812963470692954678e4)*z-1.32089234440210967447e6)*z
        +2.50083801823357915839e7)*z-5.38695755929454629881e7;
    return x+x*z*(p/q);
}
constexpr double filterCapacitance = 1.5e-9, filterFeedResistance = 4.7e6;
constexpr double filterFrequencyFloor = 1/(2*pi*filterCapacitance*filterFeedResistance);
// Cutoff lever law: the printed IG00156 law fc=125*Kv*2^Vfc (guide printed
// p.34; SM1 printed p.19, "VK .25V, Vf 5V -> 1kHz") is one octave per volt
// of Vfc. Vfc is read on the FH/FL lever bus (R7 pins 19/20), ahead of its
// 39k into pin 4; that is the one inference (guide printed p.33 draws Vfc
// reaching pin 4 through a resistor; see preset()). Chosen by ear 2026-10-20
// (listening test 2026-10-20-vcf-printed-law). The same slope serves HP and
// LP, and the law puts 125*.5V = 62.5Hz of gm at middle C (Kv=.5V) for 0V.
constexpr double leverOctavesPerVolt=1, gmHzAtMiddleC=125*.5;
// Direct R7 matrix, CS-50 service1 PDF8/printed14: every other cutoff input
// shares the lever's node through its own resistor, so it moves the cutoff
// 39k/Rin times as far per volt as the lever.
// Interpret BR as actual loaded voltage with stiff preset/source nodes,
// not raw preset-divider CV or a finite-impedance pot position.
constexpr double hpBrillianceScale=leverOctavesPerVolt*39/680, lpBrillianceScale=leverOctavesPerVolt*39/220;
// LP-octave depths (SUB, touch) reach HP through 150k against LP's 47k.
constexpr double hpSubRelativeScale=47./150.;
// Guide printed p.56: touch level amplifier I is trimmed so a 0V-centred SUB
// input leaves centred on +.5V; the SUB source is fixed at 3Vpp (p.55).
constexpr double touchVibratoBias=.5/1.5;
// R7 LC sums LEVEL (10V full, 270k) and touch LEVEL (6.0V full, 150k; guide
// printed p.55 calibration) into common VCA2: full touch adds 1.08 times the
// full-LEVEL current (SM1 printed p.14).
constexpr double touchLevelScale=(6/150e3)/(10/270e3);
// The same LC node takes the SUB VCA lever (PN3 SVR4, B-10k between SUO and
// ground; SM2 p.27) through 120k: the 3Vpp SUB at full lever adds +/-1.5V/120k
// against LEVEL's 10V/270k, so +/-.3375 of full LEVEL. The wiper feeds all
// four boards' 120k (R7 L1), 30k in all, so tremolo is lever travel and the
// wiper delivers leverFraction(x,10e3/30e3) of SUO, 7.7% low at mid-lever.
constexpr double tremoloScale=(1.5/120e3)/(10/270e3);
constexpr double tremoloLeverRatio=10e3/30e3;
// A linear pot between a source and ground, or between two sources, sags in
// the middle by what its own track draws through the load: travel x delivers
// x/(1+x(1-x)*Rtrack/Rload) of the source, and the endpoints stay exact.
double leverFraction(double x,double ratio) { return x/(1+x*(1-x)*ratio); }
// MODULATION TVR5 (PN2, B-10k) is the dry/wet crossfader itself: RMO on one
// end, the dry buffer O1 on the other, and its wiper is MI, the expression
// network's input (SM2 p.26 wiring; p.22 for what MI drives). With the pedal
// open the cell ties MI to MO, so the wiper works into the A-10k VOLUME track
// with its 330 ohm foot in parallel with the 68k leg's 34k+2.2k: 8.04k, and
// the whole mix sits 2.35dB low at mid-lever. A closed pedal raises that load
// and straightens the taper; that coupling is not modelled.
constexpr double ringMixLoad=1/(1/(68e3/2+2.2e3)+1/(10e3+330));
constexpr double ringMixLeverRatio=10e3/ringMixLoad;
// Each pin 7 has 220k to its preset Q bus, 220k to RES and 10uF (SM1 p.14;
// SM2 p.16), and pin 7 takes current into a low impedance. The CS-30 drives
// the same IG00156's VQ through 22k into a 390-470 ohm rank shunt with 10uF
// (CS-30 SM pp.8-9) and prints Q .5 at 10V, as SM1 p.19 does: a pin that read
// voltage would see at most .2V there against 10V here, so no one voltage law
// fits both boards, while a current input fits both. (The CS-15 CPB and CS-40M
// FA boards wire VQ as the CS-30 does: corroboration only.) Equal current at
// the two printed 10V points puts the input at four times the shunt, 1.72k at
// rank B's 430 ohm: an inference, not a datum. At any such input the current
// is (P+R)/2 through 110k+Rin, so the printed law reads (P+R)/2 exactly as
// before; what the input changes is the node's RC, 10uF*(110k||Rin) = 17ms
// rather than 1.1s, and the bus loads (qBranchLoad). The lever's track, 2.5k
// at most, is negligible beside Rin.
constexpr double qPinResistance=4*430.;
constexpr double qControlTau=10e-6/(2/220e3+1/qPinResistance);
// One 220k branch seen from its bus: 220k on to the pin node, where the other
// 220k and Rin make a source of qBranchReturn times the other bus behind
// 220k||Rin.
constexpr double qBranchLoad=220e3+1/(1/220e3+1/qPinResistance);
constexpr double qBranchReturn=qPinResistance/(220e3+qPinResistance);
// KAS gate/integrator, one per channel (SM2 p.3; guide printed p.24). The D-A
// output reaches the channel's .033uF hold capacitor through a permanent 3.3M
// and two TC4016 gates in parallel with it: one direct, one through 470k.
// Comparators b and c switch those gates on the PORTAMENTO lever voltage PS,
// giving the three regimes of the guide's p.25 table:
//   PS -6.5..-1.72  both gates off   3.3M       * .033uF = 108.9ms, clock slow
//   PS -1.72..3.81  470k gate on     470k||3.3M * .033uF =  13.6ms, clock mid
//   PS  3.81..7.14  direct gate on   switch Ron * .033uF ~  10us, clock fast
// Above 7.14V comparator a stops the counter altogether, so the key code is
// applied whole. The guide's prose quotes "470k 1uF" for the medium leg; the
// schematic's .033uF is what the board carries and the only value that gives a
// portamento-length glide, so the schematic is followed here.
constexpr double glideHoldCap=.033e-6;
constexpr double glideTauLarge=3.3e6*glideHoldCap;
constexpr double glideTauMedium=(470e3*3.3e6/(470e3+3.3e6))*glideHoldCap;
// Mapping a glide rate back to a PS voltage needs the lever's clock law, which
// is not printed. ESTIMATE: the counter clock period runs log-linearly in PS
// across the lever, from the printed 11s at -6.5V to 1.4ms at +8.5V (SM1 p.5,
// YM26700 pin 4 "the frequency is variable by changing the portamento VR").
// That places the two comparator thresholds at these step rates;
// Ceres.HardwareRealism recomputes both from the printed endpoints.
constexpr double glideMediumAbove=1.584522156984606;
constexpr double glideDirectAbove=43.24665787480699;
// PRA ring modulator, IC8 Fairchild uA796HC (SM2 printed p.22 / PDF16; guide
// printed p.52 / PDF61; overall circuit diagram). Pin roles are the uA796
// sheet's (Fairchild 1973 Linear IC catalog pp.7-74..7-77): 1/4 signal
// inputs, 2-3 gain (emitter) resistor, 5 bias, 6/9 outputs, 7/8 carrier
// inputs, 10 V- (grounded here: the chip runs from one +15V rail).
// The upper carrier pairs have no emitter degeneration, so a matched pair
// steers tanh(V7minus8/(2VT)) of each signal-side current.
constexpr double ringThermalVoltage=1.380649e-23*298.15/1.602176634e-19;
// Carrier: IG00150 pin 6 (SIO) sine, 3.20Vpp on its own pin sheet (SM2
// printed p.9 / PDF7). Guide p.49's 3Vpp is the PWM level-control output, a
// different node. Pin 6 reaches pin 7 through 100uF in SERIES with 33k; all
// three drawings agree no 33k end is grounded. Pin 8 sits on the 100uF-bypassed
// bias node and pin 7 hangs off it through 100 ohm, so the carrier at pins
// 7-8 is the 33k/100 divider: 4.7mV peak, deep inside the 25mV-peak linear
// region the uA796 sheet gives (p.7-77, Fig.3). The modulator multiplies by a
// sine; it does not switch.
constexpr double ringSourcePeakVolts=1.6;
// DC operating point, VR5 at centre. Pin 5 takes 10k from the 390-fed local
// rail S; with V- grounded the bias current is I5=(VS-phi)/(10k+500), the
// MC1496 second-source formula with phi=.75V at 25C (the uA796 sheet shows
// the same 500 ohm in its equivalent circuit). S feeds pin 6's and pin 9's
// 2.7k (I5 each, the quad's 2*I5 split evenly), pin 5, and the 1.2k into the
// carrier-bias divider (820 to the audio bias node: 1k and both 470+(100k+25k)
// signal-input returns to ground). IC base currents are neglected here
// (they move the rail by under .1%).
constexpr double ringPhi=.75;
constexpr double ringBiasGround=1/(1/1000.+2/(470.+125000.));
constexpr double ringBiasLoad=820.+ringBiasGround;
constexpr double ringRail=(15+390*3*ringPhi/10500.)
    /(1+390*3/10500.+390/(1200+ringBiasLoad));          // 12.15V
constexpr double ringBiasCurrent=(ringRail-ringPhi)/10500.; // 1.086mA
// uA796 sheet p.7-75: carrier-port bias current (I7+I8)/2 = 12uA typical
// (30uA max) in its Fig.1 circuit, I5=(8V-phi)/(6.8k+500)=.993mA. Scaled to
// this I5 that is beta=82.8. Pin 7's base current can only arrive through the
// 100 ohm (the 33k path is capacitor-coupled), so V7-V8=-1.31mV DC: a
// typical-part estimate of the carrier-port offset, which passes some of the
// dry signal through the modulator. VR5 trims carrier leakage on the signal
// side (guide p.52, <3mVp-p), not this offset.
constexpr double ringFigure1Bias=(8-ringPhi)/(6800+500.);
constexpr double ringBeta=ringFigure1Bias/12e-6;
constexpr double ringDifferentialDc=-100*ringBiasCurrent/ringBeta;
// Small-signal carrier port: each upper pair is 2*rpi differential at
// Ic=I5/2, the two pairs in parallel, i.e. rpi=3.92k across the 100 ohm.
constexpr double ringCarrierPortShunt=1/(1/100.+1/(ringBeta*ringThermalVoltage/(ringBiasCurrent/2)));
// Signal side: the buffer O1 feeds pin 1 through 10uF and 330 ohm. SM2 draws
// 330K; the guide p.52 outline and the overall circuit diagram both print 330,
// and 330K would leave the wet path 44dB down at the wet buffer's full gain,
// so 330 ohm is used. Pin 1 sees 470 to the AC-grounded bias node, 100k+25k
// (VR5 centre) to ground and the degenerated pair, beta*(470+2re). Pin 4 is
// AC-grounded the same way. The emitter-degenerated signal pair converts
// V1-V4 to a differential current over 470+2re, re=VT/I5.
constexpr double ringSignalDegeneration=470+2*ringThermalVoltage/ringBiasCurrent;
constexpr double ringSignalNode=1/(1/470.+1/125000.+1/(ringBeta*ringSignalDegeneration));
constexpr double ringSignalDivider=ringSignalNode/(330+ringSignalNode);
constexpr double ringSignalCouplingTau=10e-6*(330+ringSignalNode); // 20.1Hz
// Pin 6 (output +, Fig.1 of the sheet) works into 2.7k to the bypassed rail,
// the chip's own 40k single-ended output resistance (sheet p.7-75, typical)
// and, through 10uF, the wet buffer's 10k. Midband single-ended gain is
// RL/(470+2re); the coupling corner sees the 2.7k||40k source, 1.27Hz.
constexpr double ringCollectorSource=1/(1/2700.+1/40e3);
constexpr double ringCollectorLoad=1/(1/ringCollectorSource+1/10e3);
constexpr double ringWetCouplingTau=10e-6*(ringCollectorSource+10e3);
constexpr double ringModulatorGain=ringSignalDivider*ringCollectorLoad/ringSignalDegeneration;
double ringSteering(double volts) { return fastTanh(volts/(2*ringThermalVoltage)); }
// Volts at O1 per unit of the engine's voice bus. The M-board alignment sets
// the sine at CP4 to 1Vpp (CS-60 manual p.38, VI.1), the engine's +/-1, so a
// unit is .5V at CP4. VI.4 sets a CS-50 voice output O to "0.6V" (its figure
// marks peak to peak) for the .8Vpp filtered pulse of VI.2 with the VCA EG at
// full SUSTAIN, .98 of its peak (ampEnvelopeFloor): .75/.98 per unit of
// envelope. The four O outputs reach I1 through 1k each into 47k, and IC1
// follows I1 to O1 (SM2 p.22). Sibling procedure, CS-50 value; see
// outputPreampGain for the same anchor read at the HO terminal.
constexpr double ringBusVolts=.5*(.75/.98)/(4+1e3/47e3);
constexpr double ringPinOneVolts=ringBusVolts*ringSignalDivider;
// Signal pair: 470 ohm between the emitters (pins 2-3), each emitter on its
// own I5 sink (uA796 equivalent circuit), so V1-V4 = 2VT*atanh(x)+x*I5*470
// for the current split x. It limits near I5*(470+2re) = .56V at pin 1.
// Four voices at the VI.4 level, in phase, reach .17V there (H3 -62dB);
// four-note chords on the factory presets reach .12-.31V, at most 1.1%
// instantaneous compression; touch LEVEL and virtual voices beyond the
// CS-50's four go further. The 470 ohm pin-1 node keeps the pair's own
// beta*(470+2re) at its small-signal value (under 1.1% of the node).
// Returns the bus-unit input a linear pair would need for the same split,
// so small signals and the derived modulator gain are unchanged. Halley in
// u=atanh(x), from the series inverse (or, past the knee and for any input
// whose cube overflows, the limiting asymptote), stops once a step is under
// 1e-5 of u, where the remaining error is third order: at most three tanh
// evaluations land within 4e-16 of the exact split anywhere up to +/-20V at
// pin 1, one or two at playing levels.
double ringSignalPair(double signal) {
    constexpr double a=2*ringThermalVoltage, b=ringBiasCurrent*470, c=a+b;
    const double volts=ringPinOneVolts*signal, w=volts/c;
    double u=w*(1+b/(3*c)*w*w);
    const double limiting=(inlineAbs(volts)-b)/a;
    if (limiting>inlineAbs(u) || !inlineFinite(u)) u=inlineCopysign(limiting,volts);
    double split=0;
    for (int i=0;i<8;++i) {
        // tanh(u) = -m/(2+m) and 1-tanh(u)^2 = 4(1+m)/(2+m)^2 from one
        // m = expm1(-2|u|), exact down to the smallest signals.
        const double m=std::expm1(-2*inlineAbs(u)), t=inlineCopysign(-m/(2+m),u);
        const double d=4*(1+m)/((2+m)*(2+m)), f=a*u+b*t-volts, slope=a+b*d;
        const double step=f/(slope+f*b*t*d/slope);   // f'' = -2b*t*d
        split=t-d*step*(1+t*step);                   // tanh(u-step), 2nd order
        if (inlineAbs(step)<=1e-5*inlineAbs(u)) break;
        u-=step;
    }
    return split*c/ringPinOneVolts;
}
} // namespace
double ringSpeedHz(double vc1Volts) {
    return ringSpeedReferenceHz*std::exp2(ringSpeedOctavesPerVolt*(vc1Volts-5));
}
// PRA envelope lever laws: the IG00159 pin-sheet times scaled by the PRA's
// 31ms alignment at AT=5V (sources and the slope assumption are beside the
// declarations in ceres.hpp).
double ringAttackSeconds(double atVolts) {
    return ringEnvelopeTrim*std::pow(10.,-.3*atVolts);
}
double ringDecaySeconds(double dtVolts) {
    return ringEnvelopeTrim*10*std::pow(10.,-.3*dtVolts)*std::log(1000.)/5;
}
namespace {
// The wet buffer IC1-2 is non-inverting with VR4 (B-100K trimmer) over 10k:
// gain 1+VR4/10k, 1..11. Its factory setting is not printed anywhere in the
// CS-50 or CS-60 service material (the CS-60 PRA alignment trims VR3, VR5 and
// VR7 on this board, not VR4). Two trims are defensible for a steady midband
// carrier: wet RMS equal to the dry signal's (VR4=57.8k), or the two
// sidebands together peaking at the dry peak, each half the dry tone
// (VR4=38.0k). The peak-matched trim is CHOSEN BY EAR (owner listening test
// ring-wet-trim, 2026-10-20), not measured: buffer gain 4.80, inside VR4's
// range, 3.0dB less wet than the RMS-matched trim shipped in 0.5.0.
double ringWetBufferGain() {
    const double carrierPeak=ringSourcePeakVolts*ringCarrierPortShunt/(33000+ringCarrierPortShunt);
    return 1/(ringModulatorGain*std::tanh(carrierPeak/(2*ringThermalVoltage)));
}
// The patch specifies TOTAL integrator Hz at middle C. Only variable gm
// responds to exponential control; the parallel 4.7Mohm path stays fixed.
double modulatedFilterFrequency(double referenceHz, double factor) {
    return filterFrequencyFloor + std::max(0.0,referenceHz-filterFrequencyFloor)*factor;
}
// One board's static cutoff for the patch's: its offset trim matched it to
// the other boards at alignedGm Hz of gm, and its Vfc sensitivity, scale
// times nominal, stretches the bus octaves away from there (see
// BoardTolerance). The 4.7Mohm part does not move, so a cutoff at or below
// it is every board's. Exactly the patch's value at scale 1.
double boardCutoff(double referenceHz, double alignedGm, double scale) {
    const double gm=referenceHz-filterFrequencyFloor;
    if (scale==1 || gm<=0) return referenceHz;
    return filterFrequencyFloor+alignedGm*std::pow(gm/alignedGm,scale);
}
double clamp(double x, double lo, double hi) { return std::max(lo, std::min(x, hi)); }
// sin(2 pi p) for a phase p in [0,1): the PWM clock and the SUB sine each
// take one per internal sample. Reduce the turn to its nearest quarter, then
// the Cephes sin.c and cos.c kernels on |x| <= pi/4. The reduction is exact
// in turns, where glibc's sin(2*pi*p) first rounds its argument: swept at
// 1e-7 steps, this is within 7.3e-16 of glibc's result and within 1.7e-16 of
// the long double sin (glibc: 6.9e-16). The ring carrier keeps std::sin:
// Ceres.ring_carrier_circuit_contracts recomputes it bit for bit.
double sinTurn(double p) {
    const double r=4*p;
    const int quarter=static_cast<int>(r+.5);
    const double x=(r-quarter)*1.5707963267948966192313216916397514421;
    const double z=x*x;
    const double sine=((((((1.58962301576546568060e-10*z-2.50507477628578072866e-8)*z
        +2.75573136213857245213e-6)*z-1.98412698295895385996e-4)*z
        +8.33333333332211858878e-3)*z-1.66666666666666307295e-1)*z)*x+x;
    const double cosine=((((((-1.13585365213876817300e-11*z+2.08757008419747316778e-9)*z
        -2.75573141792967388112e-7)*z+2.48015872888517045348e-5)*z
        -1.38888888888730564116e-3)*z+4.16666666666665929218e-2)*z*z)-.5*z+1;
    switch (quarter&3) {
        case 0: return sine;
        case 1: return cosine;
        case 2: return -sine;
        default: return -cosine;
    }
}
double wrap(double x) { return x - std::floor(x); }
// wrap for a phase that has left [0,1) by less than one turn, which is all
// the phase arithmetic below ever does (steps are clamped under .18, the
// subtracted fractions are under 1): the same subtraction, with the floor
// replaced by a sign test, so the result is bit for bit wrap's.
double wrapUnit(double x) { return x<0 ? x+1 : x>=1 ? x-1 : x; }

struct Parameter { const char* key; double Patch::*member; double low, high; };
const Parameter parameters[] = {
    {"saw", &Patch::saw, 0, 1}, {"pulse", &Patch::pulse, 0, 1},
    {"sine", &Patch::sine, 0, 1}, {"noise", &Patch::noise, 0, 1},
    {"pulse_width", &Patch::pulseWidth, .5, .9},
    {"pwm_depth", &Patch::pwmDepth, 0, .4}, {"pwm_rate", &Patch::pwmRate, .05, 222},
    {"pwm_phase", &Patch::pwmPhase, 0, 1},
    {"hp_cutoff", &Patch::hpCutoff, 5, 20000}, {"lp_cutoff", &Patch::lpCutoff, 5, 20000},
    {"hp_resonance", &Patch::hpResonance, 0, 1}, {"lp_resonance", &Patch::lpResonance, 0, 1},
    {"brilliance", &Patch::brilliance, -15, 15},
    {"resonance_offset", &Patch::resonanceOffset, -10, 10},
    {"key_track", &Patch::keyTrack, 0, 1},
    {"hp_envelope_scale", &Patch::hpEnvelopeScale, 0, 4},
    {"lp_envelope_scale", &Patch::lpEnvelopeScale, 0, 4},
    {"filter_initial", &Patch::filterInitial, -12, 0},
    {"filter_peak", &Patch::filterPeak, 0, 12},
    {"filter_attack", &Patch::filterAttack, .0001, 30},
    {"filter_attack_headroom", &Patch::filterAttackHeadroom, .0001, 100000},
    {"filter_attack_saturation", &Patch::filterAttackSaturation, 0, 10},
    {"filter_decay", &Patch::filterDecay, .0001, 30},
    {"filter_release", &Patch::filterRelease, .0001, 30},
    {"amp_attack", &Patch::ampAttack, .0001, 30},
    {"amp_attack_headroom", &Patch::ampAttackHeadroom, .0001, 100000},
    {"amp_attack_saturation", &Patch::ampAttackSaturation, 0, 10},
    {"amp_decay", &Patch::ampDecay, .0001, 30}, {"amp_sustain", &Patch::ampSustain, 0, 1},
    {"amp_release", &Patch::ampRelease, .0001, 30},
    {"global_release", &Patch::globalRelease, 0, 30},
    {"sustain_mode", &Patch::sustainMode, 1, 2},
    {"filter_level", &Patch::filterLevel, 0, 1}, {"amp_level", &Patch::ampLevel, 0, 1},
    {"volume", &Patch::volume, 0, 1},
    {"expression", &Patch::expression, 0, 1},
    {"output_trim_db", &Patch::outputTrimDb, -24, 24},
    {"octave", &Patch::octave, -2, 1}, {"tune_cents", &Patch::tuneCents, -500, 700},
    {"voice_spread_cents", &Patch::voiceSpreadCents, 0, 30},
    {"drift_cents", &Patch::driftCents, 0, 20},
    {"board_pitch_tolerance", &Patch::boardPitchTolerance, 0, 1},
    {"board_filter_tolerance", &Patch::boardFilterTolerance, 0, 1},
    {"board_envelope_tolerance", &Patch::boardEnvelopeTolerance, 0, 1},
    {"board_envelope_time_tolerance", &Patch::boardEnvelopeTimeTolerance, 0, 1},
    {"board_vca_tolerance", &Patch::boardVcaTolerance, 0, 1},
    {"board_temperature_rise", &Patch::boardTemperatureRise, -25, 25},
    {"lfo_waveform", &Patch::lfoWaveform, 0, 5},
    {"ext_level", &Patch::extLevel, 0, 1},
    {"ring_mix", &Patch::ringMix, 0, 1}, {"ring_rate", &Patch::ringRate, .2, 220},
    {"ring_depth", &Patch::ringDepth, 0, 8},
    {"ring_attack", &Patch::ringAttack, .001, 10},
    {"ring_decay", &Patch::ringDecay, .01, 10},
    {"lfo_rate", &Patch::lfoRate, .05, 100}, {"vibrato_cents", &Patch::vibratoCents, 0, 100},
    {"tremolo", &Patch::tremolo, 0, 1}, {"filter_lfo", &Patch::filterLfo, 0, 5},
    {"pressure_volume", &Patch::pressureVolume, 0, 1},
    {"pressure_filter", &Patch::pressureFilter, 0, 5},
    {"pressure_vibrato", &Patch::pressureVibrato, 0, 100},
    {"pressure_wah", &Patch::pressureWah, 0, 5},
    {"glide_mode", &Patch::glideMode, 0, 2},
    {"glide_rate", &Patch::glideRate, 1, 200},
    {"expression_source", &Patch::expressionSource, 0, 1},
    {"velocity_sensitivity", &Patch::velocitySensitivity, 0, 1},
    {"drive", &Patch::drive, .05, 8},
    {"reset_microseconds", &Patch::resetMicroseconds, .1, 60}
};

double parameterValue(const Parameter& parameter, double value) {
    value=clamp(value,parameter.low,parameter.high);
    if (value>0 && (parameter.member==&Patch::ampAttackSaturation ||
                   parameter.member==&Patch::filterAttackSaturation))
        value=std::max(.001,value);
    if (parameter.member==&Patch::lfoWaveform || parameter.member==&Patch::sustainMode ||
        parameter.member==&Patch::expressionSource || parameter.member==&Patch::glideMode)
        value=std::round(value);
    return value;
}

Patch validatePatch(const Patch& patch) {
    Patch validated = patch;
    const Patch defaults;
    for (const auto& parameter : parameters) {
        const double value = patch.*(parameter.member);
        validated.*(parameter.member) = std::isfinite(value)
            ? parameterValue(parameter,value) : defaults.*(parameter.member);
    }
    return validated;
}

double logSinh(double x) {
    constexpr double ln2=.693147180559945309417232121458176568;
    if (x>20) return x-ln2+std::log1p(-std::exp(-2*x));
    if (x<.001) {
        const double x2=x*x;
        return std::log(x)+std::log1p(x2/6+x2*x2/120);
    }
    return std::log(std::sinh(x));
}

double asinhExp(double x) {
    constexpr double ln2=.693147180559945309417232121458176568;
    if (x>20) return x+ln2;
    if (x < -20) return std::exp(x);
    return std::asinh(std::exp(x));
}

double effectiveRelease(double selected, double global) {
    // SUB selects min(selected release CV, global SUS) for both RA and RF
    // (CS-50 service2 PDF6/printed8; Guide PDF70/printed61). Under a common
    // decreasing time-vs-CV law this becomes max(selected time, global time).
    // The SUB board (SM2 p.8) builds it from two 1S1555s whose cathodes meet
    // at SUS, each lever reaching its node through 10k, and a 2SC458
    // follower (1k base, 10k to -15V) per output: RF/RA = min(lever,
    // SUS + Vd) - VBE. Selected and global times here are both defined at
    // the EG pin, so the lever path's VBE is applied where a lever voltage
    // becomes a time (preset()); on the SUS path Vd and VBE nearly cancel
    // (1S1555, as a 1N4148-class estimate, ~.55V at .5mA against the
    // 2SC458's .67V), a residual ~.1V (~7% in time) not modelled. The
    // diode's soft knee near the crossover is not modelled. SVR5, the
    // SUSTAIN lever, is a C-10K taper (SM2 p.27) whose curve is unpublished.
    // A zero engineering time is a bypass, not literal electrical SUS=10V.
    return std::max(selected,global);
}

std::array<double,4> envelopeCoefficients(double fs, double attack, double decay,
                                        double release, double headroom, double saturation) {
    // First element is the incremental fraction, not 1 minus a rounded
    // exp(). log1p/expm1 preserve the near-linear large-headroom limit.
    double currentStep=0;
    if (saturation>0) {
        const double end=headroom/saturation;
        // For large arguments both log(sinh) terms have the same -log(2)
        // offset. Subtract their known separation to avoid cancellation.
        const double normalizedRate=end>20 ? 1/saturation
            : logSinh((1+headroom)/saturation)-logSinh(end);
        currentStep=normalizedRate/(fs*attack);
    }
    return {-std::expm1(-std::log1p(1/headroom)/(fs*attack)),
            std::exp(-std::log(1000.0)/(fs*decay)),
            std::exp(-std::log(1000.0)/(fs*release)),currentStep};
}

// Platform-independent integer PRNG. The shared noise source and initial free
// oscillator phases reproduce bit-for-bit within a given floating-point build.
struct Random {
    std::uint64_t state;
    explicit Random(std::uint64_t seed) : state(seed ? seed : 1) {}
    std::uint64_t next() {
        state += UINT64_C(0x9e3779b97f4a7c15);
        auto z = state;
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        return z ^ (z >> 31);
    }
    double unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
    double bipolar() { return 2 * unit() - 1; }
    // Move past n draws without producing them. The state advances by a
    // fixed increment per draw, so a skipped stretch leaves the sequence
    // exactly where the draws would have: the next value is unchanged.
    void skip(std::uint64_t n) { state += n * UINT64_C(0x9e3779b97f4a7c15); }
};

// Board-to-board tolerance: what the four M boards keep after Yamaha's own
// calibration, and nothing the service documents do not bound. Voices beyond
// the CS-50's four are further boards: each draws its own residuals from the
// same bands, after the four physical boards (reset()), so a board is the
// same at every voice count.
//
// Pitch. IG00153 (VCO III) is LINEAR in the key voltage Kv (Guide printed
// pp.28-29: f = I/(Iref*tau), I = Kv/Rn; NORMAL Kv 250mV..4V gives
// 130.8..2093Hz, so Kv is proportional to the NORMAL frequency). A board's
// error is therefore a gain error per transposition resistor Rn plus one
// input-offset voltage, f = G_t*(Kv+Voff). The tuning method (Guide printed
// p.65) trims each board at OCT-UP until C6 key gives C7 and C2 key gives C3,
// each "+/-1 cent" (VR2/VR3, offset and scale, Guide p.29), then trims the
// NORMAL, 1-down and 2-down Rn at the C6 key to +/-1 cent (VR4, VR5, VR6).
// The residual is five numbers per board, each inside its printed +/-1 cent
// band: eLow (C2 key, OCT-UP), and the C6-key error at each transposition.
// The C2/C6 pair fixes d = Voff/Kv(C6 key): with r = 2^((eLow-eUp)/1200),
// (1+16d)/(1+d) = r, so d = (r-1)/(16-r) (the C2 key is Kv/16, Guide p.29).
// Error at any key, transposition t: e_t + 1200*log2((1+d/x)/(1+d)), with x =
// Kv/Kv(C6 key). The 0.1% B-mark Rn resistors and 2% G-mark timing parts
// (SM2 printed pp.17-18, note 7) are inside this trim, so they add nothing.
// ASSUMPTION: each residual is uniform over its band (a technician stops
// anywhere inside it); the service guide prints only the band.
//
// Thermal drift (temperature, in kelvin). Yamaha Service News E-46 (January
// 1978, CS-50/CS-60/CS-80, "Countermeasures for stabilizing the pitch") ranks
// the IG00153 of every M board by how far the pitch of C1 and of C6 moves
// when the ambient temperature goes from 25 to 50C: one figure per 5 cents
// (C1 0..45, C6 0..30 cents; "32" is 10-15 cents at C1 and 5-10 at C6). One
// instrument may use one C6 figure only and two neighbouring C1 figures, so
// over that 25K step its boards differ by less than 5 cents at C6 and 10 at
// C1. It covers CS-50 #1546 on (November 1977) and the earlier serials it
// lists, which also had the 1S1555 compensation diode greased onto IC8. C1
// and C6 are Kv 125mV and 4V: the CS-80 adjustment procedure sets the C1
// key's Kv to 125.0mV against a 4.000V reference (TUNING 1-3, printed p.3)
// and the CS-50's C6 key is 4V (Guide p.29), so C1 is x = 1/32. Model: each
// board sits uniformly in its window about the window's centre (C6 +/-2.5,
// C1 +/-5 cents), between them in the trims' gain-and-offset form (ratio 32
// in place of 16), all scaled by the temperature change over 25K. The drift
// the boards share (the windows' centres) moves the instrument, not a board:
// it is left out. CONVENTIONS: uniform in the window; linear in temperature
// (E-46 gives the 25 -> 50C step only); the same at every transposition (the
// ranking is of the IC, not of the Rn resistors).
//
// The VCF, EG and VCA trims are not in the CS-50 guide. The CS-60 service
// manual's section VI (printed pp.37-39 / PDF 39-41) aligns the same M board:
// SM1's parts list gives the M board (NA04485) "Common Models: CS-60", the
// CS-60 M schematic (printed p.15 / PDF 17) is drawn for "CS-60,50" with the
// CS-50's transposition values, and the procedure itself gives the CS-50's
// different output level (".24V (CS-50 is 0.6V)"). SIBLING EVIDENCE, below.
// Yamaha's CS-80 "Adjustment Procedure" aligns the same board as well. SM2
// p.16 prints the CS-80's M-board differences: the transposition circuit
// (R1-R5, VR4-VR6) and the rank resistor R7, none in the EGs or VCAs. Where
// it trims the same trimmer to the CS-60's target, it prints the tolerance
// the CS-60 leaves out. SIBLING EVIDENCE as well.
//
// VCF sensitivity (filter amount). On that schematic the IG00156 rank
// resistor is NOT on the fadj pin (220k, fixed): R6/R7 load each filter's Vfc
// node, where the panel lever, the VCF EG (100k HP, 47k LP), SUB/touch and
// the VR7/VR9 offset trim (150k) sum. R6 is ~2.2k against >=60k sources, so
// the node's volts-per-input scale with R6: the rank sets each chip's control
// SENSITIVITY. Ranks (SM2 printed pp.17-18 note 9; CS-60 p.15): R6
// 2.7k/2.2k/1.8k, R7 3.3k/2.7k/2.2k (CS-60/50 column 3.0k/2.4k/2.0k); every
// column spans x1.5 over two steps, so a chip lands within half a step of
// nominal: octaves per Vfc volt scaled by 2^(u*.14624), |u|<=1, HP and LP
// independent. The trims then match the boards by ear, each with its own
// filter's lever at the centre: PANEL, pulse on, "HPF lever to the center
// position" with LPF, RES L and RES H high for VR7 (cutoff) and VR8 (Q);
// "LPF to center position" with HPF and both RES low for the LPF trims (the
// text names VR9), until "the tone color ... may be the same as that of
// other M circuit boards" (CS-60 p.38, III-IV). No number bounds the ear
// match, so a static cutoff at that lever centre is identical on every
// board. The trims are offsets, though, and the sensitivity still stretches
// every Vfc input's octaves about the point they were set at. The procedure
// names no key or envelope setting, so the model takes the levers alone: the
// EG, SUB, touch and BRILLIANCE octaves scale about their own zero, and the
// cutoff bus about the lever centre, so a static cutoff one bus volt (one
// octave at the printed law) from it differs by (scale-1) octave of gm
// (boardCutoff). Q stays identical everywhere: nothing bounds a chip's pin-7
// law, which the CS-50 does not rank. INFERENCE:
// chips uniform within contiguous one-step bins. The CS-80's numbers do not
// carry over: its complete VCF alignment (printed p.11) holds FH/FL at
// 4.0/3.3V, turns VR7/VR9 for maximum amplitude at the C1 key and sets
// VR8/VR10 to 4.3 and 2.2 +/-1Vpp at CP3, a procedure the CS-60/50 does not
// use, on a CS-80 R7 and bus.
//
// EG levels (envelope amount). The VCF EG check (CS-60 p.37): AL, IL at 10V
// give TP1 +5V +/-.5V and -4V +/-.5V; only the offset is trimmed (VR18, 0
// +/-.01V, a constant offset the static VCF trims then absorb). One buffer
// gain must meet both bands, so the VCF EG reaches HP and LP within +/-10%.
// The VCA EG's attack ends where the sequencer's 30k/60k node trips its
// comparator: TP2 10V +/-.2V (p.38, EG-VCA 2), a peak of each board's own.
// Its held and released targets are the panel's SL and IL plus VR17's offset,
// trimmed to -200 +/-20mV (EG-VCA 1; CS-80 printed p.13, the same). VR14 is
// trimmed after both, with A, D, R short and S at 10 ("CS-50 is 0.6V", CS-60
// p.39, VI.4), so every board's full-sustain output is the nominal one: the
// peak and floor residuals stay, a level offset at full sustain does not.
//
// EG times (envelope time amount). Each EG is one exponential converter: TC
// reaches the IG00151's EI through 82k + VR19 (VCF) or VR20 (VCA), with the
// VR15/VR16 offsets through 100k (SM2 p.16), trimmed to an attack of 4ms at
// 8V and 125ms at 3V (CS-60 pp.37-38). The CS-80 trims the same VR15/VR19 and
// VR16/VR20 to the same two times and prints "(+/-5%)" for each (printed
// pp.7 and 13). Each trim point is uniform in +/-5%; an exponential
// converter's log-time error is linear in its control voltage, so between
// and beyond the points it is linear in log time. D and R drive the same
// converter (Docs/research/envelopes-authenticity-2026-10-20.md), placed on
// it by preset()'s convention: an attack ends after 5 RC and a D/R time is a
// T60 of ln(1000) RC, so a T60 shares the RC of an attack of 5*T60/ln(1000).
//
// VCA levels (VCA amount). VR11 trims the sine to 1Vpp and VR12 the filtered
// pulse to .8Vpp at CP4 (CS-60 p.38, VI.1-2); the CS-80 trims the same VR11
// and VR12 to 1.00 +/-.02Vpp and 800 +/-20mVpp (printed p.15). VR13 and VR14
// act after CP4 and are trimmed on the filtered pulse (VI.3-4), restoring its
// level, so the sine keeps its ratio to it: (1+e11)/(1+e12). The PRA's VR6
// is trimmed last, in the finished instrument, for 0 dBm from the sine alone
// with all keys down (CS-60 p.39, PRA III; see outputPreamp): it takes the
// four boards' sine error out of the common gain. With the reference's
// assumptions (RMS meter, notes adding incoherently) that is
// sqrt(4/sum g^2) over the four physical boards' sine gains g. A further
// board is added after that trim and keeps its own sine error.
//
// Not modelled, no printed tolerance: Q, and the static cutoff at the trims'
// lever centre (both ear-matched),
// VR14's own residual (0.6V, no band for the CS-50), WSC level (no per-board
// procedure), the 1.5nF/180pF/.22uF capacitors and 4.7M resistors (neither
// parts list gives a tolerance), the KAS sample-holds.
constexpr double boardTrimCents=1;          // Guide printed p.65, every trim
constexpr double boardScaleKeyRatio=16;     // C6 key Kv / C2 key Kv, p.29
constexpr double boardTrimKeyNormalHz=2093; // C6 key at NORMAL, p.29/p.65
constexpr double boardThermalStepKelvin=25; // E-46: ambient 25 -> 50C
constexpr double boardThermalKeyRatio=32;   // E-46 C6 / C1: Kv 4V / 125mV
constexpr double boardThermalHighCents=5./2; // E-46: one 5-cent C6 figure
constexpr double boardThermalLowCents=10./2; // E-46: two neighbouring C1 figures
constexpr double boardFilterRankOctaves=.14624062518028906; // log2(1.5)/4
constexpr double boardFilterEnvelopeBand=.5/5; // CS-60 p.37, TP1 +5V +/-.5V
constexpr double boardAmpPeakBand=.2/10;       // CS-60 p.38, TP2 10V +/-.2V
constexpr double boardAmpFloorBand=.02/10;     // CS-60 p.38, TP2 -200 +/-20mV
constexpr double boardEnvelopeTimeBand=.05;    // CS-80 printed pp.7, 13: +/-5%
constexpr double boardEnvelopeFastSeconds=.004;  // attack trim at 8V
constexpr double boardEnvelopeSlowSeconds=.125;  // attack trim at 3V
constexpr double boardAttackTimeConstants=5;     // preset(): attack = 5 RC
constexpr double boardSineBand=.02/1;          // CS-80 p.15: VR11 1.00 +/-.02Vpp
constexpr double boardFilteredBand=.02/.8;     // CS-80 p.15: VR12 800 +/-20mVpp
struct BoardTolerance {
    // C6-key residual at transposition 2-down, 1-down, NORMAL, 1-up.
    std::array<double,4> transposeCents{};
    double offsetRatio=0, hpOctaves=0, lpOctaves=0;
    double filterEnvelopeError=0, ampPeakError=0;
    // Thermal drift over 25K at the C6 key, and its Kv offset ratio.
    double thermalHighCents=0, thermalOffsetRatio=0;
    // Log-time error at the 4ms and 125ms attack trims: VCF EG, VCA EG.
    std::array<double,2> filterTimeLog{}, ampTimeLog{};
    double ampFloorError=0, sineError=0, filteredError=0;
    // Cached at the patch's amounts; the identical-board values when off.
    double hpScale=1, lpScale=1, hpEnvelopeGain=1, lpEnvelopeGain=1;
    // Static cutoffs at middle C (boardCutoff): the patch's when off.
    double hpCutoff=0, lpCutoff=0;
    double ampPeak=1, ampFloor=0, ampEnvelopeGain=1, sineGain=1;
    std::array<double,4> filterCoefficients{}, ampCoefficients{};
    // trims keeps the first stream's order, so a seed keeps the boards it
    // had; everything added since draws from more, in this order.
    void draw(Random& trims, Random& more) {
        const double low=boardTrimCents*trims.bipolar();
        for (auto& cents : transposeCents) cents=boardTrimCents*trims.bipolar();
        const double r=std::exp2((low-transposeCents[3])/1200);
        offsetRatio=(r-1)/(boardScaleKeyRatio-r);
        hpOctaves=boardFilterRankOctaves*trims.bipolar();
        lpOctaves=boardFilterRankOctaves*trims.bipolar();
        filterEnvelopeError=boardFilterEnvelopeBand*trims.bipolar();
        ampPeakError=boardAmpPeakBand*trims.bipolar();
        const double thermalLow=boardThermalLowCents*more.bipolar();
        thermalHighCents=boardThermalHighCents*more.bipolar();
        const double q=std::exp2((thermalLow-thermalHighCents)/1200);
        thermalOffsetRatio=(q-1)/(boardThermalKeyRatio-q);
        for (auto& error : filterTimeLog) error=std::log1p(boardEnvelopeTimeBand*more.bipolar());
        for (auto& error : ampTimeLog) error=std::log1p(boardEnvelopeTimeBand*more.bipolar());
        ampFloorError=boardAmpFloorBand*more.bipolar();
        sineError=boardSineBand*more.bipolar();
        filteredError=boardFilteredBand*more.bipolar();
    }
    // octave: TRANSPOSE (-2..1); normalHz: the board's ideal NORMAL-position
    // frequency for its key voltage (key, PITCH and bend; not vibrato).
    double trimCents(double octave, double normalHz) const {
        const double position=std::max(0.0,std::min(octave+2,3.0));
        const auto index=static_cast<std::size_t>(position<3 ? position : 2);
        const double gain=transposeCents[index]
            +(transposeCents[index+1]-transposeCents[index])*(position-index);
        const double x=normalHz/boardTrimKeyNormalHz;
        return gain+1200*std::log2(std::max(1e-9,1+offsetRatio/x)/(1+offsetRatio));
    }
    // Drift over the 25K step, the same at every transposition.
    double thermalCents(double normalHz) const {
        const double x=normalHz/boardTrimKeyNormalHz;
        return thermalHighCents
            +1200*std::log2(std::max(1e-9,1+thermalOffsetRatio/x)/(1+thermalOffsetRatio));
    }
    double pitchCents(const Patch& patch, double normalHz) const {
        double cents=0;
        if (patch.boardPitchTolerance>0)
            cents=patch.boardPitchTolerance*trimCents(patch.octave,normalHz);
        if (patch.boardTemperatureRise!=0)
            cents+=patch.boardTemperatureRise/boardThermalStepKelvin*thermalCents(normalHz);
        return cents;
    }
    // Duration factor of one EG at the attack time sharing the segment's RC
    // (the duration itself for an attack, 5*T60/ln(1000) for decay/release).
    static double timeFactor(const std::array<double,2>& trim, double attack, double amount) {
        const double span=std::log(attack/boardEnvelopeFastSeconds)
            /std::log(boardEnvelopeSlowSeconds/boardEnvelopeFastSeconds);
        return std::exp(amount*(trim[0]+(trim[1]-trim[0])*span));
    }
};

// Yamaha CS Series Servicing Guide, printed pp. 33–34: IG00156 is a
// state-variable filter comprising TWO variable-gm capacitor integrators.
// This is a reduced differential-pair model, NOT two digital biquads:
//   h = z - 2*b - l
//   C db/dt = gm * Vs tanh(h / Vs) + h/R
//   C dl/dt = gm * Vs tanh(b / Vs) + b/R
//   tau dz/dt = u - z + (2-damping)*b - ratio*y
//   T dy/dt = u/ratio - z + 2*b - y
//   hp = h; lp = l; omega_total = (gm + 1/R)/C.
// z=2*(pin1+b) includes the 180pF capacitor from physical BP=-b to
// pin1/9. With equal 33k op-amp feedback resistors, h=2*pin1-l.
// KCL gives tau=180pF/(Gsource+1/33k+1/1.2M). The Q block is treated as
// an ideal controlled current source; its conductance plus the 1.2M path
// produces the nominal damping. The 1uF series source capacitor adds state
// y=2*Vcap, ratio=Gsource/Gtotal, and T=1uF/Gsource. Each HP/LP stage has its
// own physical input capacitor. Its voltage state stays continuous when the
// enabled waveform branches change source resistance. This is a coupled
// fourth-order filter, not an independent input/output EQ. Physical source
// gain is preserved. Finite op-amp gain, Q-source output conductance and
// actual signal-voltage scaling remain unidentified.
// Trapezoidal capacitor integration makes the nonlinear feedback implicit.
// Eliminating l leaves one monotone equation, solved with analytic Newton
// derivatives at every sample. Vs remains a calibration parameter: Yamaha
// does not specify the internal transistor geometry.
// CS-50 M-board schematic: C=1.5nF; each 4.7Mohm resistor bypasses its
// variable-gm block, feeding the next integrator INPUT. It is not a capacitor
// bleed resistor. This linear input-current path sets a 22.58 Hz integrator-rate floor.
// The frequency argument denotes total small-signal integrator frequency, so gm is
// the requested total conductance minus 1/R, constrained nonnegative.
// The service manual
// specifies Q=.5 at VQ=10V and Q=5 at VQ=0V (SM1 printed p.19; pin 6 sets
// the standing Q current that gives Q=5 at 0V). Guide printed p.33 draws the
// Q path as a variable-gm block (gm=K*Icont) between BP and FB, so damping is
// proportional to its control current. The sheet does not say whether the
// VQ-to-Icont converter is linear or exponential. Damping is EXPONENTIAL in
// VQ through the printed endpoints, d=.2*10^(VQ/10): chosen by ear
// 2026-10-20 (listening test 2026-10-20-vcf-q-law, B), the converter law the
// cutoff input has, against the linear d=.2+.18*VQ shipped before. The
// endpoints are printed; the path between them is that choice, not a
// measurement. Yamaha's own variable-gm cell of the period (JP S59-49726 B2,
// priority 1976-01-12, figs. 5-6) used a linear V-to-I amplifier instead,
// sibling practice that does not identify the IG00156's converter. See
// Docs/research/vcf-authenticity-2026-10-20.md.
// HP and LP have independent Q trims: the nominal endpoints are anchored at
// their usual source loads (HP three mixer branches, LP one). Changing a
// waveform gate changes the source load, not the Q transistor conductance.
// The passive network depends on rate and mixer gates, not on a voice's
// capacitor charge or instantaneous cutoff. Share its exact coefficients
// across the boards; configure() never touches a filter's electrical state.
struct OtaNetwork {
    double fs=0, conductance=0, referenceConductance=0, ratio=0;
    double qBlend=0, sourceBlend=0, divisor=0, linear=0, sourceInput=0;
    double resonance=0, qBand=0, effectiveDamping=0;
    bool resonanceReady=false;
    void configure(double rate, double sourceConductance=1/150e3,
                   double referenceSourceConductance=1/150e3) {
        fs=rate;
        constexpr double feedbackCapacitance=180e-12, sourceCapacitance=1e-6;
        conductance=sourceConductance+1/33e3+1/1.2e6;
        referenceConductance=referenceSourceConductance+1/33e3+1/1.2e6;
        ratio=sourceConductance/conductance;
        qBlend=conductance/(conductance+2*fs*feedbackCapacitance);
        sourceBlend=sourceConductance/(sourceConductance+2*fs*sourceCapacitance);
        divisor=1-qBlend*ratio*sourceBlend;
        linear=1/(2*fs*filterCapacitance*filterFeedResistance);
        sourceInput=conductance/(sourceConductance+2*fs*sourceCapacitance);
        resonanceReady=false;
    }
    void setResonance(double value) {
        if (resonanceReady && value==resonance) return;
        // resonance r is 1-VQ/10, so d=.2*10^(VQ/10)=2*10^-r.
        const double damping=2*std::pow(10.0,-clamp(value,0,1))*referenceConductance/conductance;
        qBand=qBlend*(2-damping-2*ratio*sourceBlend)/divisor;
        effectiveDamping=(2*(1-qBlend)+qBlend*damping)/divisor;
        resonance=value;
        resonanceReady=true;
    }
};

struct OtaSvf {
    double sb = 0, sl = 0, sz = 0, sy = 0, previousB = 0;
    // tanh(previousB), kept from the previous sample's final evaluation so
    // the Newton warm start does not recompute it: same argument, same bits.
    double previousTanhB = 0;
    double cachedCutoff=0, cachedRate=0, cachedG=0;
    double process(double sourceVoltage, double frequency, double resonance,
                   double fs, bool highPass, double sourceConductance = 1/150e3,
                   double referenceSourceConductance = 1/150e3) {
        OtaNetwork network;
        network.configure(fs,sourceConductance,referenceSourceConductance);
        network.setResonance(resonance);
        return process(sourceVoltage,frequency,highPass,network);
    }
    double process(double sourceVoltage, double frequency, bool highPass, const OtaNetwork& network) {
        const double cutoff = clamp(frequency, filterFrequencyFloor, .22 * network.fs);
        if (cutoff!=cachedCutoff || network.fs!=cachedRate) {
            // Exact input equality only: moving cutoff CVs retain the same
            // capacitor TPT prewarp, with no control-rate approximation.
            cachedG=prewarpTan(pi * cutoff / network.fs);
            cachedCutoff=cutoff;
            cachedRate=network.fs;
        }
        const double g = cachedG;
        const double ratio=network.ratio, qBlend=network.qBlend;
        const double sourceBlend=network.sourceBlend, divisor=network.divisor;
        // X is the physical Thevenin source voltage, in the SAME OTA voltage
        // unit at both stages. Canonical u=2*Gs*X/D0 is a local algebraic
        // coordinate, not a separate per-stage signal normalization.
        const double input=2*ratio*sourceVoltage;
        // Eliminate both linear capacitor nodes simultaneously. The remaining
        // OTA equation has positive derivative because effectiveDamping>0.
        const double qConstant=((1-qBlend)*sz+
            qBlend*(1-sourceBlend)*(input-ratio*sy))/divisor;
        const double qBand=network.qBand, effectiveDamping=network.effectiveDamping;
        const double linear = network.linear;
        const double nonlinear = cutoff<=filterFrequencyFloor ? 0.0 : std::max(0.0,g-linear);
        double b = previousB;
        // The last tanh(b) the iterations evaluated, and its argument.
        double tanhArgument = previousB, tanhValue = previousTanhB;
        for (int iteration = 0; iteration < 7; ++iteration) {
            const double tb = iteration == 0 ? previousTanhB : fastTanh(b);
            tanhArgument = b; tanhValue = tb;
            const double l = sl + nonlinear*tb + linear*b;
            const double h = qConstant - effectiveDamping * b - l;
            const double th = fastTanh(h);
            const double residual = b - sb - nonlinear*th - linear*h;
            const double dl = nonlinear*(1-tb*tb) + linear;
            const double derivative = 1 + (nonlinear*(1-th*th)+linear)*(effectiveDamping+dl);
            const double delta = residual / derivative;
            b -= delta;
            // Newton converges quadratically, so once a step is under 1e-9
            // the error left in b is of order its square, under b's ulp. The
            // earlier 1e-12 bought a further pair of tanh evaluations about a
            // third of the time (2.34 to 2.02 iterations per solve, measured
            // on the factory presets) and no change at the float output.
            if (inlineAbs(delta) < 1e-9) break;
        }
        // tanh(b) is the last evaluation carried along its tangent over that
        // final sub-ulp step; only an unconverged loop recomputes it.
        const double step = b - tanhArgument;
        const double tanhB = inlineAbs(step) < 1e-9
            ? tanhValue + (1 - tanhValue*tanhValue)*step : fastTanh(b);
        const double l = sl + nonlinear*tanhB + linear*b;
        const double z = qConstant+qBand*b;
        const double h = z - 2*b - l;
        // Expand sourceBlend/ratio analytically so tiny positive diagnostic
        // conductances cannot overflow at the intermediate input/ratio.
        const double y=(1-sourceBlend)*sy+sourceBlend*(-z+2*b)+network.sourceInput*input;
        sb = 2 * b - sb;
        sl = 2 * l - sl;
        sz = 2 * z - sz;
        sy = 2 * y - sy;
        previousB = b;
        previousTanhB = tanhB;
        return highPass ? h : l;
    }
};

// The shared M-board service procedure (CS-60 manual p.38, section VI)
// trims sine-only CP4 to 1Vpp and filtered pulse-only CP4 to .8Vpp, using
// separate VCA controls (IC3 VR11, IC4 VR12; printed p.38 / PDF 40, VI.1-2:
// sine at 2 OCT-DOWN, pulse at NORMAL with RESH/RESL low, LPF high). It is
// the CS-50's own: VI.4 gives the CS-50's O level beside the CS-60's, and the
// CS-50 M board is the same part (SM1 parts list p.2, M #21233, "Common
// Models CS-60"; SM2 p.16 tabulates only CS-80 differences, all in the
// pitch section). Neither the CS-50 guide nor SM1/SM2 prints a VCA level
// procedure of its own. The test key and HPF lever are unprinted, so this
// broad-pass estimate stays a downstream VCA gain, not an inverse gain
// inserted between the nonlinear filters.
constexpr double hpNominalConductance=3/150e3+1/33e3+1/1.2e6;
constexpr double lpNominalConductance=1/150e3+1/33e3+1/1.2e6;
constexpr double hpSingleWaveGain=2/(150e3*hpNominalConductance);
constexpr double lpSourceGain=2/(150e3*lpNominalConductance);
constexpr double filteredVcaCalibration=.8/(hpSingleWaveGain*lpSourceGain);

struct FilterChain {
    OtaSvf hp, lp;
    double process(double waveSum, double branches, double hpFrequency, double lpFrequency,
                   double hpResonance, double lpResonance, double drive, double fs) {
        const double inputScale=.3*drive; // shared, uncalibrated WSC/OTA voltage scale
        const double high=hp.process(inputScale*waveSum/branches,hpFrequency,hpResonance,
                                     fs,true,branches/150e3,3/150e3);
        const double low=lp.process(high,lpFrequency,lpResonance,fs,false);
        return low*filteredVcaCalibration/inputScale;
    }
    double process(double waveSum, double branches, double hpFrequency, double lpFrequency,
                   double drive,
                   const OtaNetwork& hpNetwork, const OtaNetwork& lpNetwork) {
        const double inputScale=.3*drive;
        const double high=hp.process(inputScale*waveSum/branches,hpFrequency,true,hpNetwork);
        const double low=lp.process(high,lpFrequency,false,lpNetwork);
        return low*filteredVcaCalibration/inputScale;
    }
};

// Conditional output circuit, using the dedicated M-board and PRA schematics
// (service2 printed pp.16 and 22–23; research/output-coupling-audit.md).
// Rin4=Rin5=0 is a low-impedance CURRENT-input limit, not an identified chip
// input resistance. The prior normalized midband gains remain calibration
// approximations. CP3 precedes filtered level; CP4 precedes the common VCA.
// All four low-Z 4.7uF/1k voice branches stay connected to the 47k PRA load,
// including silent voices. Their common bus transfer has one equivalent
// state, independent of voice allocation. The final 33uF section drives
// the LO divider (4.7k+560ohm) and HO through 560ohm into the 10k load the
// CS-80's level trim names (see outputPreamp). Expression assumes an ideal
// fully conducting CdS. No external recording-chain pole is included.
// Capacitor integration uses physical-RC trapezoids without pole prewarp.
constexpr double outputBenchLoad=10e3, outputSeriesResistance=560;
constexpr double outputLineLoad=1/(1/(4.7e3+560)+1/(outputSeriesResistance+outputBenchLoad));
constexpr double outputCouplingTau[4]={1e-6*100e3,1e-6*(27e3+100e3),
    4.7e-6*(1e3+4*47e3),33e-6*outputLineLoad};
// WSC sine shaper, IG00158 STI (pin 15) -> pin 2. Its transfer is not
// published and no measurement isolates it. The shipping shaper is the
// textbook bipolar-IC triangle-to-sine converter, an undegenerated
// differential pair: y = tanh(k*tri)/tanh(k) at unit peak, at that
// topology's own optimum, the drive k that minimises THD for an ideal
// triangle (k = 1.40211; 1.4020 is the value auditioned): THD 1.36 %, H3
// -54.1 dB, H5 -39.0 dB, H7 -45.1 dB. It replaced the ideal sin(pi/2*tri)
// by ear on 2026-10-20 (owner listening verdict, set
// 2026-10-20-vco-sine-shaper); it is not a fit to any recording.
// The one whole-path measurement found since fits neither shaper: a public
// CS-80 sample set (same M board, so the same IG00158;
// Tools/measure_cs80_wsc_recordings.py) reads H3 -22.5..-30.3, H5
// -40.4..-46.9 and H7 -59.4..-71.2 dB re H1 over 11 sine-only notes, with
// flattened peaks. That path adds three IG00151 OTAs, the output stage and
// an unknown capture. With one or two differential-pair (tanh) stages after
// the shaper, driven to match each note's H3, this shaper's H5 is 3.5-7.9 dB
// high at every note and its H7 11-20 dB high at 9 of 11, while an ideal
// sine's H5 is 0.9-11 dB low at every note (the tool's `chains` command;
// Docs/research/vco-authenticity-2026-10-20.md).
// tanh is not constexpr in C++17, so tanh(k) is written out (std::tanh(1.4020)).
constexpr double wscSineDrive=1.4020;
constexpr double wscSineTanhDrive=0.8857831884152492;
double wscSine(double input) {return fastTanh(wscSineDrive*input)/wscSineTanhDrive;}
double wscSineSlope(double input) {
    const double t=fastTanh(wscSineDrive*input);
    return wscSineDrive*(1-t*t)/wscSineTanhDrive;
}
// TO reaches STI through 4.7uF NP with 22k to ground (SM2 p.16): see Oscillator.
constexpr double wscSineInputTau=22e3*4.7e-6;
// Mean square over a period of the ideal folded triangle (no plateau):
// (1/2)*integral_{-1}^{1} tanh(k*u)^2 du/tanh(k)^2 = (1-tanh(k)/k)/tanh(k)^2,
// .4692770 (an ideal sine's .5, so -.275 dB RMS at the same peak).
constexpr double wscSineMeanSquare=
    (1-wscSineTanhDrive/wscSineDrive)/(wscSineTanhDrive*wscSineTanhDrive);
// 2*sqrt(wscSineMeanSquare): RMS of four incoherent unit-peak shaped sines.
constexpr double wscFourSineRms=1.370075901392172;
static_assert(wscFourSineRms*wscFourSineRms-4*wscSineMeanSquare<1e-15 &&
              4*wscSineMeanSquare-wscFourSineRms*wscFourSineRms<1e-15,
              "Four-sine RMS literal must be 2*sqrt(wscSineMeanSquare)");

// PRA output preamp (SM2 printed p.22 / PDF16; guide printed p.47 / PDF56).
// VOLUME's wiper (I2) drives IC2-1/2, an HA1452, as an inverting amplifier:
// 100k+47k in, 150k plus the VR6 B-1M rheostat as feedback, so 1.02 to 7.82,
// then 33uF to HO. It never sees a nonlinearity of its own at playing
// levels. The guide's VR6 procedure trims the HO terminal to 0 dBm at the
// maximum output level, and its panel description (printed p.7 / PDF16)
// defines that as all volume controls fully clockwise with four keys down.
// The chip's rails are +/-15V through its own 390 ohm (+) and 220 ohm (-)
// droppers. The HA1452 is a 25V-rated two-channel audio preamplifier, Icc
// 12mA (radiomuseum.org HA1452 data); 12mA through the droppers leaves the
// chip +10.32/-12.36V. Its output swing is not published: the NJM4558 that
// shares the PRA board (IC3,4,9) is the documented sibling, typical +/-13V
// on +/-15V into 2k or more, so 2V short of each rail. HO therefore clips at
// +8.32V and -10.36V, a hard op-amp limit with the loop opened.
// Engine reference: the sibling CS-60 procedure (CS-60 service manual
// printed p.39 / PDF 41, PRA III) trims VR6 for 0 dBm at the OUT jack with
// the sine lever, SUSTAIN and LEVEL at maximum on PANEL and all keys down:
// a sine-only tone. The PRA board is shared (CS-50 SM2 p.22 note 7: "IC9 is
// used for CS-60 only"). Each sine sits at CP4 1Vpp (M board VI.1, printed
// p.38 / PDF 40), engine +/-1 peak. Four of them add incoherently to
// 2*sqrt(wscSineMeanSquare) RMS (the shaped sine above; sqrt(4/2) for an
// ideal sine), and .18 of that at full VOLUME reads 0 dBm, .775V RMS, at HO.
// Cross-check: VI.4 sets each CS-50 voice output O to 0.6V with .8Vpp at
// CP4, and four 1Vpp sines through 0.75 and the four 1k/47k bus branches
// are .132V RMS on the bus, so VR6 must give 5.9 of its 1.02..7.82 range.
// The meter and its load: the CS-80's output-amplifier step (sibling
// evidence, CS-80 Adjustment Procedure printed p.29) reads the output jack
// on an AC voltmeter into a 10k load, .78V RMS (0 +/-3dB). So 0 dBm is
// .775V RMS into 10k. HO leaves the op-amp's 33uF node through 560 ohm (SM2
// p.22), so with 10k on HO the op-amp sits 10.56/10 (+.47dB) above the HO
// reading, and the engine plays HO into that load. Still assumed: the CS-50
// was trimmed on the CS-80's bench load (its guide and the CS-60 sheet name
// none; read unloaded the rails sit .47dB higher, into 600 ohm 5.3dB lower)
// and the four notes add incoherently. The keys are held at full SUSTAIN,
// where the aligned VCA EG sits 0.2V below its 10V peak (see
// ampEnvelopeFloor), so the common VCA passes .98 of each sine while VR6 is
// trimmed.
constexpr double outputPreampGain=.18;
constexpr double outputReferenceRms=outputPreampGain*wscFourSineRms*.98;
constexpr double outputReferenceVolts=.7745966692414834; // 0 dBm, sqrt(.6)
constexpr double outputHaSupplyCurrent=12e-3, outputSwingMargin=2;
// HO per volt at the op-amp with the bench load on HO.
constexpr double outputLoadedShare=outputBenchLoad/(outputBenchLoad+outputSeriesResistance);
// HO is the inverse of this node, so HO's +8.32V bounds it below.
constexpr double outputRailHigh=(15-outputHaSupplyCurrent*220-outputSwingMargin)
    *outputLoadedShare*outputReferenceRms/outputReferenceVolts;
constexpr double outputRailLow=-(15-outputHaSupplyCurrent*390-outputSwingMargin)
    *outputLoadedShare*outputReferenceRms/outputReferenceVolts;
double outputPreamp(double volts,double volume) {
    return clamp(volts*outputPreampGain*volume,outputRailLow,outputRailHigh);
}

struct OutputCoupling {
    double memory=0;
    // The trapezoid's g = 1/(2 fs tau) and the input's share 1/(1+g) depend
    // on the rate and the part alone: fixed once per configuration, so the
    // sample loop multiplies where it divided twice (two divisions on each
    // coupling's recursion were a measurable share of the 1x cost; the
    // reciprocal differs from the division by at most an ulp).
    struct Coefficients {
        double feedback=0, share=1;
        Coefficients() = default;
        Coefficients(double tau,double fs) {
            const double g=1/(2*fs*tau);
            feedback=2*g; share=1/(1+g);
        }
    };
    double process(double x,const Coefficients& c) {
        const double high=(x-memory)*c.share;
        memory+=c.feedback*high;
        return high;
    }
    double process(double x,double tau,double fs) { return process(x,Coefficients(tau,fs)); }
};

// CS-50 expression network (PRA, SM2 p.22; guide printed pp.45-46). The foot
// controller's voltage becomes a current through the 820 ohm leg of IC3-1/2
// and lights the PS88-G50.201B, whose CdS cell sits between MI (the
// MODULATION lever wiper) and MO (the top of the A-10k VOLUME track, whose
// foot returns to earth through 330 ohm). In parallel with the cell: 68k to a
// midpoint, 0.33uF with 2.2k from there to ground, 68k on to MO, and 0.0033uF
// from MO to ground. So a closed pedal does not simply turn the signal down.
// It hands the signal to that leg, which passes DC at -23dB but only -48dB
// above the midpoint's 219Hz corner, and whose 9k source into 0.0033uF puts a
// pole at 5.4kHz: the quieter it gets, the more bottom-heavy and the duller.
//
// Only the cell's resistance is unknown, and the guide prints what it does:
// level against pedal travel, 0 to -40dB over about 28 degrees, +/-5dB. The
// table below is that curve read off the printed graph at 600dpi; the cell's
// resistance is then whatever makes the network's midband match it (1.5 Mohm
// at the -40dB floor), so no CdS law is invented. The pedal's own level is
// measured, not estimated; its placement on the travel axis is the graph's.
constexpr double expressionArm=68e3, expressionShunt=2.2e3;
constexpr double expressionShuntCap=.33e-6, expressionOutCap=.0033e-6;
constexpr double expressionLoad=10e3+330;
// Decibels at travel 0, .1, ... 1 of the graph's 28 degrees.
constexpr double expressionCurve[]={0,-1.4,-3.8,-7.2,-11.5,-16.5,-22,-27.3,-32.4,-37.1,-39.8};
// H(s) = (n0+n1.s)/(d0+d1.s+d2.s^2) for the two-node network above, bilinear.
struct ExpressionNetwork {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    double x1=0,x2=0,y1=0,y2=0;
    void clear() { x1=x2=y1=y2=0; }
    void configure(double expression,double fs) {
        const double travel=clamp(1-expression,0.0,1.0)*10;
        const auto index=static_cast<std::size_t>(travel<10 ? travel : 9);
        const double decibels=expressionCurve[index]
            +(expressionCurve[index+1]-expressionCurve[index])*(travel-index);
        // Invert the midband (0.33uF a short, 0.0033uF open) for the cell.
        const double g=1/expressionArm, gs=1/expressionShunt, gl=1/expressionLoad;
        const double pass=g*g/(2*g+gs), shunted=g-pass;
        const double gain=std::pow(10,decibels/20);
        const double cell=(gain*(shunted+gl)-pass)/(1-gain);
        const double tau=expressionShunt*expressionShuntCap;
        const double sum=cell+g+gl;
        const double n0=2*g*cell+g*g, n1=(2*g*cell+g*g)*tau+cell*expressionShuntCap;
        const double d0=2*g*sum-g*g;
        const double d1=sum*(2*g*tau+expressionShuntCap)+2*g*expressionOutCap-g*g*tau;
        const double d2=expressionOutCap*(2*g*tau+expressionShuntCap);
        const double k=2*fs, a0=d0+d1*k+d2*k*k;
        b0=(n0+n1*k)/a0; b1=2*n0/a0; b2=(n0-n1*k)/a0;
        a1=(2*d0-2*d2*k*k)/a0; a2=(d0-d1*k+d2*k*k)/a0;
    }
    double process(double x) {
        const double y=b0*x+b1*x1+b2*x2-a1*y1-a2*y2;
        x2=x1; x1=x; y2=y1; y1=y;
        return y;
    }
};

// Direct CS-50 PRA: four 100uF capacitors around the uA796 carrier input.
// Nodes m=coupling-capacitor/33k junction, b=pin8, a=audio-bias, s=bypassed
// local supply. The 33k runs in series from m to pin 7, and pin 7 reaches
// pin 8 through 100 ohm in parallel with the carrier port's small-signal rpi,
// so m-b is one 33k+97.5 ohm branch and pins 7-8 carry its 97.5/33097.5 share.
// Ideal sine source; the IG00150's output impedance and the .047uF on its
// pin 6 are not modelled (the source impedance is unpublished).
struct RingCarrierCoupling {
    std::array<std::array<double,4>,4> inverse{};
    std::array<double,4> memory{};
    void configure(double fs) {
        std::array<std::array<double,4>,4> g{};
        auto ground=[&](unsigned i,double r){g[i][i]+=1/r;};
        auto edge=[&](unsigned i,unsigned j,double r){
            const double y=1/r;g[i][i]+=y;g[j][j]+=y;g[i][j]-=y;g[j][i]-=y;
        };
        edge(0,1,33000+ringCarrierPortShunt);edge(1,2,820);edge(1,3,1200);
        ground(2,1000);ground(3,390);
        // TPT capacitor coordinates w=(x-source,b,a,s):
        // (I+G/(2fsC))*v=memory+(source,0,0,0), memory'=2w-memory.
        // Factor once at construction, not in the sample callback.
        std::array<std::array<double,8>,4> augmented{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
            augmented[i][j]=(i==j?1.:0.)+g[i][j]/(2*fs*100e-6);
            augmented[i][j+4]=(i==j?1.:0.);
        }
        for(unsigned col=0;col<4;++col) {
            unsigned pivot=col;
            for(unsigned row=col+1;row<4;++row)
                if(std::abs(augmented[row][col])>std::abs(augmented[pivot][col]))pivot=row;
            std::swap(augmented[col],augmented[pivot]);
            const double diagonal=augmented[col][col];
            for(double& value:augmented[col])value/=diagonal;
            for(unsigned row=0;row<4;++row)if(row!=col) {
                const double multiplier=augmented[row][col];
                for(unsigned j=0;j<8;++j)augmented[row][j]-=multiplier*augmented[col][j];
            }
        }
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)inverse[i][j]=augmented[i][j+4];
        clear();
    }
    void clear() {memory.fill(0);}
    double process(double source) {
        auto rhs=memory;rhs[0]+=source;
        std::array<double,4> nodes{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)nodes[i]+=inverse[i][j]*rhs[j];
        for(unsigned i=0;i<4;++i)memory[i]=2*(nodes[i]-(i==0?source:0.))-memory[i];
        // Actual pin7-pin8 carrier per volt of source; no gain normalization.
        return (nodes[0]-nodes[1])*ringCarrierPortShunt/(33000+ringCarrierPortShunt);
    }
};

// Retrigger: G1 takes the key-on edge of G2 through a 1000pF ceramic ("K",
// SM2 p.16/17 notes), so each new key-on starts an attack from whatever
// charge the .22uF capacitor holds; on() therefore never resets voltage.
// IG00159 VCA-EG and IG00152 VCF-EG (CS-50 service manual, pp.20–21):
// a sequencer switches a variable
// resistance into an integrating capacitor. Each segment below is the exact
// RC solution v(t+dt)=target+(v(t)-target)*exp(-dt/RC). The attack target has
// configurable target headroom (1% default, not a hardware calibration);
// attack duration reaches the nominal peak from IL for every headroom value.
// The chip's ideal headroom is zero: IG00159's sequencer compares the buffer
// against the same 30k/60k node that G1-1 puts on LC (Yamaha IC Guide p.2-7;
// Guide p.43), so where an attack ends is set by unpublished comparator,
// buffer and IG00151 offsets and by the VR17 trim (470k into +I against the
// 100k LC/OUT feeds), whose procedure is not printed.
// Decay/release durations denote 60 dB settling (not hardware calibration).
// Both M-board integrating capacitors are .22uF. The parameter durations set
// equivalent resistance; e.g. R_decay = duration/(.22uF*ln(1000)).
// VCA-EG held floor. The CS-60 alignment of the shared M board (Yamaha CS-60
// Service Manual p.38, EG-VCA item 1; SM2 p.16 draws the board for
// "CS-60,50", with the same VR17 B50K/470k into IG00151 +I against the 100k
// LC and OUT feeds) sets VR17 so that TP2 reads -200 +/-20mV with SL = 0V
// and the key held, against its 10 +/-.2V peak (item 2). VR17's current is
// constant, so every VCA-EG target (SL while held, IL = 0V after release)
// sits .2V below its nominal level: -.02 of the 10V peak. The VCA gain is
// linear in the positive envelope (guide pp.40-41) and closed below zero,
// so a decay to SL = 0 and every release reach true silence in finite time
// (ln((v+.02)/.02) RC from v) instead of approaching it exponentially.
// Sibling alignment evidence for a shared board, not a CS-50 measurement.
// The VCF-EG is nulled to 0 +/-.01V (VR18, p.37) and keeps no floor.
constexpr double ampEnvelopeFloor=-.2/10;
struct Envelope {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    double voltage = 0;
    bool gate = false;
    void on() { gate = true; stage = Attack; }
    void off() { gate = false; if (stage != Idle) stage = Release; }
    double advance(double initial, double peak, double sustained,
                   double headroom, double saturation, double attackIncrement, double currentStep,
                   double decayCoefficient, double releaseCoefficient) {
        // Key-off keeps IL/RT selected (Guide pp.43–44): an idle board is
        // still a connected capacitor, not a reset switch. Continue the
        // existing release law if its target moves; Idle remains an audio
        // bookkeeping state. The physical post-settling rate is unmeasured.
        double target = initial, coefficient = 0;
        if (stage == Attack) {
            const double span = std::max(1e-9, inlineAbs(peak - initial));
            target = peak + headroom * span;
            if (saturation>0 && voltage<peak) {
                // Optional differential-pair/current-limit approximation:
                // dv/dt = k*Vs*tanh((target-v)/Vs). Integrating gives
                // log(sinh(y_next)) = log(sinh(y)) - k*dt, y=(target-v)/Vs.
                // k is normalized so a full IL->peak attack retains its
                // requested duration. This is a model experiment, not an
                // identified IG00151 transistor/current-limit parameter.
                const double vs=saturation*span;
                const double y=(target-voltage)/vs;
                const double nextLog=logSinh(y)-currentStep;
                const double increment=(y>20 && nextLog>20)
                    ? currentStep : y-asinhExp(nextLog);
                voltage+=vs*increment;
            } else {
                voltage += (target - voltage) * attackIncrement;
            }
            if (voltage >= peak) { voltage = peak; stage = gate ? Decay : Release; }
            return voltage;
        }
        if (stage == Decay || stage == Sustain) {
            target = sustained;
            coefficient = decayCoefficient;
            voltage = target + (voltage - target) * coefficient;
            if (inlineAbs(voltage - target) < 1e-7) { voltage = target; stage = Sustain; }
        } else {
            coefficient = releaseCoefficient;
            voltage = target + (voltage - target) * coefficient;
            if (inlineAbs(voltage - target) < 1e-7) { voltage = target; stage = Idle; }
        }
        return voltage;
    }
};

double blep(double phase, double step) {
    if (phase < step) { const double t = phase / step; return 2*t - t*t - 1; }
    if (phase > 1 - step) { const double t = (phase - 1) / step; return t*t + 2*t + 1; }
    return 0;
}
double blamp(double phase, double step) {
    if (phase < step) { const double t = 1 - phase / step; return step * t*t*t / 3; }
    if (phase > 1 - step) { const double t = 1 + (phase - 1) / step; return step * t*t*t / 3; }
    return 0;
}

// Noise limiter (guide printed p.32; SM2 p.8 SUB board). IC11-2 (gain
// 10M/10k) is driven into its +/-15V rails by the pick-up amplifier's
// band-limited noise. That bandwidth is NOT documented: ESTIMATE it as
// IC11-1's closed-loop pole, NJM4558 typical GBW 3MHz / (1+1M/4.7k) =
// 14.03kHz (datasheet typical, not a measured unit). The one-pole Gaussian
// process is sampled exactly, an AR(1) with pole exp(-dt/tc).
constexpr double limiterTau=1/(2*pi*(3e6/(1+1e6/4.7e3)));
// IC11-2 cannot follow that sign: at the NJM4558's typical 1V/us slew rate
// (ESTIMATE, datasheet typical) a rail-to-rail swing takes 30us, while the
// sign of the 14kHz process flips about every 18us, so most short excursions
// turn back before the rail. "Driven to saturation" makes this the large-
// overdrive limit: the output slews at full rate toward the rail the
// input's sign selects. Against an ideal sign the body (up to 3kHz) gains
// about 3.4dB and the top loses 1.2dB at 10kHz, 4.4dB at 15kHz, 7dB at
// 20kHz. The limiter runs at >=768kHz substeps, within .3dB of a 20MHz
// reference at every rate, averaged over each internal sample.
constexpr double limiterSlew=1e6/15, limiterSubstepRate=768e3;
// The zener reaches IC11-1's + input through .047uF with 100k to ground (SM2
// p.8), a 33.86Hz high-pass ahead of the limiter. The limiter keeps it as a
// shelf (an ideal sign has 11dB less density at DC, by the arcsine law); it
// takes 1.2dB off SUB-N RMS. The limiter's own 10uF/(10k||100k) coupling
// (1.75Hz) is negligible beside it.
constexpr double pickupCouplingTau=100e3*.047e-6;
// SUB-N: N through 22k/.1uF loaded by SUI's 100k, in SUB units (the 3Vpp
// SUI waveforms, 1.5V peak): the +/-15V swing times the 100/122 divider.
constexpr double subNoiseResistance=22e3*100e3/122e3;
constexpr double subNoiseRail=15*(100./122.)/1.5;
// SUB-EXT. The rear EXT IN jack's tip goes to the top of the A-50K LEVEL pot
// (PN5 VOLUME, sleeve to its foot; SM2 printed p.27), whose wiper is PRA
// EX(53). The EXT preamp, IC3-1/2 (NJM4558D), is non-inverting with 100k
// feedback over 1k to ground, gain 101 (SM2 printed p.22; guide printed p.56 /
// PDF 65 draws the same stage). Its output reaches EXO(56) through a 10uF
// non-polar capacitor with 100k to ground, and FUNCTION EXT (PN3 SSW M6)
// joins EXO to SUI, where the SUB board's 100k to ground and the IC1-1
// follower (SM2 printed p.8) take it to SUO like every other source. So the
// coupling sees 100k||100k, a .318 Hz high-pass, while EXT is selected, and
// EXO's own 100k (.159 Hz) otherwise; the capacitor charge carries across a
// switch. In units of the 1.5 V SUB peak, the stage gain is 1 by definition of
// subExternalUnitVolts (1.5 V / 101). Swings, NJM4558 datasheet typicals
// (VOM, +/-15 V; ESTIMATE): the preamp drives 101k||50k, so +/-14 V (RL>=10k);
// IC1-1 drives the three B-10k PN3 levers, about 3k, so +/-13 V (RL>=2k). The
// square's 15k/1.8k divider assumes the same 14 V (14*1.8/16.8 = 1.5 V).
// The preamp's 3 MHz typical gain-bandwidth (29.7 kHz closed-loop) and 1 V/us
// slew rate act only at the top of the audio band or in hard overload, and
// are not modelled.
constexpr double externalPreampRail=14/1.5, externalBufferRail=13/1.5;
constexpr double externalCouplingTauSelected=10e-6*(100e3*100e3/200e3);
constexpr double externalCouplingTauOpen=10e-6*100e3;

// Acklam's rational inverse normal CDF, relative error below 1.2e-9: one
// uniform draw per Gaussian sample keeps the shared random sequence intact.
double inverseNormal(double p) {
    static constexpr double a[]={-3.969683028665376e+01,2.209460984245205e+02,-2.759285104469687e+02,
        1.383577518672690e+02,-3.066479806614716e+01,2.506628277459239e+00};
    static constexpr double b[]={-5.447609879822406e+01,1.615858368580409e+02,-1.556989798598866e+02,
        6.680131188771972e+01,-1.328068155288572e+01};
    static constexpr double c[]={-7.784894002430293e-03,-3.223964580411365e-01,-2.400758277161838e+00,
        -2.549732539343734e+00,4.374664141464968e+00,2.938163982698783e+00};
    static constexpr double d[]={7.784695709041462e-03,3.224671290700398e-01,2.445134137142996e+00,
        3.754408661907416e+00};
    const auto tail=[](double q) {
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])/((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
    };
    if (p<.02425) return tail(inlineSqrt(-2*std::log(p)));
    if (p>1-.02425) return -tail(inlineSqrt(-2*std::log(1-p)));
    const double q=p-.5, r=q*q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q/(((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
}

// One shared SUB source. Discontinuities use the same short analytic BLEP
// correction as the VCO pulse, without tables. Noise comes from the shared
// limiter sample through its dedicated RC path.
double subWaveform(double phase,double step,double waveform,double noise) {
    if (waveform==1) return 1-2*phase+blep(phase,step);
    if (waveform==2) return -(1-2*phase+blep(phase,step));
    if (waveform==3) return (phase<.5 ? 1.:-1.)+blep(phase,step)-blep(wrapUnit(phase-.5),step);
    if (waveform==4) return noise;
    return sinTurn(phase);
}

struct Oscillator {
    double phase = 0;
    // The TO-STI coupling capacitor's voltage, in units of the triangle's
    // peak. A board's VCO runs from power-on, so the first sample precharges
    // it to the triangle's mean at that pitch.
    double stiCoupling = 0;
    bool stiPrecharged = false;
    // stiCoefficient: -expm1(-1/(fs*wscSineInputTau)).
    OscillatorFrame process(double frequency, double pulseWidth, double resetSeconds, double fs,
                            double stiCoefficient, bool sineEnabled = true,
                            bool sawEnabled = true, bool pulseEnabled = true) {
        // Guide pp.28-31: OUT7 conditions the integrating-capacitor signal
        // with the monostable pulse before the WSC. It is not the exposed raw
        // recharge triangle. In the nominal current law r=f*tau, fixed A0
        // normalization makes the descending segment exactly 1-2*phase.
        // A flat reset plateau followed by a 2r step is a reduced clamp model
        // of the published sketch, not a recovered hidden-chip transfer.
        const double step = clamp(frequency / fs, 1e-8, .18);
        // Charge balance (guide printed p.28): C2*Vpp=(Iref-I)*tau=I*t, so
        // f=I/(Iref*tau) and the one-shot fills r=f*tau of every period at
        // every pitch, exactly, until I reaches Iref (r=1, f=1/tau=16.7kHz),
        // where the discharge interval vanishes. The CS-50 itself stops near
        // r=.38: top C6 on 4' (4186Hz), PITCH at its 6V end (TU 6V/4V, guide
        // printed p.23, x1.5) and +14 cent fine tune (p.5) give 6.33kHz. The .9
        // guard only bounds MIDI notes and engineering tuning beyond that.
        // tau itself is a trim, not a part value: the CS-50 pitch procedure
        // (CS-60 service manual printed p.17, which lists the CS-50 values)
        // sets VR3 so the C6 key gives 4186Hz +/-1 cent at 4', where RFT is
        // the untrimmed 20k 0.1% resistor (SM2 p.16 table), and VR2 so C2
        // gives 261.6Hz. So Iref*tau=1/(1046.4Hz/V*20k), i.e. the shipped
        // 59.73us at the guide's nominal 800uA; only Iref's own tolerance
        // (unpublished) moves r.
        const double recharge = clamp(frequency * resetSeconds, 1e-7, .9);
        const double resetPhase = wrapUnit(phase-recharge);
        // WSC pin5 inverts the conditioned downward ramp. Correct both
        // voltage jumps and both slope changes analytically, without tables.
        // A muted waveform is a pure function of the phase (no state of its
        // own), so its gate open or closed costs nothing later: skip it.
        double saw = 0;
        if (sawEnabled) {
            const double sawBase = phase < recharge ? -1 : 2*phase-1;
            saw = sawBase - blep(phase,step)
                + recharge*blep(resetPhase,step)
                + blamp(resetPhase,step)-blamp(phase,step);
        }
        const double duty = clamp(pulseWidth, .01, .99);
        // WSC guide p.31 explicitly states that PWM moves only the falling
        // edge. Its rising edge stays locked to the VCO period. Applying a
        // comparator to BOTH slopes of the internal recharge capacitor would
        // incorrectly move both edges; WSC uses the conditioned VCO output.
        // This is the comparator's logical timing coordinate. PI (pin 9) is
        // wired to the same T1 node as SI and TL (SM2 p.16, 22k to ground),
        // and the IG00158 pin sheet (SM2 p.21, pins 9-11) draws PO as that
        // ramp compared with the PW/PWM level: high from the reset jump until
        // the descending ramp crosses the level. The threshold sits at p=1-2d
        // on the conditioned ramp p (bottom fixed, slope fixed per period), so
        // it is crossed at phase d; while the one-shot holds p on its top
        // plateau no crossing exists, and a level above the ramp's start
        // 1-2r (d<r, deep PWM at upper notes) is crossed at the plateau's
        // step instead. The falling edge is therefore at max(d,r).
        double square = 0;
        if (pulseEnabled) {
            const double fall = std::max(duty, recharge);
            square = phase < fall ? 1 : -1;
            square += blep(phase,step) - blep(wrapUnit(phase-fall),step);
        }
        // WSC pin14 -> pin3 folds the SAME conditioned ramp into the
        // triangle tri=2|p|-1 (TO, 3.2Vpp, SL pin 4 open; SM2 p.21): 1 on the
        // plateau, then 1-4*phase down to the fold at phase .5 and 4*phase-3
        // back up (a plateau longer than .5 covers the fold). TO reaches STI
        // through 4.7uF NP with 22k to ground (SM2 p.16). STI is the input of
        // the shaper's differential pair, which that 22k biases, so the
        // coupling is a 103ms (1.54Hz) high-pass into a high impedance: its
        // capacitor c settles at the triangle's mean, 2r^2 of peak with the
        // plateau (.2% at C4, 3% at C6 on 8', 29% at the 6.33kHz top), and
        // each new pitch re-centres STI with that time constant. pin15 -> pin2
        // shapes STI, u=tri-c, with the bipolar differential pair
        // g(u)=tanh(k*u)/tanh(k) (wscSineDrive above: textbook topology at its
        // own minimum-THD drive, not measured, chosen by ear 2026-10-20 without
        // the coupling; the CS-80 recording that questions it is noted there).
        // Sine slice trim and the chip's real transistor shape are unpublished.
        // Band-limiting, analytic as for the saw: the plateau step at r is a
        // jump g(tri(r+)-c)-g(1-c) and a slope change to -/+4*g'(tri(r+)-c);
        // the reset corner (slope 4*g'(1-c) into the flat plateau) and the
        // fold corner (-4*g'(-1-c) to +4*g'(-1-c)) are BLAMPs. The step also
        // turns c's own slope by 4*resetSeconds/tau per unit phase, under .1%
        // of the triangle's, which is left unlimited.
        const double tri = phase < .5 ? 1-4*phase : 4*phase-3;
        const double conditioned = phase < recharge ? 1 : tri;
        if (!stiPrecharged) {
            stiCoupling = recharge < .5 ? 2*recharge*recharge : 4*recharge-1-2*recharge*recharge;
            stiPrecharged = true;
        }
        double sine = 0;
        if (sineEnabled) {
            const double c = stiCoupling;
            sine = wscSine(conditioned-c);
            if (resetPhase < step || resetPhase > 1-step) {
                const bool foldAfterPlateau = recharge < .5;
                const double plateauEnd = (foldAfterPlateau ? 1-4*recharge : 4*recharge-3)-c;
                sine += .5*(wscSine(plateauEnd)-wscSine(1-c))*blep(resetPhase,step)
                    + .5*(foldAfterPlateau ? -4 : 4)*wscSineSlope(plateauEnd)*blamp(resetPhase,step);
            }
            if (phase < step || phase > 1-step) sine -= 2*wscSineSlope(1-c)*blamp(phase,step);
            const double fold = wrapUnit(phase-.5);
            if (recharge < .5 && (fold < step || fold > 1-step))
                sine += 4*wscSineSlope(-1-c)*blamp(fold,step);
        }
        // Keep the physical TO-STI capacitor and free oscillator running
        // while its output is muted, ready for a live sine-level edit.
        stiCoupling += (conditioned-stiCoupling)*stiCoefficient;
        phase = wrapUnit(phase + step);
        // Audio polarity: the completed pulse is negated, including its BLEP,
        // without moving either comparator edge, changing PW/PWM timing or
        // introducing another oscillator, so saw and pulse fall together at
        // the reset and equal levels mix constructively. Recordings of two
        // instruments support that. Public CS-50 solo/both-on recordings mix
        // constructively at equal nominal source levels. A public CS-80 sample
        // set (same M board #21233, so the same IG00158;
        // Tools/measure_cs80_wsc_recordings.py) holds the falling audio pulse
        // edge fixed under PWM while the rising one moves (period-to-period
        // spread .04-.23 against .24-12 samples, all 12 PWM notes), and its
        // saw rises and drops at the reset (all 11 SAW notes); the fixed edge
        // is the reset, so pulse and saw fall together. Both assume one chain
        // polarity across the takes; neither is a transistor-level proof.
        // Yamaha's two drawings point the other way: the guide's WSC block
        // (p.31) labels SI->SO an inverter, and the SM2 p.21 pin sheet draws
        // PO high from the reset jump; read literally, SO falls as PO rises
        // and equal-level mixing partly cancels. Nippon Gakki's own 1974
        // discrete converter (US 4,012,981, FIG. 2: NPN switch, PNP buffer)
        // is a non-inverting comparator like that PO. The M board gives both
        // the same FET gate and 150k into one node (SM2 p.16), so no board
        // stage reconciles them: the inversion the recordings need would sit
        // inside the IG00158, which no document shows. The recordings are
        // kept over the sketches; see
        // Docs/research/vco-authenticity-2026-10-20.md.
        return {saw, -square, sine};
    }
};

double besselI0(double x) {
    double term = 1, sum = 1;
    for (int k = 1; k < 40; ++k) {
        term *= (x*x*.25) / (k*k);
        sum += term;
        if (term < sum * 1e-16) break;
    }
    return sum;
}
struct Decimator {
    static constexpr unsigned maxTaps = 249;
    // Coefficients oldest tap first (h is symmetric, this is its mirror
    // image stored explicitly) and a mirrored ring: every sample is written
    // at [cursor] and [cursor+taps], so the newest `taps` samples are always
    // one contiguous ascending run, oldest first, starting at [cursor].
    std::array<double,maxTaps> h{}, oldestFirst{};
    std::array<double,2*maxTaps> history{};
    const unsigned taps;
    unsigned cursor = 0;
    explicit Decimator(unsigned factor=4) : taps(62*factor+1) {
        const double cutoff = factor==4 ? .1125 : .225;
        // 90% of output Nyquist; same31-host-sample group delay at2x/4x.
        // The4x coefficient generation and summation order stay unchanged.
        const double norm = besselI0(10);
        double sum = 0;
        for (unsigned i = 0; i < taps; ++i) {
            const double n = static_cast<double>(i) - (taps-1)*.5;
            const double r = n / ((taps-1)*.5);
            const double sinc = n == 0 ? 2*cutoff : std::sin(2*pi*cutoff*n)/(pi*n);
            h[i] = sinc * besselI0(10*std::sqrt(std::max(0.0,1-r*r))) / norm;
            sum += h[i];
        }
        for (unsigned i=0;i<taps;++i) h[i] /= sum;
        for (unsigned i=0;i<taps;++i) oldestFirst[i] = h[taps-1-i];
    }
    void push(double x) {
        history[cursor] = history[cursor+taps] = x;
        cursor = cursor+1 == taps ? 0 : cursor+1;
    }
    // One contiguous window instead of two runs around the wrap. Four
    // interleaved partial sums keep the additions off each other's latency
    // (one running sum was about a tenth of the 4x cost); the lanes are
    // fixed to the tap index, where the two runs restarted them at the
    // wrap, so the grouping of the additions differs from 1.0.4 at the last
    // bit (1e-16 relative), not at the float output. The window is read
    // oldest first against the mirrored coefficients; lane j therefore
    // holds the taps i = 3-j mod 4 and the final sum pairs them as the
    // tap-ordered lanes would.
    double output() const {
        const double* window = history.data() + cursor;
        const double* coefficient = oldestFirst.data();
        double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        unsigned k = 0;
        for (; k + 4 <= taps; k += 4) {
            s0 += coefficient[k] * window[k];
            s1 += coefficient[k+1] * window[k+1];
            s2 += coefficient[k+2] * window[k+2];
            s3 += coefficient[k+3] * window[k+3];
        }
        for (; k < taps; ++k) s3 += coefficient[k] * window[k];
        return (s3 + s2) + (s1 + s0);
    }
    void clear() { history.fill(0); cursor = 0; }
};
struct OutputRate {
    Decimator four{4}, two{2};
    std::array<double,Synth::outputLatency+1> delay{};
    unsigned cursor=0;
    void push(double value,unsigned factor) {
        if(factor==4)four.push(value);
        else if(factor==2)two.push(value);
        else {delay[cursor]=value;cursor=(cursor+1)%delay.size();}
    }
    double output(unsigned factor) const {
        if(factor==4)return four.output();
        if(factor==2)return two.output();
        return delay[cursor]; // exact31-sample delay, no redundant1x low-pass
    }
    void clear() {four.clear();two.clear();delay.fill(0);cursor=0;}
};
} // namespace

// SUB-OSCILLATOR lever laws. Guide printed p.55 / PDF 64: the SUB is VCO II,
// an IG00150; SPEED (PN3 SVR1, B-10k between 0 and +10V; SM2 p.27) drives VC2,
// which the guide draws reaching pin 1 through 180k, with 330k from pin 1 to a
// rail. The CS-50 SUB board (SM2 printed p.8) has 120k plus VR13 (B-100K
// trim) in series, 120k..220k, and the 330k to -15 V; VR9 (B-500K) on Iadj
// sets the frequency. Pin 1 is a current input: the guide's PWM oscillator,
// the same IG00150, takes VC1 through 110k (printed p.49) and spans
// log2(222/.2)=10.12 octaves over 10V, the SUB log2(50/.8)=5.97; 110k/180k
// predicts the ratio 1.636 and the tables give 1.695 (3.5%), while a
// voltage-input divider against the 330k predicts 1.16. So the rate is
// exponential in VC2, and the lever works into the guide's 180k, inside the
// board's trim range. With a scale trim and a frequency trim, both
// table ends can be set: .8+/-.16, 6.7+/-1.3 and 50+/-10 Hz at 0/5/10 V (no
// CS-50 trim procedure for them is printed). As for the PWM clock in preset()
// below, the law runs exponentially through the end points; it gives 6.325 Hz
// at 5 V, 5.6% under the printed nominal and inside its tolerance.
double subSpeedHz(double vc2Volts) {
    return .8*std::pow(50/.8,clamp(vc2Volts,0.,10.)/10);
}
double subSpeedLeverHz(double travel) {
    return subSpeedHz(10*leverFraction(clamp(travel,0.,1.),10e3/180e3));
}
// VCO lever (PN3 SVR2, B-10k between SUO and ground) -> R7 V1 -> 100k into
// each board's VB node, which also takes the touch vibrato V2 through 100k
// (SM1 printed p.14) and drives IG00153 pin 15 directly (SM2 printed p.16),
// "50 cent/1.5V" (guide printed p.27). With pin 15 drawing no current, VB is
// the mean of V1 and V2, so the full-lever 1.5V-peak SUB is .75V there:
// 25 cents. The wiper sees four 100k+100k paths to the low-impedance V2
// outputs, 50k. Pin 15's input impedance is not published; any finite value
// would lower the 25 cents.
double subVibratoLeverCents(double travel) {
    return 25*leverFraction(clamp(travel,0.,1.),10e3/50e3);
}
// VCF lever (PN3 SVR3, B-10k between SUO and ground) -> R7 SUB -> 47k LP and
// 150k HP on each of four boards (SM1 printed p.14): 8.95k load. LP octaves per
// SUB unit at full lever are 1.5V through 47k against the FL lever's 1 oct/V
// through 39k (leverOctavesPerVolt, as for lpBrillianceScale): 1.245. HP
// follows at hpSubRelativeScale inside the engine. The f-node resistors to ground (R6/R7,
// 1.8-3.0k by chip rank, SM2 p.16) are neglected against 47k/150k, as the LC
// node is for tremolo; they would ease the sag by about 4%.
double subFilterLeverOctaves(double travel) {
    constexpr double load=1/(4/47e3+4/150e3);
    return leverOctavesPerVolt*39/47*1.5*leverFraction(clamp(travel,0.,1.),10e3/load);
}

bool setParameter(Patch& patch, const char* key, double value) {
    if (!key || !std::isfinite(value)) return false;
    for (const auto& parameter : parameters) {
        if (std::strcmp(key,parameter.key) == 0) {
            patch.*(parameter.member) = parameterValue(parameter,value);
            return true;
        }
    }
    return false;
}

namespace {
// PN2 lever wiring (SM2 p.26 PN2 sheet; overall circuit diagram, which also
// numbers TVR6). Both are B-10K (SM1 parts p.9; CS-60 parts p.9, same part
// numbers, marked common to the CS-50): TVR6 terminal 1 takes the T71 +10 V bus, terminal 3 ground,
// wiper to R7 RES (51). TVR7 terminal 1 takes M4 -15 V, terminal 3 +15 V,
// centre tap 5 ground, wiper to R7 BR (29). BRILLIANCE MAX raises both
// cutoffs (guide printed p.5), so terminal 3 is the MAX end; RESONANCE MAX at
// that same end puts 0 V on the Q pins, which is the Q5 end (SM1 p.19), so
// the two levers agree on orientation.
// Loads are the R7 matrix (SM1 p.14): BR drives four 680k (HP) and four 220k
// (LP) into cutoff nodes held near ground by the M-board R6/R7 shunts (about
// 2k, SM2 p.16); RES drives eight 220k branches into the pin-7 current
// inputs (qBranchLoad), which return to .78% of their own preset buses.
constexpr double brillianceLeverLoad=1/(4/680e3+4/220e3);
constexpr double resonanceLeverLoad=qBranchLoad/8;
// A wiper at fraction a of a track R between sources Ea (a=0) and Eb (a=1)
// is Ea+(Eb-Ea)a behind R*a*(1-a); the load returns to Vload.
double loadedWiper(double ea,double eb,double a,double track,double load,double vload) {
    const double source=ea+(eb-ea)*a, series=track*a*(1-a);
    return source+(vload-source)*series/(load+series); // exact at both ends
}
} // namespace

double brillianceLeverVolts(double travel) {
    if (!std::isfinite(travel)) return std::numeric_limits<double>::quiet_NaN();
    const double t=std::min(1.0,std::max(0.0,travel));
    // Each half of the centre-tapped B-10K is 5k between the tap and its rail.
    return t>=.5 ? loadedWiper(0,15,2*t-1,5e3,brillianceLeverLoad,0)
                 : loadedWiper(0,-15,1-2*t,5e3,brillianceLeverLoad,0);
}

double resonanceLeverVolts(double travel, double presetMeanVolts) {
    if (!std::isfinite(travel) || !std::isfinite(presetMeanVolts))
        return std::numeric_limits<double>::quiet_NaN();
    // Fraction of the 10k track between the wiper and terminal 3 (ground, MAX).
    const double a=1-std::min(1.0,std::max(0.0,travel));
    return loadedWiper(0,10,a,10e3,resonanceLeverLoad,presetMeanVolts*qBranchReturn);
}

namespace {
// T71 tone-preset matrix outputs (SM2 printed pp.10-11; guide printed p.64).
// Every row a-v holds a 10k to E and takes a 1S1555 from each preset column
// (T71's, and T72's, whose rows join T71's) and one from its PANEL input
// 1-22. The tablets and the PANEL levers share the T71 +10V regulator.
// ESTIMATE: the 1S1555's forward curve is not published, so it is modelled
// as the 1N4148-class silicon switching diode its ratings describe (35V,
// 100mA, 4ns): Is = 2.52nA, N = 1.75 at 300K. That is 0.58V at the
// stiffest preset column and 0.44V at the weakest.
constexpr double presetSupply=10, presetLoad=10e3;
constexpr double presetDiodeIs=2.52e-9, presetDiodeVt=1.75*25.85e-3;
// The cutoff rows g (FH) and i (FL) are loaded beyond that 10k. The board
// wiring lists (SM2 printed p.11 T71, p.15 R7) run them straight to R7 FH
// (19) and FL (20) with no buffer between. R7 (SM1 printed p.14) takes each
// cutoff bus through four 39k, one per board, to a pin-4 node that the M
// board holds near ground (SM2 printed p.16: R6 2.2k HP, R7 2.4k LP at rank
// B, beside the trim's 330k plus its centred B-100k's 25k; BR 680k/220k,
// SUB/TBR/VCF 150k/47k each, EG 220k/47k on Overall): about 10.3k in all, as
// much as the T-board's own 10k. (Docs/accuracy/preset-dc-loading solved the
// same network under the earlier, unsourced anchors and slopes.) ESTIMATE:
// pin 4 is taken as a high-impedance input and the VR7/VR9 trims as centred;
// a virtual-ground pin 4 (bare 39k branches) or a trim at either rail moves
// a cutoff bus by at most .06V.
constexpr double hpCutoffNode=1/(1/2.2e3+1/355e3+1/680e3+3/150e3+1/220e3);
constexpr double lpCutoffNode=1/(1/2.4e3+1/355e3+1/220e3+3/47e3+1/47e3);
constexpr double fhBusLoad=(39e3+hpCutoffNode)/4, flBusLoad=(39e3+lpCutoffNode)/4;
// PANEL HPF and LPF levers: B-10K slide pots across the T71 +10V on PN1
// (Overall), so 0 to 10V ("HPF lever normal at 0", "LPF normal at 10",
// guide printed p.35). Each wiper drives an NJM4558 follower, PRA IC4 (SM2
// printed p.22: fHI to fHO, fLI to fLO; the Overall drawing numbers the
// pair IC9), into T71 input 7 or 9, whose 1S1555 ORs it onto row g or i
// beside the preset columns (SM2 printed p.11). With a stiff source only
// that diode and the loaded row remain: the bus V solves
// V + Vd(V/rl) = 10*travel, rl being the 10k beside the R7 load.
double panelCutoffBus(double travel,double load) {
    const double source=presetSupply*travel, rl=presetLoad*load/(presetLoad+load);
    // V + Vd(V/rl) rises monotonically from 0 at V=0; bisect it onto source.
    double low=0, high=source;
    for (int i=0;i<64;++i) {
        const double mid=.5*(low+high);
        (mid+presetDiodeVt*std::log1p(mid/(rl*presetDiodeIs))<source ? low : high)=mid;
    }
    return .5*(low+high);
}
double panelCutoffHz(double travel,double load) {
    if (!std::isfinite(travel)) return std::numeric_limits<double>::quiet_NaN();
    return filterFrequencyFloor+gmHzAtMiddleC*
        std::exp2(leverOctavesPerVolt*panelCutoffBus(clamp(travel,0.,1.),load));
}
} // namespace

double hpfLeverHz(double travel) { return panelCutoffHz(travel,fhBusLoad); }
double lpfLeverHz(double travel) { return panelCutoffHz(travel,flBusLoad); }

#if !defined(CERES_EMBEDDED_TARGET)
bool setParameter(Patch& patch, const std::string& key, double value) {
    if (!std::isfinite(value)) return false;
    for (const auto& parameter : parameters) {
        if (key == parameter.key) {
            patch.*(parameter.member) = parameterValue(parameter,value);
            return true;
        }
    }
    return false;
}
#endif

struct Synth::Impl {
    struct Voice {
        int note = 60, keyNote = 60;
        // Glide state: counter key code, its accumulated step phase, the
        // integrated key voltage (relative Hz, 1 at C4) and the sounding key.
        int glideCode = 60;
        double glideSteps = 0, glideVoltage = 1, keyPitch = 60;
        bool glideReady = false;
        bool held = false, pedalHeld = false;
        double velocity = 1, velocityValue = 1, drift = 0, driftTarget = 0;
        std::uint32_t pressTicket = 0;
        unsigned driftCountdown = 0;
        std::uint64_t age = 0;
        // Host sample (plus one) of the last assignment, and whether the board
        // was silent before it, so a same-sample assignment can be replaced.
        std::uint64_t assignedAt = 0;
        bool assignedSilent = false;
        Oscillator osc;
        FilterChain filters;
        // Physical board capacitor histories persist across allocation and Idle.
        OutputCoupling filteredCoupling, commonCoupling;
        Envelope amp, filter;
        BoardTolerance board;
        // The board's pitch residual excludes free-running drift/vibrato.
        // Reuse it only while the exact key/bend/PITCH expression is equal;
        // patch changes and board redraws invalidate it in updateBoardScales.
        double boardPitchExponent=0, boardPitchCents=0;
        bool boardPitchReady=false;
#if !defined(CERES_EMBEDDED_TARGET)
        double oscillatorFrequency=0, hpFrequency=0, lpFrequency=0;
#endif
    };
    double fs, internalFs, rateScale=1/double(Synth::oversampling);
    std::uint64_t seed, age = 0, hostSamples = 0;
    Random random;
    Patch patch;
    std::array<Voice,Synth::maxVoiceCount> voices;
    unsigned voiceLimit=Synth::voiceCount, rateFactor=Synth::oversampling;
    OutputRate decimator;
    double lfoPhase = 0, pwmPhase = 0, pressureTarget = 0, pressureValue = 0, bend = 0;
    OutputCoupling busCoupling, lineCoupling;
    // outputCouplingTau at the internal rate: CP3, CP4, bus, line.
    std::array<OutputCoupling::Coefficients,4> couplingCoefficients{};
    double pressureCoefficient = 0, velocityCoefficient = 0, driftCoefficient = 0;
    std::array<double,4> ampCoefficients{}, filterCoefficients{};
    double hpSourceBranches = 3, lastSub = 0;
    OtaNetwork hpFilterNetwork, lpFilterNetwork;
    // Eight equal pin7 networks reduce exactly to one common delta state
    // for stiff sources, equal pin inputs and equal initial delta.
    // Existing per-stage effective Q anchors are NOT raw chip voltages.
    // Two independently smoothed anchors plus the shared delta decompose
    // two stage capacitor coordinates, not three physical capacitors.
    double qVoltageOffset=0, qControlCoefficient=0, stiCoefficient=0;
    double hpResonanceAnchor=0, lpResonanceAnchor=0;
    bool hasProcessed=false;
    double effectiveHpResonance=0, effectiveLpResonance=0, commonVcaGain=1;
    double outputTrimGain=1;
    ExpressionNetwork expression;
    double limiterInput=0, limiterOutput=0, limiterPole=0, limiterInnovation=0;
    double limiterRate=0, limiterStep=0;
    unsigned limiterSteps=1;
    // Substeps after the first draw from their own stream, so the shared
    // stream keeps one draw per sample and drift is unchanged.
    Random limiterRandom{1};
    OutputCoupling pickupCoupling;
    OutputCoupling::Coefficients pickupCoefficients;
    // Guide printed32: SUB-N is the limiter output N through22k/.1uF,
    // independent of audio NO level: the documented single low-pass (called
    // pink in the prose), not an invented ideal1/f filter. SM2 p.8 (PN3):
    // the FUNCTION switch puts N on SUI, a buffer with100k to ground, so the
    // RC sees22k||100k (pole88.26Hz) and a100/122 divider.
    double subNoiseMemory = 0, subNoiseG = 0;
    // SUB-EXT: the host input (held between calls), the previous host value
    // and the interpolated value for this internal step; the coupling
    // capacitor between the EXT preamp and EXO/SUI.
    double externalTarget = 0, externalPrevious = 0, externalNow = 0;
    OutputCoupling externalCoupling;
    OutputCoupling::Coefficients externalSelectedCoefficients, externalOpenCoefficients;
    Envelope ringEnvelope;
    std::array<double,4> ringCoefficients{};
    double ringPhase = 0, ringCarrier = 0, ringFrequency = 30;
    double ringDepthValue = 0, ringDepthIncrement = 0;
    unsigned ringDepthRemaining = 0;
    double ringCoupledCarrier=0;
    double ringSwitchGain=ringSteering(ringDifferentialDc);
    double ringSignalInput=0, ringSignalOutput=0;
    RingCarrierCoupling ringCarrierCoupling;
    OutputCoupling ringSignalCoupling, ringWetCoupling;
    OutputCoupling::Coefficients ringSignalCoefficients, ringWetCoefficients;
    double ringWetGain=ringModulatorGain*ringWetBufferGain();
    int lastAssignedKey = 60;
    bool pedal = false;
    // Physical-key ledger is independent of four-board allocation/stealing
    // and MIDI damper. Per-pitch FIFO tickets retain stolen-press ownership
    // without an unbounded event queue. Outstanding count is at most UINT32_MAX,
    // so modulo-2^32 tickets are unique among outstanding presses. Matching uses
    // equality only, never a signed comparison/subtraction across wraparound.
    std::array<std::uint32_t,128> physicalKeys{};
    std::array<std::uint32_t,128> firstPress{}, nextPress{};
    std::size_t physicalKeyPresses = 0;
    explicit Impl(double sampleRate, std::uint64_t seedValue)
        : fs(sampleRate), internalFs(sampleRate*4), seed(seedValue), random(seedValue) {
        if (!std::isfinite(fs) || fs < 8000 || fs > 384000) {
#if defined(CERES_EMBEDDED_TARGET)
            // Hosts must provide a supported rate. Keep the target path free
            // of exceptions; malformed construction falls back to 48 kHz.
            fs=48000; internalFs=fs*4;
#else
            throw std::invalid_argument("Sample rate must be finite and within 8000..384000 Hz");
#endif
        }
        configureRate();
        reset();
    }
    void configureRate() {
        internalFs=fs*rateFactor; rateScale=1/double(rateFactor);
        limiterSteps=static_cast<unsigned>(std::ceil(limiterSubstepRate/internalFs));
        limiterRate=internalFs*limiterSteps;
        limiterPole=std::exp(-1/(limiterRate*limiterTau));
        limiterInnovation=std::sqrt(1-limiterPole*limiterPole);
        limiterStep=limiterSlew/limiterRate;
        // SM2 p.8 touch pre-amplifier: VR3's wiper reaches IC1-2 through
        // 100k with .1uF to the +15V rail, a 10ms RC. Velocity has no
        // hardware path and keeps its own 5ms digital smoothing.
        pressureCoefficient = 1-std::exp(-1/(internalFs*.01));
        velocityCoefficient = 1-std::exp(-1/(internalFs*.005));
        driftCoefficient = 1-std::exp(-1/(internalFs*.3));
        qControlCoefficient=-std::expm1(-1/(internalFs*qControlTau));
        stiCoefficient=-std::expm1(-1/(internalFs*wscSineInputTau));
        subNoiseG=1/(2*internalFs*subNoiseResistance*.1e-6);
        for (unsigned i=0;i<4;++i)
            couplingCoefficients[i]=OutputCoupling::Coefficients(outputCouplingTau[i],internalFs);
        pickupCoefficients=OutputCoupling::Coefficients(pickupCouplingTau,limiterRate);
        externalSelectedCoefficients=OutputCoupling::Coefficients(externalCouplingTauSelected,internalFs);
        externalOpenCoefficients=OutputCoupling::Coefficients(externalCouplingTauOpen,internalFs);
        ringSignalCoefficients=OutputCoupling::Coefficients(ringSignalCouplingTau,internalFs);
        ringWetCoefficients=OutputCoupling::Coefficients(ringWetCouplingTau,internalFs);
        ringCarrierCoupling.configure(internalFs);
        updateCoefficients();
    }
    void updateCoefficients() {
        outputTrimGain=std::pow(10,patch.outputTrimDb/20);
        if (patch.expression<1) expression.configure(patch.expression,internalFs);
        // M-board mixer: fixed 150k ground, 150k noise input, and one 150k
        // branch per enabled waveform gate. Ideal low-Z WSC/noise outputs,
        // negligible enabled-FET resistance; disabled gates are open.
        hpSourceBranches=2+(patch.saw>0 ? 1:0)+(patch.pulse>0 ? 1:0);
        hpFilterNetwork.configure(internalFs,hpSourceBranches/150e3,3/150e3);
        lpFilterNetwork.configure(internalFs);
        ampCoefficients = envelopeCoefficients(internalFs,patch.ampAttack,
            patch.ampDecay,effectiveRelease(patch.ampRelease,patch.globalRelease),
            patch.ampAttackHeadroom,patch.ampAttackSaturation);
        filterCoefficients = envelopeCoefficients(internalFs,patch.filterAttack,
            patch.filterDecay,effectiveRelease(patch.filterRelease,patch.globalRelease),
            patch.filterAttackHeadroom,patch.filterAttackSaturation);
        // Direct CS-50 PRA 2DT divider: +15V through 6.8k, 3.3k to ground.
        // Its unloaded 4.901V on the PRA-aligned DECAY TIME law gives .459s
        // T60 (the CS-80 procedure's 5V trim point, fixed-100k slope
        // assumption; see ceres.hpp). This fixed key-off return is separate
        // from the M-board global SUS.
        ringCoefficients=envelopeCoefficients(internalFs,patch.ringAttack,
            patch.ringDecay,ringDecaySeconds(ringReleaseVolts),.01,0);
        updateBoardScales();
    }
    void updateBoardScales() {
        const double envelope=patch.boardEnvelopeTolerance, time=patch.boardEnvelopeTimeTolerance;
        const double vca=patch.boardVcaTolerance;
        const double filterRelease=effectiveRelease(patch.filterRelease,patch.globalRelease);
        const double ampRelease=effectiveRelease(patch.ampRelease,patch.globalRelease);
        // The attack time sharing a T60's RC (see BoardTolerance).
        const auto attack=[](double t60) { return boardAttackTimeConstants*t60/std::log(1000.0); };
        // The gm at each lever's centre, where the VCF trims match the boards.
        const double hpAligned=hpfLeverHz(.5)-filterFrequencyFloor;
        const double lpAligned=lpfLeverHz(.5)-filterFrequencyFloor;
        for (auto& voice : voices) {
            voice.boardPitchReady=false;
            auto& board=voice.board;
            board.hpScale=std::exp2(patch.boardFilterTolerance*board.hpOctaves);
            board.lpScale=std::exp2(patch.boardFilterTolerance*board.lpOctaves);
            board.hpCutoff=boardCutoff(patch.hpCutoff,hpAligned,board.hpScale);
            board.lpCutoff=boardCutoff(patch.lpCutoff,lpAligned,board.lpScale);
            const double level=1+envelope*board.filterEnvelopeError;
            board.hpEnvelopeGain=level*board.hpScale;
            board.lpEnvelopeGain=level*board.lpScale;
            board.ampPeak=1+envelope*board.ampPeakError;
            board.ampFloor=ampEnvelopeFloor+envelope*board.ampFloorError;
            // VR14 is trimmed at full sustain (CS-60 VI.4): there each board
            // gives the nominal output, whatever its own floor.
            board.ampEnvelopeGain=(1+ampEnvelopeFloor)/(1+board.ampFloor);
            board.sineGain=(1+vca*board.sineError)/(1+vca*board.filteredError);
            board.filterCoefficients=filterCoefficients;
            board.ampCoefficients=ampCoefficients;
            if (time>0) {
                const auto& f=board.filterTimeLog;
                board.filterCoefficients=envelopeCoefficients(internalFs,
                    patch.filterAttack*BoardTolerance::timeFactor(f,patch.filterAttack,time),
                    patch.filterDecay*BoardTolerance::timeFactor(f,attack(patch.filterDecay),time),
                    filterRelease*BoardTolerance::timeFactor(f,attack(filterRelease),time),
                    patch.filterAttackHeadroom,patch.filterAttackSaturation);
                const auto& a=board.ampTimeLog;
                board.ampCoefficients=envelopeCoefficients(internalFs,
                    patch.ampAttack*BoardTolerance::timeFactor(a,patch.ampAttack,time),
                    patch.ampDecay*BoardTolerance::timeFactor(a,attack(patch.ampDecay),time),
                    ampRelease*BoardTolerance::timeFactor(a,attack(ampRelease),time),
                    patch.ampAttackHeadroom,patch.ampAttackSaturation);
            }
        }
        // VR6, trimmed with the four physical boards in place (see
        // BoardTolerance): one common gain, exactly 1 when the VCA amount is 0.
        if (vca>0) {
            double power=0;
            for (unsigned i=0;i<Synth::voiceCount;++i) power+=voices[i].board.sineGain*voices[i].board.sineGain;
            const double vr6=std::sqrt(Synth::voiceCount/power);
            for (auto& voice : voices) voice.board.ampEnvelopeGain*=vr6;
        }
    }
    void followFreeKeys() {
        if (patch.sustainMode==2)
            for (unsigned i=0;i<voiceLimit;++i)
                if (!voices[i].held && !voices[i].pedalHeld) voices[i].keyNote=lastAssignedKey;
    }
    bool keyOn(int note,std::uint32_t& ticket) {
        const bool wasEmpty=physicalKeyPresses==0;
        const auto pitch=static_cast<std::size_t>(note);
        auto& count=physicalKeys[pitch];
        // An unledgered key must never allocate a voice or trigger the ring EG.
        if (count==std::numeric_limits<std::uint32_t>::max() ||
            physicalKeyPresses==std::numeric_limits<std::size_t>::max()) return false;
        ticket=nextPress[pitch]++;
        ++count; ++physicalKeyPresses;
        if (wasEmpty) ringEnvelope.on();
        return true;
    }
    bool keyOff(int note,std::uint32_t& ticket) {
        const auto pitch=static_cast<std::size_t>(note);
        auto& count=physicalKeys[pitch];
        if (!count) return false;
        ticket=firstPress[pitch]++;
        --count; --physicalKeyPresses;
        if (!physicalKeyPresses) ringEnvelope.off();
        return true;
    }
    void keysOff() {
        physicalKeys.fill(0); firstPress.fill(0); nextPress.fill(0);
        physicalKeyPresses=0; ringEnvelope.off();
    }
    void reset() {
        random = Random(seed);
        voices = {}; hostSamples = 0;
        age = 0; lfoPhase = 0; pwmPhase = 0; lastSub=0;
        pressureTarget = pressureValue = bend = 0;
        pedal = false; busCoupling={}; lineCoupling={};
        physicalKeys.fill(0); firstPress.fill(0); nextPress.fill(0);
        physicalKeyPresses=0; lastAssignedKey=60;
        subNoiseMemory=limiterInput=limiterOutput=0; pickupCoupling={};
        externalTarget=externalPrevious=externalNow=0; externalCoupling={};
        limiterRandom=Random(seed^0xd1b54a32d192ed03ULL); ringEnvelope={}; ringPhase=ringCarrier=0;
        ringFrequency=patch.ringRate; ringWetCoupling={}; ringSignalCoupling={};
        ringDepthValue=patch.ringDepth; ringDepthIncrement=0; ringDepthRemaining=0;
        ringCarrierCoupling.clear();ringCoupledCarrier=0;
        ringSwitchGain=ringSteering(ringDifferentialDc);
        ringSignalInput=ringSignalOutput=0;
        qVoltageOffset=0;
        hpResonanceAnchor=patch.hpResonance; lpResonanceAnchor=patch.lpResonance;
        hasProcessed=false;
        effectiveHpResonance=patch.hpResonance; effectiveLpResonance=patch.lpResonance;
        commonVcaGain=patch.ampLevel;
        decimator.clear(); expression.clear();
        // Preserve the original four-board random initialization and shared
        // audio/drift stream exactly. Extra virtual boards never consume that
        // stream during setup; they use a separate deterministic seed stream.
        Random extra(seed^0x9e3779b97f4a7c15ULL);
        for (unsigned i=0;i<voices.size();++i) {
            auto& voice=voices[i];
            auto& initialRandom=i<Synth::voiceCount ? random : extra;
            voice.osc.phase = initialRandom.unit();
            voice.filter.voltage = patch.filterInitial;
            voice.amp.voltage = ampEnvelopeFloor;
            voice.driftTarget = initialRandom.bipolar();
            voice.driftCountdown = static_cast<unsigned>(internalFs * (.1 + initialRandom.unit()*.5));
        }
        // Board tolerances come from their own seeded streams, drawn for every
        // physical slot in a fixed order, so the shared audio/drift stream is
        // untouched and board i is the same unit at every polyphony/quality.
        Random boards(seed^0x5851f42d4c957f2dULL), moreBoards(seed^0x2545f4914f6cdd1dULL);
        for (auto& voice : voices) voice.board.draw(boards,moreBoards);
        updateBoardScales();
        // An idle board rests at its own VCA-EG floor (VR17).
        for (auto& voice : voices) voice.amp.voltage=voice.board.ampFloor;
    }
    // KAS key voltage path (guide printed pp.22-25): assigner code -> glissando
    // counter -> integrating gate -> sample-hold -> VCO and filter key CV.
    // Free channels carry the last-pressed key (p.23), so a newly assigned
    // board glides from the previous key. The sample-hold tracks while its key
    // is down or in Sustain II and holds the gliding value at key-off in I.
    double advanceGlide(Voice& v,double dt) {
        const int target=v.held || v.pedalHeld ? v.note : lastAssignedKey;
        if (patch.glideMode==0) {
            v.glideCode=target; v.glideSteps=0; v.glideReady=false;
            return v.keyPitch=v.keyNote;
        }
        if (!v.glideReady) {
            v.glideCode=static_cast<int>(std::lround(v.keyPitch));
            v.glideVoltage=std::exp2((v.keyPitch-60)/12);
            v.glideReady=true;
        }
        if (v.glideCode==target) v.glideSteps=0;
        else {
            v.glideSteps+=patch.glideRate*dt;
            while (v.glideSteps>=1 && v.glideCode!=target) {
                v.glideCode+=v.glideCode<target ? 1 : -1;
                v.glideSteps-=1;
            }
        }
        const double stepped=std::exp2((v.glideCode-60)/12.0);
        // Hz-linear key voltage: the integrator's RC glide is exponential in
        // Hz, so a wide glide covers its last semitones more slowly in pitch.
        // The gate the lever selects fixes the time constant; in the fast
        // regime the direct gate passes the counter's steps as they are, so
        // portamento and glissando meet at the short end of the lever.
        const double tau=patch.glideRate<glideMediumAbove ? glideTauLarge
            : patch.glideRate<glideDirectAbove ? glideTauMedium : 0;
        v.glideVoltage=patch.glideMode==2 || tau==0 ? stepped
            : v.glideVoltage+(stepped-v.glideVoltage)*-std::expm1(-dt/tau);
        if (v.held || v.pedalHeld || patch.sustainMode==2)
            v.keyPitch=60+12*std::log2(v.glideVoltage);
        return v.keyPitch;
    }
    double sample() {
        hasProcessed=true;
        const double dt = 1 / internalFs;
        pressureValue += (pressureTarget-pressureValue) * pressureCoefficient;
        // Guide printed p.32: the limiter amplifier drives the zener noise to
        // saturation (30Vpp), "white noise of constant amplitude", and VR10
        // trims it to 3Vpp, the WSC waveforms' 3Vpp. So the one shared source
        // is the slew-limited sign of the band-limited pick-up signal, in
        // units of the rails (see limiterSlew).
        // With NOISE at zero and FUNCTION off N nothing audible depends on
        // the source, so it is not computed: the limiter, its pick-up
        // coupling and the SUB-N filter hold their state, and both streams
        // skip the draws they would have taken (Random::skip), so the drift
        // that shares the first stream keeps its chronology exactly. Audio
        // is unchanged while the source stays unused or stays in use; when
        // it comes back into use the held states resume, and the pick-up
        // coupling's settling (about 25 ms) is a different realisation of
        // the same noise than an always-running limiter would have given.
        // At 1x the source alone was a quarter of a voice's cost.
        const bool noiseUsed=patch.noise!=0 || patch.lfoWaveform==4;
        double noise=0, subNoise=0;
        if (noiseUsed) {
            double limiterSum=0;
            for (unsigned step=0; step<limiterSteps; ++step) {
                const double draw=step ? limiterRandom.unit() : random.unit();
                limiterInput=limiterPole*limiterInput+limiterInnovation*inverseNormal(draw+0x1.0p-54);
                const double pickup=pickupCoupling.process(limiterInput,pickupCoefficients);
                limiterOutput=pickup<0 ? std::max(-1.0,limiterOutput-limiterStep)
                    : std::min(1.0,limiterOutput+limiterStep);
                limiterSum+=limiterOutput;
            }
            noise=limiterSum/limiterSteps;
            subNoise=(subNoiseMemory+subNoiseG*noise)/(1+subNoiseG);
            subNoiseMemory=2*subNoise-subNoiseMemory;
        } else {
            random.skip(1);
            limiterRandom.skip(limiterSteps-1);
        }
        // SUB-EXT (see externalPreampRail): LEVEL, the preamp's swing, the
        // switch-loaded coupling capacitor, then the IC1-1 follower's swing.
        // The preamp and capacitor run whatever FUNCTION selects.
        const bool external=patch.lfoWaveform==5;
        const double externalSub=clamp(externalCoupling.process(
            clamp(patch.extLevel*externalNow,-externalPreampRail,externalPreampRail),
            external ? externalSelectedCoefficients : externalOpenCoefficients),
            -externalBufferRail,externalBufferRail);
        const double lfo = external ? externalSub : subWaveform(lfoPhase,patch.lfoRate*dt,patch.lfoWaveform,
            patch.lfoWaveform==4 ? subNoise*subNoiseRail:0.);
        lastSub=lfo;
        // Exact zero-order-held RC update. Never bypass while offset returns
        // to zero: capacitor charge survives note allocation, patch edits,
        // and audio panic. reset() is the explicit electrical-state reset.
        qVoltageOffset+=(patch.resonanceOffset*.5-qVoltageOffset)*qControlCoefficient;
        hpResonanceAnchor+=(patch.hpResonance-hpResonanceAnchor)*qControlCoefficient;
        lpResonanceAnchor+=(patch.lpResonance-lpResonanceAnchor)*qControlCoefficient;
        // Printed endpoints Vq0:Q5,Vq10:Q.5, Vq being the pin current times
        // 110k+Rin. deltaVq moves Vq itself; OtaSvf applies the exponential
        // law to the sum.
        effectiveHpResonance=clamp(hpResonanceAnchor-qVoltageOffset*.1,0,1);
        effectiveLpResonance=clamp(lpResonanceAnchor-qVoltageOffset*.1,0,1);
        // All boards see the same smoothed Q buses. Keep the original
        // exponential and multiplication order, evaluated once per bus.
        hpFilterNetwork.setResonance(effectiveHpResonance);
        lpFilterNetwork.setResonance(effectiveLpResonance);
        // R7 LC sums LEVEL(270k) and SUB(120k) currents before common VCA2.
        // PN3 VCF/VCA depth pots share SUO and ground; their passive R7
        // paths do not invert. Positive LI raises VCA gain (Guide pp.40–41),
        // so positive SUB must raise both cutoff and common gain. SUB swings
        // LC around LEVEL; the summed current, touch included, cannot drive
        // the VCA below zero gain.
        const double levelControl=patch.ampLevel
            +tremoloScale*leverFraction(patch.tremolo,tremoloLeverRatio)*lfo;
        commonVcaGain=std::max(0.0,levelControl);
        const double pwm = sinTurn(pwmPhase+patch.pwmPhase);
        lfoPhase = wrapUnit(lfoPhase + patch.lfoRate*dt);
        pwmPhase = wrapUnit(pwmPhase + patch.pwmRate*dt);
        double mixed = 0;
        for (unsigned i = 0; i < voiceLimit; ++i) {
            auto& v = voices[i];
            // Velocity is stored per physical board on note assignment. Do
            // not replace it when a released board follows another key CV.
            // Channel pressure keeps smoothing in either mode, so switching
            // back retains its ordinary controller history.
            // Modern MIDI velocity is not a physical CS-50 control. Reusing an
            // audible board must not step its VCA gain or expression CV while
            // its capacitor/oscillator histories remain connected. Use the
            // existing 5ms controller RC clock; silent assignments precharge in
            // noteOn. This digital adaptation does not slew the keyboard CV.
            v.velocityValue+=(v.velocity-v.velocityValue)*velocityCoefficient;
            const double expression=patch.expressionSource==1 ? v.velocityValue : pressureValue;
            // Free-running wander, driftCents peak (see ceres.hpp): its 0.2-0.7s
            // hold and .3s smoothing are engineering choices with no source.
            if (v.driftCountdown-- == 0) {
                v.driftTarget = random.bipolar();
                v.driftCountdown = static_cast<unsigned>(internalFs * (.2 + random.unit()*.5));
            }
            v.drift += (v.driftTarget-v.drift) * driftCoefficient;
            // The fixed spread steps from board to board, centred on the
            // voices in use (i-1.5 at the default four).
            double cents = patch.tuneCents + (static_cast<double>(i)-.5*(voiceLimit-1))*patch.voiceSpreadCents
                + v.drift*patch.driftCents + lfo*(patch.vibratoCents + expression*patch.pressureVibrato)
                + expression*patch.pressureVibrato*touchVibratoBias;
            const double key=advanceGlide(v,dt);
            // Trim residual and thermal drift of this board's linear VCO (see
            // BoardTolerance): its key voltage is the NORMAL frequency of key,
            // bend and PITCH.
            if (patch.boardPitchTolerance>0 || patch.boardTemperatureRise!=0) {
                const double exponent=(key-69+bend+patch.tuneCents*.01)/12;
                if (!v.boardPitchReady || exponent!=v.boardPitchExponent) {
                    v.boardPitchCents=v.board.pitchCents(patch,440*std::exp2(exponent));
                    v.boardPitchExponent=exponent;
                    v.boardPitchReady=true;
                }
                cents+=v.boardPitchCents;
            }
            const double frequency = 440 * std::exp2((key-69 + bend + 12*patch.octave + cents*.01)/12);
            const auto waves = v.osc.process(frequency, patch.pulseWidth+pwm*patch.pwmDepth,
                                            patch.resetMicroseconds*1e-6,internalFs,stiCoefficient,
                                            patch.sine!=0,patch.saw!=0,patch.pulse!=0);
            // Each board's EG times, VCA-EG peak and floor (see BoardTolerance;
            // the shared values when the envelope tolerance is off).
            const auto& egf=v.board.filterCoefficients;
            const auto& ega=v.board.ampCoefficients;
            const double filterEnvelope = v.filter.advance(patch.filterInitial,patch.filterPeak,0,
                patch.filterAttackHeadroom,patch.filterAttackSaturation,egf[0],egf[3],egf[1],egf[2]);
            const double ampEnvelope = std::max(0.0,v.amp.advance(v.board.ampFloor,v.board.ampPeak,
                patch.ampSustain+v.board.ampFloor,
                patch.ampAttackHeadroom,patch.ampAttackSaturation,ega[0],ega[3],ega[1],ega[2]));
            // Guide printed p.34: fc=125*Kv*2^Vfc. Kv is LINEAR in key Hz: the
            // M board feeds IG00156 pin 2 from the same K terminal as VCO III
            // pin 2 (SM2 p.16), 0.25V at 130.8Hz to 4V at 2093Hz (Yamaha's
            // C2..C6; SM1 printed p.19, both pin sheets), so fc follows the
            // key exactly one octave per octave, through glide and PITCH but
            // not TRANSPOSITION (VCO III pin 1 FT only). keyTrack=1 is that hardware law. At Vfc=0 the printed law
            // puts gm cutoff at 125*.5V = 62.5Hz at middle C; factory presets
            // convert their lever volts with it (preset()), and the patch's
            // hp/lpCutoff stay the cutoffs at middle C.
            // This law controls the OTA gm contribution. The independent
            // feed resistor contributes the same conductance at every CV.
            // Master PITCH routes PN2 VR1 -> SUB TU -> KAS67 -> SH -> M K37,
            // changing the common Kv supplied to VCO and both filters (CS-50
            // service sheets; Guide printed p.62/PDF71). Its nominal span is
            // -5..+7 semitones. It remains a shared master offset even when
            // the engineering-only keyTrack slope is changed. TRANSPOSE FT,
            // VIB15 and the modeled oscillator drift/spread remain VCO-only.
            const double keyOctaves=(key-60+bend)*patch.keyTrack/12+patch.tuneCents/1200;
            // R7 TBR and VCF (touch SUB) enter each cutoff node through 47k LP
            // and 150k HP, like SUB: the HP coupling is hpSubRelativeScale.
            const double touchOctaves=expression*(patch.pressureFilter+lfo*patch.pressureWah);
            // Board tolerance (see BoardTolerance): each Vfc input's octaves
            // scale with this board's rank residual, the static cutoff's from
            // the lever centre; the EG also with its output-level residual.
            // Every factor is exactly 1, and each cutoff the patch's, when off.
            const auto& board=v.board;
            const double hpFactor=std::exp2(keyOctaves+filterEnvelope*board.hpEnvelopeGain*patch.hpEnvelopeScale
                + touchOctaves*board.hpScale*hpSubRelativeScale+lfo*board.hpScale*(patch.filterLfo*hpSubRelativeScale)
                + patch.brilliance*board.hpScale*hpBrillianceScale);
            const double lpFactor=patch.lpEnvelopeScale==patch.hpEnvelopeScale &&
                patch.filterLfo==0 && patch.brilliance==0 && touchOctaves==0 &&
                board.hpEnvelopeGain==board.lpEnvelopeGain ? hpFactor
                : std::exp2(keyOctaves+filterEnvelope*board.lpEnvelopeGain*patch.lpEnvelopeScale
                    + touchOctaves*board.lpScale+lfo*board.lpScale*patch.filterLfo
                    + patch.brilliance*board.lpScale*lpBrillianceScale);
            const double source = patch.saw*waves.saw + patch.pulse*waves.pulse + patch.noise*noise;
            const double hpFrequency=modulatedFilterFrequency(board.hpCutoff,hpFactor);
            const double lpFrequency=modulatedFilterFrequency(board.lpCutoff,lpFactor);
#if !defined(CERES_EMBEDDED_TARGET)
            v.oscillatorFrequency=frequency; v.hpFrequency=hpFrequency; v.lpFrequency=lpFrequency;
#endif
            const double low=v.filters.process(source,hpSourceBranches,hpFrequency,lpFrequency,
                patch.drive,hpFilterNetwork,lpFilterNetwork);
            // CP3 coupling precedes filtered level; CP4 coupling acts on
            // the filtered/sine sum before common level and envelope VCAs.
            const double filtered=v.filteredCoupling.process(low,couplingCoefficients[0]);
            // The sine's VR11/VR12 balance against the filtered path (VCA
            // amount); exactly 1 when off.
            const double summed=filtered*patch.filterLevel + waves.sine*patch.sine*board.sineGain;
            const double beforeVca=v.commonCoupling.process(summed,couplingCoefficients[1]);
            // IG00151 gain is LINEAR in envelope CV (guide pp.40–41). The
            // optional velocity term is zero in factory-derived presets.
            // Each chip's EI trim sets zero output at 0V LI (guide p.41), so
            // gain closes at zero. IC6's VR21 (B-100k across +/-15V, 10M into
            // +I; SM2 p.16) is trimmed with LEVEL at 0 and a key down for the
            // minimum level at O (CS-60 manual printed p.39 / PDF 41, VI.5),
            // nulling what the envelope would gate through: control
            // feedthrough is zero here. What remains at that minimum, and
            // IC5's own tremolo feedthrough, is unpublished. The IG00151's transfer above 10V LI (touch adds
            // up to 1.08 of full LEVEL, tremolo .3375) is unpublished too, so
            // it stays linear. The LEVEL levers' 100k/.01uF at LI (1ms with a
            // high-impedance pin) is not modelled: LI's impedance is unknown.
            const double velocityGain = 1-patch.velocitySensitivity*(1-v.velocityValue);
            const double touchLevel=patch.pressureVolume*expression*touchLevelScale;
            mixed += beforeVca * ampEnvelope * board.ampEnvelopeGain * velocityGain * std::max(0.0,levelControl+touchLevel);
        }
        // The PRA output preamp (outputPreamp) is linear up to its derived
        // op-amp rails; no hidden peak normalization or dynamics processor.
        // Four equal4.7uF/1k branches into47k define this calibrated bus.
        // Extra voices are a virtual extension: retain the same bus/gain,
        // rather than inventing a new physical load or dividing by voice count.
        // More simultaneous voices can therefore increase rail saturation.
        // Common-bus transfer,
        // with previous overall midband gain retained as a calibration choice.
        const double bus=busCoupling.process(mixed,couplingCoefficients[2]);
        // One global processor, after the voice bus and before expression/
        // output. The carrier uses the physical matched upper-BJT steering
        // law at the pins 7-8 carrier, and the degenerated signal pair its
        // large-signal law at the aligned bus level (ringSignalPair). Wet
        // gain is the derived modulator gain times the wet buffer's VR4
        // trim (see above).
        const double ringControl=ringEnvelope.advance(0,1,0,.01,0,
            ringCoefficients[0],0,ringCoefficients[1],ringCoefficients[2]);
        // Digital control dezippering only: retain the physical EG trajectory,
        // free carrier phase and frequency cap. Static configured patches use
        // the exact target, including at startup; no invented analog RC.
        if (ringDepthRemaining) {
            if (--ringDepthRemaining==0) ringDepthValue=patch.ringDepth;
            else ringDepthValue+=ringDepthIncrement;
        }
        ringFrequency=clamp(patch.ringRate*std::exp2(ringDepthValue*ringControl),.2,220);
        ringCarrier=std::sin(2*pi*ringPhase);
        ringPhase=wrapUnit(ringPhase+ringFrequency*dt);
        // Keep this free-running passive network connected even at mix0.
        ringCoupledCarrier=ringCarrierCoupling.process(ringCarrier);
        // No mean removal or gain normalization: the typical-part carrier-
        // port offset passes dry signal (about 5dB under each sideband) and
        // gives even carrier products. 25C, matched transistors, ideal source.
        ringSwitchGain=ringSteering(ringDifferentialDc+ringSourcePeakVolts*ringCoupledCarrier);
        // Pin 1 sees the bus through 10uF into 330+463 ohm (20Hz); pin 6 and
        // the wet buffer are coupled through 10uF into 2.5k+10k (1.27Hz).
        // Both run even at mix 0, like the carrier network.
        ringSignalInput=ringSignalCoupling.process(bus,ringSignalCoefficients);
        ringSignalOutput=ringSignalPair(ringSignalInput);
        const double wet=ringWetGain*ringWetCoupling.process(ringSignalOutput*ringSwitchGain,ringWetCoefficients);
        // Explicit bypass avoids even a rounding change for old patches.
        const double ringed=patch.ringMix==0 ? bus
            : (bus+patch.ringMix*(wet-bus))/(1+patch.ringMix*(1-patch.ringMix)*ringMixLeverRatio);
        // MI to MO. A fully open pedal is the fully conducting cell, which is
        // the path this engine was calibrated on, so it stays a bare wire.
        const double expressed=patch.expression>=1 ? ringed : expression.process(ringed);
        const double rail=outputPreamp(expressed,patch.volume);
        // HIGH output, 33uF into its LO divider and the 10k-loaded HO, after
        // the documented inverting preamp. No recording EQ.
        return -lineCoupling.process(rail,couplingCoefficients[3]);
    }
};

Synth::Synth(double fs, std::uint64_t seed) : impl_(std::make_unique<Impl>(fs,seed)) {}
Synth::~Synth() = default;
Synth::Synth(Synth&&) noexcept = default;
Synth& Synth::operator=(Synth&&) noexcept = default;
void Synth::setPatch(const Patch& patch) {
    const Patch validated = validatePatch(patch);
    const bool openingKeyHold=impl_->patch.sustainMode!=2 && validated.sustainMode==2;
    const bool changingRingDepth=impl_->patch.ringDepth!=validated.ringDepth;
    impl_->patch = validated;
    impl_->updateCoefficients();
    if (openingKeyHold) impl_->followFreeKeys();
    // A newly configured, unclocked instance starts at its chosen settled
    // anchors/IL. Once even silence has been processed, preset recall only
    // changes targets; it cannot erase continuously connected charge.
    if (!impl_->hasProcessed) {
        impl_->ringDepthValue=validated.ringDepth;
        impl_->ringDepthIncrement=0; impl_->ringDepthRemaining=0;
        impl_->hpResonanceAnchor=validated.hpResonance;
        impl_->lpResonanceAnchor=validated.lpResonance;
        impl_->effectiveHpResonance=validated.hpResonance;
        impl_->effectiveLpResonance=validated.lpResonance;
        for (auto& voice : impl_->voices) {
            if (voice.filter.stage == Envelope::Idle) voice.filter.voltage = validated.filterInitial;
            // ... and an idle VCA EG at its board's floor (VR17).
            if (voice.amp.stage == Envelope::Idle) voice.amp.voltage = voice.board.ampFloor;
        }
    } else if (changingRingDepth) {
        // Retarget from the actual interpolated value. Repeated patch writes
        // with the same target do not prolong the finite20ms transition.
        impl_->ringDepthRemaining=std::max(1u,static_cast<unsigned>(std::ceil(impl_->internalFs*.020)));
        impl_->ringDepthIncrement=(validated.ringDepth-impl_->ringDepthValue)/impl_->ringDepthRemaining;
    }
}
void Synth::noteOn(int note, double velocity) {
    if (note < 0 || note > 127 || !std::isfinite(velocity)) return;
    if (velocity <= 0) { noteOff(note); return; }
    auto& state = *impl_;
    std::uint32_t ticket=0;
    if (!state.keyOn(note,ticket)) return;
    Impl::Voice* selected = nullptr;
    // KAS gate control (guide printed pp.17-18) writes a new key only into a
    // channel whose code differs and whose key is up; a released channel that
    // still holds this key is re-keyed. Reuse that board first. Otherwise use
    // an idle voice, then the quietest released voice, then the oldest
    // pedal-held one (its key is up). A key pressed while every board's key is
    // down is not stored (guide printed p.19 timing chart): it sounds neither
    // now nor when a board frees, though its press still counts for the ring
    // trigger. Keys pressed together are encoded higher note first (guide
    // printed p.16), so a key pressed in the same sample as a lower stored key
    // takes that board instead: a simultaneous chord keeps its highest notes.
    // Overlapping presses of a still-held note occupy separate voices.
    for (auto it=state.voices.begin();it!=state.voices.begin()+state.voiceLimit;++it)
        if (it->note==note && !it->held && !it->pedalHeld) {
            selected=&*it; break;
        }
    if (!selected)
    for (auto it=state.voices.begin();it!=state.voices.begin()+state.voiceLimit;++it) {
        auto& voice=*it;
        if (voice.amp.stage == Envelope::Idle && (!selected || voice.age < selected->age)) selected = &voice;
    }
    if (!selected)
        for (auto it=state.voices.begin();it!=state.voices.begin()+state.voiceLimit;++it) {
            auto& voice=*it;
            if (!voice.held && !voice.pedalHeld && (!selected || voice.amp.voltage < selected->amp.voltage)) selected = &voice;
        }
    if (!selected)
        for (auto it=state.voices.begin();it!=state.voices.begin()+state.voiceLimit;++it)
            if (!it->held && (!selected || it->age < selected->age)) selected = &*it;
    bool replacing=false;
    if (!selected)
        for (auto it=state.voices.begin();it!=state.voices.begin()+state.voiceLimit;++it)
            if (it->assignedAt==state.hostSamples+1 && it->note<note && (!selected || it->note<selected->note)) {
                selected=&*it; replacing=true;
            }
    if (!selected) return;
    state.lastAssignedKey=note;
    // I/II changes the held keyboard voltage of released physical boards;
    // it does not re-gate their VCF/VCA or reset any oscillator/filter state.
    state.followFreeKeys();
    const bool silent=replacing ? selected->assignedSilent
        : selected->amp.stage==Envelope::Idle || selected->amp.voltage<=0;
    selected->assignedAt=state.hostSamples+1; selected->assignedSilent=silent;
    selected->note = selected->keyNote = note;
    selected->velocity = clamp(velocity,0,1);
    if (silent) selected->velocityValue=selected->velocity;
    selected->pressTicket=ticket;
    selected->held = true; selected->pedalHeld = false;
    selected->age = ++state.age;
    selected->amp.on(); selected->filter.on();
}
void Synth::noteOff(int note) {
    if (note<0 || note>127) return;
    std::uint32_t ticket=0;
    if (!impl_->keyOff(note,ticket)) return;
    // Consume the oldest physical press even when its voice was stolen. Its
    // eventual note-off cannot release a newer same-pitch replacement.
    Impl::Voice* selected = nullptr;
    for (auto it=impl_->voices.begin();it!=impl_->voices.begin()+impl_->voiceLimit;++it) {
        auto& voice=*it;
        if (voice.note == note && voice.held && voice.pressTicket==ticket) { selected=&voice; break; }
    }
    if (selected) {
        selected->held = false;
        selected->pedalHeld = impl_->pedal;
        if (!selected->pedalHeld) { selected->amp.off(); selected->filter.off(); }
        impl_->followFreeKeys();
    }
}
void Synth::pressure(double value) {
    if (std::isfinite(value)) impl_->pressureTarget = clamp(value,0,1);
}
void Synth::pitchBend(double semitones) {
    if (std::isfinite(semitones)) impl_->bend = clamp(semitones,-48,48);
}
void Synth::sustain(bool down) {
    impl_->pedal = down;
    if (!down)
        for (auto& voice : impl_->voices)
            if (voice.pedalHeld) {
                voice.pedalHeld = false;
                if (!voice.held) { voice.amp.off(); voice.filter.off(); }
            }
    impl_->followFreeKeys();
}
void Synth::allNotesOff() {
    impl_->keysOff();
    for (auto& voice : impl_->voices) {
        if (voice.held) voice.pedalHeld = impl_->pedal;
        voice.held = false;
        if (!voice.pedalHeld) { voice.amp.off(); voice.filter.off(); }
    }
    impl_->followFreeKeys();
}
void Synth::allSoundOff() {
    impl_->keysOff(); impl_->ringEnvelope={}; impl_->ringWetCoupling={}; impl_->ringSignalCoupling={};
    impl_->ringSignalInput=impl_->ringSignalOutput=0;
    impl_->ringCarrierCoupling.clear();impl_->ringCoupledCarrier=0;
    impl_->ringSwitchGain=ringSteering(ringDifferentialDc);
    for (auto& voice : impl_->voices) {
        voice.held = voice.pedalHeld = false;
        voice.amp = {};
        voice.amp.voltage = voice.board.ampFloor;
        voice.filter = {};
        voice.filter.voltage = impl_->patch.filterInitial;
        voice.filters = {}; voice.filteredCoupling={}; voice.commonCoupling={};
    }
    impl_->decimator.clear(); impl_->expression.clear();
    impl_->busCoupling={}; impl_->lineCoupling={};
}
void Synth::reset() { impl_->reset(); }
bool Synth::configure(unsigned voices,unsigned factor) noexcept {
    if(voices<1 || voices>maxVoiceCount || (factor!=1 && factor!=2 && factor!=4))return false;
    if(voices==impl_->voiceLimit && factor==impl_->rateFactor)return true;
    if(impl_->hasProcessed || impl_->physicalKeyPresses || activeVoices())return false;
    impl_->voiceLimit=voices;impl_->rateFactor=factor;
    impl_->configureRate();impl_->reset();
    return true;
}
unsigned Synth::polyphony() const noexcept {return impl_->voiceLimit;}
unsigned Synth::oversamplingFactor() const noexcept {return impl_->rateFactor;}
std::size_t Synth::activeVoices() const {
    return static_cast<std::size_t>(std::count_if(impl_->voices.begin(),impl_->voices.begin()+impl_->voiceLimit,
        [](const auto& v) { return v.amp.stage != Envelope::Idle; }));
}
double Synth::sampleRate() const { return impl_->fs; }
void Synth::externalInput(double sample) noexcept {
    // Numerical guard only: 1e9 units overdrives the preamp at any LEVEL
    // above 1e-8, and keeps the interpolation below overflow.
    impl_->externalTarget=inlineFinite(sample) ? clamp(sample,-1e9,1e9) : 0.;
}
float Synth::process() {
    ++impl_->hostSamples;
    // SUB-EXT arrives at the host rate: step linearly from the previous host
    // value, landing on the new one at the last internal step (exact at 1x).
    // A digital adaptation of the continuous jack, not a hardware stage.
    auto& state=*impl_;
    const double from=state.externalPrevious, to=state.externalTarget;
    for (unsigned i = 0; i < state.rateFactor; ++i) {
        state.externalNow=i+1==state.rateFactor ? to : from+(to-from)*(i+1)*state.rateScale;
        state.decimator.push(state.sample(),state.rateFactor);
    }
    state.externalPrevious=to;
    const double output=impl_->decimator.output(impl_->rateFactor);
    // Post-circuit/post-FIR utility gain: no change to analog state, timing,
    // nonlinear operating level or default sound. No output limiter is added.
    return static_cast<float>(impl_->patch.outputTrimDb==0 ? output : output*impl_->outputTrimGain);
}

#if !defined(CERES_EMBEDDED_TARGET)
std::array<Synth::VoiceState,Synth::voiceCount> Synth::voiceState() const {
    std::array<VoiceState,voiceCount> result{};
    for (unsigned i=0;i<voiceCount;++i) {
        const auto& v=impl_->voices[i];
        result[i]={v.note,v.keyNote,v.held,v.pedalHeld,v.amp.voltage,v.filter.voltage,v.osc.phase,
            v.oscillatorFrequency,v.hpFrequency,v.lpFrequency};
    }
    return result;
}
std::array<Synth::VoiceState,Synth::maxVoiceCount> Synth::allVoiceState() const {
    std::array<VoiceState,maxVoiceCount> result{};
    for(unsigned i=0;i<maxVoiceCount;++i) {
        const auto& v=impl_->voices[i];
        result[i]={v.note,v.keyNote,v.held,v.pedalHeld,v.amp.voltage,v.filter.voltage,v.osc.phase,
            v.oscillatorFrequency,v.hpFrequency,v.lpFrequency};
    }
    return result;
}
Synth::ModulationState Synth::modulationState() const {
    return {impl_->lastSub,impl_->ringEnvelope.voltage,impl_->ringFrequency,
        impl_->ringCarrier,impl_->physicalKeyPresses,impl_->ringCoupledCarrier,impl_->ringSwitchGain,impl_->ringDepthValue,
        impl_->ringSignalInput,impl_->ringSignalOutput};
}
Synth::SoundPathState Synth::soundPathState() const {
    return {impl_->qVoltageOffset,impl_->effectiveHpResonance,
        impl_->effectiveLpResonance,impl_->commonVcaGain,
        impl_->hpResonanceAnchor,impl_->lpResonanceAnchor,impl_->hasProcessed};
}
std::vector<double> outputRateImpulse(unsigned factor,std::size_t hostFrames,unsigned inputSubframe) {
    if((factor!=1 && factor!=2 && factor!=4) || inputSubframe>=factor)
        throw std::invalid_argument("Invalid output-rate factor/subframe");
    OutputRate output;
    std::vector<double> result(hostFrames);
    for(std::size_t frame=0;frame<hostFrames;++frame) {
        for(unsigned sub=0;sub<factor;++sub)output.push(frame==0 && sub==inputSubframe ? 1.:0.,factor);
        result[frame]=output.output(factor);
    }
    return result;
}
std::vector<double> ringCarrierResponse(double fs,const std::vector<double>& input) {
    if(!std::isfinite(fs)||fs<100||fs>1536000)
        throw std::invalid_argument("Invalid ring carrier probe sample rate");
    RingCarrierCoupling coupling;coupling.configure(fs);
    std::vector<double> result;result.reserve(input.size());
    for(double value:input) {
        if(!std::isfinite(value))throw std::invalid_argument("Nonfinite ring carrier probe input");
        result.push_back(coupling.process(value));
    }
    return result;
}
double ringSwitchingResponse(double differentialVolts) {
    if(!std::isfinite(differentialVolts))throw std::invalid_argument("Nonfinite carrier differential voltage");
    return ringSteering(differentialVolts);
}
double outputPreampResponse(double volts,double volume) {
    if(!std::isfinite(volts)||!std::isfinite(volume))throw std::invalid_argument("Nonfinite output preamp probe");
    return outputPreamp(volts,volume);
}
// Individual normalized section, using the same RC class/constants as voices.
std::vector<double> outputCouplingImpulse(unsigned stage,double fs,std::size_t frames) {
    if(stage>=4 || !std::isfinite(fs) || fs<100 || fs>1536000)
        throw std::invalid_argument("Invalid output coupling probe");
    OutputCoupling rc;std::vector<double> values(frames);
    for(std::size_t i=0;i<frames;++i)values[i]=rc.process(i==0?1.:0.,outputCouplingTau[stage],fs);
    return values;
}

std::vector<double> filterImpulse(double fs,double cutoff,double resonance,bool highPass,
                                 std::size_t frames,double amplitude,double sourceConductance,
                                 double referenceSourceConductance) {
    if (!std::isfinite(fs) || fs < 100 || !std::isfinite(cutoff) || cutoff <= 0 ||
        !std::isfinite(resonance) || !std::isfinite(amplitude) ||
        !std::isfinite(sourceConductance) || sourceConductance<=0 || sourceConductance>1 ||
        !std::isfinite(referenceSourceConductance) || referenceSourceConductance<=0 || referenceSourceConductance>1)
        throw std::invalid_argument("Invalid filter probe input");
    OtaSvf filter;
    std::vector<double> result(frames);
    for (std::size_t i = 0; i < frames; ++i)
        result[i] = filter.process(i == 0 ? amplitude : 0,cutoff,resonance,fs,highPass,
                                   sourceConductance,referenceSourceConductance);
    return result;
}

std::vector<double> filterCascadeImpulse(double fs,double hpCutoff,double lpCutoff,
    double hpResonance,double lpResonance,unsigned branches,std::size_t frames,double amplitude,double drive) {
    if (!std::isfinite(fs) || fs<100 || fs>1536000 ||
        !std::isfinite(hpCutoff) || hpCutoff<=0 || !std::isfinite(lpCutoff) || lpCutoff<=0 ||
        !std::isfinite(hpResonance) || !std::isfinite(lpResonance) ||
        !std::isfinite(amplitude) || !std::isfinite(drive) || drive<.05 || drive>8 ||
        branches<2 || branches>4)
        throw std::invalid_argument("Invalid filter cascade probe input");
    FilterChain chain;
    std::vector<double> result(frames);
    for (std::size_t i=0;i<frames;++i)
        result[i]=chain.process(i==0 ? amplitude : 0,branches,hpCutoff,lpCutoff,
                                hpResonance,lpResonance,drive,fs);
    return result;
}

std::vector<double> envelopeTrace(double fs,const Patch& supplied,bool filter,
    std::size_t frames,const std::vector<EnvelopeGate>& gates) {
    if (!std::isfinite(fs) || fs < 100 || fs > 1536000)
        throw std::invalid_argument("Invalid envelope probe sample rate");
    for (std::size_t i=0; i<gates.size(); ++i)
        if (gates[i].frame>=frames || (i && gates[i].frame<gates[i-1].frame))
            throw std::invalid_argument("Envelope gates must be ordered and inside the trace");
    const Patch patch=validatePatch(supplied);
    const double initial=filter ? patch.filterInitial : ampEnvelopeFloor;
    const double peak=filter ? patch.filterPeak : 1;
    const double sustained=filter ? 0 : patch.ampSustain+ampEnvelopeFloor;
    const double headroom=filter ? patch.filterAttackHeadroom : patch.ampAttackHeadroom;
    const double saturation=filter ? patch.filterAttackSaturation : patch.ampAttackSaturation;
    const auto coefficients=filter
        ? envelopeCoefficients(fs,patch.filterAttack,patch.filterDecay,
            effectiveRelease(patch.filterRelease,patch.globalRelease),headroom,saturation)
        : envelopeCoefficients(fs,patch.ampAttack,patch.ampDecay,
            effectiveRelease(patch.ampRelease,patch.globalRelease),headroom,saturation);
    Envelope envelope; envelope.voltage=initial;
    std::vector<double> result(frames);
    std::size_t nextGate=0;
    for (std::size_t i=0; i<frames; ++i) {
        while (nextGate<gates.size() && gates[nextGate].frame==i) {
            if (gates[nextGate].on) envelope.on(); else envelope.off();
            ++nextGate;
        }
        result[i]=envelope.advance(initial,peak,sustained,headroom,saturation,
            coefficients[0],coefficients[3],coefficients[1],coefficients[2]);
    }
    return result;
}

std::vector<OscillatorFrame> oscillatorTrace(double fs,double frequency,
    const Patch& supplied,std::size_t frames,double initialPhase) {
    if (!std::isfinite(fs) || fs<100 || fs>1536000 ||
        !std::isfinite(frequency) || frequency<=0 || !std::isfinite(initialPhase))
        throw std::invalid_argument("Invalid oscillator probe input");
    const Patch patch=validatePatch(supplied);
    Oscillator oscillator; oscillator.phase=wrap(initialPhase);
    const double stiCoefficient=-std::expm1(-1/(fs*wscSineInputTau));
    std::vector<OscillatorFrame> result(frames);
    double pwmPhase=0;
    for (auto& frame : result) {
        const double pwm=sinTurn(pwmPhase+patch.pwmPhase);
        frame=oscillator.process(frequency,patch.pulseWidth+patch.pwmDepth*pwm,
            patch.resetMicroseconds*1e-6,fs,stiCoefficient);
        pwmPhase=wrap(pwmPhase+patch.pwmRate/fs);
    }
    return result;
}

// Preset implementation follows below. Raw Yamaha voltages are retained in
// research/factory-preset-cv.csv; conversion coefficients are provisional.
std::vector<std::string> presetNames() {
    return {"init","string1","string2","brass1","brass2","flute","electric_piano",
            "clavichord","harpsichord","guitar1","guitar2","funky1","funky2","funky3"};
}
Patch preset(const std::string& name) {
    if (name == "init") {
        Patch p; p.name=name; p.filterInitial=p.filterPeak=0;
        p.ampAttack=.004; p.ampDecay=.2; p.ampSustain=1; p.ampRelease=.15;
        p.lpCutoff=10000; p.hpCutoff=15;
        return p;
    }
    // a..v, Yamaha CS Series Servicing Guide, printed p.64, CS-50 table.
    // Exact factory electrical data; the VCO/VCF/EG control-law conversion
    // below is an explicit starting estimate, not a measured factory match.
    struct Factory { const char* name; std::array<double,22> v; };
    static const Factory factories[] = {
        {"string1", {0,0,0,0,10,0,5,6.17,5.70,10,3.85,6.80,2.50,4.55,6.80,10,.43,1.52,1.09,8.47,8.85,10}},
        {"string2", {0,0,0,0,10,0,2.33,10,5.95,10,0,5.49,5,.63,6.80,10,0,4.35,10,8.47,6.99,8.47}},
        {"brass1", {0,0,0,0,10,0,0,10,5.71,5.71,1.89,3.57,1.39,1.64,10,10,.43,5.24,2.04,6.17,10,10}},
        {"brass2", {0,0,0,0,10,0,2.17,3.85,3.84,7.87,2.70,8.70,4,3.85,7.19,10,0,5.49,4.35,4.76,6.62,8.333}},
        {"flute", {0,0,0,0,10,0,1.18,3.57,5.24,2.33,5.24,.63,3.57,1.68,7.35,10,0,3.13,0,5.24,7.19,10}},
        {"electric_piano", {0,.63,3.35,10,0,0,1.28,6.41,1.52,3.33,0,4.76,7.52,2.57,10,10,4.35,10,2.17,.63,6.41,10}},
        {"clavichord", {0,0,7.19,10,10,0,5.24,5.95,5.71,4.55,0,4.55,10,2.23,7.52,10,0,10,3.33,1.28,10,6.99}},
        {"harpsichord", {0,0,5.95,10,0,0,6.41,10,7.87,8.47,0,0,0,0,0,10,0,10,2.17,0,7.35,10}},
        {"guitar1", {0,0,4.55,10,0,0,3.85,5.49,5.95,2.50,.43,2.94,10,5,10,10,0,7.52,3.85,1.64,6.41,8.33}},
        {"guitar2", {0,0,0,0,10,0,2.50,0,2.70,0,0,7.52,10,2.19,7.19,10,5.24,10,.99,.43,8.06,6.41}},
        {"funky1", {2.33,.91,5.41,10,10,0,5,6.17,5.95,0,10,6.80,10,6.68,4.76,10,3.33,10,10,8.47,8.85,5}},
        {"funky2", {0,0,6.62,0,10,0,5.71,0,7.35,0,9.09,9.59,6.41,5.49,9.30,10,5.24,10,0,4.76,10,6.80}},
        {"funky3", {0,0,0,10,0,0,4,2.50,4.76,3.13,0,9.47,6.17,4,7.19,10,0,7.19,10,8.47,7.35,5.95}}
    };
    // T71/T72 tone-preset matrix (SM2 pp.10-11). A pressed tablet puts the
    // board's own 10V regulator on one preset bus; each output pin takes that
    // bus through a series resistor, a 1S1555 and a 10k to ground. The printed
    // table is the bare divider -- 1.3k gives 10*10/11.3 = 8.85V, exactly as
    // printed -- so the diode's drop is not in it, and the real pin sits that
    // much lower. Where a row drives a high-impedance input the 10k is its
    // whole load, so the drop just lowers the source:
    // Vpin = Videal*(10-Vd)/10 with Vd taken at I = Vpin/10k. The four filter
    // rows drive passive R7 networks instead (loadedPresetPin below). The
    // matrix constants and the 1S1555 ESTIMATE sit with the PANEL cutoff
    // lever law (presetSupply); with them pins land 4.2-5.8% low.
    // A zero column has no matrix path at all and stays at ground.
    const auto presetPin=[](double ideal) {
        if (ideal<=0) return ideal;
        double pin=ideal;
        // Each pass moves the drop by under half a percent of the last.
        for (int i=0;i<8;++i)
            pin=ideal*(presetSupply-presetDiodeVt*std::log(pin/(presetLoad*presetDiodeIs)))/presetSupply;
        return pin;
    };
    // Filter rows g-j are loaded beyond that 10k: g and i by R7's four 39k
    // cutoff branches (fhBusLoad, flBusLoad, with the PANEL lever law). The
    // same wiring lists (SM2 printed p.11 T71, p.15 R7) run h and j straight
    // to QH (52) and QL (50), again with no buffer between. Each Q bus
    // feeds four 220k branches into the pin-7 current inputs: 55.4k in all,
    // returning to qBranchReturn of the RES bus, which is the T71 +10V at the
    // presets' RESONANCE MIN (.078V). The row V then solves
    // (10 - Vd - V)/Rs = V/10k + (V - Vret)/Rload, the diode's current, with
    // Rs recovered from the printed bare divider, Rs = 10k(10/Videal - 1); a
    // zero column leaves the row at the load's own Thevenin voltage.
    constexpr double qBusLoad=qBranchLoad/4;
    const auto loadedPresetPin=[](double ideal,double load,double loadReturn) {
        // Thevenin of the 10k and the load: vl behind rl.
        const double rl=presetLoad*load/(presetLoad+load);
        const double vl=loadReturn*presetLoad/(presetLoad+load);
        if (ideal<=0) return vl;
        const double share=rl/(presetLoad*(presetSupply/ideal-1)+rl);
        double pin=vl+(presetSupply-vl-.5)*share;
        // Each pass scales the error by Vd's slope over Rs+rl, under 1%.
        for (int i=0;i<8;++i)
            pin=vl+(presetSupply-vl-presetDiodeVt*std::log((pin-vl)/(rl*presetDiodeIs)))*share;
        return pin;
    };
    std::string canonical = name;
    if (canonical == "strings_1" || canonical == "strings1" || canonical == "string_1") canonical="string1";
    if (canonical == "strings_2" || canonical == "strings2" || canonical == "string_2") canonical="string2";
    if (canonical == "brass_1") canonical="brass1";
    if (canonical == "brass_2") canonical="brass2";
    if (canonical == "electric-piano") canonical="electric_piano";
    // Preserve the Guide transcription above. These separate corrections
    // require agreement between the original T-board table and its resistor
    // matrix; see research/preset-cv-errata.json. These are the table's ideal
    // divider values; the matrix diode below takes them to the pin.
    struct Correction { const char* name; std::size_t column; double voltage; };
    static const Correction corrections[] = {
        {"string1", 8, 8.70},
        {"brass2", 8, 3.85},
        {"flute", 13, 1.64},
        {"electric_piano", 2, 3.33},
        {"electric_piano", 13, 3.57},
        {"clavichord", 13, 2.33},
        {"guitar2", 13, 2.17},
        {"funky1", 2, 5.49},
        {"funky1", 13, 6.62}
    };
    for (const auto& factory : factories) if (canonical == factory.name) {
        auto cv = factory.v;
        for (const auto& correction : corrections)
            if (canonical == correction.name) cv[correction.column]=correction.voltage;
        const auto ideal=cv;
        for (auto& voltage : cv) voltage=presetPin(voltage);
        Patch p; p.name=canonical;
        // Guide p.49 calibrates actual PWM VC1 to 0.2/6.7/222 Hz at
        // 0/5/10 V. An exponential through the endpoints predicts 6.663 Hz
        // at 5 V, consistent with the rounded middle value. Applying this
        // law directly to the raw factory CV assumes its preset-bus transfer;
        // preset loading/offsets remain uncalibrated, as for cutoff CVs.
        p.pwmRate=.2*std::pow(1110.0,cv[0]/10); p.pwmDepth=.4*cv[1]/10;
        p.pulseWidth=.5+.4*cv[2]/10;
        p.pulse=cv[3]/10; p.saw=cv[4]/10; p.noise=cv[5]/10;
        // Cutoff columns g (HP) and i (LP): the printed IG00156 law in full,
        // fc=125*Kv*2^Vfc (guide printed p.34; SM1 printed p.19, "VK .25V,
        // Vf 5V -> 1kHz"): 62.5Hz of gm at middle C (Kv=.5V) for 0V and one
        // octave per volt, both stages. Chosen by ear 2026-10-20 (listening
        // test 2026-10-20-vcf-printed-law, which supersedes the zero-point
        // and HP/LP-slope sets). Every number is printed; the one inference
        // is where Vfc is measured. It cannot be pin 4 itself: a 10V lever
        // through 39k into the CS-50's 1.8-3.0k rank resistor (SM2 printed
        // p.16; R6 HP, R7 LP) puts only .4-.6V there, and guide printed p.33
        // draws Vfc reaching pin 4 through a variable resistor, the rank-set
        // network. So Vfc is read on the FH/FL bus, ahead of the 39k; the
        // PANEL lever reaches that bus through a PRA follower and a T71 diode,
        // a preset through its loaded divider (loadedPresetPin). One slope
        // serves both levers: the CS-50 rank column (SM2 printed p.16,
        // "CS50,60": R7 3.0k/2.4k/2.0k beside R6 2.7k/2.2k/1.8k; p.18
        // NA03655) is exactly the E24 set nearest equal 39k-lever gain into
        // the two pin-4 nodes (3.09k/2.45k/1.97k); with the EG paths the
        // as-built HP/LP gain ratio is 1.00-1.06 by rank. Not printed: the
        // series reference behind the pin sheet's Vf, and pin 4's own input
        // law. The law moves variable gm, not the fixed 4.7M feed, and the
        // Patch API expects total integrator Hz, so include the resistor
        // contribution once. See Docs/research/vcf-authenticity-2026-10-20.md.
        const double fhBus=loadedPresetPin(ideal[6],fhBusLoad,0);
        const double flBus=loadedPresetPin(ideal[8],flBusLoad,0);
        p.hpCutoff=filterFrequencyFloor+gmHzAtMiddleC*std::exp2(leverOctavesPerVolt*fhBus);
        p.lpCutoff=filterFrequencyFloor+gmHzAtMiddleC*std::exp2(leverOctavesPerVolt*flBus);
        // Overall schematic: passive FH/FL buses feed each cutoff via 39k;
        // the shared EG uses 220k for HP and 47k for LP, so at the levers'
        // 1 oct/V the EG moves the cutoffs 39/220 and 39/47 octave per EG
        // volt, relative to the loaded bus voltage above. A board's EG also
        // lifts its pin-4 node, which unloads the shared passive bus and so
        // raises every board's cutoff a little: at most .03 octave per board
        // at +5V LP EG on a weak column, .1 with all four. Not modelled.
        p.hpEnvelopeScale=leverOctavesPerVolt*39/220;
        p.lpEnvelopeScale=leverOctavesPerVolt*39/47;
        // Q columns h (HP) and j (LP): each pin 7 takes its preset Q bus P
        // and the RES bus R through 220k each, with 10uF to ground (SM1
        // printed p.14 R7; SM2 printed p.16), so its current settles at
        // (P+R)/2 through 110k+Rin (qPinResistance), VQ=(P+R)/2 and
        // r=1-(P+R)/20. Factory presets put the RESONANCE lever at MIN, where
        // TVR6 sits on the +10V bus (SM2 p.26 PN2 sheet): r=(10-P)/20. The
        // divider is printed; which lever position Yamaha voiced the presets
        // at is not, and MIN was chosen by ear 2026-10-20 (listening test
        // 2026-10-20-brilliance-resonance-preset-q-divider, B). Before, the
        // bus went straight to the pin, r=(10-P)/10, which no single lever
        // position gives for every preset. resonance_offset is then the RES
        // change from MIN: 0 to -10V. P is the loaded row (loadedPresetPin):
        // the four pin-7 branches pull it toward .078V, so a zero column sits
        // at .012V. Away from MIN that return follows RES through the 10k, so VQ
        // moves at most .12% more than half of each RES volt; not modelled.
        p.hpResonance=(10-loadedPresetPin(ideal[7],qBusLoad,presetSupply*qBranchReturn))/20;
        p.lpResonance=(10-loadedPresetPin(ideal[9],qBusLoad,presetSupply*qBranchReturn))/20;
        // VCF-EG levels at TP1 (EG OUT). The IG00152 pin sheet (SM1 p.20)
        // gives AL 0..10V -> LC up to +5V and IL 0..10V -> 0..-5V, but the
        // alignment of the same M board (Yamaha CS-60 Service Manual p.37,
        // EG-VCF item 1; the CS-50 M sheet, SM2 p.16, is drawn for "CS-60,50"
        // with the same IG00152/IG00151, 82k+VR19, 560k and .22uF) checks
        // +5V +/-.5V at AL=10V and -4V +/-.5V at IL=10V after VR18 has
        // nulled the held level to 0 +/-.01V. Its -4V window excludes -5V,
        // so IL takes the alignment value, AL the agreeing .5V/V. Sibling
        // service evidence for a shared board, not a CS-50 measurement.
        p.filterInitial=-cv[10]*.4; p.filterPeak=cv[11]*.5;
        // Service-manual endpoints: attack 1ms..1s; decay/release 10ms..10s.
        // The related CS-20M/CS-40M explanation of this IG00159 circuit
        // defines durations as 5*RC. Convert D/R to the engineering API's
        // T60, and set attack's target to reach the nominal peak after 5*RC.
        // This is a related-model inference, not a CS-50 timing calibration:
        // the published A versus D/R decade and shared TC remain unresolved.
        // Pin sheets (SM2 p.21 IG00159 AT/1DT/2DT; SM1 p.20 IG00152): each
        // law is stated at the chip pin, 0..10V. The decade between A and D/R
        // is the chip's own: Yamaha IC Guide pp.2-3/2-7 show AT/DT/RT switched
        // straight to TC, which drives one IG00151 EI pin through 82k+VR19
        // (VR20) into one .22uF capacitor, but the attack aims at the same
        // 30k/60k node the sequencer compares against (VA = VB, Guide p.43),
        // so an attack spans several RC; a ten-times-shorter attack range
        // therefore needs a ten-times-shorter RC per pin volt. Those block
        // diagrams also omit IL's printed 0..10V -> 0..-5V scaling, so they
        // do not rule out internal AT scaling. See
        // Docs/research/envelopes-authenticity-2026-10-20.md.
        const auto attackTime = [](double v) { return std::pow(10,-.3*v); };
        const auto decayTime = [](double v) { return 10*std::pow(10,-.3*v)*std::log(1000.0)/5; };
        // RF/RA are not the release lever: SUB-board sustain control (SM2
        // p.8; Guide p.61) takes each lever through 10k to a diode-OR node
        // with SUS and then 1k into a 2SC458 emitter follower with 10k to
        // -15V. The decay levers reach DF/DA directly, and D and R share the
        // EG's one TC/EI trim, so the follower's drop cannot be trimmed out of
        // R alone: release sits below the same CV's decay on the pin law.
        // Hitachi 2SC458 datasheet: VBE typ .67V at IC 2mA, VCE 12V (this
        // follower runs at (Vout+15)/10k = 1.5..2.5mA, VCE 5..15V); hFE ranks
        // C 160-320 and D 250-500 (the M/SUB notes allow C or D), taken at
        // their geometric centre, 283. ESTIMATE: typical datasheet values for
        // the fitted part, not a measured unit; 0.1V of drop is 7% of time.
        // With the factory presets' zero global release, SUS never clamps.
        const auto releaseFollower = [](double lever) {
            constexpr double vbe=.67, ic=2e-3, vt=25.85e-3, beta=283;
            double out=lever-vbe;
            for (int i=0;i<8;++i) {
                const double emitter=(out+15)/10e3;
                out=lever-emitter/(beta+1)*11e3-(vbe+vt*std::log(emitter/ic));
            }
            return out;
        };
        p.filterAttackHeadroom=p.ampAttackHeadroom=1/std::expm1(5.0);
        p.filterAttack=attackTime(cv[12]); p.filterDecay=decayTime(cv[13]); p.filterRelease=decayTime(releaseFollower(cv[14]));
        p.filterLevel=cv[15]/10; p.sine=cv[16]/10;
        p.ampAttack=attackTime(cv[17]); p.ampDecay=decayTime(cv[18]); p.ampSustain=cv[19]/10;
        // The final preset column drives common M-board VCA LEVEL, before
        // the amp EG/bus/ring. PN2 master VOLUME is a separate later control.
        p.ampRelease=decayTime(releaseFollower(cv[20])); p.ampLevel=cv[21]/10; p.volume=.55;
        return p;
    }
    throw std::invalid_argument("Unknown Ceres preset: " + name);
}
#endif // !CERES_EMBEDDED_TARGET

// Parameter descriptors read the setParameter table itself, so a key, range
// or rounding rule added there reaches every front end with no second list.
std::size_t parameterCount() noexcept {
    return sizeof parameters/sizeof parameters[0];
}

ParameterInfo parameterInfo(std::size_t index) noexcept {
    if (index>=parameterCount()) return {nullptr,0,0,0,false};
    const Parameter& parameter=parameters[index];
    const Patch defaults{};
    // A key is integer when setParameter rounds it. Probe it with a value
    // that is not a whole number for any integer-valued range (0.318... of
    // the span): rounding then moves it, a continuous key keeps it.
    const double probe=parameter.low+(parameter.high-parameter.low)*.3183098861837907;
    const bool integer=parameterValue(parameter,probe)!=probe;
    return {parameter.key,parameter.low,parameter.high,defaults.*(parameter.member),integer};
}

bool getParameter(const Patch& patch, const char* key, double& value) noexcept {
    if (!key) return false;
    for (const auto& parameter : parameters) {
        if (std::strcmp(key,parameter.key)==0) {
            const double stored=patch.*(parameter.member);
            value=std::isfinite(stored) ? parameterValue(parameter,stored) : Patch{}.*(parameter.member);
            return true;
        }
    }
    return false;
}
} // namespace ceres
