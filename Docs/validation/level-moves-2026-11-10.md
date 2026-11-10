# Level moves between host buffers, 1.1.7

2026-11-10. Development-container measurements of the engine alone, 48 kHz,
1×; no host is involved.

## What was wrong

A front end reads its controls once per host buffer (the plug-in) or once per
64-frame batch (the Rack adapter) and hands the engine the patch. The engine
used VOLUME, the mixer levels, the ring modulator's mix and the expression
pedal as they stood, so each jumped at the buffer boundary and the output
jumped with it. Only the ring depth was already brought over in a 20 ms line.

## What changed

Those levels now reach a new value along a 5 ms straight line, one step per
internal sample, and rest exactly on it. The ring depth keeps its 20 ms. The
output trim is left alone: it is a set-and-forget level the sound-path
contract requires to act at once. This is digital dezippering, not a circuit
time constant: the CS-50's levers are continuous, and a host buffer is not.

## Measurements

One note of a 220 Hz sine, the control stepped once. The click is the energy
above 4 kHz between the tone's harmonics, against the whole signal:

| Step | before | after |
| --- | ---: | ---: |
| VOLUME 1 to 0.5 | −43.1 dB | −78.6 dB |
| Expression pedal 1 to 0.8 | −42.4 dB | −81.1 dB |
| Ring mix 0 to 1 | −37.7 dB | −72.2 dB |

The same measure while the control ramps for half a second, written every 512
frames (a large plug-in buffer):

| Ramp | before | after | written every sample |
| --- | ---: | ---: | ---: |
| VOLUME, sine | −59.8 dB | −92.5 dB | −94.4 dB |
| Sine level | −59.8 dB | −92.5 dB | −94.4 dB |
| Ring mix, sine | −59.0 dB | −83.5 dB | −83.8 dB |
| Expression pedal, sine | −80.9 dB | −90.9 dB | −106.5 dB |
| VOLUME, saw | −54.9 dB | −61.0 dB | −70.2 dB |
| Low-pass cutoff 400 Hz to 8 kHz, saw | −61.8 dB | −61.8 dB | −85.2 dB |

Written every 64 frames (the Rack's batch), VOLUME under the sine read
−68.9 dB before; now every level that glides reads the same as written every
sample. The cutoff reads −78.4 dB there. A buffer longer than the 5 ms line
still turns a slow fade into short ramps, which is what remains on the saw at
512 frames. The cutoffs, BRILLIANCE and pulse width are control voltages
through the panel's networks and still change once per buffer (README, Known
gaps).

All 14 factory presets at 1× and 4×, and a patch with VOLUME, ring mix and the
pedal away from their ends, render bit for bit as before the change when no
control moves. `Tests/LevelMotionTests.cpp` holds the largest sample-to-sample
jump after a step to under twice the tone's own; it was 9 to 32 times.

## CPU

`Tools/callgrind_presets.cpp` under Callgrind, the factory presets at four
voices with nothing moving: 1,334,480,189 instructions at 1× (240 blocks)
against 1,329,370,594 without the glides, 0.4% more; 1,340,728,769 against
1,335,670,856 at 4× (60 blocks). A development-container figure.
