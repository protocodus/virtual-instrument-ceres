#!/usr/bin/env python3
"""Post-rejection, training-only native prediction check; no fit or render."""
import importlib.util
from pathlib import Path
import sys
import traceback
from types import SimpleNamespace

import numpy as np

HERE=Path(__file__).resolve().parent
BASE=HERE.parent
ROOT=HERE.parents[3]
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('frozen_joint_fit',ROOT/'Tools/accuracy/joint_fit.py')
j=importlib.util.module_from_spec(spec);spec.loader.exec_module(j)
q=j.q
PINS={
    ROOT/'Tools/accuracy/joint_fit.py':'f563762e55ab2b0c41ff4c4ba34cac103c936dc55115b5683a8a2364f330dab6',
    BASE/'fit-001/candidate.json':'0d77a1fe3e27119869e1be4f2fef2f5bfab779b3dcd5dc0d61edda8aac021245',
    BASE/'fit-replay-001/report.json':'61583571f31be2b95bd1b860558e2e54d6b80397842c88be6a72bd85d74c5587',
    BASE/'native-validation-001/report.json':'1ab95a9882a87021f843cd5c61f40bd66546daf5753cc0b854df336577f376a0',
}


def inputs():
    for p,h in PINS.items():assert q.sha(p)==h,str(p)
    plan=q.read(BASE/'native-validation-001/plan.json')
    cases=[c for c in plan['cases'] if c['id'] in q.TRAIN]
    assert [c['id'] for c in cases]==q.TRAIN
    paths=list(PINS)+[Path(__file__),j.QUALIFIER,q.METRIC_PATH,
        BASE/'fit-plan.json',BASE/'native-validation-001/plan.json',
        BASE/'fit-replay-001/declaration.json',BASE/'fit-replay-001/replay.py',j.NETWORK]
    for c in cases:
        p=BASE/'native-validation-001'/(c['id']+'-render.json')
        assert q.sha(c['audio'])==q.read(p)['audio_sha256']
        assert q.sha(c['patch'])==plan['input_sha256'][c['patch']]
        paths += [p,Path(c['audio']),Path(c['patch'])]
    return cases,{str(p):q.sha(p) for p in sorted(set(paths))}


def geometry(pred,clip):
    times=np.asarray(pred['times']);valid=np.asarray(pred['valid'],bool)
    level_valid=np.asarray(pred['level_valid'],bool)
    cells=[];groups=[]
    if clip['family']=='flute':
        assert valid.shape==(45,6) and valid.all()
        assert level_valid.shape==(45,1) and level_valid.all()
        groups=[('flute_ratio',int(valid.sum()),.25),('flute_level',int(level_valid.sum()),.25)]
    else:
        assert valid.shape==(36,len(q.m.PAIRS)) and valid.sum()==243
        assert level_valid.shape==(36,len(q.m.NOTES))
        for index,note in enumerate(q.m.NOTES):
            cols=[i for i,pair in enumerate(q.m.PAIRS) if pair[0]==note]
            assert np.array_equal(level_valid[:,index],valid[:,cols].any(axis=1))
            for label,early in [('early',True),('release',False)]:
                rows=(times+136.4<=137.08+1e-8)==early
                cells.append((note,label,rows,cols,index))
        groups=[(f'brass_{n}_{phase}_ratio',int(valid[np.ix_(rows,cols)].sum()),1/24)
                for n,phase,rows,cols,index in cells]
        groups += [(f'brass_{n}_{phase}_level',int((rows&level_valid[:,index]).sum()),1/24)
                   for n,phase,rows,cols,index in cells]
    proxy=SimpleNamespace(clip=clip,valid=valid,level_valid=level_valid,cells=cells)
    return proxy,groups


def stats(values):
    return dict(q.summary(values),mean_abs_db=float(np.mean(abs(values))))


