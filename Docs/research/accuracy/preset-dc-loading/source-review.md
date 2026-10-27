# Preset DC-loading hypothesis: source review and exact mapping

This is a **DC-intercept-only sensitivity experiment**, not a recovered CS50 calibration or complete dynamic bus model. It changes neutral HP/LP cutoff coordinates under one declared actual-bus calibration convention. The later audio comparison must keep oscillator, Q, envelopes, MIDI/gates, modulation, seed and recording gain fixed. In particular, it retains the existing own-EG scales; the full-network own/cross derivatives below are recorded but not applied. No production DSP or default is changed by this source audit.

## Primary components and unresolved assumptions

The T71 matrix in [CS50 large-sheet service manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf), PDF8/printed10, shows these selected series resistors, each followed by a diode into a shared bus with one10k return:

| Preset | HP/FH selected resistor | LP/FL selected resistor | Ideal unloaded HP/LP V, before rounding |
|---|---:|---:|---:|
| Flute |75k|9.1k|1.176470588 /5.235602094|
| Brass I |absent|7.5k|0 /5.714285714|

An absent branch is not a stiff0 V source: Brass HP retains the10k shared return and can receive signed backfeed from all four voice nodes. Exact resistor division is used in this experiment. Frozen native factory controls retain their existing rounded1.18/5.24 and0/5.71 entries; they are not silently corrected.

[Small-sheet manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%201%20of%202%20%28%208.5%20x%2011%20-%20Double%20Sided%20%29.pdf), PDF8/printed14 R7, gives per voice39k preset feeds, BR680k HP/220k LP, and three150k HP/47k LP modulation/touch feeds. The direct M sheet, large-sheet PDF12/printed16, gives rank shunts HP2.7/2.2/1.8k and LP3.0/2.4/2.0k for A/B/C and330k from each bias wiper. The centered B100k bias pot has nominal0 V and25k Thevenin resistance, giving355k total. Centered bias is a declared reference, not a measurement of the recorded instrument.

The retained HP EG220k/LP EG47k values follow the Overall drawing and current implementation. Direct-M connectivity and the related100k HP alternative remain unresolved; this conditional combination is not a newly established complete physical-board netlist. All other actual source voltages are stiff0 V. Chip input current, active loading, diode leakage, component tolerances and unequal voices are omitted. BR=0 and−15 V are the grounded-center and negative-rail endpoints, where the ideal center-tapped BR pot is stiff. Neither endpoint is recovered from a photographed slider.

## Diode states: why the grid uses .4/.6 V, not zero

The [Servicing Guide](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf), PDF72/printed63 (`presets-72.png`), shows the priority selector: unselected preset rails are **open**, not grounded. They must not be replaced by fictitious grounded preset-row clamp branches. With no leakage or supply, their outward-pointing diodes provide no DC source.

Inactive manual-panel pot tracks still reference ground. For the declared0 V inactive wipers, their input diodes remain off when `P > −Vd`. The fixed-drop hypotheses **Vd=.4 or.6 V** are not measured diode parameters. Every selected branch must simultaneously have positive `(10−Vd−P)/Rs` current. The helper checks both inequalities at the neutral point and all16 IL/AL corners of the four EGs, for both stages and all24 preset/scenario combinations:768 corner solves. The minimum manual-off margin is **.367600912 V**; all selected branches stay forward conducting.

A zero-drop model can make the Brass HP bus negative (even at neutralEG when BR=−15), activating the omitted manual paths. Their conducting impedance depends on unknown panel settings. The report therefore retains zero-drop diagnostics as **excluded from the audio grid**; it neither invents one zero-ohm clamp nor ignores a failed state assumption. Positive finite-drop margins make the selected .4/.6 grid self-consistent without selecting those unknown impedances.

## Coordinate convention and nodal equations

The current `.7/.65` slopes are provisional nominal-preset coordinates in earlier research. This experiment explicitly **reinterprets them as slopes per volt of stiff actual FH/FL**. It fixes the same12/180 Hz variable-gm anchors at actualFH/FL=0, EG=0, BR=0, other sources=0 and centered bias. These anchors and slopes remain assumptions; no audio parameter fit selects them.

For one stage, let `gp=1/39k`, `g0=1/10k`, `gs=1/Rs` (zero when absent), `b_i=gBR*BR+gEG*E_i+gBias*bias` and

```text
D = gp + 1/Rrank + gBR + 3*gmod + gEG + 1/355k
kappa = s_actual * D/gp                 # fixed for this stage/rank, never per preset
(gs + g0 + 4*gp)*P − gp*sum(u_i) = gs*(10−Vd)
D*u_i − gp*P = b_i
P = [gs*(10−Vd) + (gp/D)*sum(b_i)] /
    [gs+g0+4*gp−4*gp²/D]
u_i = (gp*P+b_i)/D
f_ref_i = f_fixed + anchor * 2^(kappa*u_i)
f_fixed = 1/(2π*4.7M*1.5nF) =22.575169232893 Hz
```

