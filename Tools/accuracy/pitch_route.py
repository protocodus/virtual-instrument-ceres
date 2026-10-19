#!/usr/bin/env python3
"""Declared native pitch-route diagnostic; no fitting or candidate selection.

prepare_cases() writes only patches and returns explicit commands for the separately
owned declaration. run/check require that declaration and preserve every attempt.
"""
from pathlib import Path
import argparse
import contextlib
import io
import itertools
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import traceback

import numpy as np
import scipy
import run_battery as b
import preset_dc_loading as dc

ROOT = b.ROOT
OUT = b.BASE / 'pitch-route'
PLAN = OUT / 'plan.json'
RUN = OUT / 'run-001'
BASELINE = b.BASE / 'runs/expression-55869df8/report.json'
JOINT_PLAN = b.BASE / 'joint-calibration/plan.json'
JOINT_REPORT = b.BASE / 'joint-calibration/native-controls-001/report.json'
BR_REPORT = b.BASE / 'brightness-boundary/qualify-001/report.json'
ROUTES = ('shared', 'vco-only', 'static-master')
CONTEXTS = ('frozen', 'network-br0', 'network-brminus15')
RAW_IDS = ('flute-high-factory-physical', 'flute-lower-factory-physical')


def path(value):
    p = Path(value)
    return p if p.is_absolute() else ROOT / p


def write(p, value):
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write('\n')


def text(p, value):
    with Path(p).open('x') as stream:
        stream.write(value)


def runtime():
    stream = io.StringIO()
    with contextlib.redirect_stdout(stream):
        np.show_config()
    return dict(python=sys.version, executable=sys.executable, platform=platform.platform(),
                numpy=np.__version__, scipy=scipy.__version__, numpy_config=stream.getvalue(),
                thread_environment={k: os.environ.get(k) for k in
                    ('OPENBLAS_NUM_THREADS', 'OMP_NUM_THREADS', 'MKL_NUM_THREADS',
                     'VECLIB_MAXIMUM_THREADS', 'NUMEXPR_NUM_THREADS')})


def first_high_bend(battery):
    clip = next(c for c in battery['cases'] if c['id'] == RAW_IDS[0])
    bends = []
    for order, line in enumerate(path(clip['events']).read_text().splitlines()):
        line = line.split('#', 1)[0].strip()
        if line:
            frame, kind, note, value = line.split()
            if kind == 'bend':
                bends.append((int(frame), order, float(value)))
    assert bends and all(math.isfinite(x[2]) for x in bends)
    frame, order, value = min(bends)
    assert frame == 0 and value == .367919921875
    return dict(semitones=value, frame=frame, file_line_1based=order + 1,
                case=clip['id'], events=clip['events'], events_sha256=b.sha(path(clip['events'])),
                interpretation='One deterministic common reference, not an identified physical DC estimate.')


def network_values(clip, context):
    values = dc.patch_values(path(clip['patch']))
    assert not any(k.startswith('calibration_') for k in values)
    for key in ('hp_cutoff', 'lp_cutoff', 'hp_envelope_scale', 'lp_envelope_scale'):
        values.pop(key, None)
    values.update(calibration_network=1., calibration_hp_bias=0., calibration_lp_bias=0.,
                  calibration_alpha=1., brilliance=0. if context == 'network-br0' else -15.)
    return values


def expected_command(case, binary, master):
    c = case['original_case']
    command = [str(path(binary)), '--preset', c['preset'], '--patch', str(path(case['patch'])),
               '--events', str(path(c['events'])), '--output', str(path(case['audio'])),
               '--rate', str(c['rate']), '--frames', str(c['frames']), '--seed', str(c['seed']),
               '--research-pitch-route', case['route']]
    if case['route'] == 'static-master':
        command += ['--research-master-semitones', format(master, '.17g')]
    return command


