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
constexpr double filterCapacitance = 1.5e-9, filterFeedResistance = 4.7e6;
constexpr double filterFrequencyFloor = 1/(2*pi*filterCapacitance*filterFeedResistance);
// Direct R7 matrix, CS-50 service1 PDF8/printed14. These ratios are known;
// .7/.65 oct/V manual slopes remain the existing provisional calibration.
// Interpret BR as actual loaded voltage with stiff preset/source nodes,
// not raw preset-divider CV or a finite-impedance pot position.
constexpr double hpBrillianceScale=.7*39/680, lpBrillianceScale=.65*39/220;
constexpr double hpSubRelativeScale=(.7/.65)*(47./150.);
// Guide printed p.56: touch level amplifier I is trimmed so a 0V-centred SUB
// input leaves centred on +.5V; the SUB source is fixed at 3Vpp (p.55).
constexpr double touchVibratoBias=.5/1.5;
// R7 LC sums LEVEL (10V full, 270k) and touch LEVEL (6.0V full, 150k; guide
// printed p.55 calibration) into common VCA2: full touch adds 1.08 times the
// full-LEVEL current (SM1 printed p.14).
constexpr double touchLevelScale=(6/150e3)/(10/270e3);
// The same LC node takes the SUB VCA lever (PN3 SVR4, B-10k between SUO and
// ground; SM2 p.27) through 120k: the 3Vpp SUB at full lever adds +/-1.5V/120k
// against LEVEL's 10V/270k, so +/-.3375 of full LEVEL. The lever's <=2.5k
// source resistance (2% of 120k) is neglected.
constexpr double tremoloScale=(1.5/120e3)/(10/270e3);
constexpr double qControlTau=10e-6*(220e3/2);
// Fairchild uA796 upper carrier pairs have no emitter degeneration. In the
// matched, forward-active/infinite-beta limit their current steering is
// tanh(V7minus8/(2VT)). Guide p49 gives the carrier sine as nominal3Vpp.
constexpr double ringThermalVoltage=1.380649e-23*298.15/1.602176634e-19;
constexpr double ringSourcePeakVolts=1.5;
// Nominal zero-IC-current DC circuit, balance VR5 at center. Unlike the
// reduced AC network, include both470+(100k+25k) signal-input returns.
constexpr double ringBiasGround=1/(1/1000.+2/(470.+125000.));
constexpr double ringBiasLoad=1/(1/33100.+1/(820.+ringBiasGround));
constexpr double ringPin8Dc=15*ringBiasLoad/(390+1200+ringBiasLoad);
constexpr double ringDifferentialDc=-ringPin8Dc*100/33100.;
double ringSteering(double volts) { return std::tanh(volts/(2*ringThermalVoltage)); }
// The patch specifies TOTAL integrator Hz at middle C. Only variable gm
// responds to exponential control; the parallel 4.7Mohm path stays fixed.
double modulatedFilterFrequency(double referenceHz, double factor) {
    return filterFrequencyFloor + std::max(0.0,referenceHz-filterFrequencyFloor)*factor;
}
double clamp(double x, double lo, double hi) { return std::max(lo, std::min(x, hi)); }
double wrap(double x) { return x - std::floor(x); }

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
    {"output_trim_db", &Patch::outputTrimDb, -24, 24},
    {"octave", &Patch::octave, -2, 1}, {"tune_cents", &Patch::tuneCents, -500, 700},
    {"voice_spread_cents", &Patch::voiceSpreadCents, 0, 30},
    {"drift_cents", &Patch::driftCents, 0, 20},
    {"lfo_waveform", &Patch::lfoWaveform, 0, 4},
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
    // This ideal selector omits diode/VBE offsets and source/chip loading.
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
// derivatives at every sample. Vs and the VQ -> damping map remain calibration
// parameters: Yamaha does not specify the internal transistor geometry.
// CS-50 M-board schematic: C=1.5nF; each 4.7Mohm resistor bypasses its
// variable-gm block, feeding the next integrator INPUT. It is not a capacitor
// bleed resistor. This linear input-current path sets a 22.58 Hz integrator-rate floor.
// The frequency argument denotes total small-signal integrator frequency, so gm is
// the requested total conductance minus 1/R, constrained nonnegative.
// The service manual
// specifies Q=.5 at VQ=10V and Q=5 at VQ=0V; linear damping interpolation
// between those calibration endpoints is an explicitly provisional choice.
// HP and LP have independent Q trims: the nominal endpoints are anchored at
// their usual source loads (HP three mixer branches, LP one). Changing a
// waveform gate changes the source load, not the Q transistor conductance.
struct OtaSvf {
    double sb = 0, sl = 0, sz = 0, sy = 0, previousB = 0;
    double process(double sourceVoltage, double frequency, double resonance,
                   double fs, bool highPass, double sourceConductance = 1/150e3,
                   double referenceSourceConductance = 1/150e3) {
        const double cutoff = clamp(frequency, filterFrequencyFloor, .22 * fs);
        const double g = std::tan(pi * cutoff / fs); // capacitor TPT prewarp
        constexpr double feedbackCapacitance=180e-12, sourceCapacitance=1e-6;
        const double conductance=sourceConductance+1/33e3+1/1.2e6;
        const double referenceConductance=referenceSourceConductance+1/33e3+1/1.2e6;
        const double damping=(2.0-1.8*clamp(resonance,0,1))*referenceConductance/conductance;
        const double ratio=sourceConductance/conductance;
        // X is the physical Thevenin source voltage, in the SAME OTA voltage
        // unit at both stages. Canonical u=2*Gs*X/D0 is a local algebraic
        // coordinate, not a separate per-stage signal normalization.
        const double input=2*ratio*sourceVoltage;
        const double qBlend=conductance/(conductance+2*fs*feedbackCapacitance);
        const double sourceBlend=sourceConductance/(sourceConductance+2*fs*sourceCapacitance);
        const double divisor=1-qBlend*ratio*sourceBlend;
        // Eliminate both linear capacitor nodes simultaneously. The remaining
        // OTA equation has positive derivative because effectiveDamping>0.
        const double qConstant=((1-qBlend)*sz+
            qBlend*(1-sourceBlend)*(input-ratio*sy))/divisor;
        const double qBand=qBlend*(2-damping-2*ratio*sourceBlend)/divisor;
        const double effectiveDamping=(2*(1-qBlend)+qBlend*damping)/divisor;
        const double linear = 1/(2*fs*filterCapacitance*filterFeedResistance);
        const double nonlinear = cutoff<=filterFrequencyFloor ? 0.0 : std::max(0.0,g-linear);
        double b = previousB;
        for (int iteration = 0; iteration < 7; ++iteration) {
            const double tb = std::tanh(b);
            const double l = sl + nonlinear*tb + linear*b;
            const double h = qConstant - effectiveDamping * b - l;
            const double th = std::tanh(h);
            const double residual = b - sb - nonlinear*th - linear*h;
            const double dl = nonlinear*(1-tb*tb) + linear;
            const double derivative = 1 + (nonlinear*(1-th*th)+linear)*(effectiveDamping+dl);
            const double delta = residual / derivative;
            b -= delta;
            if (std::abs(delta) < 1e-12) break;
        }
        const double l = sl + nonlinear*std::tanh(b) + linear*b;
        const double z = qConstant+qBand*b;
        const double h = z - 2*b - l;
        // Expand sourceBlend/ratio analytically so tiny positive diagnostic
        // conductances cannot overflow at the intermediate input/ratio.
        const double sourceInput=conductance/(sourceConductance+2*fs*sourceCapacitance);
        const double y=(1-sourceBlend)*sy+sourceBlend*(-z+2*b)+sourceInput*input;
        sb = 2 * b - sb;
        sl = 2 * l - sl;
        sz = 2 * z - sz;
        sy = 2 * y - sy;
        previousB = b;
        return highPass ? h : l;
    }
};

