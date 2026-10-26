#!/usr/bin/env python3
"""Post-stop canonical score of the one existing training anchor; no renders."""
import importlib.util
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'Tools/accuracy'))
import run_battery as b
import preset_dc_loading as dc


def main():
    target = HERE / 'training-anchor-metrics-001'
    assert not target.exists(), 'Preserve every attempt'
    qpath = HERE / 'qualify-001/report.json'
    cpath = HERE / 'fit-001/candidate.json'
    qualification, candidate = b.read(qpath), b.read(cpath)
    assert qualification['passed']
    assert candidate['status'] == 'forward_budget_stopped_diagnostic_only'
    assert not candidate['validation_permitted']
    case = qualification['native_anchor_case']
    clip = case['original_case']
    assert clip['id'] == 'flute-high-factory-physical'
    old = b.read(ROOT / 'Docs/accuracy/runs/expression-55869df8/report.json')
    frozen = next(c for c in old['cases'] if c['id'] == clip['id'])
    native = next(c for c in b.read(HERE.parent / 'joint-calibration/plan.json')['native_controls']['cases']
                  if c['kind'] == 'unfitted' and c['original_case']['id'] == clip['id'])
    paths = [Path(__file__), HERE / 'plan.json', qpath, cpath,
             Path(case['audio']), Path(case['patch']), ROOT / clip['reference'],
             ROOT / frozen['audio'], Path(native['audio']),
             ROOT / 'Docs/accuracy/runs/expression-55869df8/report.json',
             HERE.parent / 'joint-calibration/plan.json',
             ROOT / 'Tools/accuracy/run_battery.py', ROOT / 'Tools/accuracy/metrics.py',
             ROOT / 'Tools/accuracy/preset_dc_loading.py']
    bindings = {str(p): b.sha(p) for p in paths}
    target.mkdir()
    b.save(target / 'declaration.json', dict(input_sha256=bindings,
        purpose='Canonical native metrics of the existing training anchor only.',
        source_case=clip['id'], new_WAVs=0, parameter_fits=0,
        candidate_validation=False, new_transfer_scoring=False))
    metrics, signal = b.measure(clip, Path(case['audio']))
    previous, _ = b.measure(clip, Path(native['audio']))
    for p, digest in bindings.items():
        assert b.sha(p) == digest, p
    report = dict(status='completed_existing_training_anchor_score', input_sha256=bindings,
        id=clip['id'], actual_BR_V=-15, theta=[0., 0., 1.], metrics=metrics,
        signal=signal, frozen_metrics=frozen['metrics'], old_BR0_network_metrics=previous,
        against_frozen=dc.contrast('flute', metrics, frozen['metrics']),
        against_old_BR0_network=dc.contrast('flute', metrics, previous),
        new_WAVs=0, parameter_fits=0, candidate_validation=False,
        new_transfer_scoring=False, production_changed=False,
        interpretation='One fixed boundary input: no calibration or preset-default adoption.')
    b.save(target / 'report.json', report)
    print(json.dumps({k: report[k] for k in ['metrics', 'against_frozen', 'against_old_BR0_network']}))


if __name__ == '__main__':
    main()
