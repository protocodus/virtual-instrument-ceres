#!/usr/bin/env python3
"""One frozen candidate, eight native renders, unchanged recording metrics."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import numpy as np
import run_battery as b
import preset_dc_loading as dc

OUT=b.BASE/'joint-calibration'
TARGET=OUT/'native-validation-001'
CANDIDATE=OUT/'fit-001/candidate.json'
CANDIDATE_SHA='0d77a1fe3e27119869e1be4f2fef2f5bfab779b3dcd5dc0d61edda8aac021245'

def declare():
    assert b.sha(CANDIDATE)==CANDIDATE_SHA
    candidate=b.read(CANDIDATE);p=b.read(OUT/'plan.json');fit=b.read(OUT/'fit-plan.json')
    assert candidate['status']=='one_candidate_frozen' and not candidate['transfer_metrics_read']
    assert fit['native_validation']['remaining_render_count']==8
    for path,h in p['input_sha256'].items():assert b.sha(path)==h,path
    for path,h in fit['input_sha256'].items():assert b.sha(path)==h,path
    assert not TARGET.exists(),'Preserve every completed/incomplete attempt'
    TARGET.mkdir();shutil.copy2(Path(__file__),TARGET/'joint_validate.py')
    source=b.verify_plan();baseline=b.read(b.BASE/'runs/expression-55869df8/report.json')
    cases=[];paths=[Path(__file__),CANDIDATE,OUT/'plan.json',OUT/'fit-plan.json',
        b.BASE/'runs/expression-55869df8/report.json',OUT/'native-controls-001/report.json',
        Path(dc.__file__),b.ROOT/'Tools/accuracy/metrics.py',b.ROOT/'Tools/accuracy/run_battery.py']
    theta=candidate['candidate'];assert len(theta)==3
    binary=Path(p['native_controls']['binary'])
    for c in source['cases']:
        original=next(x for x in baseline['cases'] if x['id']==c['id'])
        active=c['family']=='brass' or c['id'] in ['flute-high-factory-physical','flute-lower-factory-physical']
        patch=b.ROOT/c['patch']
        if active:
            values=dc.patch_values(patch)
            for k in ['hp_cutoff','lp_cutoff','hp_envelope_scale','lp_envelope_scale']:values.pop(k,None)
            values.update(calibration_network=2 if c['family']=='brass' else 1,
                calibration_hp_bias=theta[0],calibration_lp_bias=theta[1],calibration_alpha=theta[2],
                brilliance=-15. if c['family']=='brass' else 0.)
            patch=TARGET/(c['id']+'.patch')
            patch.write_text('# Single shared candidate, fixed before transfer scoring.\n'+'\n'.join(f'{k}={v:.17g}' for k,v in values.items())+'\n')
        wav=TARGET/(c['id']+'.wav')
        command=[str(binary),'--preset',c['preset'],'--patch',str(patch),'--events',str(b.ROOT/c['events']),
            '--output',str(wav),'--rate','48000','--frames',str(c['frames']),'--seed',str(c['seed'])]
        cases.append(dict(id=c['id'],active_network=active,original_case=c,patch=str(patch),
            audio=str(wav),command=command,baseline_metrics=original['metrics'],
            baseline_audio=str(b.ROOT/original['audio']),baseline_audio_sha256=original['audio_sha256']))
        paths += [patch,b.ROOT/c['events'],b.ROOT/c['midi'],b.ROOT/c['reference'],b.ROOT/original['audio']]
        if active:
            control=next(x for x in p['native_controls']['cases'] if x['kind']=='unfitted' and x['original_case']['id']==c['id'])
            cases[-1]['unfitted_audio']=control['audio'];paths += [Path(control['audio'])]
    b.save(TARGET/'plan.json',dict(status='declared_before_candidate_native_renders_or_transfer_scores',
        candidate_sha256=CANDIDATE_SHA,candidate=theta,binary=str(binary),binary_sha256=b.sha(binary),
        cases=cases,render_count=8,total_new_native_WAVs=20,gain_fits=0,gate_changes=0,
        scoring='Unchanged run_battery.measure and preset_dc_loading.contrast; all original masks, gains and MIDI.',
        input_sha256={str(path):b.sha(path) for path in sorted(set(paths))}))

def verify():
    p=b.read(TARGET/'plan.json');assert b.sha(CANDIDATE)==p['candidate_sha256']
    for path,h in p['input_sha256'].items():assert b.sha(path)==h,path
    assert b.sha(p['binary'])==p['binary_sha256'];b.verify_plan();return p

def run():
    p=verify();assert not any(Path(c['audio']).exists() for c in p['cases'])
    for c in p['cases']:
        process=subprocess.run(c['command'],capture_output=True,text=True)
        log=TARGET/(c['id']+'.log');log.write_text(json.dumps(c['command'])+'\n'+process.stdout+process.stderr)
        if process.returncode:
            b.save(TARGET/'failure.json',dict(case=c,returncode=process.returncode,log_sha256=b.sha(log)))
            raise RuntimeError(c['id'])
        rate,x=b.audio(c['audio']);assert rate==48000 and len(x)==c['original_case']['frames']
        assert np.isfinite(x).all() and np.max(abs(x))<1
        b.save(TARGET/(c['id']+'-render.json'),dict(statistics=json.loads(process.stdout),
            audio_sha256=b.sha(c['audio']),log_sha256=b.sha(log)))
        print(c['id'],'rendered',flush=True)
    # First transfer scoring occurs only after the one candidate is frozen
    # and all eight native replays exist. No optimization follows this step.
    rows=[]
    for c in p['cases']:
        metrics,signal=b.measure(c['original_case'],Path(c['audio']))
        row=dict(id=c['id'],active_network=c['active_network'],metrics=metrics,signal=signal,
            audio=c['audio'],audio_sha256=b.sha(c['audio']),baseline_metrics=c['baseline_metrics'])
        if c['active_network']:
            unfitted,_=b.measure(c['original_case'],Path(c['unfitted_audio']))
            row.update(unfitted_metrics=unfitted,
                against_frozen=dc.contrast(c['original_case']['family'],metrics,c['baseline_metrics']),
                against_unfitted=dc.contrast(c['original_case']['family'],metrics,unfitted))
        else:
            row['byte_exact_to_frozen']=row['audio_sha256']==c['baseline_audio_sha256']
            assert row['byte_exact_to_frozen'] and metrics==c['baseline_metrics']
        rows.append(row);print(c['id'],'scored',flush=True)
    active=[r for r in rows if r['active_network']];assert len(active)==4
    verify();b.save(TARGET/'report.json',dict(status='completed',candidate_sha256=CANDIDATE_SHA,
        candidate=p['candidate'],cases=rows,render_count=8,total_new_native_WAVs=20,
        spectral_transfer_vs_frozen=all(r['against_frozen']['spectral_improves'] for r in active),
        spectral_transfer_vs_unfitted=all(r['against_unfitted']['spectral_improves'] for r in active),
        no_regression_vs_frozen=all(r['against_frozen']['no_regression'] for r in active),
        no_regression_vs_unfitted=all(r['against_unfitted']['no_regression'] for r in active),
        production_changed=False,gain_fits=0,gate_changes=0,additional_parameter_fits=0,
        limits=['Reused development transfers, not independent hardware calibration.',
            'Alpha at engineering lower bound; no physical trim recovery claimed.',
            'Only declared conditional rank/diode/bias/source assumptions tested.']))

if __name__=='__main__':
    if sys.argv[1]=='declare':declare()
    elif sys.argv[1]=='run':run()
    elif sys.argv[1]=='check':verify();print('Input identities verified')
    else:raise ValueError('declare/run/check')
