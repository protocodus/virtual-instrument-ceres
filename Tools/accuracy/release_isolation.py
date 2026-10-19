#!/usr/bin/env python3
"""Predeclared effective-filter-release diagnostic; no DSP, gate or gain fitting."""
from pathlib import Path
import argparse
import json
import math
import re
import shutil
import subprocess
import numpy as np
from scipy.io import wavfile
import run_battery as b

ROOT = b.ROOT
OUT = b.BASE / 'release-isolation'
FACTORS = (1., .5, .25, .1)
BASELINE = b.BASE / 'runs/expression-55869df8/report.json'


def patch_values(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            key, value = line.split('=', 1)
            assert key.strip() not in values, 'Duplicate patch key'
            values[key.strip()] = float(value)
    return values


def frozen_plan():
    plan = b.read(OUT / 'plan.json')
    b.verify_plan()
    for path, digest in plan['input_sha256'].items():
        assert b.sha(ROOT / path) == digest, path
    for path, digest in plan['external_source_sha256'].items():
        assert b.sha(path) == digest, path
    assert b.read_identity(ROOT / plan['binary'], ROOT / plan['source_root']) == plan['compiled_source_sha256']
    return plan


def declare():
    assert not (OUT / 'plan.json').exists(), 'Never overwrite a declaration'
    battery = b.verify_plan()
    baseline = b.read(BASELINE)
    assert baseline['status'] == 'pass'
    assert baseline['plan_sha256'] == b.sha(b.BASE / 'plan.json')
    source, binary = Path(baseline['source_root']), Path(baseline['binary'])
    identity = b.read_identity(binary, source)
    assert identity == baseline['compiled_source_sha256']
    assert b.sha(binary) == baseline['binary_sha256']
    # Retain an independent identity, never a mutable build/ pointer.
    snapshot = OUT / 'identity'
    assert not snapshot.exists()
    snapshot.mkdir()
    for name, digest in identity.items():
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        assert b.sha(target) == digest
    retained = snapshot / 'ceres_render'
    shutil.copy2(binary, retained)
    assert b.read_identity(retained, snapshot) == identity
    raw = (snapshot / 'Source/DSP/ceres.cpp').read_text()
    match = re.search(r'\{"brass1",\s*\{([^}]+)\}\}', raw)
    assert match
    cv = [float(x) for x in match[1].split(',')]
    assert len(cv) == 22 and cv[14] == cv[20] == 10
    selected_release = 10 * 10 ** (-.3 * cv[14]) * math.log(1000) / 5
    cases, bindings = [], {}
    for original in battery['cases']:
        if original['family'] != 'brass':
            continue
        control = next(x for x in baseline['cases'] if x['id'] == original['id'])
        assert b.sha(ROOT / control['audio']) == control['audio_sha256']
        values = patch_values(ROOT / original['patch'])
        global_release = values.get('global_release', 0.)
        amp_release = max(values.get('amp_release', selected_release), global_release)
        filter_release = max(values.get('filter_release', selected_release), global_release)
        first_off = min(int(line.split()[0]) for line in (ROOT / original['events']).read_text().splitlines()
                        if line.strip() and not line.startswith('#') and line.split()[1] == 'off')
        assert not any(line.split()[1] in ('sustain', 'reset', 'panic', 'alloff')
                       for line in (ROOT / original['events']).read_text().splitlines()
                       if line.strip() and not line.startswith('#'))
        b.midi_check(ROOT / original['midi'], ROOT / original['events'])
        for factor in FACTORS:
            ident = original['id'] + '-filterR-' + format(factor, 'g')
            new = dict(values, global_release=0., amp_release=amp_release,
                       filter_release=factor * filter_release)
            assert all(new[key] == value for key, value in values.items()
                       if key not in ('global_release', 'amp_release', 'filter_release'))
            path = OUT / (ident + '.patch')
            assert not path.exists()
            path.write_text('# Predeclared release isolation; same preset, MIDI and other controls.\n' +
                            '\n'.join(f'{k}={v:.17g}' for k, v in new.items()) + '\n')
            bindings[b.rel(path)] = b.sha(path)
            audio = OUT / (ident + '.wav')
            cmd = [str(retained), '--events', str(ROOT / original['events']), '--output', str(audio),
                   '--preset', original['preset'], '--patch', str(path), '--rate', '48000',
                   '--frames', str(original['frames']), '--seed', '50']
            cases.append(dict(id=ident, history=int(original['id'].replace('brass-history', '')),
                release_factor=factor, original_case=original, patch=b.rel(path), audio=b.rel(audio), command=cmd,
                current_baseline_audio=control['audio'], current_baseline_audio_sha256=control['audio_sha256'],
                original_values=values, transformed_values=new, selected_factory_release_T60=selected_release,
                original_effective_amp_release_T60=amp_release,
                original_effective_filter_release_T60=filter_release,
                changed_effective_filter_release_T60=factor*filter_release,
                guaranteed_unchanged_prefix_frames=first_off))
        for key in ('patch', 'midi', 'events', 'reference'):
            bindings[original[key]] = b.sha(ROOT / original[key])
        bindings[control['audio']] = control['audio_sha256']
    assert len(cases) == 8
    paths = [Path(__file__), BASELINE, b.BASE/'plan.json', Path(b.__file__), ROOT/'Tools/accuracy/metrics.py',
             OUT/'source-review.md', OUT/'source-evidence.json', OUT/'.gitignore', retained]
    paths += [snapshot/name for name in identity]
    paths += [ROOT/name for name in battery['input_sha256']]
    bindings.update({b.rel(p): b.sha(p) for p in paths})
    evidence = b.read(OUT/'source-evidence.json')
    plan = dict(status='declared_before_audio', cases=cases, input_sha256=bindings,
        external_source_sha256=evidence['external_source_sha256'],
        binary=b.rel(retained), binary_sha256=b.sha(retained), source_root=b.rel(snapshot),
        compiled_source_sha256=identity, origin_identity=dict(binary=str(binary), source_root=str(source)),
        baseline_report=b.rel(BASELINE), factors=list(FACTORS), native_render_count=8,
        parameter_fits=0, gain_fits=0, gate_changes=0, original_hardware_MIDI=False, strict_eligible=False,
        metric_function='Unchanged Tools/accuracy/run_battery.py measure() and its frozen source masks/gain',
        primary_checks=['factor1 audio must match current baseline bytes',
            'all factors must match current baseline before first note-off',
            'exact effective amplifier-release coefficient preserved',
            'only global_release/amp_release/filter_release patch coordinates change'],
        transfer_criterion='For the same factor in both histories: six-cell score strictly improves; every note release-cell error and every per-note floored tail RMS are no worse; native weakness count does not increase. Report all outcomes regardless.',
        limits=['Engineering release-rate isolation, not a recovered CS50 law or slider position.',
            'D, Q, HP/LP, endpoint span, AM, gates, MIDI and recording gain remain frozen.',
            'Both histories and all recordings are reused development evidence, not an independent holdout.',
            'A favorable result cannot identify the physical release voltage or justify a default change.',
            'Published D/R range still supports current defaults; separate RF/RA trims do not identify a numeric ratio.',
            'Hardware original MIDI and exact global settings remain unknown.'])
    b.save(OUT/'plan.json', plan)
    print('DECLARED', b.sha(OUT/'plan.json'), flush=True)


def measurements(case):
    metrics, signal = b.measure(case['original_case'], ROOT/case['audio'])
    _, native = b.audio(ROOT/case['audio'])
    _, baseline = b.audio(ROOT/case['current_baseline_audio'])
    prefix = case['guaranteed_unchanged_prefix_frames']
    return dict(metrics=metrics, signal=signal,
        prefix_samples_identical=bool(np.array_equal(native[:prefix], baseline[:prefix])),
        baseline_bytes_identical=b.sha(ROOT/case['audio']) == case['current_baseline_audio_sha256'],
        max_sample_difference=float(np.max(np.abs(native-baseline))))


def summarize(rows):
    transfers = []
    for factor in FACTORS[1:]:
        results=[]
        for history in (20,10):
            base=next(r for r in rows if r['history']==history and r['release_factor']==1.)['metrics']
            new=next(r for r in rows if r['history']==history and r['release_factor']==factor)['metrics']
            bs,ns=base['spectral_score'],new['spectral_score']
            release_cells=[(a,bv) for a,bv in zip(ns['cells'],bs['cells']) if a['phase']=='release']
            results.append(dict(history=history,
                six_cell_improves=ns['six_cell_mean_ratio_MAE_db']<bs['six_cell_mean_ratio_MAE_db'],
                every_release_cell_not_worse=all(a['ratio_MAE_db']<=bv['ratio_MAE_db'] for a,bv in release_cells),
                every_tail_not_worse=all(a<=bv for a,bv in zip(new['tail_metrics']['per_note_floored_RMS_db'],base['tail_metrics']['per_note_floored_RMS_db'])),
                weak_count_not_worse=ns['total_native_weak_partial_frames']<=bs['total_native_weak_partial_frames']))
        transfers.append(dict(factor=factor, histories=results,
            passes=all(all(v for k,v in row.items() if k!='history') for row in results)))
    return transfers


def note(report):
    lines=['# Effective VCF release isolation', '',
        'Eight predeclared native conditions hold both reconstructed gate histories, factory D/Q and all other controls fixed. Global release is flattened into the selected amplifier duration, preserving AMP timing exactly; only effective VCF release varies. No DSP/default change or fit is made.', '',
        '| History | Filter R factor | Six-cell ratio MAE, dB | A3/C4 tail RMS, dB | E4 tail RMS, dB | Weak records |',
        '|---:|---:|---:|---:|---:|---:|']
    for row in report['cases']:
        m=row['metrics'];s=m['spectral_score'];t=m['tail_metrics']
        lines.append(f"| {row['history']} | {row['release_factor']:g} | {s['six_cell_mean_ratio_MAE_db']:.6f} | {t['A3_C4_joint_floored_RMS_db']:.6f} | {t['E4_floored_RMS_db']:.6f} | {s['total_native_weak_partial_frames']} |")
    lines += ['', f"Strict cross-history transfer criterion: {sum(r['passes'] for r in report['transfer'])}/3 factors pass. This criterion also requires no worsening of any note's release-cell/tail error and no increased weakness. All per-note/early/release/C4 higher-ratio values remain in report.json, including adverse outcomes.", '',
        'Both factor1 controls are byte-identical to the current renderer baseline. All eight renders preserve their guaranteed prefix before the first note-off. This is an implementation isolation check, not evidence that either gate history equals the hardware performance.', '',
        'These reused public recordings have named Brass I but no original MIDI, measured timing voltages or fully identified global modulation. The result tests effective filter-release sensitivity; it does not identify a physical timing law, repair the published A-versus-D/R discrepancy, or authorize a factory/default change.', '',
        'Reproduce without changing evidence: `python3 Tools/accuracy/release_isolation.py check`. The immutable plan binds the retained renderer and its five compiled inputs, exact commands, patches, MIDI, original source masks/gain, and unmodified measurement helper. `declare` and `run` refuse overwrite. Audio is local ignored analysis evidence, never a synthesis asset.','']
    return '\n'.join(lines)


def run():
    plan=frozen_plan()
    assert not (OUT/'report.json').exists(), 'Preserve completed evidence'
    assert not any((ROOT/c['audio']).exists() for c in plan['cases']), 'Preserve incomplete attempts'
    rows=[]
    for case in plan['cases']:
        process=subprocess.run(case['command'],capture_output=True,text=True)
        log=OUT/(case['id']+'.log')
        log.write_text(json.dumps(case['command'])+'\n'+process.stdout+process.stderr)
        if process.returncode:
            b.save(OUT/'failure.json',dict(case=case,returncode=process.returncode,log=b.rel(log),log_sha256=b.sha(log)))
            raise RuntimeError('Native failure retained: '+case['id'])
        row=dict(id=case['id'],history=case['history'],release_factor=case['release_factor'],
            audio=case['audio'],audio_sha256=b.sha(ROOT/case['audio']),command=case['command'],
            statistics=json.loads(process.stdout),log=b.rel(log),log_sha256=b.sha(log),**measurements(case))
        rows.append(row)
        b.save(OUT/(case['id']+'.json'),row)
        assert row['prefix_samples_identical']
        if case['release_factor']==1:assert row['baseline_bytes_identical']
        assert row['signal']['finite'] and row['signal']['peak']>0 and row['signal']['samples_above_unity']==0
        print(case['id'],row['metrics']['spectral_score']['six_cell_mean_ratio_MAE_db'],flush=True)
    frozen_plan()
    report=dict(status='pass',plan_sha256=b.sha(OUT/'plan.json'),cases=rows,transfer=summarize(rows),
        render_count=8,parameter_fits=0,gain_fits=0,gate_changes=0,DSP_changes=False,strict_eligible=False)
    b.save(OUT/'report.json',report)
    (OUT/'README.md').write_text(note(report))
    print('REPORT',b.sha(OUT/'report.json'),flush=True)


def check():
    plan=frozen_plan();report=b.read(OUT/'report.json')
    assert report['plan_sha256']==b.sha(OUT/'plan.json') and len(report['cases'])==8
    for case,row in zip(plan['cases'],report['cases']):
        assert case['id']==row['id'] and row['command']==case['command']
        assert b.sha(ROOT/row['audio'])==row['audio_sha256']
        assert b.sha(ROOT/row['log'])==row['log_sha256']
        for key,value in measurements(case).items():assert row[key]==value,(case['id'],key)
        assert b.read(OUT/(case['id']+'.json'))==row
    assert report['transfer']==summarize(report['cases'])
    assert (OUT/'README.md').read_text()==note(report)
    print('VERIFIED',b.sha(OUT/'report.json'))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=('declare','run','check'))
    args=parser.parse_args()
    {'declare':declare,'run':run,'check':check}[args.command]()
