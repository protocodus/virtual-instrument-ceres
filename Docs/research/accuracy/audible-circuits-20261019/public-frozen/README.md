# Frozen public-recording comparison

Protocol and integrity checks passed. This is reused development evidence, not an independent hardware-accuracy certification.

Parent is the actual d5e0 source identity retained by sub-polarity-phase0. All eight cases retain their original parameters, gains, MIDI/gates, masks and analysis windows. No fitting was performed.

The two raw factory Flute cases are reported separately from the six effective/previously fitted cases. No aggregate accuracy percentage is computed. Negative error deltas indicate improvement; signed pitch errors are descriptive.

## flute-high-factory-physical

raw_factory_physical_flute.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| median_harmonic_MAE_db | 21.840790 | 21.971326 | +0.130536 |
| trajectory_ratio_MAE_db | 21.771715 | 21.904168 | +0.132453 |
| held_gain_RMS_error_db | 0.667553 | 0.659489 | -0.008064 |
| median_pitch_error_cents | 0.121251 | 0.121301 | +0.000050 |
| RMS_slope_error_abs_db_per_s | 1.223598 | 1.222964 | -0.000634 |

Named factory Flute and physical note/transpose visually supported; unknown globals, pressure, exact gates and voice count. Effective family is a prior high-window fit, not hardware settings.

Source-pitch following synthetic fresh held gate; physical notes 88/84, octave -1. Bends also affect filter key tracking.

## flute-high-effective-compensated

previously_fitted_effective_flute.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| median_harmonic_MAE_db | 0.731332 | 0.844290 | +0.112958 |
| trajectory_ratio_MAE_db | 0.933640 | 1.053809 | +0.120169 |
| held_gain_RMS_error_db | 0.029749 | 0.038318 | +0.008569 |
| median_pitch_error_cents | 0.101602 | 0.101640 | +0.000038 |
| RMS_slope_error_abs_db_per_s | 0.000063 | 0.001349 | +0.001286 |

Named factory Flute and physical note/transpose visually supported; unknown globals, pressure, exact gates and voice count. Effective family is a prior high-window fit, not hardware settings.

Source-pitch following synthetic fresh held gate; physical notes 88/84, octave -1. Bends also affect filter key tracking.

## flute-lower-factory-physical

raw_factory_physical_flute.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| median_harmonic_MAE_db | 19.165210 | 19.235121 | +0.069911 |
| trajectory_ratio_MAE_db | 19.018261 | 19.086651 | +0.068390 |
| held_gain_RMS_error_db | 0.387948 | 0.393018 | +0.005070 |
| median_pitch_error_cents | 0.022611 | 0.022647 | +0.000036 |
| RMS_slope_error_abs_db_per_s | 1.147912 | 1.147617 | -0.000295 |

Named factory Flute and physical note/transpose visually supported; unknown globals, pressure, exact gates and voice count. Effective family is a prior high-window fit, not hardware settings.

Source-pitch following synthetic fresh held gate; physical notes 88/84, octave -1. Bends also affect filter key tracking.

## flute-lower-effective-compensated

previously_fitted_effective_flute.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| median_harmonic_MAE_db | 0.931825 | 0.857181 | -0.074644 |
| trajectory_ratio_MAE_db | 1.083864 | 1.011514 | -0.072350 |
| held_gain_RMS_error_db | 0.224299 | 0.244878 | +0.020579 |
| median_pitch_error_cents | 0.007830 | 0.007857 | +0.000027 |
| RMS_slope_error_abs_db_per_s | 0.143039 | 0.141605 | -0.001433 |

Named factory Flute and physical note/transpose visually supported; unknown globals, pressure, exact gates and voice count. Effective family is a prior high-window fit, not hardware settings.

Source-pitch following synthetic fresh held gate; physical notes 88/84, octave -1. Bends also affect filter key tracking.

## brass-history20

conditional_effective_brass.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| six_cell_mean_ratio_MAE_db | 3.626135 | 3.623799 | -0.002336 |
| native_weak_partial_frames | 1.000000 | 1.000000 | +0.000000 |
| note57/early/ratio_MAE_db | 2.941015 | 2.939015 | -0.001999 |
| note57/release/ratio_MAE_db | 4.928657 | 4.932646 | +0.003989 |
| note60/early/ratio_MAE_db | 2.908004 | 2.906544 | -0.001460 |
| note60/release/ratio_MAE_db | 4.292472 | 4.297064 | +0.004591 |
| note64/early/ratio_MAE_db | 2.803759 | 2.792228 | -0.011531 |
| note64/release/ratio_MAE_db | 3.882902 | 3.875299 | -0.007604 |
| note57/tail_floored_RMS_db | 8.879355 | 8.880116 | +0.000761 |
| note60/tail_floored_RMS_db | 9.071499 | 9.068218 | -0.003281 |
| note64/tail_floored_RMS_db | 8.459240 | 8.462687 | +0.003447 |
| C4_H3_H2_change_error_abs_db | 11.456703 | 11.456722 | +0.000019 |
| C4_H4_H2_change_error_abs_db | 16.487539 | 16.487549 | +0.000010 |

Named Brass I; BR -15 boundary, common AM and global release are conditional effective controls; actual control voltages and modulation route unknown.

Reused fixed chord gate hypothesis 20, not original MIDI; physical chord+octave correction retained.