def reference_catalog():
    """Read identity metadata only; never select a reference from a sound score."""
    refs = {}
    for c in b.read(BASELINE)['cases']:
        refs[(c['id'], 'frozen')] = dict(audio=str(path(c['audio'])), sha256=c['audio_sha256'],
                                        origin=str(BASELINE), original_id=c['id'])
    declared = b.read(JOINT_PLAN)['native_controls']['cases']
    retained = {c['id']: c for c in b.read(JOINT_REPORT)['cases']}
    for c in declared:
        if c['kind'] == 'unfitted' and c['original_case']['id'] in RAW_IDS:
            saved = retained[c['id']]
            assert path(c['audio']) == path(saved['audio'])
            refs[(c['original_case']['id'], 'network-br0')] = dict(audio=str(path(saved['audio'])),
                sha256=saved['audio_sha256'], origin=str(JOINT_REPORT), original_id=c['id'])
    br = b.read(BR_REPORT)
    assert br['passed'] is True
    c = br['native_anchor_case']
    assert c['original_case']['id'] == RAW_IDS[0] and c['theta'] == [0., 0., 1.]
    digest = br['input_sha256'].get(str(path(c['audio'])))
    if digest is None:
        digest = br['artifact_sha256'][str(path(c['audio']))]
    refs[(RAW_IDS[0], 'network-brminus15')] = dict(audio=str(path(c['audio'])), sha256=digest,
        origin=str(BR_REPORT), original_id=c['id'])
    assert len(refs) == 11
    return refs


def prepare_cases(binary):
    """Called by root's declaration builder only; no renderer or target analysis."""
    assert not PLAN.exists() and not RUN.exists(), 'Do not alter a declared or begun study'
    folder = OUT / 'patches'
    assert not folder.exists(), 'Preserve previous preparation attempts'
    battery = b.read(b.BASE / 'plan.json')
    master = first_high_bend(battery)['semitones']
    refs = reference_catalog()
    folder.mkdir(parents=True)
    cases = []

    def add(clip, context, route, patch, role):
        name = (clip['id'] + '--' + context + '--' + route if role == 'matrix'
                else 'compat--' + clip['id'])
        c = dict(id=name, role=role, original_case=clip, context=context, route=route,
                 patch=str(path(patch)), patch_sha256=b.sha(path(patch)), audio=str(RUN / (name + '.wav')),
                 matched_shared_id=(clip['id'] + '--' + context + '--shared' if role == 'matrix' else name),
                 byte_exact_reference=refs.get((clip['id'], context)) if route == 'shared' else None)
        c['command'] = expected_command(c, binary, master)
        cases.append(c)

    for clip_id in RAW_IDS:
        clip = next(c for c in battery['cases'] if c['id'] == clip_id)
        for context in CONTEXTS:
            patch = path(clip['patch'])
            if context != 'frozen':
                patch = folder / (clip_id + '--' + context + '.patch')
                text(patch, '# Fixed theta0 network context; original events, source, Q, EG and gain.\n' +
                     ''.join(f'{key}={value:.17g}\n' for key, value in network_values(clip, context).items()))
            for route in ROUTES:
                add(clip, context, route, patch, 'matrix')
    for clip in battery['cases']:
        if clip['id'] not in RAW_IDS:
            add(clip, 'frozen', 'shared', path(clip['patch']), 'compatibility')
    assert len(cases) == 24 and sum(c['byte_exact_reference'] is not None for c in cases) == 11
    return cases


def required_inputs(plan):
    renderer = plan['renderer']
    files = {Path(__file__).resolve(), path(b.__file__), path(dc.__file__), ROOT / 'Tools/accuracy/metrics.py',
             b.BASE / 'plan.json', BASELINE, JOINT_PLAN, JOINT_REPORT, BR_REPORT,
             OUT / 'method-review.md', OUT / 'source-review.md', OUT / 'code-review.md', path(renderer['binary'])}
    assert plan['state_invariance_evidence'], 'Native state invariants must have bound evidence'
    files.update(path(p) for p in plan['state_invariance_evidence'])
    files.update(path(renderer['source_root']) / p for p in b.IDENTITY)
    for c in plan['cases']:
        files.add(path(c['patch']))
        files.update(path(c['original_case'][key]) for key in ('reference', 'patch', 'events', 'midi'))
        if c['byte_exact_reference']:
            files.add(path(c['byte_exact_reference']['audio']))
    return {p.resolve() for p in files}


