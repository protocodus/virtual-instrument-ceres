#!/usr/bin/env python3
"""Frozen disabled/unfitted native controls; no source-score inspection or fit."""
import argparse
import json
from pathlib import Path
import subprocess
import run_battery as b

OUT=b.BASE/'joint-calibration'

def verify():
    p=b.read(OUT/'plan.json')
    for path,digest in p['input_sha256'].items():
        assert b.sha(path)==digest, path
    b.verify_plan()
    n=p['native_controls']
    assert n['allow_render'] and not n['measure_hardware_before_selection']
    assert b.sha(n['binary'])==n['binary_sha256']
    assert b.read_identity(Path(n['binary']),Path(n['source_root']))==n['compiled_source_sha256']
    return p

def run():
    p=verify();n=p['native_controls'];out=OUT/'native-controls-001'
    assert not out.exists(), 'Preserve completed and incomplete attempts'
    out.mkdir()
    b.save(out/'declaration.json',dict(plan_sha256=b.sha(OUT/'plan.json'),
        tool_sha256=b.sha(__file__),cases=n['cases'],hardware_scores_read=False))
    rows=[]
    for c in n['cases']:
        process=subprocess.run(c['command'],capture_output=True,text=True)
        log=out/(c['id']+'.log')
        log.write_text(json.dumps(c['command'])+'\n'+process.stdout+process.stderr)
        if process.returncode:
            b.save(out/'failure.json',dict(case=c,returncode=process.returncode,log_sha256=b.sha(log)))
            raise RuntimeError(c['id'])
        stats=json.loads(process.stdout);rate,audio=b.audio(c['audio'])
        assert rate==48000 and len(audio)==c['original_case']['frames']
        import numpy as np
        assert np.isfinite(audio).all() and np.max(np.abs(audio))<1
        row=dict(id=c['id'],kind=c['kind'],original_id=c['original_case']['id'],
            audio=c['audio'],audio_sha256=b.sha(c['audio']),statistics=stats,
            peak=float(np.max(np.abs(audio))),log_sha256=b.sha(log))
        if c['kind']=='disabled':
            row['byte_exact_to_frozen']=row['audio_sha256']==c['baseline_audio_sha256']
            assert row['byte_exact_to_frozen'], c['id']
        rows.append(row);b.save(out/(c['id']+'.json'),row)
        print(c['id'], 'PASS',flush=True)
    assert len(rows)==12 and sum(r['kind']=='disabled' for r in rows)==8
    verify()
    b.save(out/'report.json',dict(status='pass',plan_sha256=b.sha(OUT/'plan.json'),
        cases=rows,native_render_count=12,hardware_scores_computed=False,
        parameter_fits=0,candidate_selected=False,production_default_changed=False))

def check():
    verify();out=OUT/'native-controls-001';r=b.read(out/'report.json')
    assert r['plan_sha256']==b.sha(OUT/'plan.json')
    for row in r['cases']:
        assert b.sha(row['audio'])==row['audio_sha256']
        if row['kind']=='disabled': assert row['byte_exact_to_frozen']
    print('VERIFIED:12native controls;8byte-exact;no hardware-score read')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('command',choices=['run','check'])
    args=ap.parse_args();run() if args.command=='run' else check()
