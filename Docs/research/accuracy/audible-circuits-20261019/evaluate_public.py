#!/usr/bin/env python3
"""One unchanged frozen public battery; destination-only relocation, no fitting."""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

STUDY = Path(__file__).resolve().parent
ROOT = STUDY.parents[2]
ORIGINAL = ROOT / 'Docs/accuracy'
DEST = STUDY / 'public-frozen'
DECLARATION = STUDY / 'public-evaluation-plan.json'
PARENT = ORIGINAL / 'runs/sub-polarity-phase0/report.json'
PARENT_COMMIT = 'd5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43'
LABEL = 'audible-final'
sys.path.insert(0, str(ROOT / 'Tools/accuracy'))
sys.dont_write_bytecode = True
import run_battery as battery
import run_frozen
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def save_new(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x') as stream:
        stream.write(json.dumps(value, indent=2, allow_nan=False) + '\n')


def equal(a, b, path=''):
    if isinstance(a, dict):
        assert isinstance(b, dict) and set(a) == set(b), path
        for key in a:
            equal(a[key], b[key], path + '/' + key)
    elif isinstance(a, list):
        assert isinstance(b, list) and len(a) == len(b), path
        for i, (x, y) in enumerate(zip(a, b)):
            equal(x, y, path + '/' + str(i))
    elif isinstance(a, (float, int)) and not isinstance(a, bool):
        assert math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12), (path, a, b)
    else:
        assert a == b, (path, a, b)


def case_class(case):
    if case['id'].endswith('factory-physical'):
        return 'raw_factory_physical_flute'
    if case['id'].endswith('effective-compensated'):
        return 'previously_fitted_effective_flute'
    return 'conditional_effective_brass' if case['family'] == 'brass' else 'previously_fitted_dry_waveform'


def canonical_command(case, binary, audio):
    return [str(binary), '--events', str(ROOT / case['events']), '--output', str(audio),
            '--preset', case['preset'], '--patch', str(ROOT / case['patch']), '--rate', '48000',
            '--frames', str(case['frames']), '--seed', '50']


def validate_report(report, plan, recompute):
    assert report['status'] == 'pass' and report['render_count'] == 8
    assert report['plan_sha256'] == sha(ORIGINAL / 'plan.json')
    assert report['parameter_fits'] == report['gain_fits'] == 0
    assert report['strict_eligible'] is False
    assert sha(report['binary']) == report['binary_sha256']
    assert battery.read_identity(Path(report['binary']), Path(report['source_root'])) == report['compiled_source_sha256']
    assert [x['id'] for x in report['cases']] == [x['id'] for x in plan['cases']]
    for case, row in zip(plan['cases'], report['cases']):
        audio = ROOT / row['audio']
        assert sha(audio) == row['audio_sha256']
        assert row['command'] == canonical_command(case, report['binary'], audio)
        midi = battery.midi_check(ROOT / case['midi'], ROOT / case['events'])
        assert midi['sample_exact_schedule']
        rate, samples = wavfile.read(audio)
        stats = row['statistics']
        assert rate == stats['sample_rate'] == 48000 and samples.ndim == 1
        assert str(samples.dtype) == stats['sample_format'] == 'float32'
        assert len(samples) == stats['frames'] == case['frames'] and stats['channels'] == 1
        assert stats['preset'] == case['preset'] and stats['seed'] == case['seed'] == 50
        assert stats['normalization'] == 'none' and np.isfinite(samples).all()
        samples = samples.astype(float)
        actual = dict(finite=True, peak=float(np.max(np.abs(samples))),
                      rms=float(np.sqrt(np.mean(samples * samples))), energy=float(np.sum(samples * samples)),
                      samples_above_unity=int(np.sum(np.abs(samples) > 1)))
        equal(actual, row['signal'])
        for key in ('peak', 'rms', 'samples_above_unity'):
            assert math.isclose(actual[key], stats[key], rel_tol=5e-12, abs_tol=5e-12), key
        # Renderer stdout uses 12 significant digits; analysis JSON retains doubles.
        assert math.isclose(len(samples) / rate, stats['audio_seconds'], rel_tol=5e-12, abs_tol=5e-12)
        if recompute:
            metrics, signal = battery.measure(case, audio)
            equal(metrics, row['metrics'], case['id'] + '/metrics')
            equal(signal, row['signal'], case['id'] + '/signal')