def verify():
    plan = b.read(PLAN)
    assert plan['schema'] == 'ceres-pitch-route-v1'
    assert plan['routes'] == list(ROUTES) and plan['contexts'] == list(CONTEXTS)
    assert plan['fixed_calibration'] == [0., 0., 1.]
    assert plan['native_render_count'] == 24 and plan['known_byte_exact_reference_count'] == 11
    assert plan['score_only_after_all_renders'] is True
    assert plan['numeric_replay_tolerance'] == 1e-10
    for name, digest in plan['input_sha256'].items():
        assert b.sha(path(name)) == digest, name
    bindings = {path(name).resolve(): digest for name, digest in plan['input_sha256'].items()}
    assert required_inputs(plan) <= set(bindings), 'Missing mandatory input binding'
    battery = b.verify_plan()
    assert plan['common_master_semitones'] == first_high_bend(battery)['semitones']
    renderer = plan['renderer']
    assert b.sha(path(renderer['binary'])) == renderer['binary_sha256']
    assert b.read_identity(path(renderer['binary']), path(renderer['source_root'])) == renderer['compiled_source_sha256']
    original = {c['id']: c for c in battery['cases']}
    refs = reference_catalog()
    cases = plan['cases']
    assert len(cases) == len({c['id'] for c in cases}) == len({c['audio'] for c in cases}) == 24
    triples = set()
    compat = []
    for c in cases:
        clip = c['original_case']
        assert clip == original[clip['id']]
        assert c['route'] in ROUTES and c['context'] in CONTEXTS
        assert c['command'] == expected_command(c, renderer['binary'], plan['common_master_semitones'])
        assert path(c['audio']) == RUN / (c['id'] + '.wav')
        assert b.sha(path(c['patch'])) == c['patch_sha256']
        expected_ref = refs.get((clip['id'], c['context'])) if c['route'] == 'shared' else None
        assert c['byte_exact_reference'] == expected_ref
        if expected_ref:
            assert b.sha(path(expected_ref['audio'])) == expected_ref['sha256']
        if c['role'] == 'matrix':
            assert clip['id'] in RAW_IDS and clip['family'] == 'flute'
            assert c['id'] == clip['id'] + '--' + c['context'] + '--' + c['route']
            assert c['matched_shared_id'] == clip['id'] + '--' + c['context'] + '--shared'
            triples.add((clip['id'], c['context'], c['route']))
        else:
            assert c['role'] == 'compatibility' and c['context'] == 'frozen' and c['route'] == 'shared'
            assert c['id'] == 'compat--' + clip['id'] and c['matched_shared_id'] == c['id']
            compat.append(clip['id'])
        if c['context'] == 'frozen':
            assert path(c['patch']) == path(clip['patch'])
        else:
            assert dc.patch_values(path(c['patch'])) == network_values(clip, c['context'])
    assert triples == set(itertools.product(RAW_IDS, CONTEXTS, ROUTES))
    assert set(compat) == set(original) - set(RAW_IDS) and len(compat) == 6
    assert sum(c['byte_exact_reference'] is not None for c in cases) == 11
    return plan


def artifacts(folder):
    return {str(p.resolve()): b.sha(p) for p in sorted(folder.rglob('*')) if p.is_file()
            and p.name not in ('report.json', 'failure.json')
            and 'check-001' not in p.relative_to(folder).parts}


def render(plan, case):
    process = subprocess.run(case['command'], text=True, capture_output=True)
    log = RUN / (case['id'] + '.log')
    text(log, json.dumps(case['command']) + '\nSTDOUT\n' + process.stdout + '\nSTDERR\n' + process.stderr)
    assert process.returncode == 0, (case['id'], process.returncode, str(log))
    statistics = json.loads(process.stdout)
    assert statistics['research_pitch_route'] == case['route']
    assert statistics['research_master_semitones'] == (plan['common_master_semitones']
                                                     if case['route'] == 'static-master' else 0.)
    original = case['original_case']
    assert statistics['frames'] == original['frames'] and statistics['sample_rate'] == original['rate']
    assert statistics['seed'] == original['seed'] and statistics['preset'] == original['preset']
    assert statistics['channels'] == 1 and statistics['sample_format'] == 'float32'
    rate, audio = b.audio(path(case['audio']))
    assert rate == original['rate'] and len(audio) == original['frames']
    peak = float(np.max(np.abs(audio)))
    assert 0 < peak <= 1. and statistics['samples_above_unity'] == 0
    digest = b.sha(path(case['audio']))
    reference = case['byte_exact_reference']
    receipt = dict(id=case['id'], command=case['command'], audio=case['audio'], audio_sha256=digest,
                   statistics=statistics, peak=peak, log=str(log), log_sha256=b.sha(log),
                   byte_exact_reference=reference,
                   byte_exact_pass=None if reference is None else digest == reference['sha256'])
    write(RUN / (case['id'] + '-render.json'), receipt)
    assert receipt['byte_exact_pass'] is not False, ('Shared compatibility failed', case['id'])
    return receipt


