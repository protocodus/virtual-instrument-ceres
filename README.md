# Ceres

Ceres is a sample-free Yamaha CS-50 research synthesizer with a reusable C++17 circuit engine and configurable 1–16 voice polyphony. This repository also contains the existing JUCE VST3/AU/CLAP/Standalone scaffold: that plug-in still uses its separate silent placeholder engine and is **not yet connected** to the circuit synthesizer. The Reason Rack Extension consumes the shared DSP through a pinned submodule in its separate adapter repository.

![Ceres editor scaffold](Docs/screenshots/ceres.png)

## Audio demos

The CI-generated demos below use the **standalone circuit engine's existing 14 research presets**, including Init, without preset edits, gain normalization or external audio. They do not demonstrate the still-silent JUCE plug-in, which has no instrument preset bank yet. The source phrases are original committed MIDI, not recovered hardware performances; names inherited from the research table do not establish calibrated Yamaha preset matches.

<!-- audio-demos-begin -->

| # | Preset | Demo | Length | Peak |
| --- | --- | --- | --- | --- |
| 1 | init | [01-init.mp3](Docs/audio/01-init.mp3) | 12.0 s | -9.5 dBFS |
| 2 | string1 | [02-string1.mp3](Docs/audio/02-string1.mp3) | 12.0 s | -8.6 dBFS |
| 3 | string2 | [03-string2.mp3](Docs/audio/03-string2.mp3) | 12.0 s | -13.6 dBFS |
| 4 | brass1 | [04-brass1.mp3](Docs/audio/04-brass1.mp3) | 12.0 s | -11.9 dBFS |
| 5 | brass2 | [05-brass2.mp3](Docs/audio/05-brass2.mp3) | 12.0 s | -13.8 dBFS |
| 6 | flute | [06-flute.mp3](Docs/audio/06-flute.mp3) | 12.0 s | -8.5 dBFS |
| 7 | electric_piano | [07-electric-piano.mp3](Docs/audio/07-electric-piano.mp3) | 12.0 s | -6.8 dBFS |
| 8 | clavichord | [08-clavichord.mp3](Docs/audio/08-clavichord.mp3) | 12.0 s | -7.3 dBFS |
| 9 | harpsichord | [09-harpsichord.mp3](Docs/audio/09-harpsichord.mp3) | 12.0 s | -9.3 dBFS |
| 10 | guitar1 | [10-guitar1.mp3](Docs/audio/10-guitar1.mp3) | 12.0 s | -9.1 dBFS |
| 11 | guitar2 | [11-guitar2.mp3](Docs/audio/11-guitar2.mp3) | 12.0 s | -12.6 dBFS |
| 12 | funky1 | [12-funky1.mp3](Docs/audio/12-funky1.mp3) | 12.0 s | -9.1 dBFS |
| 13 | funky2 | [13-funky2.mp3](Docs/audio/13-funky2.mp3) | 12.0 s | -6.5 dBFS |
| 14 | funky3 | [14-funky3.mp3](Docs/audio/14-funky3.mp3) | 12.0 s | -11.1 dBFS |

<!-- audio-demos-end -->

## Version history

### Unreleased — 0.5.0 — component authenticity