## brass-history10

conditional_effective_brass.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| six_cell_mean_ratio_MAE_db | 3.634643 | 3.634171 | -0.000472 |
| native_weak_partial_frames | 0.000000 | 0.000000 | +0.000000 |
| note57/early/ratio_MAE_db | 2.942509 | 2.940509 | -0.002000 |
| note57/release/ratio_MAE_db | 5.759386 | 5.763356 | +0.003970 |
| note60/early/ratio_MAE_db | 2.907904 | 2.906449 | -0.001455 |
| note60/release/ratio_MAE_db | 4.496149 | 4.500724 | +0.004575 |
| note64/early/ratio_MAE_db | 2.432353 | 2.420852 | -0.011501 |
| note64/release/ratio_MAE_db | 3.269553 | 3.273135 | +0.003581 |
| note57/tail_floored_RMS_db | 9.559431 | 9.557785 | -0.001646 |
| note60/tail_floored_RMS_db | 10.643830 | 10.639342 | -0.004488 |
| note64/tail_floored_RMS_db | 7.253543 | 7.254767 | +0.001224 |
| C4_H3_H2_change_error_abs_db | 11.803841 | 11.803874 | +0.000032 |
| C4_H4_H2_change_error_abs_db | 17.400566 | 17.400524 | -0.000042 |

Named Brass I; BR -15 boundary, common AM and global release are conditional effective controls; actual control voltages and modulation route unknown.

Reused fixed chord gate hypothesis 10, not original MIDI; physical chord+octave correction retained.

## dry-sawtooth

previously_fitted_dry_waveform.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| early/all_H2_H32/trajectory_ratio_MAE_db | 0.267310 | 0.259749 | -0.007561 |
| early/odd_H3_H31/trajectory_ratio_MAE_db | 0.283201 | 0.276134 | -0.007066 |
| early/held_gain_RMS_error_db | 0.196110 | 0.189800 | -0.006310 |
| prior_train/all_H2_H32/trajectory_ratio_MAE_db | 0.368423 | 0.380165 | +0.011743 |
| prior_train/odd_H3_H31/trajectory_ratio_MAE_db | 0.365488 | 0.385581 | +0.020092 |
| prior_train/held_gain_RMS_error_db | 0.256743 | 0.262698 | +0.005955 |
| late/all_H2_H32/trajectory_ratio_MAE_db | 0.392913 | 0.396360 | +0.003447 |
| late/odd_H3_H31/trajectory_ratio_MAE_db | 0.367074 | 0.382040 | +0.014966 |
| late/held_gain_RMS_error_db | 0.289725 | 0.283896 | -0.005829 |

Named waveform, dry creator context; all effective oscillator/filter/envelope values previously fitted. No known factory preset or complete hardware settings.

Inferred monophonic pitch/onset and estimated gate, not original MIDI.

## dry-square

previously_fitted_dry_waveform.

| Metric | d5e0 parent | Candidate | Delta |
|---|---:|---:|---:|
| early/all_H2_H32/trajectory_ratio_MAE_db | 0.909992 | 0.909992 | +0.000000 |
| early/odd_H3_H31/trajectory_ratio_MAE_db | 1.069949 | 1.069949 | +0.000000 |
| early/held_gain_RMS_error_db | 0.392093 | 0.392093 | +0.000000 |
| prior_train/all_H2_H32/trajectory_ratio_MAE_db | 1.082312 | 1.082312 | +0.000000 |
| prior_train/odd_H3_H31/trajectory_ratio_MAE_db | 1.390416 | 1.390416 | +0.000000 |
| prior_train/held_gain_RMS_error_db | 0.271034 | 0.271034 | +0.000000 |
| late/all_H2_H32/trajectory_ratio_MAE_db | 2.292589 | 2.292589 | +0.000000 |
| late/odd_H3_H31/trajectory_ratio_MAE_db | 2.576775 | 2.576775 | +0.000000 |
| late/held_gain_RMS_error_db | 0.064779 | 0.064779 | +0.000000 |

Named waveform, dry creator context; all effective oscillator/filter/envelope values previously fitted. No known factory preset or complete hardware settings.

Inferred monophonic pitch/onset and estimated gate, not original MIDI.

## Limits

- All recordings and windows are reused development evidence, no untouched known-settings holdout.
- Audio references are offline analysis/listening only; synthesis uses no sample assets.
- No controls, gain, gate, source eligibility, or masking may be fitted by this battery.
- Neutral global controls and filter_lfo=0 do not verify newly added control paths.
- Public creator audio copyright remains with the creator; local analysis excerpts are not a redistribution license.
- The two raw factory Flute cases are separate from six previously fitted or conditional effective cases.
- Raw factory means unmodified named Flute preset plus declared octave correction; globals, touch, exact gate and voice allocation are unknown.
- All eight cases use inferred MIDI and reused development recordings; none is a known-settings independent hardware holdout.
- All eight cases have ring mix zero. This battery cannot validate the new audible ring-switching path.
- Executable success means protocol/integrity completion, not proof of hardware accuracy or an improvement verdict.
- Listening artifact is local analysis material, not a source-audio redistribution license.

Listening order per case: hardware → d5e0 parent → candidate. Frozen recording gains are retained; a single common playback gain of 1.0 is applied to the entire file. No individual normalization.
