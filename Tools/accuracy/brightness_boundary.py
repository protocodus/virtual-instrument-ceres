#!/usr/bin/env python3
"""Separately declared BR=-15 endpoint study; preserve every prior experiment."""
import argparse
import importlib.util
import itertools
import json
from pathlib import Path
import shutil
import subprocess
import sys
import traceback

import numpy as np
from scipy.optimize import minimize

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Docs/research/accuracy/brightness-boundary'
OLD=ROOT/'Docs/research/accuracy/joint-calibration'
FIT_TOOL=ROOT/'Tools/accuracy/joint_fit.py'
FIT_SHA='f563762e55ab2b0c41ff4c4ba34cac103c936dc55115b5683a8a2364f330dab6'
import hashlib
assert hashlib.sha256(FIT_TOOL.read_bytes()).hexdigest()==FIT_SHA
spec=importlib.util.spec_from_file_location('pinned_joint_fit',FIT_TOOL)
j=importlib.util.module_from_spec(spec);spec.loader.exec_module(j)
q=j.q
import run_battery as b
import preset_dc_loading as dc

TRAIN=list(j.TRAIN)
BR=-15.
X0=j.X0.copy()
BOUNDS=tuple(x.copy() for x in j.BOUNDS)
TINY=np.array([1e-4,1e-4,1e-5])
PHASES={'qualify':'qualify-001','jacobian':'jacobian-001','fit':'fit-001','validate':'native-validation-001'}


def load_plan():
    p=q.read(OUT/'plan.json')
    assert p['training_ids']==TRAIN and p['shared_actual_BR_V']==BR
    assert p['qualification']['thresholds']==q.THRESHOLDS
    assert p['jacobian']['central_steps']==j.STEPS.tolist()
    assert p['jacobian']['hp_rms_db_per_V_min']==.02
    assert p['jacobian']['normalized_singular_ratio_min']==1e-3
    assert p['jacobian']['analytic_derivative_max_abs_error']==1e-6
    f=p['fit']
    assert f['x0']==X0.tolist() and f['lower_bounds']==BOUNDS[0].tolist() and f['upper_bounds']==BOUNDS[1].tolist()
    assert f['method']=='SLSQP' and f['max_distinct_forwards']==80 and f['maxiter']==80 and f['ftol']==1e-9
    assert f['objective']=='flute_ratio_trajectory_MSE'
    assert f['constraints']=='frozen_disabled_Flute_level_MSE_and_six_Brass_ratio_MAEs_and_six_Brass_H1_level_MSEs'
    assert f['derivative_steps']==TINY.tolist() and f['derivative_max_abs_error']==1e-6
    assert f['fail_on_MAE_sign_crossing'] is True
    assert p['numeric_tolerance_db']==1e-10 and p['native_validation']['new_WAV_ceiling']==10
    for path,digest in p['input_sha256'].items():assert q.sha(path)==digest,path
    r=p['renderer'];assert q.sha(r['binary'])==r['binary_sha256']
    assert set(r['compiled_source_sha256'])==set(b.IDENTITY)
    for path,digest in r['compiled_source_sha256'].items():assert q.sha(Path(r['source_root'])/path)==digest,path
    j.load_plan()
    return p


def original_cases():
    return j.training_cases()


def bindings():
    p=load_plan()
    paths=[Path(__file__),OUT/'plan.json',OUT/'method.md',FIT_TOOL,j.QUALIFIER,j.NETWORK,q.METRIC_PATH,
        Path(b.__file__),Path(dc.__file__),OLD/'plan.json',OLD/'fit-plan.json',
        OLD/'dynamic-qualification-001/report.json',OLD/'attempt-001/report.json']
    result=dict(p['input_sha256'])
    result.update({str(x):q.sha(x) for x in paths})
    result[str(Path(p['renderer']['binary']))]=p['renderer']['binary_sha256']
    for name,digest in p['renderer']['compiled_source_sha256'].items():
        result[str(Path(p['renderer']['source_root'])/name)]=digest
    return result


def verify_bindings(bound):
    for path,digest in bound.items():assert q.sha(path)==digest,path


def start(folder,phase,extras=()):
    assert not folder.exists(),'Preserve completed/incomplete attempts: '+str(folder)
    p=load_plan();assert p['authorized_phases'][phase] is True
    folder.mkdir(parents=True)
    shutil.copy2(Path(__file__),folder/'brightness_boundary.py')
    bound=bindings()
    bound.update({str(x):q.sha(x) for x in extras})
    q.write(folder/'declaration.json',dict(phase=phase,input_sha256=bound,runtime=j.runtime(),
        training_ids=TRAIN,actual_BR_V=BR,source_targets_allowed=phase in ['fit','validate'],
        transfer_access_allowed=phase=='validate',new_WAV_ceiling=10,production_changed=False))
    return p,bound