def calculate(cases):
    # Native aggregate report is hash-bound but never parsed: only training
    # WAVs are measured. Reuse the saved prediction/residual arrays; no source
    # recording, source target file, model evaluation, optimizer, or renderer.
    replay=q.read(BASE/'fit-replay-001/report.json')
    candidate=q.read(BASE/'fit-001/candidate.json')
    result=[];all_initial=[];all_pred=[];all_native=[];all_error=[]
    for name in q.TRAIN:
        case=next(c for c in cases if c['id']==name);clip=case['original_case']
        pred=next(p for p in replay['predictions'] if p['clip']==name)
        proxy,groups=geometry(pred,clip)
        initial=next(s for s in pred['states'] if s['label']=='initial')
        final=next(s for s in pred['states'] if s['label']=='candidate')
        assert final['theta']==candidate['candidate']
        nr,nl=q.native_measurement(case,np.asarray(pred['times']),pred['duration'])
        pr=np.asarray(final['ratios_db']);pl=np.asarray(final['native_level_db'])
        gain_db=20*np.log10(clip['recording_gain'])
        assert np.max(abs(np.asarray(final['fixed_gain_level_db'])-(pl+gain_db)))<1e-12
        er=nr-pr;el=nl-pl
        vector=lambda r,l:j.Cache.vector(proxy,r,l)
        ew=vector(er,el)
        r0=np.asarray(initial['weighted_residual']);rp=np.asarray(final['weighted_residual'])
        rn=rp+ew
        target0=vector(np.asarray(initial['ratios_db']),np.asarray(initial['fixed_gain_level_db']))-r0
        targetp=vector(pr,pl+gain_db)-rp
        target_identity=float(np.max(abs(target0-targetp)))
        assert target_identity<=1e-10
        assert np.max(abs((vector(nr,nl+gain_db)-targetp)-rn))<=1e-10
        cells=[]
        for note,phase,rows,cols,index in proxy.cells:
            selected=proxy.valid[np.ix_(rows,cols)]
            cells.append(dict(note=note,phase=phase,
                ratio_prediction_error=stats(er[np.ix_(rows,cols)][selected]),
                level_prediction_error=stats(el[rows&proxy.level_valid[:,index],index])))
        component=[];offset=0
        for label,count,weight in groups:
            sl=slice(offset,offset+count);scale=np.sqrt(weight/count)
            component.append(dict(name=label,count=count,objective_weight=weight,
                initial_source_residual=stats(r0[sl]/scale),
                surrogate_candidate_source_residual=stats(rp[sl]/scale),
                native_candidate_source_residual=stats(rn[sl]/scale),
                surrogate_error=stats(ew[sl]/scale),
                initial_objective_db2=float(r0[sl]@r0[sl]),
                surrogate_objective_db2=float(rp[sl]@rp[sl]),
                native_objective_db2=float(rn[sl]@rn[sl])))
            offset += count
        assert offset==len(rp)
        rs=stats(er[proxy.valid]);ls=stats(el[proxy.level_valid]);t=q.THRESHOLDS
        checks=dict(ratio_rms=rs['rms_db']<=t['ratio_rms_db'],ratio_p95=rs['p95_abs_db']<=t['ratio_p95_abs_db'],
            level_rms=ls['rms_db']<=t['level_rms_db'],level_p95=ls['p95_abs_db']<=t['level_p95_abs_db'],
            brass_cells=all(abs(c['ratio_prediction_error']['signed_mean_db'])<=t['brass_cell_signed_mean_abs_db'] for c in cells))
        result.append(dict(clip=name,ratio_prediction_error=rs,level_prediction_error=ls,brass_cells=cells,
            checks_against_prior_numeric_limits=checks,all_comparable_limits_pass=all(checks.values()),
            quadrature_recomputed=False,objective_components=component,
            initial_objective_db2=float(r0@r0),surrogate_objective_db2=float(rp@rp),native_objective_db2=float(rn@rn),
            target_identity_max_difference_db=target_identity,recording_gain=clip['recording_gain'],gain_db=float(gain_db),
            times=pred['times'],duration=pred['duration'],valid=pred['valid'],level_valid=pred['level_valid'],
            native_ratios_db=nr.tolist(),native_raw_levels_db=nl.tolist(),native_fixed_gain_levels_db=(nl+gain_db).tolist(),
            surrogate_ratios_db=pr.tolist(),surrogate_raw_levels_db=pl.tolist(),
            ratio_error_db=er.tolist(),level_error_db=el.tolist(),
            initial_weighted_residual=r0.tolist(),surrogate_weighted_residual=rp.tolist(),
            native_weighted_residual=rn.tolist(),weighted_prediction_error=ew.tolist()))
        all_initial.append(r0);all_pred.append(rp);all_native.append(rn);all_error.append(ew)
        print(name,'ratio RMS',rs['rms_db'],'level RMS',ls['rms_db'],flush=True)
    r0,rp,rn,ew=[np.concatenate(a) for a in [all_initial,all_pred,all_native,all_error]]
    assert abs(r0@r0-candidate['initial_weighted_MSE_db2'])<1e-10
    assert abs(rp@rp-candidate['final_weighted_MSE_db2'])<1e-10
    actual=float(rn@rn);predicted=float(rp@rp)
    cross=float(2*rp@ew);error=float(ew@ew)
    assert abs(actual-(predicted+cross+error))<1e-10
    return dict(status='completed_post_rejection_training_diagnostic',training_ids=q.TRAIN,cases=result,
        weighted_objective=dict(initial_MSE_db2=float(r0@r0),surrogate_candidate_MSE_db2=predicted,
            native_candidate_MSE_db2=actual,native_minus_surrogate_MSE_db2=actual-predicted,
            residual_error_cross_term_db2=cross,prediction_error_energy_db2=error,
            identity='native_MSE = surrogate_MSE + 2*surrogate_residual.dot(prediction_error) + prediction_error.dot(prediction_error)'),
        candidate_sha256=q.sha(BASE/'fit-001/candidate.json'),candidate_adoption='rejected_unchanged',
        optimizer_calls=0,model_evaluations=0,new_audio_renders=0,source_recordings_read=False,
        source_target_files_read=False,frozen_source_residuals_reused=True,transfer_audio_or_metrics_read=False,
        original_masks_unchanged=True,native_weak_partials_excluded=False,fixed_recording_gains_unchanged=True,
        measurement_function='Unchanged joint_calibration.native_measurement',
        error_sign='native minus frozen surrogate prediction',
        limits=['Post-selection diagnostic; no new fit or promotion gate.',
            'Original training masks retain every candidate weak partial; no new exclusions.',
            'Dynamic gates at the starting point did not certify the entire engineering box.',
            'This compares one conditional candidate and does not identify physical hardware calibration.'])


