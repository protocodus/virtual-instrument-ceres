# Filter and envelope capacitor continuity

19 October 2026. Source and code audit against research DSP `ceres.cpp` SHA-256 `7b0bfc9b9ff2377390ca43ebc4491bf804d53e83ebcff273dfa20cbcfd780306`. No DSP edits, audio renders, recording measurements or fits were performed for this audit. The two choices below complete missing runtime state behavior while retaining the existing settled control mappings. They do not identify new chip transfer laws.

## Findings and primary evidence

**Choice 1: preserve the Q-control capacitors when either stage's resonance anchor changes.** The existing engine smooths the common `resonanceOffset` through the documented pole, but applies `hpResonance` and `lpResonance` immediately. Thus changing a stage slider or recalling a preset bypasses those same capacitors.

The [CS-50 small-sheet service manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%201%20of%202%20%28%208.5%20x%2011%20-%20Double%20Sided%20%29.pdf), PDF8/printed14, R7, shows each Q node driven from its QH or QL source and common RES through separate **220 kΩ** resistors. The [large-sheet service manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf), PDF12/printed16, M board, shows **10 µF** from each IG00156 VQ pin7 to ground. Q-adjust trim pin6 is separate. There are four HP and four LP capacitors in the physical instrument.

With stiff source voltages and zero pin7 input current, one stage obeys

```
C dq/dt = (P - q)/R + (RES - q)/R
q_target = (P + RES)/2
tau = C * (R || R) = 10 uF * 110 kohm = 1.1 s.
```

This is the limit already used by `qControlTau` and `qVoltageOffset`, not an exact measured hardware pole. Finite preset Thevenin resistance changes the common-mode pole to `C/(1/(R+4*Rth)+1/R)` even at zero pin current and a stiff actual RES voltage. The earlier Flute example gives approximately 1.161/1.172 s. Finite RES impedance couples the stages; unknown pin7 incremental current, offsets and voice mismatches remain unresolved. The reviewed earlier notes `global-resonance-loading.md`, `resonance-control-dynamics.md` and `q-control-loading-followup.md` retain those limitations. No new absolute VQ-to-Q mapping follows from the present finding.

**Choice 2: preserve filter-envelope capacitor charge through a live IL change after the software envelope reaches Idle.** The engine already preserves the voltage on ordinary retriggers and finite Release. Two software paths bypass it: `Envelope::advance` assigns `voltage=initial` in Idle, and live `Synth::setPatch` immediately assigns every idle filter envelope the new IL.

