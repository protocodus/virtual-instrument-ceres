#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#if !defined(CERES_EMBEDDED_TARGET)
#include <array>
#include <string>
#include <vector>
#endif

// Ceres: experimental, reduced-circuit Yamaha CS-50 model. No sample playback,
// third-party synthesizer code, or claim of a calibrated transistor-level clone.
namespace ceres {

struct Patch {
    // Metadata belongs to the native/offline adapter, not realtime state.
#if !defined(CERES_EMBEDDED_TARGET)
    std::string name = "init";
#endif
    // Audio noise is the limiter source (guide p.32): the pick-up noise,
    // band-limited at an estimated 14kHz after its 33.86Hz input coupling,
    // clipped to the rails with an estimated 1V/us slew. Its density is the
    // same at every quality and host rate. SUB noise filters the same sample.
    double saw = 1.0, pulse = 0.0, sine = 0.0, noise = 0.0;
    // PW is the comparator's logical timing window (50..90%), modulated by
    // PWM with its fixed cycle edge retained. The completed audio pulse has
    // the opposite sign to that logical coordinate: public solo/both-on
    // CS-50 recordings support this relative saw/pulse audio calibration.
    // It changes neither edge time nor shared phase and does not identify an
    // active-low hardware stage; absolute circuit phase remains uncalibrated.
    double pulseWidth = 0.5, pwmDepth = 0.0, pwmRate = 1.0;
    // Continuous sine-LFO phase offset in cycles, applied immediately by
    // setPatch. The free-running PWM clock is never reset by note events.
    double pwmPhase = 0.0;
    // Effective total small-signal integrator frequencies in Hz at middle C
    // (MIDI 60). Hardware keyboard CV tracks pitch; the 4.7Mohm feed paths
    // impose a ~22.58 Hz minimum even when requested cutoff is lower. The
    // feedback RC moves the actual resonant poles, particularly at high fc.
    // Key/EG modulation scales only the variable gm component above this
    // fixed floor. Values at/below the floor mean zero variable gm.
    double hpCutoff = 30.0, lpCutoff = 6000.0;
    double hpResonance = 0.0, lpResonance = 0.0, keyTrack = 1.0;
    // Actual loaded BR-bus voltage relative to the calibrated neutral, V.
    // The 39k/(680k HP,220k LP) network uses provisional .7/.65 oct/V
    // manual slopes; it is not the position/taper of the passive panel pot.
    double brilliance = 0.0; // [-15,15]
    // Change of loaded RES-bus voltage around the existing effective Q
    // calibration, V. Positive lowers Q. The signed [-10,10] engineering
    // offset does not imply every value is reachable from an unknown real
    // 0..10V wiper baseline. Stiff preset/source, zero chip input-current
    // limit: each pin7 has 220k to each source and 10uF to ground, giving
    // deltaVq -> offset/2 with tau1.1s. This incremental control is
    // smoothed together with the independent hp/lp_resonance anchor terms;
    // live edits preserve both stage capacitor coordinates.
    double resonanceOffset = 0.0;
    // Separate envelope coupling in octaves per EG unit. Defaults retain
    // direct octave-valued engineering patches. Factory EG units are volts;
    // its scales use documented resistor ratios and provisional manual slopes.
    double hpEnvelopeScale = 1.0, lpEnvelopeScale = 1.0;
    // VCF EG: negative IL -> positive AL -> fixed zero sustain -> IL.
    double filterInitial = -2.0, filterPeak = 2.0; // EG units; octaves at scale 1
    double filterAttack = 0.01, filterDecay = 0.35, filterRelease = 0.2;
    // Excess attack target as a fraction of (peak - initial). Unmeasured:
    // .01 preserves the original engineering-patch estimate; large values
    // approach a linear ramp. Factory patches infer five-RC timing instead.
    // Attack time still means full initial-to-peak duration.
    double filterAttackHeadroom = 0.01;
    // Optional reduced VCA current-limit experiment. 0 keeps the RC law;
    // positive .001..10 sets Vs relative to (peak - initial). Uncalibrated.
    double filterAttackSaturation = 0.0;
    // Engineering decay/release times mean T60; attack means time to peak.
    // Factory voltage conversion is separate and provisional. See ceres.cpp.
    // The VCA EG aims .02 of its peak below S while held and below zero
    // after release (the aligned VR17 floor); the VCA closes below zero.
    double ampAttack = 0.01, ampDecay = 0.35, ampSustain = 0.8, ampRelease = 0.25;
    double ampAttackHeadroom = 0.01;
    double ampAttackSaturation = 0.0;
    // Shared minimum release T60 in seconds [0,30]. Each effective release
    // is max(its selected amp/filter release, globalRelease); held-note
    // attack/decay and the release targets are unchanged. Zero adds no
    // extension and preserves existing patches. This is the ideal CS-50
    // minimum-CV selector expressed through a common monotone timing law,
    // not a calibrated panel position or literal SHORT-end voltage. Both
    // times are taken at the EG pin; the lever-side follower drop belongs to
    // the factory CV conversion (ceres.cpp preset()). Changes via
    // setPatch affect active releases without resetting capacitor state.
    double globalRelease = 0.0;
    // I retains released-board key CV; II lets released/free boards follow
    // newly assigned keys without retriggering envelopes or oscillator phase.
    // Entering II immediately adopts the last assigned key on free boards.
    // MIDI-pedal-held boards remain protected (software damper adaptation).
    double sustainMode = 1.0;
    // VCF LEVEL affects the filtered branch. VCA LEVEL affects the common
    // filtered+sine signal before amp EG/bus/ring; volume is final master.
    // SUB AM subtracts an additive normalized control current from ampLevel,
    // with gain closure at zero; the inherited AM/DC scale is uncalibrated.
    double filterLevel = 1.0, ampLevel = 1.0, volume = 0.55;
    // EXPRESSION pedal, 1 fully open (no pedal, the hardware's own default)
    // down to 0 fully closed. It is the PRA photo-coupler between MI and MO,
    // not a gain: closing it leaves the 68k-2.2k/0.33uF leg and the 0.0033uF
    // at MO in charge, which lifts the bottom and rolls off the top. 1
    // bypasses the network exactly. The pedal's travel-to-level law is the
    // guide's printed curve, read off the graph; see ceres.cpp.
    double expression = 1.0;
    // Utility calibration gain AFTER the complete circuit and FIR. This is
    // not an original Yamaha control and cannot change internal drive/state.
    // Zero bypasses the gain exactly; [-24,+24] dB, with no limiter/AGC.
    double outputTrimDb = 0.0;
    // TRANSPOSE changes the oscillator FT path, not filter keyboard voltage.
    double octave = 0.0;
    // Master PITCH: nominal -500..+700 cents. Changes VCO pitch and the
    // variable-gm contribution of both filters via the shared keyboard CV;
    // this master offset is independent of the engineering keyTrack slope.
    double tuneCents = 0.0;
    // These modeled oscillator variations, like vibrato below, are VCO-only.
    double voiceSpreadCents = 0.0, driftCents = 0.0;
    // Board-to-board tolerance of the four M boards, each 0..1 of the
    // documented spread; 0 (default) keeps the boards identical. Each seed is
    // one fixed instrument: its offsets never move while it plays.
    // Pitch: the residual the service tuning trims leave (+/-1 cent at each
    // trim point, Guide printed p.65). Filter: the IG00156 rank-bin residual
    // of each filter's control sensitivity (+/-.146 oct per octave of Vfc
    // input, SM2 printed pp.17-18 rank table; the static cutoff and Q are
    // trimmed by ear and stay identical). Envelope: the VCF EG (+/-10%) and
    // VCA EG (+/-2%) output-level check bands of the sibling CS-60 alignment
    // (CS-60 SM printed pp.37-38). See ceres.cpp.
    double boardPitchTolerance = 0.0, boardFilterTolerance = 0.0, boardEnvelopeTolerance = 0.0;
    // filterLfo is LP octave depth. HP gets .3374358974 times that depth:
    // (.7/.65)*(47k/150k), independent of user-editable EG coupling scales.
    // tremolo is SUB VCA lever travel, 0..1: R7 adds .3375 of the wiper's
    // share of SUB to LEVEL, and the B-10k lever into its 30k load delivers
    // x/(1+x(1-x)/3) of it, 7.7% low at mid-travel with exact endpoints.
    // lfoRate is Hz, wider than the hardware's .8-50 Hz SPEED range;
    // vibratoCents is cents per SUB unit (hardware full lever 25), filterLfo
    // LP octaves per SUB unit (full lever .809). subSpeedLeverHz and its
    // siblings below map the PN3 levers onto these. The one IG00150 clock
    // free-runs: note events never reset its phase.
    double lfoRate = 5.0, vibratoCents = 0.0, tremolo = 0.0, filterLfo = 0.0;
    // Shared SUB source: 0 sine, 1 down saw, 2 up saw, 3 square, 4 noise.
    // Noise uses the sourced22k/.1uF low-pass loaded by SUI's100k on the
    // +/-15V limiter: an estimated .77 RMS (limiter bandwidth and slew rate
    // from datasheet typicals, after the 33.86Hz pick-up coupling), within +/-8.2, independent of rate/audio Noise mix. The free-running PWM sine source is separate and is unaffected.
    double lfoWaveform = 0.0;
    // One global balanced ring multiplier after the voice bus. Base carrier
    // Hz, positive frequency-envelope excursion in octaves, initial-to-peak
    // attack seconds and decay T60. These are the quantities at the carrier;
    // the panel levers map onto them through the laws below the struct
    // (ringSpeedHz, ringDepthOctavesPerLever, ringAttackSeconds,
    // ringDecaySeconds). Attack/decay stay seconds: the PRA envelope is the
    // M-board VCA-EG chip pair, so its lever law is that EG's, offset by the
    // PRA's own trim (31ms attack at AT=10V, CS-60 PRA alignment). Total
    // carrier is bounded .2..220Hz (guide p.53's printed range), so rate/depth
    // interact at the top.
    // Trigger once per physical all-keys-up -> any-key-down transition; final
    // key-up uses the fixed 2DT's 14.50s T60 return (ringReleaseVolts),
    // independent of MIDI pedal/globalRelease. Retrigger retains capacitor
    // charge, not phase reset.
    // mix=0 preserves the previous signal path exactly; carrier phase is free.
    // mix is MODULATION lever travel: the B-10k crossfades dry against wet and
    // its own track loads the 8.04k it works into, so the whole mix sits
    // 2.35dB low at mid-lever. Both ends stay exact. The load assumes the
    // expression pedal open; closing it straightens the taper (not modelled).
    // Live digital Depth edits interpolate over20ms without restarting the EG
    // or carrier. Startup/reset precharge the saved value. This dezipper is a
    // DAW-control policy, not a measured component or physical timing law.
    double ringMix = 0.0, ringRate = 30.0, ringDepth = 0.0;
    double ringAttack = 0.01, ringDecay = 0.35;
    // Touch response (CS-50 panel 2, guide printed pp.55-56). pressureFilter is
    // touch BRILLIANCE in LP octaves; R7 feeds TBR through 47k LP / 150k HP, so
    // HP moves .3374358974 times as far, as for SUB. pressureVibrato is SUB
    // vibrato cents per unit touch; level amplifier I is trimmed to centre the
    // 3Vpp SUB on +.5V, so touch vibrato rises by a third of its depth.
    // pressureWah is touch-scaled SUB filter depth (level amplifier II -> R7
    // VCF pin70), LP octaves with the same .3374 HP ratio. pressureVolume is
    // touch LEVEL: R7 adds its current to LEVEL's at common VCA2, so full depth
    // and full touch add 1.08 times full amp_level; zero touch leaves amp_level.
    // Uncalibrated filter and vibrato depths.
    double pressureVolume = 0.0, pressureFilter = 0.0, pressureVibrato = 0.0;
    double pressureWah = 0.0;
    // PORTAMENTO/GLISSANDO (panel 3; guide printed pp.23-25, KAS YM26700):
    // 0 off, 1 portamento, 2 glissando. A per-board counter steps the key code
    // one semitone at a time toward its assigner key at glideRate semitones/s.
    // Glissando sends the steps to the sample-hold; portamento integrates the
    // stepped key voltage, which is linear in Hz, through the gate the lever's
    // comparators select: 108.9ms below 1.58 steps/s, 13.6ms below 43.2, and
    // the direct gate above that, where portamento steps like glissando. Those
    // two thresholds assume a log-linear counter clock, since the lever's
    // PS-voltage law is not printed; glideRate is an engineering rate.
    double glideMode = 0.0, glideRate = 24.0;
    // Modern optional expression routing: 0=channel aftertouch (default),
    // through the touch pre-amplifier's 10ms RC, 1=each board's latched
    // note-on velocity. The three
    // pressure* depths above consume this selected value. Velocity remains
    // unchanged through release, pedal and Sustain II pitch-following; only
    // assigning/retriggering a board replaces its target. On an audible board,
    // the consumed velocity follows that target with a 5ms digital controller
    // RC to avoid a gain/CV step. Silent/zero-envelope assignments precharge
    // immediately; keyboard pitch, physical EGs and phase are never slewed or
    // reset by this adaptation. The same value feeds velocitySensitivity.
    // Switching sources is live
    // without regating/resetting circuit state. This is not a CS-50 circuit.
    double expressionSource = 0.0;
    // Independent legacy velocity-to-gain depth, multiplicative with the
    // common VCA gain including touch LEVEL; both apply when both are set.
    double velocitySensitivity = 0.0;
    // Effective differential-pair input scaling, uncalibrated. 1 is restrained.
    double drive = 1.0;
    // Nominal one-shot time from Yamaha's Normal calibration: 523.2 Hz/V,
    // 40 kohm FT branch, 800 uA reference. Not the bare CT resistor*C.
    // Actual chip current and trim tolerances remain unmeasured.
    double resetMicroseconds = 1e6 / (523.2 * 40e3 * 800e-6);
};

// PRA ring lever laws (header-only, both targets). SPEED is TVR4, B-10k from
// +10V to ground, wiper to VC1; the CS-60 PRA alignment, the same board
// (SM2 p.22 notes the CS-60 differences on it), trims the IG00150 to a
// 150+/-10ms period at VC1=5V and 5+/-1ms at 10V (CS-60 service manual
// p.39, section VI PRA II), i.e. 6.667Hz and 200Hz; the IG00150 pin sheet
// (SM2 p.9) also states 200Hz at VC1=10V, VC2=0. Exponential through those:
// .9814 oct/V, .222Hz at 0V (the guide's p.53 range says .2Hz).
constexpr double ringSpeedReferenceHz = 1000.0 / 150.0;
constexpr double ringSpeedOctavesPerVolt = 0.98137811912170374; // log2(30)/5
// DEPTH is TVR3, B-10k from the envelope output EO to ground, wiper to VC2.
// VC1 enters the VCO's pin-1 node through 100k and VC2 through 180k, so VC2
// moves the carrier 100/180 as far per volt as VC1, whatever that node's
// trimmed impedance. EO peaks at 10+/-2V (CS-60 PRA alignment I; the same
// IG00159/IG00151 pair as the M-board VCA EG, whose peak aligns to 10+/-.2V).
// Full lever at the envelope peak therefore lifts the carrier 5.45 octaves;
// ring_depth in octaves = travel*ringDepthOctavesPerLever, the lever's own
// sag into 180k (at most 1.3% mid-travel) aside.
constexpr double ringDepthOctavesPerVolt = ringSpeedOctavesPerVolt * 100.0 / 180.0;
constexpr double ringEnvelopePeakVolts = 10.0;
constexpr double ringDepthOctavesPerLever = ringDepthOctavesPerVolt * ringEnvelopePeakVolts;
// Carrier Hz for a VC1 voltage (0..10V is the SPEED lever's travel times 10).
double ringSpeedHz(double vc1Volts);
// ATTACK TIME and DECAY TIME are TVR1 and TVR2, B-10k from +10V to ground,
// wipers to the PRA's AT and DT pins (SM2 p.26, PN2); 10V is the SHORT end
// (CS-60 manual p.39). IC7 IG00159 with IC6 IG00151 into .22uF is the M-board
// VCA-EG pair (SM2 p.16), whose pin sheets give attack 10^(-.3V)s to the peak
// and decay 10*10^(-.3V)s as five-RC durations (SM2 p.21); the CS-60 M-board
// alignment confirms .3 decade/V (4ms at 8V, 125ms at 3V, manual p.38).
// The PRA EG is aligned 31 times slower: CS-60 PRA alignment I (sibling
// evidence, the same board; manual p.39) puts the EO peak 31ms after key-on
// at AT=DT=10V, where the pin sheet gives 1ms. Every segment drives the one
// TC->EI exponential converter, so the factor applies to all of them.
// ASSUMPTION: the PRA's fixed 100k span into EI gives the M board's trimmed
// .3 decade/V (the M board's span is 82k+VR20, B-50K, so .246...396 decade/V
// is possible). The fixed release follows this law: CHOSEN BY EAR over the
// untrimmed pin-sheet law (owner listening test ring-release, 2026-10-20).
// Over 10..0V attack runs 31ms..31s and decay .428..428s T60; ring_attack and
// ring_decay stop at 10s (AT below 1.64V, DT below 5.44V) and also allow
// times shorter than the hardware's 31ms and .428s.
constexpr double ringEnvelopeTrim = 0.031 / 0.001;
// Attack seconds, key-on to peak, for an AT pin voltage (0..10V).
double ringAttackSeconds(double atVolts);
// Decay T60 seconds toward the grounded SL for a DT pin voltage (0..10V).
double ringDecaySeconds(double dtVolts);
// 2DT (release) is fixed by 6.8k from +15V and 3.3k to ground (SM2 p.22):
// every final key-up returns over ringDecaySeconds(4.901V) = 14.50s T60.
constexpr double ringReleaseVolts = 15.0 * 3.3 / (6.8 + 3.3);

// Keys use snake_case (e.g. lp_cutoff, filter_initial, amp_attack). Finite values
// are range-limited; false means an unknown key or a nonfinite input.
bool setParameter(Patch&, const char* key, double value);

// SUB-OSCILLATOR panel laws (PN3) for adapters that offer the hardware levers.
// The Patch keeps its engineering meanings (Hz, cents and LP octaves per SUB
// unit); these map a lever onto them. Travel is 0..1 and clamped. See ceres.cpp.
// SPEED: VC2 terminal volts 0..10 -> lfo_rate Hz, .8 Hz at 0 V to 50 Hz at 10 V.
double subSpeedHz(double vc2Volts);
// SPEED lever travel -> lfo_rate Hz, including the lever's 180k load.
double subSpeedLeverHz(double travel);
// VCO lever travel -> vibrato_cents; 25 cents at full lever.
double subVibratoLeverCents(double travel);
// VCF lever travel -> filter_lfo (LP octaves per SUB unit).
double subFilterLeverOctaves(double travel);
// Panel 2 lever laws, for front ends that show the hardware levers. They do
// not change what `brilliance` and `resonance_offset` mean: both stay loaded
// bus volts. travel is lever travel, 0 at MIN and 1 at MAX, clamped; a
// nonfinite input returns NaN, which setParameter rejects. Both levers are
// B-10K slide pots (SM1 parts p.9, and the CS-60 parts list p.9 under the
// same part numbers, marked common to the CS-50; the CS-50 PN2 sheet's
// "C-100K" is the outlier). See Docs/research/brilliance-resonance-
// authenticity-2026-10-20.md.
// BRILLIANCE TVR7, centre-click type: -15 V at MIN, +15 V at MAX, grounded
// centre tap, wiper loaded by 4x680k + 4x220k into the cutoff nodes.
// Returns `brilliance` volts.
double brillianceLeverVolts(double travel);
// RESONANCE TVR6: +10 V at MIN, ground at MAX. In steady state the eight
// pin-7 networks (220k to the preset Q bus, 220k to RES, 10 uF) load the
// wiper with 55k returning to the mean of the HP and LP preset Q buses,
// presetMeanVolts. Returns the loaded RES bus voltage, from which a front end
// takes its own neutral to form `resonance_offset`.
double resonanceLeverVolts(double travel, double presetMeanVolts);
#if !defined(CERES_EMBEDDED_TARGET)
bool setParameter(Patch&, const std::string& key, double value);
Patch preset(const std::string& name);
std::vector<std::string> presetNames();
#endif

struct OscillatorFrame { double saw, pulse, sine; };

class Synth {
public:
    explicit Synth(double sampleRate = 48000.0, std::uint64_t seed = 1);
    ~Synth();
    Synth(Synth&&) noexcept;
    Synth& operator=(Synth&&) noexcept;
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;
    // Before the first processed sample, configure settled Q/idle-IL state.
    // Live edits change targets while preserving those capacitor voltages.
    void setPatch(const Patch&);
    void noteOn(int note, double velocity = 1.0);
    void noteOff(int note);
    void pressure(double value);
    void pitchBend(double semitones);
    // MIDI damper adaptation: holds envelope gates until pedal-up. Distinct
    // from globalRelease and the CS-50 SUSTAIN I/II key-CV modes.
    void sustain(bool down);
    void allNotesOff();
    // Clears voices, output-coupling capacitors and FIR history; preserves
    // controllers, Q-control capacitor coordinates and clock-start history.
    void allSoundOff();
    // Restores seeded oscillator phases, controllers and configured capacitor
    // initial conditions; permits pre-clock configuration again.
    void reset();
    // Virtual voice-count / numerical-quality configuration, independent of
    // Patch and its hardware controls. Valid voices 1..16, factors 1/2/4.
    // Changing configuration is accepted only before any process() since
    // construction/reset, with no assigned/gated notes. It resets electrical
    // state and controllers; set the patch/controllers afterwards. A request
    // equal to the current configuration is always a harmless no-op.
    // Invalid/live requests return false, with no mutation or allocation.
    // Adapters should defer changes until idle and fade residual output tails
    // before reset/configure. activeVoices()==0 alone does not mean silence.
    bool configure(unsigned voices, unsigned factor) noexcept;
    unsigned polyphony() const noexcept;
    unsigned oversamplingFactor() const noexcept;
    float process();
    // Counts active amplitude envelopes, not residual output capacitor tails.
    // The common output bus has a .8883s time constant and keeps running at zero.
    std::size_t activeVoices() const;
    double sampleRate() const;
    static constexpr unsigned voiceCount = 4;
    static constexpr unsigned oversampling = 4;
    static constexpr unsigned maxVoiceCount = 16;
    // Default constants above retain source compatibility. Actual configuration
    // is available through the getters. Every quality has nominal group delay
    // 31 host samples: direct delay at1x, FIR125 at2x, original FIR249 at4x.
    // Final-of-factor decimation preserves the legacy fractional sampling phase;
    // qualities do not promise sample-identical transients or alias behavior.
    static constexpr unsigned outputLatency = 31;
#if !defined(CERES_EMBEDDED_TARGET)
    // Read-only native diagnostics; excluded from the embedded ABI. Values
    // expose modeled capacitor/CV state, not calibrated hardware voltages.
    struct VoiceState {
        int note, keyNote;
        bool held, pedalHeld;
        double ampEnvelope, filterEnvelope, oscillatorPhase;
        double oscillatorFrequency, hpFrequency, lpFrequency;
    };
    struct ModulationState {
        // ringCarrier is the unchanged raw free-running oscillator sine.
        // ringCoupledCarrier is pin7-pin8 after the passive carrier network,
        // per volt of source sine (1.6V peak in production, so about .00295).
        double sub, ringEnvelope, ringFrequency, ringCarrier;
        std::size_t physicalKeyPresses;
        double ringCoupledCarrier;
        double ringSwitchGain; // matched upper-pair steering, includes nominal DC bias
        double ringDepth; // actual interpolated digital Depth coordinate
    };
    struct SoundPathState {
        double resonanceVoltageOffset, hpResonance, lpResonance;
        double commonVcaGain;
        double hpResonanceAnchor, lpResonanceAnchor;
        bool hasProcessed;
    };
    std::array<VoiceState,voiceCount> voiceState() const;
    // Includes inactive preallocated slots. Only [0,polyphony()) is clocked.
    std::array<VoiceState,maxVoiceCount> allVoiceState() const;
    ModulationState modulationState() const;
    SoundPathState soundPathState() const;
#endif
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#if !defined(CERES_EMBEDDED_TARGET)
// Allocating measurement APIs belong to offline research/native tests only.
// Internal-rate impulse through the exact selected output stage. The returned
// host-rate stream uses production final-of-factor sampling and zero history.
std::vector<double> outputRateImpulse(unsigned factor, std::size_t hostFrames,
    unsigned inputSubframe = 0);
// Exact production passive carrier network, zero initial incremental charge.
// Ideal source, four 100uF caps, 33k in series to pin 7, the uA796 carrier
// port's rpi across the 100 ohm; returns pin7-pin8 per volt of source.
std::vector<double> ringCarrierResponse(double sampleRate,const std::vector<double>& input);
// Matched carrier-pair steering at25C, input in physical differential volts.
// No source amplitude/DC offset is added by this pointwise diagnostic.
double ringSwitchingResponse(double differentialVolts);
// Normalized output-coupling sections at their actual processing rate
// (4*hostRate in production), without the final line inversion or rail.
// Stages: 0=CP3 filtered branch, 1=CP4 common voice, 2=four-voice bus,
// 3=unloaded HIGH line output. These expose the conditional component model,
// not measured VCA input impedances or external recording-chain loading.
std::vector<double> outputCouplingImpulse(unsigned stage,double sampleRate,std::size_t frames);

// Exact PRA output preamp as the engine applies it: the summed, expressed
// signal times the .18 gain and VOLUME, hard-limited at the HA1452 rails
// derived from its supply droppers (see ceres.cpp). Before the HO coupling
// capacitor and the final line inversion.
double outputPreampResponse(double volts,double volume);

// Exposes the exact circuit used in the voices for independent measurements.
// Input is a physical Thevenin source-voltage impulse in the common normalized
// OTA voltage unit; no oscillator/VCA/decimator. This replaces v0.6's separately
// normalized canonical-u probe. Outputs use the same voltage unit as input.
// Both outputs default to the documented LP-stage 150k source/33k ground
// network, including its 1uF source coupling capacitor. Override conductance
// to probe the HP mixer: (2+enabledSaw+enabledPulse)/150k siemens. It must be
// finite and in (0,1] siemens. Reference conductance anchors the provisional
// nominal Q range and remains fixed when load changes. Voices use HP reference
// 3/150k and actual branch count, and LP reference/source 1/150k.
std::vector<double> filterImpulse(double sampleRate, double cutoff,
    double resonance, bool highPass, std::size_t frames,
    double inputAmplitude = 0.001, double sourceConductance = 1/150e3,
    double referenceSourceConductance = 1/150e3);

// Exact production mixer/HP/LP/relative-VCA-calibration chain, with no
// oscillator, envelope, output rail, output coupling or decimator. Input is the
// sum of enabled normalized waveform voltages, before the passive mixer.
// Branches=2+enabledSaw+enabledPulse; noise and grounded branches are fixed.
// Drive must be in the production patch range [.05,8].
std::vector<double> filterCascadeImpulse(double sampleRate,double hpCutoff,double lpCutoff,
    double hpResonance,double lpResonance,unsigned branches,std::size_t frames,
    double inputAmplitude = 0.001,double drive = 1);

struct EnvelopeGate { std::size_t frame; bool on; };
// Uses the same capacitor/sequence engine as voices. Amp output is normalized
// to peak 1; filter output uses the patch's initial/peak EG units, before the
// independent HP/LP envelope coupling scales.
// Includes globalRelease's shared minimum release T60, as in production.
// Gates are applied before the sample at each frame; output is after one dt.
std::vector<double> envelopeTrace(double sampleRate, const Patch&, bool filter,
    std::size_t frames, const std::vector<EnvelopeGate>& gates);

// Raw source generator, before voice filters/VCA/decimation. sampleRate is
// its actual processing rate (use 4*hostRate to match the synth core).
std::vector<OscillatorFrame> oscillatorTrace(double sampleRate, double frequency,
    const Patch&, std::size_t frames, double initialPhase = 0.0);

#endif // !CERES_EMBEDDED_TARGET

} // namespace ceres