def main():
    cases,bound=inputs()
    if sys.argv[1]=='run':
        q.write(HERE/'declaration.json',dict(input_sha256=bound,runtime=j.runtime(),training_ids=q.TRAIN,
            error_sign='native minus frozen surrogate prediction',optimizer_calls=0,model_evaluations=0,
            new_audio_renders=0,source_target_files_read=False,transfer_audio_or_metrics_read=False,
            numeric_reproduction_tolerance_db=1e-10))
        report=calculate(cases);report['input_sha256']=bound
        for p,h in bound.items():assert q.sha(p)==h,p
        q.write(HERE/'report.json',report)
        print('report',q.sha(HERE/'report.json'),flush=True)
    elif sys.argv[1]=='check':
        saved=q.read(HERE/'report.json');assert bound==saved['input_sha256']
        actual=calculate(cases);actual['input_sha256']=bound
        differences=[]
        def walk(a,b,path):
            if isinstance(a,dict):
                assert isinstance(b,dict) and a.keys()==b.keys(),path
                for k in a:walk(a[k],b[k],path+'/'+k)
            elif isinstance(a,list):
                assert isinstance(b,list) and len(a)==len(b),path
                for i,(x,y) in enumerate(zip(a,b)):walk(x,y,path+'/'+str(i))
            elif a!=b:
                assert isinstance(a,(float,int)) and isinstance(b,(float,int)),path
                differences.append(dict(path=path,saved=a,actual=b,absolute_difference=abs(a-b)))
        walk(saved,actual,'')
        maximum=max([d['absolute_difference'] for d in differences] or [0.])
        q.write(HERE/'reproduction.json',dict(status='numeric_reproduction_pass' if maximum<=1e-10 else 'failed',
            runtime=j.runtime(),report_sha256=q.sha(HERE/'report.json'),differences=differences,
            maximum_numeric_difference=maximum,tolerance_db=1e-10,optimizer_calls=0,new_audio_renders=0))
        assert maximum<=1e-10
        print('numeric_reproduction_pass',maximum,flush=True)
    else:raise ValueError('run/check')


if __name__=='__main__':
    try:main()
    except Exception:
        p=HERE/('failure-'+sys.argv[1]+'.txt')
        with p.open('x') as f:f.write(traceback.format_exc())
        raise