def gate_report(path):
    r=q.read(path);verify_bindings(r['input_sha256']);verify_bindings(r.get('artifact_sha256',{}));assert r['passed'],str(path)
    return r


def artifacts(folder):
    """Bind every completed phase artifact before its final report is written."""
    assert q.sha(folder/'brightness_boundary.py')==q.sha(Path(__file__))
    return {str(path):q.sha(path) for path in sorted(folder.rglob('*')) if path.is_file()}


class BoundaryCache(j.Cache):
    """Same source/window cache, with full-network BR=-15 for both presets."""
    def __init__(self,case,metadata,stride):
        super().__init__(case,metadata,stride)
        complete,_=q.read_trace(metadata)
        eg=complete[:,5:].reshape(-1,4,10)[:,:,5]
        old_br=0. if self.clip['preset']=='flute' else -15.
        assert self.p['filterLfo']==0.
        self.boundary_checks={}
        for stage in ['HP','LP']:
            a,gs,kappa=j.matrix(self.clip['preset'],stage);s=j.net.STAGE[stage]
            delta=np.linalg.solve(a,np.r_[0.,np.full(4,(BR-old_br)/s['Rbr'])])
            model=self.network[stage]
            model['pin']=model['pin']+delta[1:][None,:]
            model['full_pin']=model['full_pin']+delta[1:][None,:]
            # Algebraically separate Schur reduction verifies the shifted full
            # history; the analytical delta is not assumed to be correct.
            gp=1/j.net.RP;D=a[1,1];h=a[0,0]
            rhs=BR/s['Rbr']+eg/s['Reg']
            bus=(gs*9.4+(gp/D)*rhs.sum(axis=1))/(h-4*gp*gp/D)
            pins=(gp*bus[:,None]+rhs)/D
            discrepancy=float(np.max(abs(pins-model['full_pin'])))
            solution=np.column_stack([bus,pins])
            full_rhs=np.column_stack([np.full(len(eg),gs*9.4),rhs])
            denominator=np.maximum(abs(solution)@abs(a.T)+abs(full_rhs),1e-30)
            kcl=float(np.max(abs(solution@a.T-full_rhs)/denominator))
            assert discrepancy<1e-12 and kcl<1e-12
            corner_margins=[];corner_currents=[]
            for es,bias in itertools.product(itertools.product([-10.,10.],repeat=4),[-15.,15.]):
                rhs_corner=np.r_[gs*9.4,BR/s['Rbr']+np.asarray(es)/s['Reg']+bias/j.net.RBIAS]
                node=np.linalg.solve(a,rhs_corner)
                corner_margins.append(float(node[0]+.6))
                if gs:corner_currents.append(float((9.4-node[0])*gs))
            assert min(corner_margins)>=0 and (not corner_currents or min(corner_currents)>=0)
            model.update(actual_BR_V=BR,KCL=kcl,corner_minimum_manual_margin=min(corner_margins),
                corner_minimum_selected_current=min(corner_currents) if corner_currents else None)
            # Recompute sampled margins at bias extremes. Derivative of shared
            # bus to the fixed 355k bias comes from the five-node solve.
            bias_delta=np.linalg.solve(a,np.r_[0.,np.full(4,1/j.net.RBIAS)])
            model['minimum_manual_margin']=min(float(np.min(bus+b*bias_delta[0]+.6)) for b in [-15.,15.])
            model['minimum_selected_current']=min(float(np.min((9.4-bus-b*bias_delta[0])*gs)) for b in [-15.,15.]) if gs else None
            self.boundary_checks[stage]=dict(old_BR_V=old_br,actual_BR_V=BR,
                exact_affine_node_delta_V=delta.tolist(),independent_full_pin_max_difference_V=discrepancy,
                normalized_KCL_max_error=kcl,corner_cases=32,
                minimum_corner_manual_diode_margin_V=min(corner_margins),
                minimum_corner_selected_diode_current_A=min(corner_currents) if corner_currents else None,
                full_trace_EG_range_V=[float(eg.min()),float(eg.max())])

    def description(self):
        return dict(super().description(),boundary_checks=self.boundary_checks)


def caches(stride=2):
    cases=original_cases();old=q.read(OLD/'attempt-001/report.json');result={}
    for name in TRAIN:
        case=next(c for c in cases if c['kind']=='disabled' and c['original_case']['id']==name)
        result[name]=BoundaryCache(case,old['trace_metadata'][name],stride)
    return result


