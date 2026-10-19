# Relative saw/pulse audio polarity

Ceres 0.2.2 negates the completed pulse audio output, including its existing
analytic BLEP correction. The shared VCO phase, comparator timing window,
PW/PWM ranges, reset duration and edge positions are unchanged. Saw and sine
remain unchanged. No second oscillator, phase offset, mixer inverter, crossfade,
normalization or extra smoothing stage was introduced.

This is an empirical relative-audio-polarity calibration supported by public
CS-50 solo and both-on recordings. The Yamaha WSC drawings do not provide an
unambiguous common timing reference; the physical mixer uses equal source
resistors without a separate pulse inverter. The correction therefore does
not claim to identify an active-low chip stage. Reference selection, comparison
settings and recording-chain uncertainty belong to the
[public-recording study](../../accuracy/waveform-mixing-20261019/).

## Independent regression witnesses

The additions in [OscillatorCircuitTests.cpp](../../../Tests/OscillatorCircuitTests.cpp)
measure the resulting mixture instead of asserting that one implementation
sample is the negative of an earlier sample.

A coherent 256 Hz trace at 1,048,576 samples/s uses nominal 50% PW, no PWM,
the default finite-reset duration and the same oscillator for all source
measurements. Fourier coefficients and centered RMS establish solo level,
both-on harmonic balance and combined level. The broad test bands encode
the independently observed constructive relationship while allowing the
remaining uncalibrated analog transfer.

| Metric | Unchanged f46 | Corrected source |
| --- | ---: | ---: |
| Raw solo pulse/saw fundamental | 1.99991 | 1.99991 |
| Raw mixed H2/H1 | −6.01856 dB | −15.5615 dB |
| Raw mixed H3/H1 | −9.54566 dB | −9.54136 dB |
| Raw mixed/saw centered RMS | 0.999299 | 2.64471 |
| Production mixed/saw centered RMS | 0.883743 | 2.35103 |

The production check runs the actual filters, passive source loading, voice
VCA, output capacitors and output-rate stage. Three seeded instruments render
saw only, pulse only and both on with identical settings: 440 Hz, nominal PW,
open filters, low drive, settled envelopes and no other source/modulation.
After one second of settling, an exact 44-cycle window is measured. Both-on
source loading remains connected, and no individual signal is normalized.

The new raw harmonic and production level checks both fail against unchanged
f46 and pass after the pulse sign correction. The previous native phase test
now checks reinforcing saw/pulse audio fundamentals. Existing PW mean,
fixed-edge and PWM-phase checks observe the unchanged logical comparator
coordinate (the negative of the calibrated audio output), preserving their
timing and range assertions.

## Validation scope

Fresh focused Release and ASan/UBSan runs cover oscillator circuits, the native
DSP contracts, capacitor continuity, repeated-note ownership and fast-note
envelopes. The latter includes 54 capacitor cases, 72 rapid retarget cases
and 432 single-note/chord burst cases. See the source-bound
[validation report](validation.json) for final results and evidence hashes.

The earlier [f46 fast-note study](../fast-notes/) remains unchanged and retains
its original source identity. This follow-up separately checks those corrected
MIDI and envelope behaviors with the new waveform polarity.

This correction changes mixed-source timbre and level, so an earlier preset's
stored gain is not evidence of its new peak. Bank calibration and Rack-host
validation require their own results. These regressions do not prove absolute
hardware phase, a dry recording match, zero aliasing at every setting, or the
absence of host CPU underruns. Very short envelope transients remain intentional;
the physical envelope charge is preserved rather than reset on rapid re-gates.
