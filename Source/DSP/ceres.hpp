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
    // Audio noise keeps the legacy4x in-band density at a fixed host rate:
    // its source amplitude scales sqrt(internalFactor/4). SUB noise uses
    // its separate RC/RMS convention and the unchanged raw random sample.
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
    double ampAttack = 0.01, ampDecay = 0.35, ampSustain = 0.8, ampRelease = 0.25;
    double ampAttackHeadroom = 0.01;
    double ampAttackSaturation = 0.0;
    // Shared minimum release T60 in seconds [0,30]. Each effective release
    // is max(its selected amp/filter release, globalRelease); held-note
    // attack/decay and the release targets are unchanged. Zero adds no
    // extension and preserves existing patches. This is the ideal CS-50
    // minimum-CV selector expressed through a common monotone timing law,
    // not a calibrated panel position or literal SHORT-end voltage. Diode,
    // transistor and source-loading offsets remain unmodeled. Changes via
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
    // filterLfo is LP octave depth. HP gets .3374358974 times that depth:
    // (.7/.65)*(47k/150k), independent of user-editable EG coupling scales.
    double lfoRate = 5.0, vibratoCents = 0.0, tremolo = 0.0, filterLfo = 0.0;
    // Shared SUB source: 0 sine, 1 down saw, 2 up saw, 3 square, 4 noise.
    // Noise uses the sourced22k/.1uF low-pass; level is uncalibrated and
    // independent of rate/audio Noise mix. The free-running PWM sine source is separate and is unaffected.
    double lfoWaveform = 0.0;
    // One global balanced ring multiplier after the voice bus. Base carrier
    // Hz, positive frequency-envelope excursion in octaves, initial-to-peak
    // attack seconds and decay T60. Engineering mapping, not panel calibration.
    // Total carrier is bounded .2..220Hz, so rate/depth interact at the top.
    // Trigger once per physical all-keys-up -> any-key-down transition; final
    // key-up uses a fixed nominal .46781044s T60 return, independent of MIDI
    // pedal/globalRelease. Retrigger retains capacitor charge, not phase reset.
    // mix=0 preserves the previous signal path exactly; carrier phase is free.
    // Live digital Depth edits interpolate over20ms without restarting the EG
    // or carrier. Startup/reset precharge the saved value. This dezipper is a
    // DAW-control policy, not a measured component or physical timing law.
    double ringMix = 0.0, ringRate = 30.0, ringDepth = 0.0;
    double ringAttack = 0.01, ringDecay = 0.35;
    double pressureVolume = 0.0, pressureFilter = 0.0, pressureVibrato = 0.0;
    // Modern optional expression routing: 0=smoothed channel aftertouch
    // (legacy/default), 1=each board's latched note-on velocity. The three
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
    // selected expression gain. Leave zero when using pressureVolume alone
    // for velocity response; otherwise both gains intentionally apply.
    double velocitySensitivity = 0.0;
    // Effective differential-pair input scaling, uncalibrated. 1 is restrained.
    double drive = 1.0;
    // Nominal one-shot time from Yamaha's Normal calibration: 523.2 Hz/V,
    // 40 kohm FT branch, 800 uA reference. Not the bare CT resistor*C.
    // Actual chip current and trim tolerances remain unmeasured.
    double resetMicroseconds = 1e6 / (523.2 * 40e3 * 800e-6);
};

// Keys use snake_case (e.g. lp_cutoff, filter_initial, amp_attack). Finite values
// are range-limited; false means an unknown key or a nonfinite input.
bool setParameter(Patch&, const char* key, double value);
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
        // ringCoupledCarrier is pin7-pin8 after the passive carrier network.
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
// Ideal source, four100uF caps, zero incremental IC current; returns pin7-pin8.
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
