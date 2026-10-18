# Research-only instantaneous cutoff calibration network

2026-10-18. This isolated branch starts at `55869df8ba728e25a6b5792aa07dfd55f134439c`.
It intentionally retains that frozen engine's SUB-AM phase convention to
isolate cutoff mapping. The separately retained SUB-polarity correction is
not reverted or replaced. No public-recording-linked render or fit has been
performed for this implementation.

## Scope and controls

The native/offline build adds four numeric patch keys. The embedded build and
its 57-key ABI are unchanged. All original controls and factory conversion
remain in force when `calibration_network=0`, the default.

| Key | Range/default | Meaning |
|---|---|---|
|`calibration_network`|0..2, rounded; default0|0 disabled, 1 Flute resistor matrix, 2 Brass I resistor matrix|
|`calibration_hp_bias`|−15..15, default0|Shared HP bias-source voltage through fixed355k|
|`calibration_lp_bias`|−15..15, default0|Shared LP bias-source voltage through fixed355k|
|`calibration_alpha`|.01..8, default1|Positive multiplier of the complete non-key node exponent|

Bias is a voltage-source coordinate in the declared fixed355k circuit, **not
an additive octave offset**. A real pot's output resistance changes with
position; holding355k while varying its source voltage is a conditional
calibration hypothesis, not full potentiometer emulation. Numeric limits are
research guardrails, not fitted or newly measured hardware ranges.

Select the ordinary factory preset for its waveform, envelopes, Q and levels,
then set the explicit research mode. The mode does not infer the resistor
matrix from patch metadata. BR is the actual stiff shared voltage in the
existing `brilliance` key: use0 for the declared Flute case and−15 for the
declared Brass boundary. Remove previously baked BR/cutoff transformations
from the experimental interpretation: the old `hp_cutoff`, `lp_cutoff`,
`hp_envelope_scale` and `lp_envelope_scale` are completely ignored in this mode.

All filter EG states use the original attack/decay/release equations, gates,
allocation and capacitor history. The mode changes only their coupling to
cutoff. Q, source drive, oscillator, amplitude path and final gain are unchanged.
Nonzero `pressure_filter` is explicitly rejected while enabled; no new touch
or velocity-to-filter current is invented. Other original audio controls
retain their existing behavior.

## Exact declared network

The oracle is copied byte-for-byte as `Tests/oracles/preset_dc_network.py`
from the historical `Docs/accuracy/preset-dc-loading/network.py`:
SHA256 `b03cdb48f68c73cc70312cb2c8061d69a1d81fea1f6e0330464f8e052f631297`.
Its full five-node matrix is independent of the native reduced solution.

Fixed assumptions: rankB HP2.2k/LP2.4k; selected diode drop.6V;
10V selected rail; one10k return per preset bus; each of four boards has
39k to that bus; bias branch355k; zero chip input current; stiff BR/SUB/other
sources; inactive preset/manual diode branches off. HP/LP preset resistors
are Flute75k/9.1k and Brass absent/7.5k. Stage parameters are:

| |HP|LP|
|---|---:|---:|
|BR resistor|680k|220k|
|Each of three modulation branches|150k|47k|
|EG resistor|220k|47k|
|Provisional actual-bus slope, oct/V|.7|.65|
|Variable-gm anchor at zero node voltage|12Hz|180Hz|

For each stage, `gp=1/39k`, `gs=1/Rpreset` (zero for absent branch),
`D=gp+1/Rrank+1/Rbr+3/Rmod+1/Reg+1/355k`, and
`rhs_i=BR/Rbr+E_i/Reg+bias/355k+SUBvoltage/Rmod`. The equations are

```text
(gs+1/10k+4gp)P - gp*sum(u_i) = gs*(10-.6)
D*u_i - gp*P = rhs_i
kappa = nominal_actual_bus_slope*D/gp
q_i = alpha*kappa*u_i
frequency_i = fixed_floor + anchor*2^(key_octaves_i + q_i)
fixed_floor = 1/(2*pi*4.7M*1.5nF)
```

Thus alpha acts on the preset, bias, all-board EG/backfeed, BR and SUB terms
together. Keyboard/bend tracking and master tuning remain outside alpha;
the passive frequency floor never scales. All four EGs are advanced once at
the internal rate before solving either network, including idle boards at IL
and releasing boards at their instantaneous voltage. No own-EG term is added
a second time. The original audio-filter high-frequency clamp still applies;
diagnostic frequencies are the requested frequencies before that clamp.

Existing `filter_lfo` remains an LP-equivalent octave-depth input. Its
engineering conversion is `SUBvoltage=filter_lfo*SUB/(.65*39/47)`, one shared
source feeding the documented unequal resistors. Alpha and bus loading then
act on that source normally. This is not a measured SUB depth calibration.
The other two modulation sources stay at zero but retain their shunts.

The solve is quasi-static: it does not newly model cutoff-control shunt
capacitor dynamics, finite-source BR loading, diode exponential I–V, chip
input currents or rank/trim tolerances. It is not a general PANEL model.

## Diagnostics and validation

`calibrationNetworkState(patch, fourEGs, sub)` exposes the stateless production
solve; `Synth::calibrationState()` exposes the last internal-frame solve.
Each includes bus/pin voltages, full exponent, reference frequency, selected
branch current, inactive-manual-diode margin and normalized KCL residual.
For an absent selected branch the current is0 and `selectedPresent=false`.
The native audio path throws if the declared diode states become invalid;
it does not silently clamp a node or continue with a wrong conducting set.

The new native contracts cover unequal/current/idle/released EGs, unchanged
envelope timing, alpha on all terms, key/floor separation, ignored legacy
cutoff/EG scales, invalid pressure scope, diode detection and disabled controls.
The Python oracle checks328 scenarios /656 stage solves, including all16
four-board IL/AL corners at bias/BR/SUB endpoints. Largest node error is
1.34e−15V; normalized independent KCL residual is below2.21e−16. Minimum
manual-off margin is.4668848V and selected current115.777µA in that declared
test set. These are model tests, not measured hardware tolerances.

All original seven CTests and both research CTests pass. Initial automatic
CMake Python selection lacked NumPy; selecting the installed Python3.11
resolved that environment failure without changing equations. Targeted
ASan/UBSan research tests also pass (`detect_leaks=0`).

`Tools/check_joint_disabled_parity.py` freshly compiles frozen55869df and
compares the unchanged active-control/voice-expression synthetic driver:
384000 float32 samples, four sample rates, eight scenes/rate, exact bytes.
SHA256 `c3a7b248b1823d8254efef20968c2127054e1508deb7ca65e83047f76bfee480`.
This includes nonzero AM/SUB/ring/noise, pressure, mixed velocity, stealing,
release, live controls and reset. The first report-writing attempt had a
`/tmp` versus `/private/tmp` path-format error after audio equality passed;
the resolved-path script completed successfully on rerun.

Reproduce:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=/Library/Frameworks/Python.framework/Versions/3.11/bin/python3
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 Tools/check_joint_disabled_parity.py
```

Build identities, exact source diff, oracle results and test logs are recorded
separately in the implementation validation report. No default calibration
or improvement against hardware recordings is claimed.