Bias current is included in the equations, but its chosen wiper voltage is0. It still loads each node through355k. A nonzero bias would backfeed the preset bus and change the intercept; the fixed anchors cannot identify that unknown hardware trim. Rank selection normally compensates chip differences: the rank-dependent `kappa` here preserves the same declared actual-bus sensitivity, rather than claiming one measured internal chip constant across ranks.

**Neutral means all four `E_i=0`, not the preset's negative initial level.** The negative initial/release level is applied later by the unchanged native EG. Baking it into cutoff here and applying the envelope again would double-count it. The separate pin2 key voltage and fixed audio conductance floor are also excluded from the pin4 DC exponent.

For context only, exact full-network EG derivatives are recorded. With `a=1/(39k+4Rth)` and `Dcommon=D−gp+a`, differential gain is `s_actual*39k/REG`, common gain is that differential gain times`D/Dcommon`, each cross gain is(common−differential)/4, and own gain is differential+cross. **The DC-only audio experiment does not apply these changed own/cross terms.** Its preserved envelopes therefore prevent a claim of complete dynamic circuit equivalence.

## Frozen mapping interface and controls

Run `python3 Docs/accuracy/preset-dc-loading/network.py` from any directory. It resolves the repository from its own path and prints [source-evidence.json](source-evidence.json); `--research-root` relocates cached primary sources. No audio or repository writes occur inside the helper. NumPy is required. Its source and all inspected inputs are SHA-bound in the JSON.

`rows` is the runner interface. Every row has `id`, `preset`, `rank`, `brilliance`, `diode_drop`, `loaded.{hp_cutoff,lp_cutoff}` and `stiff.{hp_cutoff,lp_cutoff}`. There are24 rows: two presets×three ranks×two drops×two BR endpoints. `scenarios` retains the complete physical-node solutions, current margins and derivative diagnostics.

The matched **stiff-preset controls** use the exact same resistor division and drop, but force actualFH/FL to `(10−Vd)*10k/(Rs+10k)` (0 for absent branch). Thus

`f_stiff=f_fixed+anchor*2^[s_actual*P_stiff+s_actual*(39k/RBR)*BR]`.

These controls are rank independent. Comparing loaded versus matched stiff isolates shared-bus loading from the new calibration convention, diode drop, exact-divider rounding and BR mapping. Eight unique preset/drop/BR controls map to16 clip renders. Together with48 loaded clip renders and four untouched native controls, the proposed runner has68 calls. The source audit itself performs zero audio calls.

The analytic null uses **rounded native table voltages**, zero drop and stiffP: at BR0 it recovers current factory frequencies for every rank; at BR−15 it recovers the current core's explicit `brilliance=-15` formula. Maximum independent nodal/formula discrepancy is1.37e−12 Hz. This mathematical null is separate from actual audio equality checks.

### Loaded reference cutoff results

Ranges below cover ranks A/B/C, with each exact value retained in the JSON.

| Preset | Drop | BR | HP range, Hz | LP range, Hz |
|---|---:|---:|---:|---:|
|Flute|.4|0|38.668936–38.711322|864.038558–870.712562|
|Flute|.6|0|38.570821–38.612070|837.432984–843.760801|
|Flute|.4|−15|33.074627–33.087090|272.350484–272.770013|
|Flute|.6|−15|33.010046–33.023005|264.453033–264.819424|
|Brass I|.4|0|34.575169|1046.984471–1055.496874|
|Brass I|.6|0|34.575169|1010.536983–1018.574797|
|Brass I|.4|−15|30.376789–30.408694|327.010840–327.773122|
|Brass I|.6|−15|30.376789–30.408694|316.179314–316.863736|

The five-node matrix and separate scalar reduction agree within1e−14 V; maximum normalized KCL residual is1.47e−16. These are circuit-equation checks, not evidence of an improved recording match.

## Brass override and prior-study distinction

Frozen Brass case10/case20 patches contain HP27.727086977293183 and LP442.85422297489009 Hz: the prior **nominal-source-slope BR−15 offsets are already baked in**. Both values must be replaced in full by the selected loaded or matched-stiff row. Do not multiply those old frequencies by a new loading correction or apply `brilliance=-15` again. The scenario's BR voltage is already included in its neutral cutoff. Keep the native `brilliance` parameter at0 for these rendered patch overrides. The untouched frozen-control case retains its original baked values.

The prior `benchmark_whitmore_brilliance_endpoint.py` preserved neutral native factory frequencies and applied an incremental BR shift under nominal-source slopes. The LP-bus candidate likewise studied incremental EG coupling with a matched common anchor. Neither performed this exact unloaded-divider→loaded-neutral-cutoff transform under fixed actual-bus anchors. Their outcomes remain valid and must not be relabeled as this experiment. Even an improved result here would establish only conditional sensitivity, because unknown chip scale, trims, original controls and MIDI still prevent a calibrated factory-law claim.