def create_render_case(folder,clip,label,theta,active=True):
    p=load_plan();patch=ROOT/clip['patch']
    if active:
        values=dc.patch_values(patch)
        for key in ['hp_cutoff','lp_cutoff','hp_envelope_scale','lp_envelope_scale']:values.pop(key,None)
        values.update(calibration_network=1 if clip['preset']=='flute' else 2,
            calibration_hp_bias=float(theta[0]),calibration_lp_bias=float(theta[1]),
            calibration_alpha=float(theta[2]),brilliance=BR)
        patch=folder/(label+'.patch')
        with patch.open('x') as f:
            f.write('# Frozen shared dark-endpoint hypothesis; existing timing/source/Q/gain retained.\n')
            f.write('\n'.join(f'{key}={value:.17g}' for key,value in values.items())+'\n')
    audio=folder/(label+'.wav')
    command=[p['renderer']['binary'],'--preset',clip['preset'],'--patch',str(patch),
        '--events',str(ROOT/clip['events']),'--output',str(audio),'--rate','48000',
        '--frames',str(clip['frames']),'--seed',str(clip['seed'])]
    return dict(id=label,original_case=clip,patch=str(patch),patch_sha256=q.sha(patch),
        audio=str(audio),command=command,active_network=active,theta=np.asarray(theta).tolist() if active else None)


def render(case,folder):
    assert not Path(case['audio']).exists(),'Preserve rendered output'
    assert q.sha(case['patch'])==case['patch_sha256']
    process=subprocess.run(case['command'],capture_output=True,text=True)
    log=folder/(case['id']+'.log')
    with log.open('x') as f:f.write(json.dumps(case['command'])+'\n'+process.stdout+process.stderr)
    if process.returncode:
        q.write(folder/(case['id']+'-render-failure.json'),dict(returncode=process.returncode,log_sha256=q.sha(log)))
        raise RuntimeError('Native render failed: '+case['id'])
    rate,x=b.audio(case['audio']);assert rate==48000 and len(x)==case['original_case']['frames']
    assert np.isfinite(x).all() and np.max(abs(x))<1
    row=dict(case=case,statistics=json.loads(process.stdout),audio_sha256=q.sha(case['audio']),
        peak=float(np.max(abs(x))),log_sha256=q.sha(log))
    q.write(folder/(case['id']+'-render.json'),row)
    print('rendered',case['id'],flush=True)
    return row


def qualify(folder):
    p,bound=start(folder,'qualify')
    old_gate=j.require_gate(OLD/'dynamic-qualification-001/report.json')
    brass_gate=next(c for c in old_gate['cases'] if c['clip']=='brass-history20')
    assert brass_gate['passed']
    old_cases=original_cases();name=TRAIN[0]
    old_case=next(c for c in old_cases if c['kind']=='unfitted' and c['original_case']['id']==name)
    clip=old_case['original_case']
    new_case=create_render_case(folder,clip,name+'-BRminus15-theta0',X0)
    q.write(folder/'render-plan.json',dict(cases=[new_case],new_render_count=1,transfer_access=False,
        input_sha256=bound))
    native_row=render(new_case,folder)
    coarse=caches(2);fine=caches(1)
    metadata=q.read(OLD/'attempt-001/report.json')['trace_metadata'][name]
    disabled=next(c for c in old_cases if c['kind']=='disabled' and c['original_case']['id']==name)
    old_coarse=j.Cache(disabled,metadata,2);old_fine=j.Cache(disabled,metadata,1)
    cache=coarse[name]
    native_old=q.native_measurement(old_case,cache.times,cache.duration)
    native_new=q.native_measurement(new_case,cache.times,cache.duration)
    analytical_old=old_coarse.evaluate(X0);analytical_new=cache.evaluate(X0)
    fine_old=old_fine.evaluate(X0);fine_new=fine[name].evaluate(X0)
    result=j.gate(cache,[a-b for a,b in zip(native_new,native_old)],
        [a-b for a,b in zip(analytical_new,analytical_old)],
        [a-b for a,b in zip(fine_new,fine_old)])
    bound.update({str(Path(new_case['audio'])):native_row['audio_sha256'],
        str(Path(new_case['patch'])):new_case['patch_sha256'],
        str(folder/(new_case['id']+'-render.json')):q.sha(folder/(new_case['id']+'-render.json')),
        str(folder/(new_case['id']+'.log')):native_row['log_sha256'],
        str(folder/'render-plan.json'):q.sha(folder/'render-plan.json')})
    report=dict(status='qualification_pass' if result['passed'] else 'qualification_failed',passed=result['passed'],
        flute_BR_change=result,reused_brass_dynamic_gate=brass_gate,
        reused_brass_dynamic_report_sha256=q.sha(OLD/'dynamic-qualification-001/report.json'),
        native_anchor_case=new_case,cache=[c.description() for c in coarse.values()],
        theta0_cutoff_state={name:c.cutoff_state(X0) for name,c in coarse.items()},
        input_sha256=bound,artifact_sha256=artifacts(folder),thresholds=q.THRESHOLDS,new_WAV_count=1,optimizer_calls=0,
        source_targets_read=False,transfer_data_or_metrics_read=False)
    verify_bindings(bound);q.write(folder/'report.json',report)
    print(report['status'],q.sha(folder/'report.json'),flush=True)


