# Circuit realism previews, 2026-10-28

One-time review evidence for the 1.1.0 circuit changes, rendered at 48 kHz, 4 voices, 4×, seed 50, no normalization. Each pair isolates one commit: "before" is that commit's parent, built the same way. Frozen on merge. The [research note](../../../research/circuit-realism-2026-10-28.md) has the sources.

| Files | Before → after | Preset and events | Shows |
| --- | --- | --- | --- |
| `kas-before.mp3` / `-after.mp3` | `933b571` → `3319311` | init, `amp_attack=.8 amp_release=3`; C4 held 1 s, then E4, G4, B4 for 0.15 s each, then D5 at 1.9 s | The new key takes the board released longest ago: after, D5 cuts C4's loud tail instead of the quietest one |
| `ring-before.mp3` / `-after.mp3` | `3319311` → `3a7cdb6` | init, `ring_mix=.5 expression=.7`; C3 G3 C4 held | With the pedal part closed the MODULATION lever's mid-travel sag shrinks: 1.1 dB louder |
| `ext-before.mp3` / `-after.mp3` | `3a7cdb6` → `481cab8` | init with `lfo_waveform=5 ext_level=1 filter_lfo=1.5`, a 0.3 V (20-unit) 110 Hz square at EXT IN; `Tools/init.events.tsv` | The overdriven EXT preamp slews at 1 V/µs: up to 0.8 dB in 100 ms windows, 22.6 dB below the signal overall |
| `rows-before.mp3` / `-after.mp3` | `481cab8` → `32c7116` | init, `lp_cutoff=85 lp_resonance=.5 filter_peak=5 filter_decay=6`, the LPF lever at its floor; C3 alone, then C3 G3 C4 E4 | The boards' EGs lift the shared FL row: 0.03 octave under one, 0.13 under the chord, 23 dB below the signal overall |

The EXT pair is rendered with a scratch copy of `ceres_render` that also feeds a square into `Synth::externalInput()`; the other pairs use `ceres_render` itself.