- The ring modulator now multiplies by a sine instead of switching a square: the 33k sits in series between the carrier's 100 µF and the multiplier, as the PRA schematic draws it, so the µA796 stays in its linear region. You hear the two sum and difference tones without square-wave sidebands, and slow SPEED settings are no longer up to 38 dB weaker. Some dry signal leaks through, about 5 dB below each sideband, as a typical µA796 does. The wet path now loses lows below about 20 Hz. Wet loudness at full MODULATION is unchanged.
- The output stage no longer squashes loud chords. It is now the PRA board's linear preamp, clipping hard at rails derived from its parts, about 12 dB above the 0 dBm reference. At high VOLUME, chords keep their attack peaks (up to 6 dB more on loud presets); at the default VOLUME 0.55 presets change by at most about 0.5 dB. Full VOLUME with touch LEVEL and tremolo can now exceed digital full scale; there is no limiter, so set the host level with Output Trim.
- The amplitude envelope now sits 2 % of its peak below its target, as the M board is aligned (TP2 at −200 mV with sustain at zero). Releases and zero-sustain decays end in true silence after about four time constants, and a full-sustain note holds at 98 % of its attack peak.
- Factory release CVs now pass the sustain-control transistor follower, as the release levers do on the CS-50, so every preset's release is about 1.7× longer (Brass 1's amp release T60 goes from 0.021 s to 0.035 s). Manual patches are unchanged.
- Factory filter initial levels follow the M-board alignment (−4 V at full IL instead of −5 V), so presets start their filter sweep 20 % less far down.
- Deep pulse-width modulation on upper notes follows the WSC comparator: the pulse cannot get narrower than the oscillator's reset time, so very thin PWM above about 1.7 kHz widens slightly instead of thinning to a needle. The top octave above 5.9 kHz keeps the real reset timing instead of a software clamp.
- New board tolerance controls (`board_pitch_tolerance`, `board_filter_tolerance`, `board_envelope_tolerance`, all off by default) let the four voices differ the way calibrated CS-50 boards do: a cent or two off pitch, most at the bottom of the keyboard; slightly more or less filter response to envelope, SUB, touch and BRILLIANCE; envelope depth within ±10 % and amplitude envelope level within ±2 %. Each seed is one fixed instrument. Existing patches sound exactly as before.
- Front ends get the hardware's panel lever laws: SUB SPEED runs exponentially from 0.8 to 50 Hz, full VCO lever is ±25 cents of vibrato, the VCF lever sags at mid-travel, BRILLIANCE is a B-10K lever from −15 V to +15 V about a grounded centre detent, and RESONANCE runs +10 V at MIN to 0 V at MAX. Saved parameters keep their meaning.
- Research for each component is in `Docs/research/*-authenticity-2026-10-20.md`.

### Unreleased — 0.4.0 — circuit details