def jacobian(folder):
    qualify_path=OUT/PHASES['qualify']/'report.json';gate_report(qualify_path)
    p,bound=start(folder,'jacobian',[qualify_path]);cached=caches(2)
    columns=[]
    for index,step in enumerate(j.STEPS):
        high=X0.copy();low=X0.copy();high[index]+=step;low[index]-=step
        columns.append(np.concatenate([(c.vector(*c.evaluate(high))-c.vector(*c.evaluate(low)))/(2*step) for c in cached.values()]))
    matrix=np.column_stack(columns);norms=np.linalg.norm(matrix,axis=0)
    hp_fixed=bool(norms[0]<.02);active=[1,2] if hp_fixed else [0,1,2]
    normalized=matrix[:,active]/np.maximum(norms[active],1e-300)
    singular=np.linalg.svd(normalized,compute_uv=False)
    ratio=float(singular[-1]/singular[0]) if singular[0]>0 else 0.
    analytical=np.concatenate([j.feature_jacobian(c,X0)[1] for c in cached.values()],axis=0)
    errors=[]
    for index,step in enumerate(TINY):
        high=X0.copy();low=X0.copy();high[index]+=step;low[index]-=step
        central=np.concatenate([(c.vector(*c.evaluate(high))-c.vector(*c.evaluate(low)))/(2*step) for c in cached.values()])
        errors.append(float(np.max(abs(central-analytical[:,index]))))
    passed=ratio>=1e-3 and all(norms[i]>1e-12 for i in active) and max(errors)<=1e-6
    report=dict(status='jacobian_pass' if passed else 'jacobian_failed',passed=passed,
        parameter_order=['HP_bias_V','LP_bias_V','alpha'],weighted_jacobian=matrix.tolist(),
        objective_weighted_derivative_norms=norms.tolist(),hp_fixed_at_zero=hp_fixed,
        active_parameter_indices=active,normalized_singular_values=singular.tolist(),
        normalized_smallest_to_largest_ratio=ratio,small_step_analytic_jacobian_max_error=errors,
        cache=[c.description() for c in cached.values()],input_sha256=bound,artifact_sha256=artifacts(folder),
        source_targets_read=False,transfer_data_or_metrics_read=False,new_WAV_count=0,optimizer_calls=0)
    verify_bindings(bound);q.write(folder/'report.json',report)
    print(report['status'],q.sha(folder/'report.json'),flush=True)


def metrics_and_derivatives(cached,residuals):
    """One Flute objective plus13 inequalities, all in original record weights."""
    flute,brass=[cached[n] for n in TRAIN]
    fr,fl,fdr,fdl=residuals[TRAIN[0]]
    br,bl,bdr,bdl=residuals[TRAIN[1]]
    r=fr[flute.valid];dr=fdr[flute.valid]
    objective=float(np.mean(r*r));objective_jac=np.mean(2*r[:,None]*dr,axis=0)
    r=fl[flute.level_valid];dr=fdl[flute.level_valid]
    errors=[float(np.mean(r*r))];derivatives=[np.mean(2*r[:,None]*dr,axis=0)]
    names=['flute_fixed_gain_level_MSE'];mae_records=[];counts=[len(r)]
    for note,label,rows,columns,index in brass.cells:
        mask=brass.valid[np.ix_(rows,columns)]
        r=br[np.ix_(rows,columns)][mask];dr=bdr[np.ix_(rows,columns)][mask]
        # Same six_cells definition: pool all source-valid partial/frame
        # records within the cell, not equally weighted partial means.
        errors.append(float(np.mean(abs(r))));derivatives.append(np.mean(np.sign(r)[:,None]*dr,axis=0))
        names.append(f'brass_{note}_{label}_ratio_MAE');counts.append(len(r));mae_records.extend(r.tolist())
    for note,label,rows,columns,index in brass.cells:
        mask=rows&brass.level_valid[:,index]
        r=bl[mask,index];dr=bdl[mask,index]
        errors.append(float(np.mean(r*r)));derivatives.append(np.mean(2*r[:,None]*dr,axis=0))
        names.append(f'brass_{note}_{label}_H1_level_MSE');counts.append(len(r))
    assert len(names)==13 and sum(counts[1:7])==243 and sum(counts[7:])==70
    return dict(objective=objective,objective_jacobian=objective_jac,errors=np.asarray(errors),
        error_jacobian=np.asarray(derivatives),names=names,record_counts=counts,MAE_records=np.asarray(mae_records))