// The shared M-board service procedure (CS-60 manual p.38, section VI)
// trims sine-only CP4 to 1Vpp and filtered pulse-only CP4 to .8Vpp, using
// separate VCA controls. Exact test key/HP setting and CS-50 calibration
// remain unresolved. This broad-pass estimate is a PROVISIONAL downstream
// VCA gain, not an inverse gain inserted between the nonlinear filters.
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
};

// Conditional output circuit, using the dedicated M-board and PRA schematics
// (service2 printed pp.16 and 22–23; research/output-coupling-audit.md).
// Rin4=Rin5=0 is a low-impedance CURRENT-input limit, not an identified chip
// input resistance. The prior normalized midband gains remain calibration
// approximations. CP3 precedes filtered level; CP4 precedes the common VCA.
// All four low-Z 4.7uF/1k voice branches stay connected to the 47k PRA load,
// including silent voices. Their common bus transfer has one equivalent
// state, independent of voice allocation. The final 33uF section assumes an
// unloaded HIGH output (4.7k+560ohm internal load). Expression assumes an
// ideal fully conducting CdS. No external recording-chain pole is included.
// Capacitor integration uses physical-RC trapezoids without pole prewarp.
constexpr double outputCouplingTau[4]={1e-6*100e3,1e-6*(27e3+100e3),
    4.7e-6*(1e3+4*47e3),33e-6*(4.7e3+560)};