- The noise source is now hard-limited, as the CS-50's limiter amplifier makes it: a binary ±1 signal at the waveforms' peak level instead of uniform random values. Audio NOISE is 4.8 dB louder at the same lever setting; its spectrum is unchanged and SUB-N keeps its level. Patches using NOISE change sound.
- The limiter now has a bandwidth, estimated at 14 kHz from the pick-up op-amp's datasheet gain-bandwidth, so NOISE has the same level at every host sample rate. At 48 kHz it is another 4.7 dB louder (9.5 dB above 0.3.0), at 96 kHz 7.7 dB louder. The bandwidth is an estimate; the level moves 3 dB per octave of error in it.
- SUB-N, the noise SUB waveform, now has the hardware's level: the ±15 V limiter through its 22k/0.1µF filter and the 100k load of the SUB buffer. It is about 6.7 dB deeper than before and a little brighter (corner 88 Hz, was 72 Hz). The level rests on an estimated limiter bandwidth from the op-amp's datasheet.
- Tremolo (`tremolo`, the SUB VCA lever) now swings the common VCA around LEVEL by the R7 resistor ratio, ±0.34 of full LEVEL at full lever, instead of dipping up to the whole LEVEL below it. Tremolo patches become shallower and no longer lose average loudness.
- Channel pressure now follows the touch pre-amplifier's 10 ms RC (100k and 0.1 µF), twice the previous 5 ms, so touch swells are slightly softer. Velocity smoothing is unchanged.
- The noise now passes the pick-up amplifier's input coupling (0.047 µF into 100k, a 34 Hz high-pass) before the limiter. NOISE loses most of its sub-bass rumble (about 11 dB less at DC) and SUB-N its slowest wander, which also makes SUB-N 1.3 dB shallower: 5.4 dB deeper than 0.3.0 rather than 6.7 dB.
- The noise limiter now slews: at the op-amp's datasheet 1 V/µs it needs 30 µs to cross between its rails, so short noise spikes turn back before reaching them. NOISE gains about 3.5 dB below 3 kHz while its top falls (−1 dB at 10 kHz, −4 dB at 15 kHz), a fuller, less fizzy hiss, and SUB-N is 4.3 dB deeper (0.77 RMS). Both rest on datasheet-typical slew rate and gain-bandwidth; at 1.7 V/µs the gain would be about 2.5 dB.
- Keys pressed together are stored highest first, as the key coder encodes them: a five-note chord whose notes arrive in the same sample keeps its four highest notes in any MIDI order, instead of the first four received. Sequenced chords of five or more notes keep their top line.
- Portamento now uses the key assigner's own three integrator legs instead of a time constant tied to the glide rate: 109 ms below 1.6 steps/s, 13.6 ms below 43 steps/s, and the direct gate above that, where portamento steps like glissando. At the default `glide_rate` of 24 the glide trails its staircase by a third of a semitone rather than a whole one, so steps are more distinct and the glide arrives sooner. The two thresholds rest on an assumed log-linear counter clock between the printed 11 s and 1.4 ms endpoints.
- The factory presets now reach the engine through the tone-preset boards' diodes. Every preset CV leaves the T71/T72 matrix through a 1S1555, which the guide's printed table does not include, so each pin sits 4.4–5.8 % low. Presets are about 0.5 dB quieter, 12–20 % darker, a little slower in attack and slightly more resonant where the table asked for none. Manual patches are unchanged. The diode's curve is not published, so a 1N4148-class model stands in for it.
- Adds the expression pedal (`expression`), the PRA photo-coupler rather than a volume control. Closing it hands the signal to the 68k–2.2k/0.33 µF leg and the 0.0033 µF at the volume track, so the sound does not just get quieter: fully closed, 65 Hz sits 4.8 dB and 30 Hz 9.7 dB above 1 kHz while 10 kHz falls 6.4 dB below it. Level follows the guide's printed pedal curve, 0 to −40 dB over about 28 degrees. The default of 1 is the hardware's own no-pedal state and bypasses the network exactly, so existing patches are unchanged.
- The two linear panel levers now load their own tracks, as the pots do. `tremolo` is SUB VCA lever travel, which delivers 7.7 % less of the SUB at mid-lever, and `ring_mix` is the MODULATION lever's crossfade, which puts the whole mix 2.35 dB down at mid-lever instead of blending flat. Both ends of each lever are unchanged, so patches at 0 or 1 sound as before. The RESONANCE and BRILLIANCE levers are C-taper pots whose curve is not published, and those controls are defined as bus voltages, so they keep no taper.

### Unreleased — 0.3.0 — hardware realism

- Adds the CS-50's PORTAMENTO/GLISSANDO (`glide_mode`, `glide_rate`): a semitone counter, plus an Hz-domain integrator for portamento, on the key voltage shared by the oscillator and both filters. In Sustain II a released note keeps gliding. Off by default; existing patches are unchanged.
- A repeated key now retriggers its own released voice, as the key assigner does, instead of starting an idle one.
- Touch BRILLIANCE now moves the high-pass a third as far as the low-pass, following the hardware resistor matrix. Touch vibrato is centred slightly above pitch, as Yamaha trimmed it. A new `pressure_wah` adds the hardware's touch-controlled SUB filter route.
- Touch LEVEL (`pressure_volume`) now adds to LEVEL at the common VCA, as the hardware resistor matrix sums them: full touch at full depth adds 1.08 times full `amp_level`, and no touch leaves `amp_level` as set. Patches using it get louder with touch rather than quieter without it.
- A key pressed while every voice is held is ignored, as on the CS-50, instead of stealing the oldest voice. Released and damper-held voices are still reused.
- Patches with nonzero `pressure_filter`, `pressure_vibrato` or `pressure_volume`, or that play more notes than voices, change sound. See the [research write-up](Docs/research/hardware-realism-2026-10-20.md) and the [previews](Docs/audio/previews/2026-10-20-hardware-realism/).

### Unreleased — 0.2.2 — waveform mixing

- Corrects the relative pulse audio polarity so equal-level saw/pulse blends reinforce their fundamentals, following a public hardware-recording comparison. Shared oscillator phase and PWM timing remain unchanged.
- Adds independent mixture regressions and broader rapid-note/envelope checks. The recording supports relative mixing balance; absolute phase, full pulse-width behavior and complete hardware calibration remain unresolved. See the [recording study](Docs/accuracy/waveform-mixing-20261019/) and [polarity validation](Docs/validation/waveform-polarity/).
- Changes mixed-source tone and level intentionally; the Rack bank requires new output-trim calibration.