def waveform_difference(a, z):
    ar, ax = b.audio(path(a))
    zr, zx = b.audio(path(z))
    assert ar == zr and ax.shape == zx.shape
    difference = ax - zx
    return dict(sample_rate=ar, frames=len(ax), bytes_identical=b.sha(path(a)) == b.sha(path(z)),
                samples_identical=bool(np.array_equal(ax, zx)), different_samples=int(np.count_nonzero(difference)),
                max_absolute_difference=float(np.max(np.abs(difference))),
                RMS_difference=float(np.sqrt(np.mean(difference * difference))), gain_fit=False)


def metric_difference(a, z):
    assert set(a) == set(z)
    return {key: a[key] - z[key] for key in a if type(a[key]) in (float, int)}


def analyze(plan, receipts):
    """First target read is here, after all 24 render receipts and exact gates."""
    assert len(receipts) == 24 and all(r['byte_exact_pass'] is not False for r in receipts)
    assert sum(r['byte_exact_pass'] is True for r in receipts) == 11
    rows = []
    for c, receipt in zip(plan['cases'], receipts):
        assert c['id'] == receipt['id'] and b.sha(path(c['audio'])) == receipt['audio_sha256']
        metrics, signal = b.measure(c['original_case'], path(c['audio']))
        rows.append(dict(id=c['id'], role=c['role'], original_id=c['original_case']['id'],
                         context=c['context'], route=c['route'], metrics=metrics, signal=signal))
    by_id = {r['id']: r for r in rows}
    case_map = {c['id']: c for c in plan['cases']}
    contrasts = []
    for c in plan['cases']:
        if c['role'] != 'matrix' or c['route'] == 'shared':
            continue
        a, z = by_id[c['id']], by_id[c['matched_shared_id']]
        contrasts.append(dict(case=c['id'], control=c['matched_shared_id'],
            canonical_contrast=dc.contrast('flute', a['metrics'], z['metrics']),
            metric_difference=metric_difference(a['metrics'], z['metrics']),
            waveform_difference=waveform_difference(c['audio'], case_map[z['id']]['audio'])))
    direct = []
    for clip, context in itertools.product(RAW_IDS, CONTEXTS):
        aid, zid = clip + '--' + context + '--static-master', clip + '--' + context + '--vco-only'
        a, z = by_id[aid], by_id[zid]
        direct.append(dict(case=aid, control=zid, constant_key_exponent_difference=plan['common_master_semitones'] / 12.,
            canonical_contrast=dc.contrast('flute', a['metrics'], z['metrics']),
            metric_difference=metric_difference(a['metrics'], z['metrics']),
            waveform_difference=waveform_difference(case_map[aid]['audio'], case_map[zid]['audio'])))
    return dict(cases=rows, altered_vs_matched_shared=contrasts, static_master_vs_vco_only=direct,
                audio_estimated_F0='Descriptive only; no exact equality asserted. Oscillator invariance is established by bound native state tests.',
                aggregate_winner_selected=False, optimizer_calls=0, parameter_fits=0, gain_fits=0,
                production_default_changed=False, physical_route_identified=False,
                strict_eligible=False, user_fidelity_goal_proven_complete=False)


def run():
    plan = verify()
    assert not RUN.exists(), 'Preserve all completed and partial attempts; do not retry in place'
    assert not any(path(c['audio']).exists() for c in plan['cases'])
    RUN.mkdir()
    try:
        shutil.copyfile(__file__, RUN / 'producer.py')
        shutil.copyfile(PLAN, RUN / 'declaration.json')
        write(RUN / 'runtime.json', runtime())
        receipts = []
        for c in plan['cases']:
            b.midi_check(path(c['original_case']['midi']), path(c['original_case']['events']))
            receipts.append(render(plan, c))
            print('RENDERED', len(receipts), '/24', c['id'], flush=True)
        verify()
        write(RUN / 'all-renders-complete.json', dict(render_count=24, known_byte_exact_pass_count=11,
             hardware_scores_computed=False, receipts_sha256={str(RUN / (c['id'] + '-render.json')):
             b.sha(RUN / (c['id'] + '-render.json')) for c in plan['cases']}))
        result = analyze(plan, receipts)
        verify()
        write(RUN / 'report.json', dict(status='completed_diagnostic_not_promotion', plan_sha256=b.sha(PLAN),
             native_render_count=24, known_byte_exact_reference_count=11, render_receipts=receipts,
             input_sha256=plan['input_sha256'], artifact_sha256=artifacts(RUN), analysis=result))
        print('REPORT', b.sha(RUN / 'report.json'), flush=True)
    except Exception:
        write(RUN / 'failure.json', dict(status='failed_attempt_preserved', traceback=traceback.format_exc(),
                                        plan_sha256=b.sha(PLAN), artifact_sha256=artifacts(RUN)))
        raise


