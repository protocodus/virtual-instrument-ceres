#!/usr/bin/env python3
"""Conditional no-fit polarity contrast, with one gain held across both engines.

The D4 saw source sets a single master tuning and recording gain. A3 is held out.
All source windows passing the frozen screen contribute to their note summaries.
Unknown hardware controls/capture remain limitations; no exact fidelity is claimed.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess

import numpy as np
from scipy.io import wavfile


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, data) -> None:
    path.write_text(json.dumps(data, indent=2, allow_nan=False) + '\n')


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--candidate', type=Path, required=True)
    ap.add_argument('--source-report', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    root = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location('mix', root/'Tools/verify_waveform_mixing.py')
    mix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mix)
    sr = json.loads(args.source_report.read_text())
    source = {(s['label'], s['note']): s for s in sr['summary']}
    d4 = [r['f0_hz'] for r in sr['rows'] if r['label'] == 'saw'
          and round(r['midi_float']) == 62 and r['eligible_source_shape']]
    anchor_f0 = float(np.median(d4))
    tune = 1200*math.log2(anchor_f0/(440*2**((62-69)/12)))
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out/'declaration.json').exists() or (out/'report.json').exists():
        raise RuntimeError('Output attempt exists; choose a new directory to preserve evidence.')
    binaries = [('baseline', args.baseline.resolve()), ('candidate', args.candidate.resolve())]
    decl = {
        'purpose': 'Conditional no-fit pulse-polarity contrast, not full hardware fidelity.',
        'source_report_sha256': sha(args.source_report),
        'script_sha256': sha(Path(__file__)),
        'pitch_nuisance': 'One master tuning from median D4 isolated-saw f0, held for A3 and all wave variants. No wave-specific tuning/gain/phase correction.',
        'common_tune_cents': tune,
        'notes': [57, 62], 'waves': {'saw': [1, 0], 'square': [0, 1], 'both': [1, 1]},
        'settings': 'Production probe common patch: PW50%, no PWM/sine/noise; HP30/LP6000Hz, Q0, key track0, amp sustain1; four voices, 4x, 48kHz, seed50. Actual hardware filter/EG/level controls remain uncalibrated.',
        'measurement_window_seconds': [1.5, 1.9],
        'gain_rule': 'One scalar from D4-saw reference median raw RMS divided by D4-saw baseline model RMS; held across both binaries, all wave combinations and A3.',
        'sources': {key: json.loads(subprocess.check_output([str(path), '--build-info'], text=True))
                    for key, path in binaries},
        'binary_sha256': {key: sha(path) for key, path in binaries},
        'no_parameter_fitting': True,
        'internal_branch_phase_preserved': True,
        'independent_variant_normalization': False,
    }
    write_json(out/'declaration.json', decl)  # Freeze before any variant render.
    rows = []
    for binary, path in binaries:
        for note in decl['notes']:
            for wave, (saw, pulse) in decl['waves'].items():
                stem = f'{binary}-{note}-{wave}'
                patch = dict(mix.COMMON, saw=saw, pulse=pulse, tune_cents=tune)
                pp = out/f'{stem}.patch'
                pp.write_text(''.join(f'{k} = {v:.17g}\n' for k, v in patch.items()))
                ep = out/f'{stem}.events.tsv'
                ep.write_text(f'4800 on {note} 1\n')
                wav = out/f'{stem}.wav'
                command = [str(path), '--preset', 'init', '--patch', str(pp), '--events', str(ep),
                           '--output', str(wav), '--frames', '96000', '--rate', '48000',
                           '--seed', '50', '--voices', '4', '--oversampling', '4']
                subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
                fs, x = wavfile.read(wav)
                assert fs == 48000 and x.shape == (96000,) and np.all(np.isfinite(x))
                mix.FREQUENCY = 440*2**((note-69)/12+tune/1200)
                measured = mix.measure(x[72000:91200])
                hs = np.asarray(measured['harmonic_amplitude'])
                rows.append({
                    'binary': binary, 'note': note, 'wave': wave,
                    'rms': measured['rms'], 'peak': measured['peak'],
                    'harmonic_amplitude': hs.tolist(),
                    'harmonic_db_relative_h1': (20*np.log10(np.maximum(hs, 1e-20)/hs[0])).tolist(),
                    'relative_phase_degrees': measured['relative_phase_degrees'],
                    'audio_sha256': sha(wav), 'patch_sha256': sha(pp), 'events_sha256': sha(ep),
                })
    anchor = next(r for r in rows if (r['binary'], r['wave'], r['note']) == ('baseline', 'saw', 62))
    gain = source[('saw', 62)]['rms_median']/anchor['rms']
    for row in rows:
        ref = source[(row['wave'], row['note'])]
        row['held_gain_rms'] = gain*row['rms']
        row['source_median'] = ref
        row['rms_error_db'] = 20*math.log10(row['held_gain_rms']/ref['rms_median'])
        row['h2_error_db'] = row['harmonic_db_relative_h1'][1]-ref['h2_db_median']
        row['h3_error_db'] = row['harmonic_db_relative_h1'][2]-ref['h3_db_median']
    summary = []
    for binary, _ in binaries:
        for note in decl['notes']:
            z = {r['wave']: r for r in rows if r['binary'] == binary and r['note'] == note}
            summary.append({
                'binary': binary, 'note': note,
                'square_to_saw_rms': z['square']['rms']/z['saw']['rms'],
                'both_to_saw_rms': z['both']['rms']/z['saw']['rms'],
                'both_h2_relative_db': z['both']['harmonic_db_relative_h1'][1],
                'both_h3_relative_db': z['both']['harmonic_db_relative_h1'][2],
                'both_h2_phase_degrees': z['both']['relative_phase_degrees'][1],
                'both_held_gain_rms_error_db': z['both']['rms_error_db'],
            })
    report = dict(decl, recording_gain=gain, rows=rows, summary=summary,
                  conclusion='Conditional comparison only: source controls/capture gain and PWM edge phase remain unresolved. Candidate must predict both mixture harmonic balance and held-gain level, including the A3 holdout; no independent normalizations or fitted filter/phase corrections are allowed.')
    write_json(out/'report.json', report)
    print(json.dumps({'recording_gain': gain, 'summary': summary}, indent=2))


if __name__ == '__main__':
    main()