### Unreleased — 0.2.1 — rapid-note continuity

- Smooths velocity expression when an audible voice is reused, avoiding an instantaneous gain or filter-expression jump. Fresh silent notes retain their immediate velocity response.
- Keeps overlapping repeated notes attached to their original key presses after voice stealing, so an older stolen note's key-up cannot release its newer replacement.
- Retains free oscillator phase, envelope and filter capacitor state, keyboard pitch changes and the existing ring model.

### Unreleased — 0.2.0 — configurable polyphony and processing quality

- Adds 1–16 preallocated voices and actual 1×, 2× or 4× internal processing. The standalone default remains four voices at 4×.
- Smooths live Ring Depth edits over 20 ms without restarting the carrier or envelope; saved/static depths start immediately at their configured value.
- Keeps audio-noise density consistent across 1×/2×/4× at the same host sample rate, preserving the original 4× level and the separate SUB-noise path.
- Keeps the existing four-voice, 4× audio byte-identical in the recorded parent comparison. Configuration changes require an unclocked/reset engine; adapters can defer them until idle and fade residual tails.
- Retains nominal 31-host-sample output latency at all qualities. Lower quality trades oversampling headroom and alias rejection for CPU cost; it is not the same sound under a new label.

### Unreleased — 0.1.0 — circuit engine and existing plug-in scaffold

- Adds the tested four-voice oscillator/filter/envelope/ring model and embedded interface alongside the existing plug-in scaffold.
- The README demo renderer now exercises the existing circuit research presets with fixed original MIDI; the separate JUCE plug-in remains unchanged.
- Retains the source-timed VCO conditioning, Q/idle-envelope capacitor continuity and physically scaled ring switching described in the [audible-circuit review](Docs/audible-circuits/README.md). The hardware-recording comparison remains mixed; this is not a verified full hardware match.

### Unreleased — 0.0.1 — scaffold

- Repository, silent plug-in front end, test tiers and CI set up to the portfolio standard.

## Downloads

