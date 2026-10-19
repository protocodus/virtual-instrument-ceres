#!/usr/bin/env python3
"""Aggregate the frozen comparison without fitting or rerendering any case."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def phase_error(model: float, reference: float) -> float:
    return (model-reference+180) % 360-180


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--comparison', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists():
        raise RuntimeError('Output exists; preserve the previous aggregation.')
    source = json.loads(args.source.read_text())
    comparison = json.loads(args.comparison.read_text())
    eligible = [r for r in source['rows'] if r['eligible_source_shape']]
    matched = [r for r in eligible if round(r['midi_float']) in comparison['notes']]
    by_case = {(r['binary'], r['note'], r['wave']): r for r in comparison['rows']}
    windows = []
    for ref in matched:
        note = round(ref['midi_float'])
        for binary in ['baseline', 'candidate']:
            model = by_case[(binary, note, ref['label'])]
            windows.append({
                'binary': binary, 'note': note, 'wave': ref['label'],
                'source_start_seconds': ref['start_seconds'],
                'source_raw_rms': ref['rms'], 'held_gain_model_rms': model['held_gain_rms'],
                'rms_error_db': 20*math.log10(model['held_gain_rms']/ref['rms']),
                'h2_error_db': model['harmonic_db_relative_h1'][1]-ref['harmonic_db_relative_h1'][1],
                'h3_error_db': model['harmonic_db_relative_h1'][2]-ref['harmonic_db_relative_h1'][2],
                'h2_phase_error_degrees': phase_error(model['relative_phase_degrees'][1], ref['relative_phase_degrees'][1]),
                'h2_phase_interpretable': model['harmonic_db_relative_h1'][1] > -60 and ref['harmonic_db_relative_h1'][1] > -60,
            })
    aggregate = []
    for binary in ['baseline', 'candidate']:
        rows = [r for r in comparison['rows'] if r['binary'] == binary and r['wave'] == 'both']
        rms = [r['rms_error_db'] for r in rows]
        h2 = [r['h2_error_db'] for r in rows]
        phases = [phase_error(r['relative_phase_degrees'][1], r['source_median']['h2_relative_phase_circular_mean_degrees']) for r in rows]
        aggregate.append({
            'binary': binary, 'notes': [r['note'] for r in rows],
            'both_rms_mae_db': statistics.mean(abs(x) for x in rms),
            'both_rms_rmse_db': math.sqrt(statistics.mean(x*x for x in rms)),
            'both_h2_mae_db': statistics.mean(abs(x) for x in h2),
            'both_h2_rmse_db': math.sqrt(statistics.mean(x*x for x in h2)),
            'both_h2_relative_phase_mae_degrees': statistics.mean(abs(x) for x in phases),
        })
    report = {
        'source_report_sha256': sha(args.source),
        'comparison_report_sha256': sha(args.comparison), 'script_sha256': sha(Path(__file__)),
        'source_windows_screened': len(source['rows']), 'source_windows_retained': len(eligible),
        'source_notes_retained': sorted({round(r['midi_float']) for r in eligible}),
        'source_windows_matched_by_frozen_comparison': len(matched),
        'unmatched_source_windows': len(eligible)-len(matched),
        'scope': 'All retained A3/D4 windows are included; other notes corroborate the observed both-on harmonic family but have no production match in the frozen 12-case contrast. These are reused development excerpts, not a blind confidence estimate.',
        'two_note_mixture_aggregate': aggregate, 'all_matched_source_window_errors': windows,
    }
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'all_matched_source_window_errors'}, indent=2))


if __name__ == '__main__':
    main()
