#!/usr/bin/env python3
"""Frozen DC-intercept sensitivity experiment; no production DSP or audio fitting."""
from pathlib import Path
import argparse
import itertools
import math
import shutil
import subprocess
import json
import numpy as np
import run_battery as b

OUT = b.BASE / 'preset-dc-loading'
BASELINE = b.BASE / 'runs/expression-55869df8/report.json'


def patch_values(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            key, value = line.split('=', 1)
            assert key.strip() not in values
            values[key.strip()] = float(value)
    return values


def verify():
    b.verify_plan()
    plan = b.read(OUT / 'plan.json')
    for path, digest in plan['input_sha256'].items():
        assert b.sha(path) == digest, path
    assert b.read_identity(Path(plan['binary']), Path(plan['source_root'])) == plan['compiled_source_sha256']
    return plan


def declare():
    assert not (OUT / 'plan.json').exists(), 'Never overwrite a declaration'
    battery = b.verify_plan()
    baseline = b.read(BASELINE)
    evidence = b.read(OUT / 'source-evidence.json')
    for path, digest in evidence['input_sha256'].items():
        assert b.sha(path) == digest, path
    source, binary = Path(baseline['source_root']), Path(baseline['binary'])
    identity = b.read_identity(binary, source)
    assert identity == baseline['compiled_source_sha256']
    assert b.sha(binary) == baseline['binary_sha256']
    snapshot = OUT / 'identity'
    snapshot.mkdir()
    for name, digest in identity.items():
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        assert b.sha(target) == digest
    retained = snapshot / 'ceres_render'
    shutil.copy2(binary, retained)
    assert b.sha(retained) == baseline['binary_sha256']
    assert b.read_identity(retained, snapshot) == identity
    clips = [c for c in battery['cases'] if c['family'] == 'brass' or
             (c['family'] == 'flute' and c['id'].endswith('factory-physical'))]
    assert len(clips) == 4
    cases, bindings = [], {}
    parameters = evidence['parameters']

    def add(clip, kind, rank=None, drop=None, br=None, cutoffs=None, network_id=None):
        control = next(c for c in baseline['cases'] if c['id'] == clip['id'])
        assert b.sha(b.ROOT / control['audio']) == control['audio_sha256']
        b.midi_check(b.ROOT / clip['midi'], b.ROOT / clip['events'])
        original = patch_values(b.ROOT / clip['patch'])
        assert original.get('brilliance', 0.) == 0.
        values = original | (cutoffs or {})
        assert set(k for k in values if values[k] != original.get(k)) <= {'hp_cutoff', 'lp_cutoff'}
        suffix = '' if kind == 'frozen' else f'-{rank}-Vd{drop:g}-BR{br:g}'
        name = clip['id'] + '-' + kind + suffix
        patch = OUT / (name + '.patch')
        assert not patch.exists()
        patch.write_text('# DC-intercept-only comparison: only HP/LP reference frequencies change.\n' +
                         '\n'.join(f'{key}={value:.17g}' for key, value in values.items()) + '\n')
        wav = OUT / (name + '.wav')
        command = [str(retained), '--preset', clip['preset'], '--patch', str(patch),
                   '--events', str(b.ROOT / clip['events']), '--output', str(wav),
                   '--rate', str(clip['rate']), '--frames', str(clip['frames']), '--seed', str(clip['seed'])]
        cases.append(dict(id=name, kind=kind, rank=rank, diode_drop_V=drop, actual_BR_V=br,
            original_case=clip, source_network_id=network_id,
            original_values=original, transformed_values=values, patch=b.rel(patch),
            audio=b.rel(wav), command=command, baseline_audio=control['audio'],
            baseline_audio_sha256=control['audio_sha256'], baseline_metrics=control['metrics']))
        for path in [patch, b.ROOT / control['audio']] + [b.ROOT / clip[k] for k in ('reference', 'midi', 'events', 'patch')]:
            bindings[str(path)] = b.sha(path)

    for clip in clips:
        add(clip, 'frozen')
        for drop, br in itertools.product(evidence['grid']['diode_drop_V'], evidence['grid']['actual_BR_V']):
            # Same declared actual-bus calibration and diode drop, with a stiff
            # preset source: isolates loading from the changed BR convention.
            cutoffs = {}
            for stage, key in [('HP', 'hp_cutoff'), ('LP', 'lp_cutoff')]:
                spec = parameters['stage_parameters'][stage]
                rs = parameters['preset_resistors_ohms_and_EG_bounds'][clip['preset']][stage]
                r0 = parameters['shared_return_ohms']
                bus = 0. if rs is None else (10. - drop) * r0 / (rs + r0)
                exponent = spec['slope'] * (bus + parameters['preset_branch_ohms'] / spec['Rbr'] * br)
                cutoffs[key] = parameters['fixed_floor_Hz'] + spec['gm_anchor_Hz'] * 2 ** exponent
            add(clip, 'stiff', 'all', drop, br, cutoffs)
        for row in evidence['scenarios']:
            if row['preset'] != clip['preset']:
                continue
            cutoffs = {}
            for stage, key in [('HP', 'hp_cutoff'), ('LP', 'lp_cutoff')]:
                frequencies = row['stages'][stage]['reference_Hz']
                assert max(frequencies) - min(frequencies) < 1e-10
                cutoffs[key] = frequencies[0]
            assert all(5 <= value <= 20000 for value in cutoffs.values())
            add(clip, 'loaded', row['rank'], row['diode_drop_V'], row['actual_BR_V'], cutoffs, row['id'])
    assert len(cases) == 68
    paths = [Path(__file__).resolve(), BASELINE, b.BASE / 'plan.json', Path(b.__file__),
             b.ROOT / 'Tools/accuracy/metrics.py', OUT / 'source-review.md', OUT / 'source-evidence.json',
             OUT / 'network.py', OUT / '.gitignore', retained] + [snapshot / name for name in identity]
    bindings.update({str(path): b.sha(path) for path in paths})
    bindings.update(evidence['input_sha256'])
    b.save(OUT / 'plan.json', dict(status='declared_before_any_audio', cases=cases, input_sha256=bindings,
        binary=str(retained), source_root=str(snapshot), compiled_source_sha256=identity,
        native_render_count=68, grids=dict(frozen_controls=4, stiff_controls=16, loaded_conditions=48),
        parameter_fits=0, gain_fits=0, gate_changes=0, strict_eligible=False, DSP_changes=False,
        calibration='Same provisional actual-bus .7/.65 oct/V slopes and 12/180Hz zero-bus variable-gm anchors across both presets; centered bias, zero chip input current, exact resistor divider, fixed .4/.6V diodes.',
        interpretation='DC intercept at all four EGs=0 only; current EG scales, trajectories and cross-voice behavior retained. Replace entire baked Brass cutoff/BR term once. Native brilliance remains zero.',
        scoring='Unchanged battery.measure: fixed source windows, source masks, original MIDI, renderer seed and held recording gain. No recording changes or native exclusions.',
        criteria='Same rank/drop/BR across all four clips. Spectral transfer requires lower Flute median MAE and trajectory MSE plus lower Brass six-cell MAE. Stronger no-regression check additionally requires no increased held-gain Flute RMS, any Brass cell or per-note tail RMS, or weak count. Also report the same contrasts against matched stiff controls. Neither criterion establishes physical calibration.',
        limits=['All clips are reused development evidence without original hardware MIDI or complete controls.',
                'Actual-bus slope convention is a new explicit calibration hypothesis, not an identified chip law.',
                'Bias, internal input current, diode curve and ranks are not measured on the recorded instrument.',
                'This excludes envelope-dependent preset-bus feedback; its computed bound is in source review.',
                'No per-clip parameter choice or gain normalization; every tested result remains visible.',
                'Positive spectral results with level regressions may motivate further study, not a default change.']))
    print('DECLARED', b.sha(OUT / 'plan.json'), flush=True)


def measurements(case):
    metrics, signal = b.measure(case['original_case'], b.ROOT / case['audio'])
    _, rendered = b.audio(b.ROOT / case['audio'])
    _, baseline = b.audio(b.ROOT / case['baseline_audio'])
    return dict(metrics=metrics, signal=signal,
        baseline_bytes_identical=b.sha(b.ROOT / case['audio']) == case['baseline_audio_sha256'],
        max_sample_difference=float(np.max(np.abs(rendered - baseline))))


def contrast(family, candidate, control):
    if family == 'flute':
        spectral = candidate['median_harmonic_MAE_db'] < control['median_harmonic_MAE_db'] and candidate['trajectory_ratio_MSE_db2'] < control['trajectory_ratio_MSE_db2']
        return dict(spectral_improves=spectral,
                    no_regression=spectral and candidate['held_gain_RMS_error_db'] <= control['held_gain_RMS_error_db'])
    a, z = candidate['spectral_score'], control['spectral_score']
    assert [(c['note'], c['phase']) for c in a['cells']] == [(c['note'], c['phase']) for c in z['cells']]
    spectral = a['six_cell_mean_ratio_MAE_db'] < z['six_cell_mean_ratio_MAE_db']
    no_worse = all(c['ratio_MAE_db'] <= d['ratio_MAE_db'] for c, d in zip(a['cells'], z['cells']))
    no_worse &= a['total_native_weak_partial_frames'] <= z['total_native_weak_partial_frames']
    no_worse &= all(c <= d for c, d in zip(candidate['tail_metrics']['per_note_floored_RMS_db'], control['tail_metrics']['per_note_floored_RMS_db']))
    return dict(spectral_improves=spectral, no_regression=bool(spectral and no_worse))


def transfers(plan, rows):
    by_id = {r['id']: r for r in rows}
    results = []
    for rank, drop, br in itertools.product('ABC', (.4, .6), (0., -15.)):
        conditions = [c for c in plan['cases'] if c['kind'] == 'loaded' and (c['rank'], c['diode_drop_V'], c['actual_BR_V']) == (rank, drop, br)]
        assert len(conditions) == 4
        comparisons = []
        for c in conditions:
            stiff = next(x for x in plan['cases'] if x['kind'] == 'stiff' and x['original_case']['id'] == c['original_case']['id'] and (x['diode_drop_V'], x['actual_BR_V']) == (drop, br))
            metrics = by_id[c['id']]['metrics']
            comparisons.append(dict(clip=c['original_case']['id'],
                against_frozen=contrast(c['original_case']['family'], metrics, c['baseline_metrics']),
                against_stiff=contrast(c['original_case']['family'], metrics, by_id[stiff['id']]['metrics'])))
        results.append(dict(rank=rank, diode_drop_V=drop, actual_BR_V=br, clips=comparisons,
            spectral_transfer_vs_frozen=all(x['against_frozen']['spectral_improves'] for x in comparisons),
            no_regression_vs_frozen=all(x['against_frozen']['no_regression'] for x in comparisons),
            spectral_transfer_vs_stiff=all(x['against_stiff']['spectral_improves'] for x in comparisons),
            no_regression_vs_stiff=all(x['against_stiff']['no_regression'] for x in comparisons)))
    return results


def run():
    plan = verify()
    assert not (OUT / 'report.json').exists(), 'Preserve completed evidence'
    assert not any((b.ROOT / c['audio']).exists() for c in plan['cases']), 'Preserve incomplete attempts'
    rows = []
    for case in plan['cases']:
        process = subprocess.run(case['command'], capture_output=True, text=True)
        log = OUT / (case['id'] + '.log')
        log.write_text(json.dumps(case['command']) + '\n' + process.stdout + process.stderr)
        if process.returncode:
            b.save(OUT / 'failure.json', dict(case=case, returncode=process.returncode, log=b.rel(log)))
            raise RuntimeError('Renderer failed: ' + case['id'])
        row = dict(id=case['id'], kind=case['kind'], audio=case['audio'], audio_sha256=b.sha(b.ROOT / case['audio']),
            command=case['command'], statistics=json.loads(process.stdout), log=b.rel(log), log_sha256=b.sha(log), **measurements(case))
        rows.append(row)
        b.save(OUT / (case['id'] + '.json'), row)
        assert row['signal']['finite'] and row['signal']['peak'] > 0 and row['signal']['samples_above_unity'] == 0
        if case['kind'] == 'frozen':
            assert row['baseline_bytes_identical'] and row['metrics'] == case['baseline_metrics']
        print(case['id'], 'complete', flush=True)
    verify()
    b.save(OUT / 'report.json', dict(status='pass', plan_sha256=b.sha(OUT / 'plan.json'), cases=rows,
        transfer=transfers(plan, rows), render_count=len(rows), parameter_fits=0, gain_fits=0,
        gate_changes=0, DSP_changes=False, strict_eligible=False))
    print('REPORT', b.sha(OUT / 'report.json'), flush=True)


def check():
    plan, report = verify(), b.read(OUT / 'report.json')
    assert report['plan_sha256'] == b.sha(OUT / 'plan.json')
    assert len(report['cases']) == len(plan['cases']) == 68
    for case, row in zip(plan['cases'], report['cases']):
        assert case['id'] == row['id'] and row['command'] == case['command']
        assert b.sha(b.ROOT / row['audio']) == row['audio_sha256']
        assert b.sha(b.ROOT / row['log']) == row['log_sha256']
        assert b.read(OUT / (case['id'] + '.json')) == row
        for key, value in measurements(case).items():
            assert row[key] == value, (case['id'], key)
        if case['kind'] == 'frozen':
            assert row['baseline_bytes_identical'] and row['metrics'] == case['baseline_metrics']
    assert report['transfer'] == transfers(plan, report['cases'])
    print('VERIFIED', b.sha(OUT / 'report.json'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('declare', 'run', 'check'))
    args = parser.parse_args()
    {'declare': declare, 'run': run, 'check': check}[args.command]()