<!-- build-download-begin -->
**[Download main build 22](https://github.com/protocodus/virtual-instrument-ceres/actions/runs/39504191462/artifacts/12303755099)**, built from [`0f1d00f096f9`](https://github.com/protocodus/virtual-instrument-ceres/commit/0f1d00f096f968cf4ca027304123e28cf0f9aaf9). It holds the macOS, Windows and Linux packages, `SHA256SUMS` and `BUILD.txt`.
<!-- build-download-end -->

---

## How it works

`Source/DSP/ceres.cpp` and `ceres.hpp` implement the standalone circuit engine and its sample-by-sample API. `Source/DSP/CeresEngine.*` remains the distinct silent placeholder used by `Source/Plugin*`; this merge does not silently change the plug-in's sound, controls or saved-state contract. The native and embedded circuit libraries do not depend on JUCE, samples, hardware recordings or manuals at runtime.

The original product version 0.1.0 imported the canonical v0.10.0 research model and subsequent documented circuit changes. It is a reduced circuit model with provisional control calibration, not a transistor-exact recreation. No existing historical validation report is relabelled as a validation of this merged build.

### Use the standalone core


Compile `Source/DSP/ceres.cpp` and include `Source/DSP/ceres.hpp`. For a Rack target define **`CERES_EMBEDDED_TARGET=1` consistently for the core and all consumers** and use C++17. The macro changes `Patch`'s layout, so native and embedded objects must never be mixed across an ABI boundary.

```cpp
#include "ceres.hpp"

// Construct outside the audio callback, once per device/sample-rate lifetime.
ceres::Synth synth(48000.0, 50);
ceres::Patch patch;
ceres::setParameter(patch, "lp_cutoff", 6000.0);
synth.setPatch(patch);
synth.noteOn(60, 1.0);
// Dispatch events at their actual frame before calling process().
for (unsigned frame = 0; frame < 64; ++frame)
    output[frame] = synth.process();
```

The embedded interface has a numeric `Patch`, a C-string parameter setter, no exception path, and no allocating offline probes/preset APIs. Construction allocates the synth state. Rendering, controller changes, validated patch updates, panic, reset and configuration reuse existing state. Do not construct or replace a synth inside `RenderBatch`.

The output is mono. All configured voices and their capacitor states continue running when no note is held. `activeVoices()==0` does **not** imply silence: the common output bus has a roughly 0.888-second time constant. `allNotesOff()` honors the MIDI pedal; `allSoundOff()` clears audio states while preserving controllers; `reset()` also restores seeded oscillator/controller state. The FIR has nominal group delay 31 host samples, with the decimator's sampling phase affecting event alignment.

### Polyphony and processing quality

`configure(voices, factor)` accepts 1–16 voices and factors 1, 2 or 4. It returns `false` without mutation for invalid values, after processing has started, or when notes are already assigned before the first sample. A request equal to the current configuration is a no-op. `polyphony()` and `oversamplingFactor()` report the applied configuration; the old static constants describe the four-voice, 4× defaults.

Configure before sending notes, or explicitly reset first, then reapply controllers. A changed configuration resets capacitor, oscillator and controller state while retaining the patch. A live adapter should wait for physical keys, pedal and active envelopes to be released, fade the residual tail, reset/configure, restore patch/controllers and fade back in. Panic alone does not reopen configuration. This API does not silently cut held notes or allocate new voices on the audio thread.

The 4× path retains its original 249-tap FIR. The 2× path has a 125-tap FIR with the same nominal group delay; 1× uses an exact 31-sample delay and no redundant low-pass. Final-of-factor sampling retains the legacy fractional decimation phase, so the modes do not promise identical transients. All oscillators, filters, envelopes and circuit states actually run at the selected internal rate. The existing frequency guards operate at that rate, so lower qualities can also limit extreme settings earlier.

Additional voices are a virtual extension of the four-board model. The calibrated common bus and per-voice gain remain unchanged; no voice-count normalization or limiter is added. Larger chords can increase rail saturation and output level. Extra voices use a separate initialization stream; at the default four-voice setting the original shared noise/drift stream remains exact. See [configuration validation](Docs/validation/poly-quality/README.md).

Audio noise is the hard-limited "white noise of constant amplitude" the guide describes (printed p.32), trimmed to the same 3 Vpp as the waveforms. It is the limiter's band-limited input driven into the ±15 V rails, with the bandwidth estimated at 14 kHz from the pick-up op-amp's datasheet gain-bandwidth and the limiter's slew rate at the datasheet's 1 V/µs, simulated at 768 kHz or more so its density is the same at every quality and host sample rate. SUB noise filters the same signal. Different quality settings can still differ through filter discretization, nonlinear processing and alias rejection; this is not hardware gain calibration. See [noise-quality validation](Docs/validation/noise-quality/README.md).

The native renderer accepts `--voices 1..16 --oversampling 1|2|4`, with defaults `4` and `4`. Host adapters may choose and save different defaults independently of the 60 hardware/engineering patch keys.

### Ring, SUB and SUSTAIN controls

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `ring_mix` | 0..1 | 0 | Global dry/wet ring blend |
| `ring_rate` | 0.2..220 Hz | 30 | Base free-running carrier frequency |
| `ring_depth` | 0..8 octaves | 0 | Positive carrier-frequency envelope excursion |
| `ring_attack` | 0.001..10 s | 0.01 | Frequency-envelope time to peak |
| `ring_decay` | 0.01..10 s T60 | 0.35 | Frequency-envelope decay toward the base rate |
| `lfo_waveform` | 0..4 | 0 | Sine, Down, Up, Square, Noise |
| `sustain_mode` | 1..2 | 1 | I holds released key voltage; II follows the last assigned key |

Live `ring_depth` changes interpolate over 20 ms in the digital control domain. This dezipper preserves the existing exponential frequency law and 220 Hz cap; it is not a newly inferred analog time constant. Startup/reset use the saved depth immediately. Repeated notifications at the same target do not prolong the transition, and a reversal starts from the current interpolated value. The change removes the immediate frequency-control step in a synthetic test; it does not prove the exact cause of a reported host crackle.

The ring envelope triggers when the first physical key is pressed after all keys have been released. Additional held keys do not retrigger it, including a key that finds no free voice. Final key-up returns its frequency envelope toward zero through a fixed nominal release; the carrier phase keeps running. The total carrier is bounded at 220 Hz, so SPEED and DEPTH interact near that limit. This is a circuit-informed balanced-multiplier model with provisional envelope and wet-gain calibration.

The selected SUB waveform drives VCO, VCF, VCA and pressure vibrato. Noise uses the documented 22k/0.1µF low-pass and a fixed engineering level; it is independent of SUB rate and audio Noise mix. PWM retains its separate sine oscillator. No external modulation input is implemented.

In Sustain II, released/free boards follow the last assigned key in both oscillator pitch and filter tracking. Their envelope charge, gates, oscillator phase and filter state remain continuous. Entering II applies this immediately; returning to I captures the current key voltage. MIDI-damper-held boards remain protected until pedal-up, an explicit software adaptation rather than recovered Yamaha allocation behavior. This selector is distinct from `global_release` and MIDI damper gating.

The ring/SUB/Sustain additions preserved the earlier audio path in their recorded same-build regression. The subsequent unequal SUB filter route intentionally changes nonzero `filter_lfo` patches. Controls and circuit uncertainties are detailed in [DSP_SOURCE.md](Docs/DSP_SOURCE.md).

### Global sound-path controls

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `brilliance` | −15..+15 V | 0 | Loaded shared BR-node offset; different HP/LP cutoff gains |
| `resonance_offset` | −10..+10 V | 0 | Shared RES-node voltage change; positive lowers Q, with sourced 1.1-second RC response |
| `amp_level` | 0..1 | 1 | Common filtered+sine VCA level before amp EG, bus and ring |
| `output_trim_db` | −24..+24 dB | 0 | Utility gain after all circuit processing and FIR, for saved preset calibration |

The first two controls use an explicitly anchored stiff-source model, not recovered passive-pot positions. The signed resonance offset is an engineering delta around existing effective Q settings; the actual hardware wiper is 0..10 V. Its capacitor state survives note/preset changes and audio panic. SUB filter depth remains the LP depth; HP now gets approximately 0.3374 times as much modulation. Common VCA level adds to the existing SUB-amplitude control and closes at zero, rather than multiplying two gain controls. Factory LEVEL now drives that common VCA instead of final master volume.

Neutral offsets, common level 1 and output trim 0 preserve the prior paths when the intentionally corrected SUB filter route is inactive. The output trim is a modern linear utility, not part of the analog model; it adds no limiter and cannot promise a fixed peak under arbitrary MIDI. The [source audit](Docs/sound-path-gap-audit.md) distinguishes known resistor/capacitor relationships from provisional control calibration.

### Portamento, glissando and the key assigner

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `glide_mode` | 0..2 | 0 | Off, Portamento, Glissando |
| `glide_rate` | 1..200 semitones/s | 24 | Glissando counter clock |

The CS-50 builds each board's key voltage in the KAS board (Servicing Guide printed pp.22–25). The assigner's key code passes a glissando counter, an integrating gate and a sample-hold, then reaches the VCO and both filters. Ceres follows that order. The counter steps one semitone at a time toward the board's assigner key at `glide_rate`. Glissando sends each step straight on. Portamento integrates the stepped key voltage with a time constant of one step period, which matches the guide's statement that the integrating rate is matched to the counter rate. The key voltage is linear in Hz, so the glide approaches its target exponentially in Hz rather than in pitch.

Free channels carry the last-pressed key, so a newly assigned board glides from the previous key. While its key is down, or in Sustain II, the sample-hold follows the glide. In Sustain I it freezes the pitch at key-off. The guide's 470 kΩ/3.3 MΩ/1 µF integrator and the comparator table fix the topology but not the clock: the counter oscillator's control law is not printed, so `glide_rate` is an engineering rate, not a calibrated lever.

The assigner only writes a new key into a channel whose code differs from that key and whose key is up (guide pp.17–18). A released board that still holds the pressed key is therefore re-keyed first, then an idle board, then the quietest released board, then the oldest damper-held board (its key is up). A key pressed while every board's key is down is not stored (guide p.19 timing chart): it does not sound, and it does not start later when a board frees. Keys pressed together are encoded higher note first (guide p.16), so a key arriving in the same sample as a lower stored key takes that key's board: a simultaneous five-note chord keeps its four highest notes, whatever order the MIDI events arrive in.

### Velocity expression

`expression_source` selects **0 = channel aftertouch** (the legacy default) or **1 = note-on velocity**. Values clamp to 0..1 and round to the nearest integer. The existing `pressure_volume`, `pressure_filter` and `pressure_vibrato` depths then use the selected source. This optional velocity mode is a modern DAW mapping, not a claim about the original hardware.

Velocity is latched independently for each allocated voice and stays with its release tail, including during Sustain II pitch following. A new note assignment or repeated-note retrigger stores the incoming velocity only on that board. When the board is still audible, its consumed velocity approaches the new target using the existing 5 ms digital expression RC; silent and zero-envelope assignments precharge immediately. This avoids a gain/filter-expression step without adding keyboard glide or resetting oscillator, envelope or filter state. Channel pressure has no audible effect in velocity mode; switching back uses its continuously smoothed history. Live source changes preserve oscillator/envelope/filter state and may change the expression gain immediately.

Each physical note press also retains a FIFO identity, including a press that found no free board. Its eventual key-up consumes that press alone; it never releases a board another press owns. These velocity and MIDI ownership policies are software adaptations, not recovered CS-50 keyboard circuitry. See [the rapid-note validation](Docs/validation/note-reuse/README.md).

Touch LEVEL adds to the common VCA gain: `amp_level + 1.08*pressure_volume*expression`. R7 sums LEVEL (10 V full, 270k) and touch LEVEL (calibrated to 6.0 V full, 150k) into the same node, so (6/150k)/(10/270k) = 1.08 (service manual 1 p.14; guide p.55). The VCA's headroom above full LEVEL is not documented, so full touch on a full-LEVEL patch may drive the output rail harder. Filter expression is touch BRILLIANCE: `expression*pressure_filter` octaves on the low-pass variable conductance above its fixed resistor floor, and 0.3374 times that on the high-pass. The R7 matrix feeds TBR through 47k to LP and 150k to HP, the same split as SUB. `pressure_wah` scales the selected SUB waveform by touch into the same two nodes: `expression*SUB*pressure_wah` LP octaves, the hardware's level amplifier II → R7 VCF route. Vibrato adds `expression*pressure_vibrato*(SUB + 1/3)` cents. Yamaha trims level amplifier I so the 3 Vpp SUB leaves centred on +0.5 V (guide p.56), so touch vibrato bends slightly upward on average. The 1/3 offset assumes that centring scales with the touch gain. The separate native `velocity_sensitivity` gain remains multiplicative with that sum. All native research presets retain source 0. Hosts should default missing saved properties to 0 to preserve old patches, and explicitly save 1 for velocity-oriented patches.


## Known gaps

- The JUCE plug-in remains a silent placeholder with a plain editor; connecting the actual instrument and controls is separate work. Its source, identity codes, release helpers and shared portfolio tools are preserved.
- The plug-in still has no preset bank. The audio tier checks the separately identified circuit research presets; passing it does not establish audible JUCE plug-in functionality.
- Factory control-voltage calibration, source impedances, some chip loading/nonlinearities and absolute ring wet gain remain uncertain. See [DSP source and model limits](Docs/DSP_SOURCE.md) and the [audible circuit evidence](Docs/audible-circuits/README.md).
- Historical research reports bind their original sources and may reference external working directories or source recordings. They are retained provenance, not portable current-CI results.
- No external modulation input is implemented. Optional velocity and MIDI-pedal behavior are explicitly modern adaptations.
- The portamento/glissando clock rate and the noise limiter's bandwidth and slew rate are not calibrated.

## Build

CMake 3.22+, a C++17/20 compiler and Python3 are required for the full unit suite. The circuit targets explicitly compile as C++17; the existing JUCE scaffold uses C++20. Build the standalone DSP without JUCE:

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp -L '^unit$' --output-on-failure
./build-dsp/ceres_render --events Examples/init.events.tsv --preset init \
  --rate 48000 --frames 288000 --seed 50 --output /tmp/ceres-init.wav
```

`ceres` and `ceres_embedded` are the circuit libraries. `ceres_render` writes mono float32 WAV without normalization; sorted event rows are `frame type note value`. It supports note-on/off, bend, pressure, pedal, all-notes-off, panic and reset. `--patch path` reads key=value overrides and `--build-info` reports the exact compiled sources including the merged CMake file. The native research preset API is excluded from the embedded ABI.

The 12 circuit checks cover oscillator/filter/envelope/ring contracts, expression, all 60 keys with zero post-construction allocations, and native/embedded parity. They are named `Ceres.<Suite>` and labelled `unit` alongside the existing scaffold unit tests. Test executables are resolved by CMake target paths for single- and multi-configuration generators. The `audio`, `render` and `fuzz` tiers remain separate; the unchanged audio contract now renders the actual circuit preset list twice.

```sh
ctest --test-dir build-dsp -L '^audio$' --output-on-failure
./build-dsp/RenderCeresPresetDemos --jobs 4 Docs/audio/midi build-dsp/demo-audio
```

The demo tool uses the shared fixed 48 kHz/256-frame/seed protocol and the original [MIDI phrase definitions](Docs/audio/midi/README.md). It duplicates the mono engine into two identical channels without gain changes. Every 12 s raw render must be finite, audible, below 0 dBFS and identical on repetition. MP3 export uses ffmpeg; no captured sound or preset-specific level adjustment is involved. Historical empty-bank failure evidence remains frozen in the [initial integration report](Docs/validation/main-integration-2026-10-19/report.json); the subsequent [demo integration validation](Docs/validation/research-demos-2026-10-19/report.json) records the resolved audio path and its new build identity.

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_BUILD_PLUGIN=OFF -DCERES_SANITIZE=ON
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize -L '^unit$' --output-on-failure
```

`CERES_SANITIZE` is a compatibility alias for the existing `CERES_ENABLE_SANITIZERS`; either instruments development targets with ASan/UBSan. `CERES_BUILD_TOOLS=OFF` excludes both renderer tools without dropping core tests. `CERES_BUILD_LIBFUZZER` retains the existing scaffold fuzz target.

The separate plug-in build fetches pinned JUCE 8.0.14 and clap-juce-extensions unless `CERES_JUCE_PATH` and `CERES_CLAP_JUCE_EXTENSIONS_PATH` point to local checkouts. `CERES_BUILD_CLAP` controls the CLAP wrapper; on macOS `CERES_BUILD_UNIVERSAL` defaults to ON. Linux dependencies are listed in `.github/workflows/ci.yml`.

```sh
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=ON -DBUILD_TESTING=ON
cmake --build build-plugin --parallel
ctest --test-dir build-plugin -L '^unit$' --output-on-failure
# Full suite also includes circuit audio, editor rendering and fuzz tiers:
ctest --test-dir build-plugin --output-on-failure
```

On multi-configuration generators add `--config Release` to the build and `-C Release` to CTest; command-line executables are then in the corresponding configuration directory. `scripts/build-macos.sh` retains the existing universal build and full-suite behavior.

## Licensing

Ceres's source is available under the [PolyForm Noncommercial License
1.0.0](LICENSE): you may read, study, change and share it for any non-commercial
purpose, but not sell it or use it commercially. Official Protocodus builds
carry an additional permission in the same file to use them for any purpose,
including making music commercially. Commits made before this licence change
were published under MIT and remain so. The plug-in builds with JUCE (official
builds use a commercial JUCE 8 Starter licence) and clap-juce-extensions; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No Reason/Jukebox SDK,
third-party hardware capture, firmware or ROM data is part of the standalone
runtime.