def declare():
    assert not DECLARATION.exists() and not DEST.exists(), 'Refuse to redeclare or overwrite'
    plan = battery.verify_plan()
    parent = read(PARENT)
    validate_report(parent, plan, True)
    for name, digest in parent['compiled_source_sha256'].items():
        blob = subprocess.check_output(['git', 'show', PARENT_COMMIT + ':' + name], cwd=ROOT)
        assert hashlib.sha256(blob).hexdigest() == digest, name
    DEST.mkdir()
    (DEST / 'plan.json').write_bytes((ORIGINAL / 'plan.json').read_bytes())
    (DEST / 'inputs').symlink_to(ORIGINAL / 'inputs', target_is_directory=True)
    helpers = [Path(__file__), ROOT / 'Tools/accuracy/run_frozen.py', ROOT / 'Tools/accuracy/run_battery.py',
               ROOT / 'Tools/accuracy/metrics.py']
    save_new(DECLARATION, dict(
        status='declared_before_candidate_audio', declared_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        frozen_plan_sha256=sha(ORIGINAL / 'plan.json'), input_count=len(plan['input_sha256']),
        helper_sha256={str(p): sha(p) for p in helpers}, parent_commit=PARENT_COMMIT,
        parent_report=str(PARENT), parent_report_sha256=sha(PARENT),
        parent_binary_sha256=parent['binary_sha256'], parent_compiled_source_sha256=parent['compiled_source_sha256'],
        parent_audio_sha256={row['audio']: row['audio_sha256'] for row in parent['cases']},
        parent_all_metrics_recomputed=True, candidate_identity='must be explicitly bound before rendering',
        validation_tolerances=dict(recomputed_metrics_relative=1e-12, recomputed_metrics_absolute=1e-12,
                                   renderer_12_digit_stdout_relative=5e-12, renderer_12_digit_stdout_absolute=5e-12),
        runner='unchanged run_frozen.run; run_battery.BASE relocated only; ROOT canonical',
        destination=str(DEST), label=LABEL, renders=8, parameter_fits=0, gain_fits=0,
        parameter_gate_gain_mask_eligibility_changes=False, case_order=[x['id'] for x in plan['cases']],
        case_classes={x['id']: case_class(x) for x in plan['cases']},
        comparisons='Full per-case metric trees and predeclared spectral/level/tail/pitch summaries. No pooled accuracy percentage or success threshold.',
        direction='For nonnegative error metrics lower is better; deltas are candidate minus actual d5e0 parent. Signed pitch/slope values are descriptive.',
        listening=dict(order=['hardware', 'd5e0_parent', 'candidate'], recording_gains='exact frozen per-case gains',
                       silence_frames=12000, sample_rate=48000, common_gain='min(1,0.95/max(abs(all segments)))',
                       individual_normalization=False),
        limits=plan['limits'] + [
            'The two raw factory Flute cases are separate from six previously fitted or conditional effective cases.',
            'Raw factory means unmodified named Flute preset plus declared octave correction; globals, touch, exact gate and voice allocation are unknown.',
            'All eight cases use inferred MIDI and reused development recordings; none is a known-settings independent hardware holdout.',
            'All eight cases have ring mix zero. This battery cannot validate the new audible ring-switching path.',
            'Executable success means protocol/integrity completion, not proof of hardware accuracy or an improvement verdict.',
            'Listening artifact is local analysis material, not a source-audio redistribution license.'] ))
    print('Declared', DECLARATION, sha(DECLARATION), flush=True)


def check():
    d = read(DECLARATION)
    for path, digest in d['helper_sha256'].items():
        assert sha(path) == digest, path
    assert sha(ORIGINAL / 'plan.json') == sha(DEST / 'plan.json') == d['frozen_plan_sha256']
    assert (DEST / 'inputs').is_symlink() and (DEST / 'inputs').resolve() == ORIGINAL / 'inputs'
    assert sha(PARENT) == d['parent_report_sha256']
    for path, digest in d['parent_audio_sha256'].items():
        assert sha(ROOT / path) == digest, path
    battery.BASE = DEST
    assert run_frozen.b is battery and battery.ROOT == ROOT
    plan = battery.verify_plan()
    assert [x['id'] for x in plan['cases']] == d['case_order'] and len(plan['input_sha256']) == d['input_count']
    return d, plan