struct OutputCoupling {
    double memory=0;
    double process(double x,double tau,double fs) {
        const double g=1/(2*fs*tau);
        const double high=(x-memory)/(1+g);
        memory+=2*g*high;
        return high;
    }
};

// Direct CS-50 PRA: four 100uF capacitors around the uA796 carrier input.
// Nodes x=pin7, b=pin8, a=audio-bias, s=bypassed local supply. This passive
// limit assumes an ideal sine source and zero incremental IC input currents.
// It does not identify the multiplier's transistor law or wet-gain trim.
struct RingCarrierCoupling {
    std::array<std::array<double,4>,4> inverse{};
    std::array<double,4> memory{};
    void configure(double fs) {
        std::array<std::array<double,4>,4> g{};
        auto ground=[&](unsigned i,double r){g[i][i]+=1/r;};
        auto edge=[&](unsigned i,unsigned j,double r){
            const double y=1/r;g[i][i]+=y;g[j][j]+=y;g[i][j]-=y;g[j][i]-=y;
        };
        ground(0,33000);edge(0,1,100);edge(1,2,820);edge(1,3,1200);
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
        return nodes[0]-nodes[1]; // actual differential carrier; no gain normalization
    }
};

// IG00159 VCA-EG and IG00152 VCF-EG (CS-50 service manual, pp.20–21):
// a sequencer switches a variable
// resistance into an integrating capacitor. Each segment below is the exact
// RC solution v(t+dt)=target+(v(t)-target)*exp(-dt/RC). The attack target has
// configurable target headroom (1% default, not a hardware calibration);
// attack duration reaches the nominal peak from IL for every headroom value.
// Decay/release durations denote 60 dB settling (not hardware calibration).
// Both M-board integrating capacitors are .22uF. The parameter durations set
// equivalent resistance; e.g. R_decay = duration/(.22uF*ln(1000)).
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
            const double span = std::max(1e-9, std::abs(peak - initial));
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
            if (std::abs(voltage - target) < 1e-7) { voltage = target; stage = Sustain; }
        } else {
            coefficient = releaseCoefficient;
            voltage = target + (voltage - target) * coefficient;
            if (std::abs(voltage - target) < 1e-7) { voltage = target; stage = Idle; }
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
    if (p<.02425) return tail(std::sqrt(-2*std::log(p)));
    if (p>1-.02425) return -tail(std::sqrt(-2*std::log(1-p)));
    const double q=p-.5, r=q*q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q/(((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
}

// One shared SUB source. Discontinuities use the same short analytic BLEP
// correction as the VCO pulse, without tables. Noise comes from the shared
// limiter sample through its dedicated RC path.
double subWaveform(double phase,double step,double waveform,double noise) {
    if (waveform==1) return 1-2*phase+blep(phase,step);
    if (waveform==2) return -(1-2*phase+blep(phase,step));
    if (waveform==3) return (phase<.5 ? 1.:-1.)+blep(phase,step)-blep(wrap(phase-.5),step);
    if (waveform==4) return noise;
    return std::sin(2*pi*phase);
}

struct Oscillator {
    double phase = 0;
    OscillatorFrame process(double frequency, double pulseWidth, double resetSeconds, double fs) {
        // Guide pp.28-31: OUT7 conditions the integrating-capacitor signal
        // with the monostable pulse before the WSC. It is not the exposed raw
        // recharge triangle. In the nominal current law r=f*tau, fixed A0
        // normalization makes the descending segment exactly 1-2*phase.
        // A flat reset plateau followed by a 2r step is a reduced clamp model
        // of the published sketch, not a recovered hidden-chip transfer.
        const double step = clamp(frequency / fs, 1e-8, .18);
        const double recharge = clamp(frequency * resetSeconds, 1e-7, .35);
        // The .35 guard is a defensive extrapolation outside the calibrated
        // Normal keyboard range; no high-CV/pulse overlap behavior is claimed.
        const double resetPhase = wrap(phase-recharge);
        // WSC pin5 inverts the conditioned downward ramp. Correct both
        // voltage jumps and both slope changes analytically, without tables.
        const double sawBase = phase < recharge ? -1 : 2*phase-1;
        const double saw = sawBase - blep(phase,step)
            + recharge*blep(resetPhase,step)
            + blamp(resetPhase,step)-blamp(phase,step);
        const double duty = clamp(pulseWidth, .01, .99);
        // WSC guide p.31 explicitly states that PWM moves only the falling
        // edge. Its rising edge stays locked to the VCO period. Applying a
        // comparator to BOTH slopes of the internal recharge capacitor would
        // incorrectly move both edges; WSC uses the conditioned VCO output.
        // This is the comparator's logical timing coordinate. At
        // deep PWM where duty<recharge, this fixed-edge rule is an independent
        // WSC approximation; hidden reset/comparator compensation is unknown.
        double square = phase < duty ? 1 : -1;
        square += blep(phase,step) - blep(wrap(phase-duty),step);
        // WSC pin14 -> pin3 folds the SAME conditioned ramp, then pin15 ->
        // pin2 rounds it. Retain the existing ideal sine transfer, but stop
        // bypassing the finite reset with an unrelated ideal phase ramp.
        // The flat plateau is thus 1; the descending branch is cos(2pi*phi).
        // Sine slice trim/compensation and exact transistor shape are unknown.
        const double resetAngle = 2*pi*recharge;
        const double resetSine = std::sin(resetAngle);
        const double resetCosine = std::cos(resetAngle);
        const double sineBase = phase < recharge ? 1 : std::cos(2*pi*phase);
        const double sine = sineBase + .5*(resetCosine-1)*blep(resetPhase,step)
            - pi*resetSine*blamp(resetPhase,step);
        phase = wrap(phase + step);
        // Empirical audio-polarity calibration: public CS-50 solo/both-on
        // recordings support constructive saw/pulse mixing at equal nominal
        // source levels. Negate the completed pulse, including its BLEP,
        // without moving either comparator edge, changing PW/PWM timing or
        // introducing another oscillator. The guide's small waveform sketches
        // do not establish an unambiguous common timing reference; this is not
        // proof of an active-low chip output or an extra hardware inverter.
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
    std::array<double,maxTaps> h{}, history{};
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
    }
    void push(double x) { history[cursor] = x; cursor = (cursor+1)%taps; }
    double output() const {
        double y = 0;
        unsigned at = cursor;
        for (unsigned i = 0; i < taps; ++i) {
            at = at ? at-1 : taps-1;
            y += h[i] * history[at];
        }
        return y;
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
#if !defined(CERES_EMBEDDED_TARGET)
        double oscillatorFrequency=0, hpFrequency=0, lpFrequency=0;
#endif
    };
    double fs, internalFs;
    std::uint64_t seed, age = 0, hostSamples = 0;
    Random random;
    Patch patch;
    std::array<Voice,Synth::maxVoiceCount> voices;
    unsigned voiceLimit=Synth::voiceCount, rateFactor=Synth::oversampling;
    OutputRate decimator;
    double lfoPhase = 0, pwmPhase = 0, pressureTarget = 0, pressureValue = 0, bend = 0;
    OutputCoupling busCoupling, lineCoupling;
    double pressureCoefficient = 0, velocityCoefficient = 0, driftCoefficient = 0;
    std::array<double,4> ampCoefficients{}, filterCoefficients{};
    double hpSourceBranches = 3, lastSub = 0;
    // Eight equal pin7 networks reduce exactly to one common delta state
    // for this stiff-source/zero-input-current limit and equal initial delta.
    // Existing per-stage effective Q anchors are NOT raw chip voltages.
    // Two independently smoothed anchors plus the shared delta decompose
    // two stage capacitor coordinates, not three physical capacitors.
    double qVoltageOffset=0, qControlCoefficient=0;
    double hpResonanceAnchor=0, lpResonanceAnchor=0;
    bool hasProcessed=false;
    double effectiveHpResonance=0, effectiveLpResonance=0, commonVcaGain=1;
    double outputTrimGain=1;
    double limiterInput=0, limiterOutput=0, limiterPole=0, limiterInnovation=0;
    double limiterRate=0, limiterStep=0;
    unsigned limiterSteps=1;
    // Substeps after the first draw from their own stream, so the shared
    // stream keeps one draw per sample and drift is unchanged.
    Random limiterRandom{1};
    OutputCoupling pickupCoupling;
    // Guide printed32: SUB-N is the limiter output N through22k/.1uF,
    // independent of audio NO level: the documented single low-pass (called
    // pink in the prose), not an invented ideal1/f filter. SM2 p.8 (PN3):
    // the FUNCTION switch puts N on SUI, a buffer with100k to ground, so the
    // RC sees22k||100k (pole88.26Hz) and a100/122 divider.
    double subNoiseMemory = 0, subNoiseG = 0;
    Envelope ringEnvelope;
    std::array<double,4> ringCoefficients{};
    double ringPhase = 0, ringCarrier = 0, ringFrequency = 30;
    double ringDepthValue = 0, ringDepthIncrement = 0;
    unsigned ringDepthRemaining = 0;
    double ringCoupledCarrier=0;
    double ringSwitchGain=ringSteering(ringDifferentialDc);
    RingCarrierCoupling ringCarrierCoupling;
    OutputCoupling ringWetCoupling;
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
        internalFs=fs*rateFactor;
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
        subNoiseG=1/(2*internalFs*subNoiseResistance*.1e-6);
        ringCarrierCoupling.configure(internalFs);
        updateCoefficients();
    }
    void updateCoefficients() {
        outputTrimGain=std::pow(10,patch.outputTrimDb/20);
        // M-board mixer: fixed 150k ground, 150k noise input, and one 150k
        // branch per enabled waveform gate. Ideal low-Z WSC/noise outputs,
        // negligible enabled-FET resistance; disabled gates are open.
        hpSourceBranches=2+(patch.saw>0 ? 1:0)+(patch.pulse>0 ? 1:0);
        ampCoefficients = envelopeCoefficients(internalFs,patch.ampAttack,
            patch.ampDecay,effectiveRelease(patch.ampRelease,patch.globalRelease),
            patch.ampAttackHeadroom,patch.ampAttackSaturation);
        filterCoefficients = envelopeCoefficients(internalFs,patch.filterAttack,
            patch.filterDecay,effectiveRelease(patch.filterRelease,patch.globalRelease),
            patch.filterAttackHeadroom,patch.filterAttackSaturation);
        // Direct CS-50 PRA RT divider: +15V through6.8k,3.3k toground.
        // Its unloaded4.901V mapped by the existing nominal five-RC law
        // implies .46781044s T60. External timing gain is not calibrated;
        // this fixed key-off return is separate from the M-board global SUS.
        constexpr double ringRelease=.4678104397473947;
        ringCoefficients=envelopeCoefficients(internalFs,patch.ringAttack,
            patch.ringDecay,ringRelease,.01,0);
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
        limiterRandom=Random(seed^0xd1b54a32d192ed03ULL); ringEnvelope={}; ringPhase=ringCarrier=0;
        ringFrequency=patch.ringRate; ringWetCoupling={};
        ringDepthValue=patch.ringDepth; ringDepthIncrement=0; ringDepthRemaining=0;
        ringCarrierCoupling.clear();ringCoupledCarrier=0;
        ringSwitchGain=ringSteering(ringDifferentialDc);
        qVoltageOffset=0;
        hpResonanceAnchor=patch.hpResonance; lpResonanceAnchor=patch.lpResonance;
        hasProcessed=false;
        effectiveHpResonance=patch.hpResonance; effectiveLpResonance=patch.lpResonance;
        commonVcaGain=patch.ampLevel;
        decimator.clear();
        // Preserve the original four-board random initialization and shared
        // audio/drift stream exactly. Extra virtual boards never consume that
        // stream during setup; they use a separate deterministic seed stream.
        Random extra(seed^0x9e3779b97f4a7c15ULL);
        for (unsigned i=0;i<voices.size();++i) {
            auto& voice=voices[i];
            auto& initialRandom=i<Synth::voiceCount ? random : extra;
            voice.osc.phase = initialRandom.unit();
            voice.filter.voltage = patch.filterInitial;
            voice.driftTarget = initialRandom.bipolar();
            voice.driftCountdown = static_cast<unsigned>(internalFs * (.1 + initialRandom.unit()*.5));
        }
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
        v.glideVoltage=patch.glideMode==2 ? stepped
            : v.glideVoltage+(stepped-v.glideVoltage)*-std::expm1(-patch.glideRate*dt);
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
        double limiterSum=0;
        for (unsigned step=0; step<limiterSteps; ++step) {
            const double draw=step ? limiterRandom.unit() : random.unit();
            limiterInput=limiterPole*limiterInput+limiterInnovation*inverseNormal(draw+0x1.0p-54);
            const double pickup=pickupCoupling.process(limiterInput,pickupCouplingTau,limiterRate);
            limiterOutput=pickup<0 ? std::max(-1.0,limiterOutput-limiterStep)
                : std::min(1.0,limiterOutput+limiterStep);
            limiterSum+=limiterOutput;
        }
        const double noise=limiterSum/limiterSteps;
        const double subNoise=(subNoiseMemory+subNoiseG*noise)/(1+subNoiseG);
        subNoiseMemory=2*subNoise-subNoiseMemory;
        const double lfo = subWaveform(lfoPhase,patch.lfoRate*dt,patch.lfoWaveform,
            patch.lfoWaveform==4 ? subNoise*subNoiseRail:0.);
        lastSub=lfo;
        // Exact zero-order-held RC update. Never bypass while offset returns
        // to zero: capacitor charge survives note allocation, patch edits,
        // and audio panic. reset() is the explicit electrical-state reset.
        qVoltageOffset+=(patch.resonanceOffset*.5-qVoltageOffset)*qControlCoefficient;
        hpResonanceAnchor+=(patch.hpResonance-hpResonanceAnchor)*qControlCoefficient;
        lpResonanceAnchor+=(patch.lpResonance-lpResonanceAnchor)*qControlCoefficient;
        // Direct chip endpoints Vq0:Q5,Vq10:Q.5 only. The intermediate
        // d=2-1.8*r law is provisional; delta d=.18*deltaVq retains that law.
        effectiveHpResonance=clamp(hpResonanceAnchor-qVoltageOffset*.1,0,1);
        effectiveLpResonance=clamp(lpResonanceAnchor-qVoltageOffset*.1,0,1);
        // R7 LC sums LEVEL(270k) and SUB(120k) currents before common VCA2.
        // PN3 VCF/VCA depth pots share SUO and ground; their passive R7
        // paths do not invert. Positive LI raises VCA gain (Guide pp.40–41),
        // so positive SUB must raise both cutoff and common gain. SUB swings
        // LC around LEVEL; the summed current, touch included, cannot drive
        // the VCA below zero gain.
        const double levelControl=patch.ampLevel+tremoloScale*patch.tremolo*lfo;
        commonVcaGain=std::max(0.0,levelControl);
        const double pwm = std::sin(2*pi*(pwmPhase+patch.pwmPhase));
        lfoPhase = wrap(lfoPhase + patch.lfoRate*dt);
        pwmPhase = wrap(pwmPhase + patch.pwmRate*dt);
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
            if (v.driftCountdown-- == 0) {
                v.driftTarget = random.bipolar();
                v.driftCountdown = static_cast<unsigned>(internalFs * (.2 + random.unit()*.5));
            }
            v.drift += (v.driftTarget-v.drift) * driftCoefficient;
            const double cents = patch.tuneCents + (static_cast<double>(i)-1.5)*patch.voiceSpreadCents
                + v.drift*patch.driftCents + lfo*(patch.vibratoCents + expression*patch.pressureVibrato)
                + expression*patch.pressureVibrato*touchVibratoBias;
            const double key=advanceGlide(v,dt);
            const double frequency = 440 * std::exp2((key-69 + bend + 12*patch.octave + cents*.01)/12);
            const auto waves = v.osc.process(frequency, patch.pulseWidth+pwm*patch.pwmDepth,
                                            patch.resetMicroseconds*1e-6,internalFs);
            const double filterEnvelope = v.filter.advance(patch.filterInitial,patch.filterPeak,0,
                patch.filterAttackHeadroom,patch.filterAttackSaturation,filterCoefficients[0],
                filterCoefficients[3],filterCoefficients[1],filterCoefficients[2]);
            const double ampEnvelope = v.amp.advance(0,1,patch.ampSustain,
                patch.ampAttackHeadroom,patch.ampAttackSaturation,ampCoefficients[0],
                ampCoefficients[3],ampCoefficients[1],ampCoefficients[2]);
            // Guide p.34 fc=125*Kv*2^Vfc; octave-linear key CV. The measured
            // panel-to-Vfc offset is currently replaced by cutoff-at-middle-C.
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
            const double hpFactor=std::exp2(keyOctaves+filterEnvelope*patch.hpEnvelopeScale
                + touchOctaves*hpSubRelativeScale+lfo*(patch.filterLfo*hpSubRelativeScale)
                + patch.brilliance*hpBrillianceScale);
            const double lpFactor=patch.lpEnvelopeScale==patch.hpEnvelopeScale &&
                patch.filterLfo==0 && patch.brilliance==0 && touchOctaves==0 ? hpFactor
                : std::exp2(keyOctaves+filterEnvelope*patch.lpEnvelopeScale
                    + touchOctaves+lfo*patch.filterLfo
                    + patch.brilliance*lpBrillianceScale);
            const double source = patch.saw*waves.saw + patch.pulse*waves.pulse + patch.noise*noise;
            const double hpFrequency=modulatedFilterFrequency(patch.hpCutoff,hpFactor);
            const double lpFrequency=modulatedFilterFrequency(patch.lpCutoff,lpFactor);
#if !defined(CERES_EMBEDDED_TARGET)
            v.oscillatorFrequency=frequency; v.hpFrequency=hpFrequency; v.lpFrequency=lpFrequency;
#endif
            const double low=v.filters.process(source,hpSourceBranches,hpFrequency,lpFrequency,
                effectiveHpResonance,effectiveLpResonance,patch.drive,internalFs);
            // CP3 coupling precedes filtered level; CP4 coupling acts on
            // the filtered/sine sum before common level and envelope VCAs.
            const double filtered=v.filteredCoupling.process(low,outputCouplingTau[0],internalFs);
            const double summed=filtered*patch.filterLevel + waves.sine*patch.sine;
            const double beforeVca=v.commonCoupling.process(summed,outputCouplingTau[1],internalFs);
            // IG00151 gain is LINEAR in envelope CV (guide pp.40–41). The
            // optional velocity term is zero in factory-derived presets.
            const double velocityGain = 1-patch.velocitySensitivity*(1-v.velocityValue);
            const double touchLevel=patch.pressureVolume*expression*touchLevelScale;
            mixed += beforeVca * ampEnvelope * velocityGain * std::max(0.0,levelControl+touchLevel);
        }
        // Output rail saturation is a deliberately mild, uncalibrated op-amp
        // approximation; no hidden peak normalization or dynamics processor.
        // Four equal4.7uF/1k branches into47k define this calibrated bus.
        // Extra voices are a virtual extension: retain the same bus/gain,
        // rather than inventing a new physical load or dividing by voice count.
        // More simultaneous voices can therefore increase rail saturation.
        // Common-bus transfer,
        // with previous overall midband gain retained as a calibration choice.
        const double bus=busCoupling.process(mixed,outputCouplingTau[2],internalFs);
        // One global processor, after the voice bus and before expression/
        // output. The carrier uses the physical matched upper-BJT steering
        // law; signal-pair nonlinearity/loading, mismatch and wet gain remain
        // unidentified. Direct PRA/Overall wet output uses
        // 10uF into10k (Guide differs): retain that physical RC at tau=.1s.
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
        ringPhase=wrap(ringPhase+ringFrequency*dt);
        // Keep this free-running passive network connected even at mix0.
        ringCoupledCarrier=ringCarrierCoupling.process(ringCarrier);
        // No mean removal or gain normalization: the nominal DC mismatch
        // permits signal feedthrough and even carrier harmonics. This is a
        // 25C ideal-source/zero-IC-DC-load and zero-incremental-input-current
        // limit, not fitted hardware gain (collector/bias loads also omitted).
        ringSwitchGain=ringSteering(ringDifferentialDc+ringSourcePeakVolts*ringCoupledCarrier);
        const double wet=ringWetCoupling.process(bus*ringSwitchGain,.1,internalFs);
        // Explicit bypass avoids even a rounding change for old patches.
        const double ringed=patch.ringMix==0 ? bus : bus+patch.ringMix*(wet-bus);
        const double rail=.9*std::tanh(ringed*.18*patch.volume/.9);
        // Unloaded HIGH output 33uF/5260ohm and documented inverting preamp.
        // Expression assumes an ideal fully conducting CdS; no recording EQ.
        return -lineCoupling.process(rail,outputCouplingTau[3],internalFs);
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
        for (auto& voice : impl_->voices)
            if (voice.filter.stage == Envelope::Idle) voice.filter.voltage = validated.filterInitial;
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
        : selected->amp.stage==Envelope::Idle || selected->amp.voltage==0;
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
    impl_->keysOff(); impl_->ringEnvelope={}; impl_->ringWetCoupling={};
    impl_->ringCarrierCoupling.clear();impl_->ringCoupledCarrier=0;
    impl_->ringSwitchGain=ringSteering(ringDifferentialDc);
    for (auto& voice : impl_->voices) {
        voice.held = voice.pedalHeld = false;
        voice.amp = {};
        voice.filter = {};
        voice.filter.voltage = impl_->patch.filterInitial;
        voice.filters = {}; voice.filteredCoupling={}; voice.commonCoupling={};
    }
    impl_->decimator.clear();
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
float Synth::process() {
    ++impl_->hostSamples;
    for (unsigned i = 0; i < impl_->rateFactor; ++i) impl_->decimator.push(impl_->sample(),impl_->rateFactor);
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
        impl_->ringCarrier,impl_->physicalKeyPresses,impl_->ringCoupledCarrier,impl_->ringSwitchGain,impl_->ringDepthValue};
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
    const double initial=filter ? patch.filterInitial : 0;
    const double peak=filter ? patch.filterPeak : 1;
    const double sustained=filter ? 0 : patch.ampSustain;
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
    std::vector<OscillatorFrame> result(frames);
    double pwmPhase=0;
    for (auto& frame : result) {
        const double pwm=std::sin(2*pi*(pwmPhase+patch.pwmPhase));
        frame=oscillator.process(frequency,patch.pulseWidth+patch.pwmDepth*pwm,
            patch.resetMicroseconds*1e-6,fs);
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
    std::string canonical = name;
    if (canonical == "strings_1" || canonical == "strings1" || canonical == "string_1") canonical="string1";
    if (canonical == "strings_2" || canonical == "strings2" || canonical == "string_2") canonical="string2";
    if (canonical == "brass_1") canonical="brass1";
    if (canonical == "brass_2") canonical="brass2";
    if (canonical == "electric-piano") canonical="electric_piano";
    // Preserve the Guide transcription above. These separate corrections
    // require agreement between the original T-board table and its resistor
    // matrix; see research/preset-cv-errata.json. Voltages remain ideal
    // divider values, not measurements of the loaded preset buses.
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
        Patch p; p.name=canonical;
        // Guide p.49 calibrates actual PWM VC1 to 0.2/6.7/222 Hz at
        // 0/5/10 V. An exponential through the endpoints predicts 6.663 Hz
        // at 5 V, consistent with the rounded middle value. Applying this
        // law directly to the raw factory CV assumes its preset-bus transfer;
        // preset loading/offsets remain uncalibrated, as for cutoff CVs.
        p.pwmRate=.2*std::pow(1110.0,cv[0]/10); p.pwmDepth=.4*cv[1]/10;
        p.pulseWidth=.5+.4*cv[2]/10;
        p.pulse=cv[3]/10; p.saw=cv[4]/10; p.noise=cv[5]/10;
        // The exponential voltage law controls variable gm, not the fixed
        // feed resistor. These 12/180Hz anchors and .7/.65oct/V slopes are
        // still uncalibrated gm-component estimates. The Patch API expects
        // total integrator Hz, so include the resistor contribution once.
        p.hpCutoff=filterFrequencyFloor+12*std::exp2(cv[6]*.7);
        p.lpCutoff=filterFrequencyFloor+180*std::exp2(cv[8]*.65);
        // Overall schematic: passive FH/FL buses feed each cutoff via 39k;
        // the shared EG uses 220k for HP and 47k for LP. Relative coupling
        // is Rmanual/REG times the manual CV slope. Those slopes (.7/.65
        // oct/V) remain provisional; this corrects the supported resistor
        // ratio without claiming absolute pin-4 CV calibration. The ratio is
        // relative to actual loaded FH/FL voltage, not proven to equal the raw
        // preset table: T71/T72, diode drops and four-voice loading remain
        // unresolved. Factory scales are an effective/manual-slope estimate.
        p.hpEnvelopeScale=.7*39/220;
        p.lpEnvelopeScale=.65*39/47;
        p.hpResonance=(10-cv[7])*.1; p.lpResonance=(10-cv[9])*.1;
        p.filterInitial=-cv[10]*.5; p.filterPeak=cv[11]*.5;
        // Service-manual endpoints: attack 1ms..1s; decay/release 10ms..10s.
        // The related CS-20M/CS-40M explanation of this IG00159 circuit
        // defines durations as 5*RC. Convert D/R to the engineering API's
        // T60, and set attack's target to reach the nominal peak after 5*RC.
        // This is a related-model inference, not a CS-50 timing calibration:
        // the published A versus D/R decade and shared TC remain unresolved.
        const auto attackTime = [](double v) { return std::pow(10,-.3*v); };
        const auto decayTime = [](double v) { return 10*std::pow(10,-.3*v)*std::log(1000.0)/5; };
        p.filterAttackHeadroom=p.ampAttackHeadroom=1/std::expm1(5.0);
        p.filterAttack=attackTime(cv[12]); p.filterDecay=decayTime(cv[13]); p.filterRelease=decayTime(cv[14]);
        p.filterLevel=cv[15]/10; p.sine=cv[16]/10;
        p.ampAttack=attackTime(cv[17]); p.ampDecay=decayTime(cv[18]); p.ampSustain=cv[19]/10;
        // The final preset column drives common M-board VCA LEVEL, before
        // the amp EG/bus/ring. PN2 master VOLUME is a separate later control.
        p.ampRelease=decayTime(cv[20]); p.ampLevel=cv[21]/10; p.volume=.55;
        return p;
    }
    throw std::invalid_argument("Unknown Ceres preset: " + name);
}
#endif // !CERES_EMBEDDED_TARGET
} // namespace ceres
