#!/usr/bin/env python3
"""Reproducible production-render mixing probes; no fitted hardware-accuracy claim.

The public-reference eligibility decision lives beside the report. This tool does
not infer unknown hardware settings, fit branch phase/gain, normalize variants,
or load any reference into Ceres. Requires NumPy and SciPy for offline analysis.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

RATE = 48000
NOTE = 60
FREQUENCY = 440.0 * 2 ** ((NOTE - 69) / 12)
COMMON = {
    'saw': 1, 'pulse': 0, 'sine': 0, 'noise': 0,
    'pulse_width': .5, 'pwm_depth': 0, 'key_track': 0,
    'hp_cutoff': 30, 'lp_cutoff': 6000,
    'hp_resonance': 0, 'lp_resonance': 0,
    'filter_initial': 0, 'filter_peak': 0,
    'amp_attack': .01, 'amp_decay': .1, 'amp_sustain': 1,
    'amp_release': .25, 'volume': .55, 'filter_level': 1,
    'amp_level': 1, 'voice_spread_cents': 0, 'drift_cents': 0,
    'vibrato_cents': 0, 'tremolo': 0, 'filter_lfo': 0,
    'ring_mix': 0, 'pressure_volume': 0, 'pressure_filter': 0,
    'pressure_vibrato': 0, 'velocity_sensitivity': 0,
    'drive': 1,
}
PROFILES = {
    'pulse-isolated': {'saw': 0},
    'saw-pulse-50pct': {},
    'saw-pulse-70pct': {'pulse_width': .7},
    # Source blend/filter coordinates from the user's edited Bloom screenshot;
    # envelopes/modulators frozen to isolate mixing, NOT the original preset.
    'bloom-blend-static': {'sine': .84, 'pulse_width': .58,
                          'hp_cutoff': 28, 'lp_cutoff': 1800,
                          'lp_resonance': .2},
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def measure(data: np.ndarray) -> dict:
    x = np.asarray(data, dtype=np.float64)
    t = np.arange(len(x)) / RATE
    angles = 2 * np.pi * FREQUENCY * t[:, None] * np.arange(1, 17)[None, :]
    design = np.column_stack((np.ones(len(x)), np.cos(angles), np.sin(angles)))
    coef, *_ = np.linalg.lstsq(design, x, rcond=None)
    # x = Re(C_h * exp(i*h*w*t)); keep common time coordinate for all branches.
    c = coef[1:17] - 1j * coef[17:33]
    phase = np.angle(c)
    relative = np.angle(np.exp(1j * (phase - np.arange(1, 17) * phase[0])))
    return {
        'rms': float(np.sqrt(np.mean(x*x))),
        'peak': float(np.max(np.abs(x))),
        'dc': float(np.mean(x)),
        'harmonic_amplitude': np.abs(c).tolist(),
        'harmonic_complex_real': c.real.tolist(),
        'harmonic_complex_imag': c.imag.tolist(),
        'relative_phase_degrees': np.degrees(relative).tolist(),
        'harmonic_fit_residual_rms': float(np.sqrt(np.mean((x - design @ coef)**2))),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True)
    ap.add_argument('--source-root', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--oversampling', type=int, choices=[1, 2, 4], default=4)
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out/'declaration.json').exists() or (out/'production-report.json').exists():
        raise RuntimeError('Output attempt exists; use a new directory to preserve evidence.')
    renderer = args.renderer.resolve()
    root = args.source_root.resolve()
    source_files = ['Source/DSP/ceres.cpp', 'Source/DSP/ceres.hpp', 'Tools/render_main.cpp']
    identities = {name: sha(root/name) for name in source_files}
    build = json.loads(subprocess.check_output([str(renderer), '--build-info'], text=True))
    for name in source_files:
        if build[name] != identities[name]:
            raise RuntimeError(f'renderer source mismatch: {name}')
    events = out / 'held-c4.events.tsv'
    events.write_text('4800 on 60 1\n')
    declaration = {
        'kind': 'synthetic_production_probe_not_hardware_verification',
        'reference_gain_fit': None, 'independent_variant_normalization': False,
        'rate': RATE, 'note': NOTE, 'frequency_hz': FREQUENCY,
        'frames': RATE*2, 'measurement_window_seconds': [1.5, 1.9],
        'voices': 4, 'seed': 50, 'oversampling': args.oversampling,
        'common_patch': COMMON, 'profiles': PROFILES,
        'pulse_values': [i/10 for i in range(11)],
        'dsp_sources': identities, 'renderer_sha256': sha(renderer),
        'renderer_build_identity': build,
        'script_sha256': sha(Path(__file__)),
        'metric': 'Raw RMS/peak plus joint16-harmonic least squares at known unmodulated pitch; common absolute phase coordinate. No per-variant normalization or phase shift.',
    }
    # Written before executing any parameter variant or measuring its audio.
    write_json(out/'declaration.json', declaration)
    results = []
    for profile, changes in PROFILES.items():
        rows = []
        for pulse in declaration['pulse_values']:
            key = f'{profile}-pulse-{pulse:.1f}'
            patch = dict(COMMON, **changes, pulse=pulse)
            patch_path = out/f'{key}.patch'
            patch_path.write_text(''.join(f'{k} = {v:.17g}\n' for k, v in patch.items()))
            audio_path = out/f'{key}.wav'
            command = [str(renderer), '--preset', 'init', '--patch', str(patch_path),
                       '--events', str(events), '--output', str(audio_path), '--frames',
                       str(declaration['frames']), '--rate', str(RATE), '--seed', '50',
                       '--voices', '4', '--oversampling', str(args.oversampling)]
            render = json.loads(subprocess.check_output(command, text=True))
            rate, data = wavfile.read(audio_path)
            if rate != RATE or data.ndim != 1 or not np.all(np.isfinite(data)):
                raise RuntimeError(f'invalid render: {key}')
            metrics = measure(data[int(1.5*RATE):int(1.9*RATE)])
            rows.append({'pulse': pulse, 'metrics': metrics,
                         'patch_sha256': sha(patch_path), 'audio_sha256': sha(audio_path),
                         'render': render})
        nonzero = [r for r in rows if r['metrics']['rms'] > 0]
        minimum = min(nonzero, key=lambda r:r['metrics']['rms'])
        start, end = rows[0]['metrics']['rms'], rows[-1]['metrics']['rms']
        results.append({'profile': profile, 'rows': rows,
                        'rms_monotonic_increasing': all(a['metrics']['rms'] <= b['metrics']['rms']+1e-12 for a,b in zip(rows,rows[1:])),
                        'minimum_nonzero_pulse': minimum['pulse'],
                        'minimum_rms': minimum['metrics']['rms'],
                        'minimum_vs_pulse_zero_db': 20*math.log10(minimum['metrics']['rms']/start) if start else None,
                        'pulse_full_vs_zero_db': 20*math.log10(end/start) if start else None})
    if identities != {name:sha(root/name) for name in source_files}:
        raise RuntimeError('source changed while rendering; evidence incomplete, rerun')
    report = dict(declaration, results=results,
                  hardware_mixing_accuracy='unidentified: requires eligible recording documented separately',
                  no_dsp_changes_from_recording_fit=True)
    write_json(out/'production-report.json', report)
    print(json.dumps({'report':str(out/'production-report.json'), 'profiles':[
        {k:v for k,v in r.items() if k!='rows'} for r in results]}, indent=2))


if __name__ == '__main__':
    main()