def metric_summary(case, metric):
    if case['family'] == 'flute':
        return {key: metric[key] for key in ('median_harmonic_MAE_db', 'trajectory_ratio_MAE_db',
                'held_gain_RMS_error_db', 'median_pitch_error_cents')} | {
                'RMS_slope_error_abs_db_per_s': abs(metric['native_RMS_slope_db_per_s'] - metric['source_RMS_slope_db_per_s'])}
    if case['family'] == 'dry':
        result = {}
        for win in metric['windows']:
            for subset in ('all_H2_H32', 'odd_H3_H31'):
                result[win['name'] + '/' + subset + '/trajectory_ratio_MAE_db'] = win[subset]['trajectory_ratio_MAE_db']
            result[win['name'] + '/held_gain_RMS_error_db'] = win['all_H2_H32']['held_gain_RMS_error_db']
        return result
    result = {'six_cell_mean_ratio_MAE_db': metric['spectral_score']['six_cell_mean_ratio_MAE_db'],
              'native_weak_partial_frames': metric['spectral_score']['total_native_weak_partial_frames']}
    for cell in metric['spectral_score']['cells']:
        result[f"note{cell['note']}/{cell['phase']}/ratio_MAE_db"] = cell['ratio_MAE_db']
    for i, error in enumerate(metric['tail_metrics']['per_note_floored_RMS_db']):
        result[f'note{[57,60,64][i]}/tail_floored_RMS_db'] = error
    for ratio in metric['C4_higher_ratios']:
        result[f"C4_H{ratio['numerator']}_H2_change_error_abs_db"] = abs(ratio['change_error_db'])
    return result