def cell_weight_oracle(cache,ratio_residual,expected):
    """Use the frozen native cell reducer itself on stored ratio residuals."""
    partials=[]
    for column,(note,harmonic) in enumerate(q.m.PAIRS):
        records=[dict(source_seconds=float(t+136.4),source_valid=bool(cache.valid[i,column]),
            raw_ratio_residual_db=float(ratio_residual[i,column]),native_weak_H1_Hh=[False,False])
            for i,t in enumerate(cache.times)]
        partials.append(dict(note=note,harmonic=harmonic,all_frames=records))
    canonical=q.m.six_cells(dict(per_partial=partials))
    actual=np.asarray([c['ratio_MAE_db'] for c in canonical['cells']])
    difference=float(np.max(abs(actual-expected)))
    assert difference<=1e-10
    return dict(max_abs_difference_db=difference,canonical_cells=canonical['cells'],
        weakness_flags_placeholder=True,weak_counts_not_tested=True,
        interpretation='Reducer/record-weight oracle only; this does not qualify a weak-power or tail predictor.')


def compact(row):
    return {key:(value.tolist() if isinstance(value,np.ndarray) else value) for key,value in row.items()
            if key not in ['predictions','residuals']}


class CallBudget(Exception):pass


class Evaluator:
    def __init__(self,cached,anchors,targets,limits,active,cap,folder):
        self.cached=cached;self.anchors=anchors;self.targets=targets;self.limits=limits
        self.active=active;self.cap=cap;self.memo={};self.rows=[]
        self.folder=folder/'evaluations';self.folder.mkdir()
        # The initial analytical anchor and Jacobian are obtained together in
        # the first counted evaluation below; no uncounted model call here.
        self.zero=None

    def evaluate(self,x):
        theta=X0.copy();theta[self.active]=x;key=tuple(float(t) for t in theta)
        if key in self.memo:return self.memo[key]
        if len(self.rows)>=self.cap:
            q.write(self.folder.parent/'forward-budget-stop.json',dict(requested_unevaluated_theta=list(key),
                completed_distinct_forward_count=len(self.rows),limit=self.cap))
            raise CallBudget('80 distinct forward theta values exhausted')
        assert np.all(theta>=BOUNDS[0]-1e-12) and np.all(theta<=BOUNDS[1]+1e-12)
        label=f'forward-{len(self.rows)+1:03d}'
        q.write(self.folder/(label+'-request.json'),dict(theta=list(key),ordinal=len(self.rows)+1))
        values={name:c.evaluate(theta,True) for name,c in self.cached.items()}
        if self.zero is None:
            assert np.array_equal(theta,X0),'Evaluate theta0 before any other point'
            self.zero={name:(v[0].copy(),v[1].copy()) for name,v in values.items()}
        predictions={};residuals={}
        for name,c in self.cached.items():
            v=values[name]
            ratio=self.anchors[name][0]+v[0]-self.zero[name][0]
            raw_level=self.anchors[name][1]+v[1]-self.zero[name][1]
            level=raw_level+20*np.log10(c.clip['recording_gain'])
            predictions[name]=dict(ratios_db=ratio.tolist(),native_levels_db=raw_level.tolist(),fixed_gain_levels_db=level.tolist())
            residuals[name]=(ratio-self.targets[name][0],level-self.targets[name][1],v[2],v[3])
        measures=metrics_and_derivatives(self.cached,residuals)
        constraints=self.limits-measures['errors']
        row=dict(theta=theta,objective=measures['objective'],objective_jacobian=measures['objective_jacobian'],
            constraint_values=constraints,constraint_jacobian=-measures['error_jacobian'],
            error_values=measures['errors'],MAE_records=measures['MAE_records'],
            maximum_constraint_violation=float(max(0.,-np.min(constraints))),
            predictions=predictions,residuals={n:dict(ratio_db=v[0].tolist(),level_db=v[1].tolist()) for n,v in residuals.items()})
        assert all(np.isfinite(row[k]).all() for k in ['objective_jacobian','constraint_values','constraint_jacobian'])
        assert np.isfinite(row['objective'])
        self.memo[key]=row;self.rows.append(row)
        q.write(self.folder/(label+'.json'),dict(compact(row),predictions=predictions,residuals=row['residuals']))
        print('forward',len(self.rows),'theta',list(key),'objective',row['objective'],
              'max_violation',row['maximum_constraint_violation'],flush=True)
        return row