def differences(expected, actual, where='$'):
    """Retain every exact difference, while testing declared scalar tolerance."""
    rows = []
    if type(expected) is not type(actual):
        return [dict(path=where, expected=expected, actual=actual, kind='type', within_tolerance=False)]
    if isinstance(expected, dict):
        if set(expected) != set(actual):
            return [dict(path=where, expected_keys=sorted(expected), actual_keys=sorted(actual),
                         kind='keys', within_tolerance=False)]
        for key in expected:
            rows.extend(differences(expected[key], actual[key], where + '.' + str(key)))
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            return [dict(path=where, expected_length=len(expected), actual_length=len(actual),
                         kind='length', within_tolerance=False)]
        for index, (a, z) in enumerate(zip(expected, actual)):
            rows.extend(differences(a, z, where + '[' + str(index) + ']'))
    elif expected != actual:
        if isinstance(expected, float):
            delta = abs(expected - actual)
            rows.append(dict(path=where, expected=expected, actual=actual, kind='float',
                             absolute_difference=delta, within_tolerance=math.isfinite(delta) and delta <= 1e-10))
        else:
            rows.append(dict(path=where, expected=expected, actual=actual, kind='exact', within_tolerance=False))
    return rows


def check():
    plan = verify()
    report_path = RUN / 'report.json'
    saved = b.read(report_path)
    assert saved['plan_sha256'] == b.sha(PLAN) and saved['input_sha256'] == plan['input_sha256']
    for name, digest in saved['artifact_sha256'].items():
        assert b.sha(path(name)) == digest, name
    folder = RUN / 'check-001'
    assert not folder.exists(), 'Preserve previous replay attempts'
    folder.mkdir()
    try:
        write(folder / 'runtime.json', runtime())
        receipts = [b.read(RUN / (c['id'] + '-render.json')) for c in plan['cases']]
        assert receipts == saved['render_receipts']
        for c, receipt in zip(plan['cases'], receipts):
            assert c['id'] == receipt['id'] and c['command'] == receipt['command']
            assert b.sha(path(c['audio'])) == receipt['audio_sha256']
            assert receipt['byte_exact_reference'] == c['byte_exact_reference']
            if c['byte_exact_reference']:
                assert receipt['audio_sha256'] == c['byte_exact_reference']['sha256']
        actual = analyze(plan, receipts)
        write(folder / 'actual.json', actual)
        delta = differences(saved['analysis'], actual)
        write(folder / 'exact-differences.json', delta)
        passed = all(d['within_tolerance'] for d in delta)
        write(folder / 'report.json', dict(status='pass' if passed else 'fail',
             report_sha256=b.sha(report_path), plan_sha256=b.sha(PLAN), bitwise_equal=not delta,
             numeric_replay_tolerance=plan['numeric_replay_tolerance'], numeric_replay_pass=passed,
             tolerance_units='Absolute scalar roundoff allowance in each reported native metric unit; hashes, types, non-floats and booleans remain exact.',
             runtime_exactly_matches_original=b.read(folder / 'runtime.json') == b.read(RUN / 'runtime.json'),
             exact_difference_count=len(delta), artifact_sha256=artifacts(folder),
             old_failures_or_reports_rewritten=False, new_WAV_count=0))
        verify()
        assert passed, 'Numerical replay failed; preserve actual.json and exact-differences.json'
        print('CHECK', b.sha(folder / 'report.json'), 'exact', not delta, flush=True)
    except Exception:
        write(folder / 'failure.json', dict(status='failed_replay_preserved', traceback=traceback.format_exc()))
        raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('run', 'check'))
    args = parser.parse_args()
    {'run': run, 'check': check}[args.command]()
