#!/usr/bin/env python3
"""Replay the one frozen candidate's training predictions; never optimize."""
import importlib.util
from pathlib import Path
import numpy as np

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
BASE=HERE.parent
spec=importlib.util.spec_from_file_location('frozen_joint_fit',ROOT/'Tools/accuracy/joint_fit.py')
j=importlib.util.module_from_spec(spec);spec.loader.exec_module(j)
q=j.q


def main():
    plan=j.load_plan()
    candidate_path=BASE/'fit-001/candidate.json'
    saved=q.read(candidate_path)
    for path,digest in saved['input_sha256'].items():assert q.sha(path)==digest,path
    assert q.sha(candidate_path)=='0d77a1fe3e27119869e1be4f2fef2f5bfab779b3dcd5dc0d61edda8aac021245'
    q.write(HERE/'declaration.json',dict(input_sha256={str(path):q.sha(path) for path in
        [Path(__file__),candidate_path,BASE/'fit-plan.json',ROOT/'Tools/accuracy/joint_fit.py']},
        runtime=j.runtime(),optimizer_calls=0,new_audio_renders=0,transfer_metrics_read=False,
        numerical_tolerance_db=1e-10,comparison='Frozen initial/final weighted residuals, numerical agreement only.'))
    cases=j.training_cases();cached=j.caches(cases,2)
    initial=[];final=[];predictions=[]
    for name,cache in cached.items():
        case=next(c for c in cases if c['original_case']['id']==name and c['kind']=='unfitted')
        native=q.native_measurement(case,cache.times,cache.duration)
        baseline=cache.evaluate(j.X0)
        source=j.targets(cache);target=cache.vector(*source)
        states=[]
        for label,theta in [('initial',j.X0),('candidate',np.asarray(saved['candidate']))]:
            value=cache.evaluate(theta)
            ratio=native[0]+value[0]-baseline[0]
            raw_level=native[1]+value[1]-baseline[1]
            gained_level=raw_level+20*np.log10(cache.clip['recording_gain'])
            residual=cache.vector(ratio,gained_level)-target
            (initial if label=='initial' else final).append(residual)
            states.append(dict(label=label,theta=theta.tolist(),ratios_db=ratio.tolist(),
                native_level_db=raw_level.tolist(),fixed_gain_level_db=gained_level.tolist(),
                weighted_residual=residual.tolist()))
        predictions.append(dict(clip=name,times=cache.times.tolist(),duration=cache.duration,
            valid=cache.valid.tolist(),level_valid=cache.level_valid.tolist(),states=states))
    initial=np.concatenate(initial);final=np.concatenate(final)
    initial_diff=initial-np.asarray(saved['initial_weighted_residual'])
    final_diff=final-np.asarray(saved['final_weighted_residual'])
    maximum=max(float(np.max(abs(initial_diff))),float(np.max(abs(final_diff))))
    assert maximum<=1e-10
    report=dict(status='numerical_replay_pass',comparison_tolerance_db=1e-10,
        maximum_weighted_residual_difference_db=maximum,
        initial_difference_db=initial_diff.tolist(),final_difference_db=final_diff.tolist(),
        recomputed_initial_MSE_db2=float(initial@initial),recomputed_final_MSE_db2=float(final@final),
        initial_MSE_difference_db2=float(initial@initial)-saved['initial_weighted_MSE_db2'],
        final_MSE_difference_db2=float(final@final)-saved['final_weighted_MSE_db2'],
        predictions=predictions,candidate_sha256=q.sha(candidate_path),
        declaration_sha256=q.sha(HERE/'declaration.json'),
        optimizer_calls=0,new_audio_renders=0,transfer_metrics_read=False,
        bitwise_reproducibility_claim=False)
    for path,digest in saved['input_sha256'].items():assert q.sha(path)==digest,path
    q.write(HERE/'report.json',report)
    print(report['status'],maximum,q.sha(HERE/'report.json'),flush=True)


if __name__=='__main__':main()