The same M-board sheet shows the external **0.22 µF** envelope capacitor between the IG00151 VCA output and IG00152 buffer input; the buffered output returns through the feedback network. The [Yamaha IC Guide](https://bama.edebris.com/download/yamaha/ic_guide/Yamaha%20IC%20Guide%20Book.pdf), PDF33/printed2-3, shows IL/SL/AL switched into LC and AT/DT/RT switched into TC. A dedicated capacitor-reset short is not drawn. The [CS Series Servicing Guide](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf), printed43–44/PDF52–53, retains the IL and RT path through Gate3 and Gate3-2 while no key is down, and describes key-off return timing as controlled by RT.

There is a material wording ambiguity: printed43 also describes the no-key VCA transconductance as maximum. The schematic and sequence do not supply a separate numerical post-settling time constant or a drawn zero-impedance reset. **Finite charge continuity is source-backed; using the existing normalized release law after a live idle IL change is the minimal continuation of the current model, not a newly calibrated physical idle rate.** The original prose ambiguity must remain documented.

## Minimal implementation contract

For Q, preserve the old common-offset arithmetic and add two smoothed anchor coordinates:

```
a = -expm1(-1/(fs_internal*1.1))
qVoltageOffset += (patch.resonanceOffset*.5 - qVoltageOffset)*a
hpAnchor += (patch.hpResonance - hpAnchor)*a
lpAnchor += (patch.lpResonance - lpAnchor)*a
effectiveHp = clamp(hpAnchor - .1*qVoltageOffset, 0, 1)
effectiveLp = clamp(lpAnchor - .1*qVoltageOffset, 0, 1)
```

The three computational states are a linear decomposition of **two absolute stage capacitor coordinates** into two anchor terms and one shared contribution under identical symmetric loading assumptions. They do not mean three physical capacitors. Identical voices share those stage coordinates in the present reduced model. Do not halve the existing effective anchor again, remap raw preset CV, add another pole to the already smoothed common term, or change the provisional damping law.

When both anchors stay fixed at their initialized values, their update is exactly zero and the old common-offset arithmetic can remain byte-identical. A runtime anchor change updates only its target, not the state.

For Idle EG, retain its stored voltage and advance toward the current IL using the existing release coefficient:

```
E_next = IL + (E_previous - IL)*cR
cR = exp(-ln(1000)/(fs_internal*effectiveReleaseT60))
```

Keeping stage `Idle` during this return preserves existing voice accounting. `on()` then starts the existing attack from retained `E`, as it already does from Release. The existing `1e-7` terminal snap may remain a documented numerical endpoint policy. No new rate curve, saturation parameter or current-limit fit is introduced. The amplifier Idle target remains zero, so its settled behavior stays zero.

Initialization and lifecycle requirements:

1. Before the first internally processed sample, `setPatch` may precharge configured Q anchors and idle filter IL. This preserves the established offline convention of starting a newly configured device in its chosen settled state. Keep the historical common-offset initialization rather than silently precharging it differently.
2. Once the internal clock has advanced, live controls and preset recall preserve Q and EG charge. Silent processing counts; a later no-voices state must not reopen the initialization exception. Note allocation and retrigger do not reset Q.
3. Explicit `reset()` reinitializes to the current patch's anchor/IL convention and reopens initial configuration. It is not a model of switching a preset on a continuously powered instrument.
4. Preserve the current explicit panic behavior: `allSoundOff()` clears audio and envelope state to the current IL, while Q states survive. Panic must not reset the clock flag and thereby permit the next patch change to erase Q charge.
5. No public parameter, preset value, GUI, key allocation, cutoff law, gain or timing calibration changes are required.

## Audible potential without a recording fit

`analytic_examples.py` performs only standard-library constant arithmetic and file hashing. Its `analytic-examples.json` binds the reviewed core, primary PDFs, visually reviewed crops and relevant earlier audits. Reproduce with `python3 Docs/accuracy/filter-eg-capacitor-continuity/analytic_examples.py --check` from the repository root. These are conditional model calculations, not measured sound or a claim about an individual instrument's component tolerance.

For a normal LP resonance change **0.2 → 0.8**, the 1.1 s state reaches 0.226662 at 50 ms and 0.5 at 762.46 ms. Using the existing linearized circuit at a 1 kHz bare integrator rate—including the physical 150 kΩ source, 33 kΩ shunt, 1.2 MΩ/180 pF feedback and 1 µF source capacitor—the response at 1 kHz is **9.03 dB below an instantaneous final-Q change after 50 ms**, and 5.83 dB below it after 762 ms. This is a quasistatic small-signal transfer example; it does not include transient ringing, OTA saturation or actual audio. Its scale establishes plausible audible potential at a normal control setting. A low-level held note or sine near the filter peak can test the transition directly, with the same final response after settling.

The transfer used by the independent constant calculation is the earlier pin-node derivation:

```
Ys(s) = 1/(150k + 1/(s*1uF)); Yg = 1/33k; G = 1/1.2M; C = 180pF
D0 = 1/150k + Yg + G; N = (2-1.8*r)*D0/2; D(s) = Ys(s)+Yg+G
P(s) = C*s^3 + (D+2*a*C)*s^2 + (2*a*N+C*a*a)*s + D*a*a
H_LP(s) = 2*Ys(s)*a*a/P(s); a = 2*pi*1000; s = j*2*pi*1000.
```

For the EG, begin at settled IL=0, change to **IL=−5 V** while idle with an effective release **T60=1 s**, and play a note **50 ms** later. The existing release law retains **−1.460271 V**, whereas the present snap gives −5 V: a 3.539729 V difference at note-on. In the existing effective HP/LP envelope coordinates, that is 0.439248/1.909194 octaves of variable-gm cutoff difference; the LP variable-gm component differs by a factor **3.755993**. The fixed 22.575 Hz integrator-rate floor remains additive. This difference can affect the following attack; no audible output is claimed while all amplitude envelopes are silent. A subsequent attack can remove it rapidly, so the test must inspect the first tens of milliseconds as well as settled output.

## Verification that distinguishes the change from a new calibration

Use deterministic native state checks and controlled musical demonstrations, not another recording-fit grid:

- Q: check both independent anchor step responses at several elapsed times and sample rates against the closed-form 1.1 s response; a second mid-transition edit must start from the retained state. Check the old common-offset response with static anchors remains exact. Preset recall and panic must preserve the states as declared, while reset must reinitialize them.
- EG: check a fully idle live IL change for finite state evolution with the existing release coefficient; immediate and delayed retriggers must begin from the retained value. Exercise silent processing, a second edit during return, pre-clock setup, reset and explicit panic separately. Keep the software Idle bookkeeping behavior stable.
- Audio: compare a held low-level tone with one normal Q edit and a short note after a normal idle IL edit; retain gain and all other parameters. Verify finite, bounded output and identical long-settled targets. The unmodified static configuration and old deterministic renderer controls should remain exact where the changed runtime branches are never used.

The audible examples are not a universal RMS bound or a claim of perfect hardware accuracy. The Q current law, its output conductance and saturation, op-amp bandwidth, physical signal-voltage scale and the idle EG rate ambiguity remain unknown. Existing 180 pF feedback, two 1.5 nF integrators with 4.7 MΩ feed paths, 1 µF source capacitors, nonlinear OTA integration and finite active EG returns are already modeled. They must not be re-added. A tiny pin4 pole, arbitrary Q-current saturation or another bias/decay fit would not address these concrete runtime state bypasses.