def compare():
    declaration, plan = check()
    candidate_path = DEST / 'runs' / LABEL / 'report.json'
    parent, candidate = read(PARENT), read(candidate_path)
    validate_report(parent, plan, True)
    validate_report(candidate, plan, True)
    ready = read(DEST / 'candidate-identity.json')
    assert candidate['binary_sha256'] == ready['binary_sha256']
    assert candidate['compiled_source_sha256'] == ready['compiled_source_sha256']
    rows, chunks, segments = [], [], []
    position = 0
    for case, p, c in zip(plan['cases'], parent['cases'], candidate['cases']):
        ps, cs = metric_summary(case, p['metrics']), metric_summary(case, c['metrics'])
        delta = {key: cs[key] - ps[key] for key in ps}
        rows.append(dict(id=case['id'], classification=case_class(case), settings_status=case['settings_status'],
                         midi_status=case['midi_status'], recording_gain=case['recording_gain'],
                         parent_metrics=p['metrics'], candidate_metrics=c['metrics'],
                         summary_parent=ps, summary_candidate=cs, summary_delta_candidate_minus_parent=delta,
                         parent_signal=p['signal'], candidate_signal=c['signal'],
                         parent_audio_sha256=p['audio_sha256'], candidate_audio_sha256=c['audio_sha256']))
        rate, source = battery.audio(ROOT / case['reference'])
        if rate != 48000:
            gcd = math.gcd(rate, 48000)
            source = resample_poly(source, 48000 // gcd, rate // gcd)
        source = source[:round(case['duration'] * 48000)]
        offset = round(case['offset_seconds'] * 48000)
        _, pa = battery.audio(ROOT / p['audio']); _, ca = battery.audio(ROOT / c['audio'])
        pa = pa[offset:offset + len(source)] * case['recording_gain']
        ca = ca[offset:offset + len(source)] * case['recording_gain']
        assert len(source) == len(pa) == len(ca)
        for kind, audio in [('hardware', source), ('d5e0_parent', pa), ('candidate', ca)]:
            segments.append(dict(case=case['id'], kind=kind, start_frame=position, frames=len(audio)))
            chunks.extend([audio, np.zeros(12000)])
            position += len(audio) + 12000
    combined = np.concatenate(chunks)
    gain = min(1., .95 / max(1e-15, float(np.max(np.abs(combined)))))
    listening = DEST / 'listening-hardware-parent-candidate.wav'
    assert not listening.exists()
    wavfile.write(listening, 48000, (combined * gain).astype(np.float32))
    report = dict(status='protocol_and_integrity_pass', declaration_sha256=sha(DECLARATION),
                  frozen_plan_sha256=declaration['frozen_plan_sha256'], parent_commit=PARENT_COMMIT,
                  parent_report_sha256=sha(PARENT), candidate_report_sha256=sha(candidate_path),
                  candidate_identity_sha256=sha(DEST / 'candidate-identity.json'),
                  parent_binary_sha256=parent['binary_sha256'], candidate_binary_sha256=candidate['binary_sha256'],
                  candidate_compiled_source_sha256=candidate['compiled_source_sha256'],
                  parent_and_candidate_metrics_recomputed=True, render_count=8, parameter_fits=0, gain_fits=0,
                  original_hardware_MIDI=False, strict_eligible=False, cases=rows,
                  listening=dict(path=str(listening), sha256=sha(listening), sample_rate=48000,
                                 frames=len(combined), common_playback_gain=gain, segments=segments,
                                 individual_normalization=False), limits=declaration['limits'])
    save_new(DEST / 'comparison.json', report)
    lines = ['# Frozen public-recording comparison', '',
             'Protocol and integrity checks passed. This is reused development evidence, not an independent hardware-accuracy certification.', '',
             'Parent is the actual d5e0 source identity retained by sub-polarity-phase0. All eight cases retain their original parameters, gains, MIDI/gates, masks and analysis windows. No fitting was performed.', '',
             'The two raw factory Flute cases are reported separately from the six effective/previously fitted cases. No aggregate accuracy percentage is computed. Negative error deltas indicate improvement; signed pitch errors are descriptive.', '']
    for row in rows:
        lines += ['## ' + row['id'], '', row['classification'] + '.', '',
                  '| Metric | d5e0 parent | Candidate | Delta |', '|---|---:|---:|---:|']
        for key, pv in row['summary_parent'].items():
            cv = row['summary_candidate'][key]
            lines.append(f'| {key} | {pv:.6f} | {cv:.6f} | {cv-pv:+.6f} |')
        lines += ['', row['settings_status'], '', row['midi_status'], '']
    lines += ['## Limits', ''] + ['- ' + x for x in declaration['limits']]
    lines += ['', 'Listening order per case: hardware → d5e0 parent → candidate. Frozen recording gains are retained; a single common playback gain of ' + str(gain) + ' is applied to the entire file. No individual normalization.', '']
    with (DEST / 'README.md').open('x') as stream:
        stream.write('\n'.join(lines))
    check()
    print('Compared', DEST / 'comparison.json', sha(DEST / 'comparison.json'), flush=True)


def run(args):
    _, plan = check()
    assert args.ready, 'Final candidate readiness must be explicitly supplied after root confirmation'
    binary, source = args.binary.resolve(), args.source_root.resolve()
    assert sha(binary) == args.binary_sha256
    identity = battery.read_identity(binary, source)
    assert identity['Source/DSP/ceres.cpp'] == args.cpp_sha256
    assert not (DEST / 'runs').exists() and not (DEST / 'run-identities').exists(), 'Refuse second run'
    save_new(DEST / 'candidate-identity.json', dict(binary=str(binary), source_root=str(source),
             binary_sha256=sha(binary), compiled_source_sha256=identity,
             source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip(),
             declaration_sha256=sha(DECLARATION), root_final_ready_confirmed=True))
    run_frozen.run(LABEL, binary, source)
    compare()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['declare', 'check', 'run', 'compare'])
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--source-root', type=Path)
    parser.add_argument('--binary-sha256')
    parser.add_argument('--cpp-sha256')
    parser.add_argument('--ready', action='store_true')
    args = parser.parse_args()
    if args.action == 'declare': declare()
    elif args.action == 'check': check(); print('Frozen predeclaration and inputs verified')
    elif args.action == 'compare': compare()
    else: run(args)
