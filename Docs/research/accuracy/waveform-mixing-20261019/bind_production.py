#!/usr/bin/env python3
"""Bind a production renderer to all six frozen pulse-polarity candidate WAVs."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
from scipy.io import wavfile


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True)
    ap.add_argument('--source-root', type=Path, required=True)
    ap.add_argument('--comparison', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    renderer = args.renderer.resolve()
    root = args.source_root.resolve()
    comparison = args.comparison.resolve()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out/'declaration.json').exists() or (out/'report.json').exists():
        raise RuntimeError('Output attempt exists; choose a new directory to preserve evidence.')
    witness = json.loads((comparison/'report.json').read_text())
    sources = {name: sha(root/name) for name in
               ['Source/DSP/ceres.cpp', 'Source/DSP/ceres.hpp', 'Tools/render_main.cpp', 'CMakeLists.txt']}
    build = json.loads(subprocess.check_output([str(renderer), '--build-info'], text=True))
    for name, digest in sources.items():
        if build[name] != digest:
            raise RuntimeError(f'Production renderer source identity mismatch: {name}')
    declaration = {
        'comparison_report_sha256': sha(comparison/'report.json'),
        'script_sha256': sha(Path(__file__)), 'renderer_sha256': sha(renderer),
        'renderer_build_identity': build, 'source_sha256': sources,
        'cases': 'Every candidate case from the frozen 12-case contrast: A3/D4 × saw/square/both.',
        'normalization': 'None; exact original patches, events, rate, seed, voices and quality.',
        'required_result': 'Whole float32 WAV bytes equal to temporary-candidate witness, including attack and output tail.',
    }
    (out/'declaration.json').write_text(json.dumps(declaration, indent=2)+'\n')
    rows = []
    for case in witness['rows']:
        if case['binary'] != 'candidate':
            continue
        stem = f"candidate-{case['note']}-{case['wave']}"
        expected = comparison/f'{stem}.wav'
        patch = comparison/f'{stem}.patch'
        events = comparison/f'{stem}.events.tsv'
        if sha(expected) != case['audio_sha256'] or sha(patch) != case['patch_sha256'] or sha(events) != case['events_sha256']:
            raise RuntimeError(f'Frozen witness changed: {stem}')
        actual = out/f'{stem}.wav'
        command = [str(renderer), '--preset', 'init', '--patch', str(patch), '--events', str(events),
                   '--output', str(actual), '--frames', '96000', '--rate', '48000',
                   '--seed', '50', '--voices', '4', '--oversampling', '4']
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        expected_rate, a = wavfile.read(expected)
        actual_rate, b = wavfile.read(actual)
        assert expected_rate == actual_rate == 48000 and a.shape == b.shape == (96000,)
        digest = sha(actual)
        rows.append({'note': case['note'], 'wave': case['wave'],
                     'expected_sha256': case['audio_sha256'], 'actual_sha256': digest,
                     'whole_wav_byte_exact': digest == case['audio_sha256'],
                     'max_sample_difference': float(np.max(np.abs(a.astype(float)-b.astype(float)))),
                     'steady_window_max_difference': float(np.max(np.abs(a[72000:91200].astype(float)-b[72000:91200].astype(float))))})
    assert len(rows) == 6
    for name, digest in sources.items():
        if sha(root/name) != digest:
            raise RuntimeError('Production source changed while rendering; repeat in a new attempt.')
    report = dict(declaration, rows=rows, all_six_whole_wav_byte_exact=all(r['whole_wav_byte_exact'] for r in rows))
    (out/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps(report, indent=2))
    if not report['all_six_whole_wav_byte_exact']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