def fit(folder):
    qp=OUT/PHASES['qualify']/'report.json';jp=OUT/PHASES['jacobian']/'report.json'
    qualification=gate_report(qp);rank=gate_report(jp)
    p,bound=start(folder,'fit',[qp,jp]);cached=caches(2);old_cases=original_cases()
    active=rank['active_parameter_indices'];anchors={};targets={};disabled={};baseline_residuals={};training_evidence={}
    for name,c in cached.items():
        if name==TRAIN[0]:anchor=qualification['native_anchor_case']
        else:anchor=next(x for x in old_cases if x['kind']=='unfitted' and x['original_case']['id']==name)
        off=next(x for x in old_cases if x['kind']=='disabled' and x['original_case']['id']==name)
        anchors[name]=q.native_measurement(anchor,c.times,c.duration)
        disabled[name]=q.native_measurement(off,c.times,c.duration)
        targets[name]=j.targets(c)
        baseline_residuals[name]=(disabled[name][0]-targets[name][0],
            disabled[name][1]+20*np.log10(c.clip['recording_gain'])-targets[name][1],
            np.zeros(disabled[name][0].shape+(3,)),np.zeros(disabled[name][1].shape+(3,)))
        training_evidence[name]=dict(times=c.times.tolist(),duration=c.duration,valid=c.valid.tolist(),
            level_valid=c.level_valid.tolist(),recording_gain=c.clip['recording_gain'],
            frozen_disabled_audio=off['audio'],frozen_disabled_audio_sha256=q.sha(off['audio']),
            native_theta0_anchor_audio=anchor['audio'],native_theta0_anchor_audio_sha256=q.sha(anchor['audio']),
            frozen_disabled_ratios_db=disabled[name][0].tolist(),frozen_disabled_raw_levels_db=disabled[name][1].tolist(),
            frozen_disabled_ratio_residual_db=baseline_residuals[name][0].tolist(),
            frozen_disabled_fixed_gain_level_residual_db=baseline_residuals[name][1].tolist(),
            native_theta0_anchor_ratios_db=anchors[name][0].tolist(),native_theta0_anchor_raw_levels_db=anchors[name][1].tolist())
        for path in [Path(anchor['audio']),Path(off['audio']),ROOT/c.clip['reference']]:bound[str(path)]=q.sha(path)
    baseline=metrics_and_derivatives(cached,baseline_residuals)
    weight_oracle=cell_weight_oracle(cached[TRAIN[1]],baseline_residuals[TRAIN[1]][0],baseline['errors'][1:7])
    limits=baseline['errors'];engine=Evaluator(cached,anchors,targets,limits,active,p['fit']['max_distinct_forwards'],folder)
    initial=engine.evaluate(X0[active])
    checks=[];crossings=[]
    for index in active:
        step=TINY[index];high=X0.copy();low=X0.copy();high[index]+=step;low[index]-=step
        plus=engine.evaluate(high[active]);minus=engine.evaluate(low[active])
        crossed=(np.sign(plus['MAE_records'])!=np.sign(minus['MAE_records'])) | (initial['MAE_records']==0)
        indices=np.flatnonzero(crossed).tolist();crossings.append(dict(parameter=index,record_indices=indices))
        objective_error=abs((plus['objective']-minus['objective'])/(2*step)-initial['objective_jacobian'][index])
        constraint_error=float(np.max(abs((plus['constraint_values']-minus['constraint_values'])/(2*step)-initial['constraint_jacobian'][:,index])))
        checks.append(dict(parameter=index,step=float(step),objective_derivative_abs_error=float(objective_error),
            constraint_derivative_max_abs_error=constraint_error))
    derivative_pass=not any(c['record_indices'] for c in crossings) and all(
        max(c['objective_derivative_abs_error'],c['constraint_derivative_max_abs_error'])<=1e-6 for c in checks)
    derivative=dict(passed=derivative_pass,checks=checks,MAE_sign_crossings=crossings,
        threshold=1e-6,no_records_excluded=True)
    q.write(folder/'derivative-check.json',derivative)
    accepted=[X0.copy()];callbacks=[];solution=None;budget_stop=False
    if derivative_pass:
        def callback(x):
            theta=X0.copy();theta[active]=x;key=tuple(float(t) for t in theta)
            # A callback must identify an evaluated trial, never consume a
            # hidden 81st evaluation or change the last accepted point.
            assert key in engine.memo,'SLSQP callback point was not evaluated'
            row=engine.memo[key];accepted.append(theta.copy())
            callbacks.append(dict(theta=theta.tolist(),objective=row['objective'],
                maximum_constraint_violation=row['maximum_constraint_violation']))
            q.write(folder/f'callback-{len(callbacks):03d}.json',callbacks[-1])
        try:
            solution=minimize(lambda x:engine.evaluate(x)['objective'],X0[active],
                jac=lambda x:engine.evaluate(x)['objective_jacobian'][active],method='SLSQP',
                bounds=list(zip(BOUNDS[0][active],BOUNDS[1][active])),
                constraints=[dict(type='ineq',fun=lambda x:engine.evaluate(x)['constraint_values'],
                    jac=lambda x:engine.evaluate(x)['constraint_jacobian'][:,active])],
                callback=callback,options=dict(maxiter=80,ftol=1e-9,disp=False))
        except CallBudget:budget_stop=True
    theta=accepted[-1].copy() if solution is None else X0.copy()
    if solution is not None:theta[active]=solution.x
    key=tuple(float(t) for t in theta);assert key in engine.memo
    final=engine.memo[key]
    feasible=bool(np.min(final['constraint_values'])>=-p['numeric_tolerance_db'])
    if not derivative_pass:status='derivative_gate_failed'
    elif budget_stop:status='forward_budget_stopped_diagnostic_only'
    else:status='one_solver_return_frozen'
    report=dict(status=status,candidate=theta.tolist(),candidate_is_solver_return=solution is not None,
        candidate_sha_identity_note='Hash this entire file before any transfer work.',
        optimizer=None if solution is None else dict(success=bool(solution.success),status=int(solution.status),
            message=str(solution.message),nit=int(solution.nit),nfev=int(solution.nfev),njev=int(solution.njev)),
        budget_exception=budget_stop,derivative_pass=derivative_pass,derivative_check=derivative,
        surrogate_constraints_pass=feasible,constraint_names=baseline['names'],constraint_record_counts=baseline['record_counts'],
        constraint_limits=limits.tolist(),constraint_limit_origin='Original frozen-disabled native training outputs, with original fixed source gain.',
        frozen_six_cell_reducer_oracle=weight_oracle,
        initial=compact(initial),final=compact(final),initial_predictions=initial['predictions'],final_predictions=final['predictions'],
        initial_residuals=initial['residuals'],final_residuals=final['residuals'],
        evaluated_points=[dict(compact(row),predictions=row['predictions'],residuals=row['residuals']) for row in engine.rows],
        accepted_callback_points=callbacks,distinct_forward_count=len(engine.rows),forward_limit=80,
        derivative_probe_points_included_in_forward_count=True,active_parameter_indices=active,
        candidate_cutoff_state={name:c.cutoff_state(theta) for name,c in cached.items()},
        candidate_on_bounds=[bool(abs(theta[i]-BOUNDS[0][i])<=1e-10 or abs(theta[i]-BOUNDS[1][i])<=1e-10) for i in range(3)],
        training_evidence=training_evidence,input_sha256=bound,artifact_sha256=artifacts(folder),
        source_targets_read=TRAIN,transfer_data_or_metrics_read=False,new_WAV_count=0,
        validation_permitted=solution is not None and derivative_pass and not budget_stop,
        complete_native_no_regression_claim=False,production_changed=False,
        limits=['13 training constraints do not replace canonical median, weak-count or 2ms tail gates.',
            'An unsuccessful or infeasible solver return is preserved, not promoted.',
            'A budget exception is diagnostic only and cannot proceed to native candidate validation.'])
    assert len(engine.rows)<=80;verify_bindings(bound);q.write(folder/'candidate.json',report)
    print(status,'theta',theta.tolist(),'feasible',feasible,'sha',q.sha(folder/'candidate.json'),flush=True)


def validate(folder):
    candidate_path=OUT/PHASES['fit']/'candidate.json';candidate=q.read(candidate_path)
    verify_bindings(candidate['input_sha256']);verify_bindings(candidate['artifact_sha256'])
    assert candidate['validation_permitted'] and candidate['candidate_is_solver_return']
    p=load_plan()
    if not (candidate['optimizer']['success'] and candidate['surrogate_constraints_pass']):
        assert p['native_validation']['allow_unsuccessful_or_infeasible_solver_return'] is True
    cp=q.sha(candidate_path)
    p,bound=start(folder,'validate',[candidate_path,OUT/PHASES['qualify']/'report.json'])
    theta=np.asarray(candidate['candidate']);source=b.verify_plan()
    baseline=q.read(ROOT/'Docs/research/accuracy/runs/expression-55869df8/report.json')
    old_native_plan=q.read(OLD/'plan.json')['native_controls']
    old_native_report=q.read(OLD/'native-controls-001/report.json')
    qualification=gate_report(OUT/PHASES['qualify']/'report.json')
    lower=next(c for c in source['cases'] if c['id']=='flute-lower-factory-physical')
    lower_control=create_render_case(folder,lower,lower['id']+'-BRminus15-theta0',X0)
    cases=[]
    for clip in source['cases']:
        active=clip['family']=='brass' or clip['id'] in ['flute-high-factory-physical','flute-lower-factory-physical']
        cases.append(create_render_case(folder,clip,clip['id'],theta,active))
    assert len(cases)==8
    bound.update({c['patch']:q.sha(c['patch']) for c in [lower_control]+cases})
    q.write(folder/'render-plan.json',dict(candidate_sha256=cp,candidate=theta.tolist(),
        source_conditioned_lower_control=lower_control,cases=cases,new_render_count=9,
        total_study_new_WAV_count=10,input_sha256=bound,transfer_selection=False))
    lower_row=render(lower_control,folder)
    native_rows={c['id']:render(c,folder) for c in cases}
    # No transfer metric is evaluated until the frozen candidate and all nine
    # validation outputs exist. There is no optimizer call after this point.
    rows=[];controls={}
    for c in cases:
        clip=c['original_case'];name=clip['id']
        metrics,signal=b.measure(clip,Path(c['audio']))
        original=next(x for x in baseline['cases'] if x['id']==name)
        assert q.sha(ROOT/original['audio'])==original['audio_sha256']
        row=dict(id=name,active_network=c['active_network'],metrics=metrics,signal=signal,
            audio=c['audio'],audio_sha256=native_rows[name]['audio_sha256'],baseline_metrics=original['metrics'])
        if c['active_network']:
            if name==TRAIN[0]:control=qualification['native_anchor_case'];control_sha=q.sha(control['audio'])
            elif name==lower['id']:control=lower_control;control_sha=lower_row['audio_sha256']
            else:
                control=next(x for x in old_native_plan['cases'] if x['kind']=='unfitted' and x['original_case']['id']==name)
                stored=next(x for x in old_native_report['cases'] if x['id']==control['id'])
                control_sha=stored['audio_sha256'];assert q.sha(control['audio'])==control_sha
            unfit,_=b.measure(clip,Path(control['audio']))
            controls[name]=dict(audio=control['audio'],audio_sha256=control_sha,metrics=unfit,
                actual_BR_V=BR,theta=X0.tolist(),new_render=name==lower['id'])
            row.update(source_conditioned_unfitted_metrics=unfit,
                against_frozen=dc.contrast(clip['family'],metrics,original['metrics']),
                against_source_conditioned_unfitted=dc.contrast(clip['family'],metrics,unfit))
        else:
            row['byte_exact_to_frozen']=row['audio_sha256']==original['audio_sha256']
            assert row['byte_exact_to_frozen'] and metrics==original['metrics']
        rows.append(row)
        print('scored',name,flush=True)
    active_rows=[r for r in rows if r['active_network']];assert len(active_rows)==4
    assert q.sha(candidate_path)==cp;verify_bindings(bound)
    output_bindings={str(Path(r['audio'])):r['audio_sha256'] for r in rows}
    output_bindings.update({str(Path(r['audio'])):r['audio_sha256'] for r in controls.values()})
    new_audio_paths=[qualification['native_anchor_case']['audio'],lower_control['audio']]+[c['audio'] for c in cases]
    assert len(new_audio_paths)==len(set(new_audio_paths))==10
    report=dict(status='completed',candidate_sha256=cp,candidate=theta.tolist(),cases=rows,
        source_conditioned_theta0_controls=controls,input_sha256=bound,output_sha256=output_bindings,
        artifact_sha256=artifacts(folder),
        spectral_transfer_vs_frozen=all(r['against_frozen']['spectral_improves'] for r in active_rows),
        spectral_transfer_vs_source_conditioned_unfitted=all(r['against_source_conditioned_unfitted']['spectral_improves'] for r in active_rows),
        no_regression_vs_frozen=all(r['against_frozen']['no_regression'] for r in active_rows),
        no_regression_vs_source_conditioned_unfitted=all(r['against_source_conditioned_unfitted']['no_regression'] for r in active_rows),
        new_WAV_count=9,total_study_new_WAV_count=10,new_WAV_paths=new_audio_paths,optimizer_calls=0,gain_fits=0,
        candidate_changed_after_transfer=False,production_changed=False,
        limits=['Shared BR=-15 is an endpoint hypothesis, not a measured hardware setting.',
            'Reused development transfer data; no independent original hardware MIDI.',
            'All canonical adverse metrics retained regardless of optimizer success.'])
    q.write(folder/'report.json',report);print('validation completed',q.sha(folder/'report.json'),flush=True)


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('command',choices=list(PHASES))
    args=ap.parse_args();folder=OUT/PHASES[args.command]
    # Refuse old attempt folders outside failure-writing try: never add a
    # misleading new failure to completed or incomplete frozen evidence.
    assert not folder.exists(),'Attempt already exists: '+str(folder)
    try:globals()[args.command](folder)
    except Exception:
        if folder.exists():
            with (folder/'failure.txt').open('x') as f:f.write(traceback.format_exc())
        raise


if __name__=='__main__':main()
