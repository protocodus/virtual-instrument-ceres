#!/usr/bin/env python3
"""Gated shared full-network calibration analysis; never renders audio.

The pinned DC qualifier is imported unchanged. Source-target access occurs only
inside the separately authorized fit command, after dynamic and rank gates pass.
"""
import argparse
from contextlib import redirect_stdout
import hashlib
import importlib.util
import io
import itertools
import json
import os
from pathlib import Path
import shutil
import sys
import traceback

import numpy as np
import scipy
from scipy import sparse
from scipy.optimize import least_squares
from scipy.io import wavfile

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Docs/accuracy/joint-calibration'
QUALIFIER=ROOT/'Tools/accuracy/joint_calibration.py'
QUALIFIER_SHA='5c4b5aabafc28cc5d8dcaf502a2956e03899f0dd8977d8f7d94410a4a06d7114'
assert hashlib.sha256(QUALIFIER.read_bytes()).hexdigest()==QUALIFIER_SHA
spec=importlib.util.spec_from_file_location('pinned_dc_qualification',QUALIFIER)
q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
NETWORK=ROOT/'Docs/accuracy/preset-dc-loading/network.py'
spec=importlib.util.spec_from_file_location('frozen_network',NETWORK)
net=importlib.util.module_from_spec(spec);spec.loader.exec_module(net)
TRAIN=q.TRAIN
X0=np.array([0.,0.,1.])
BOUNDS=(np.array([-15.,-15.,.5]),np.array([15.,15.,2.]))
STEPS=np.array([.25,.25,.01])
LN2=np.log(2.); DB_FACTOR=10/np.log(10.)


def runtime():
    stream=io.StringIO()
    with redirect_stdout(stream):np.__config__.show()
    try:
        import threadpoolctl
        pools=threadpoolctl.threadpool_info()
    except ImportError:pools=None
    return dict(python=sys.version,numpy=np.__version__,scipy=scipy.__version__,
        numpy_configuration=stream.getvalue(),threadpools=pools,
        thread_environment={k:os.environ.get(k) for k in
            ['OPENBLAS_NUM_THREADS','OMP_NUM_THREADS','OMP_DYNAMIC','MKL_NUM_THREADS',
             'MKL_DYNAMIC','VECLIB_MAXIMUM_THREADS']})


def load_plan():
    plan=q.read(OUT/'fit-plan.json')
    assert plan['training_ids']==TRAIN
    assert plan['dynamic_qualification']['authorized'] is True
    assert plan['dynamic_qualification']['thresholds']==q.THRESHOLDS
    assert plan['jacobian']['central_steps']==STEPS.tolist()
    assert plan['jacobian']['hp_rms_db_per_V_min']==.02
    assert plan['jacobian']['normalized_singular_ratio_min']==1e-3
    fit=plan['fit']
    assert fit['x0']==X0.tolist() and fit['lower_bounds']==BOUNDS[0].tolist()
    assert fit['upper_bounds']==BOUNDS[1].tolist() and fit['max_nfev']==80
    assert fit['anchor']=='native_unfitted_plus_analytical_delta_from_theta0'
    assert fit['objective']=='equal_clip_ratio_level_and_six_brass_cells'
    assert fit['method']=='trf' and fit['loss']=='linear'
    for path,digest in plan['input_sha256'].items():assert q.sha(path)==digest,path
    return plan


def training_cases():
    plan=q.read(OUT/'plan.json')
    cases=[c for c in plan['native_controls']['cases'] if c['original_case']['id'] in TRAIN]
    assert len(cases)==4
    for c in cases:
        row=q.read(OUT/'native-controls-001'/(c['id']+'.json'))
        assert row['audio']==c['audio'] and q.sha(c['audio'])==row['audio_sha256']
        if c['kind']=='disabled':assert row['byte_exact_to_frozen']
    return cases


def bindings(cases,include_targets=False):
    paths=[Path(__file__).resolve(),QUALIFIER,NETWORK,OUT/'fit-plan.json',OUT/'plan.json',
        OUT/'attempt-001/report.json',ROOT/'Tools/accuracy/metrics.py',
        ROOT/'Docs/accuracy/inputs/research/brass1-release-spectra-160.json']
    old=q.read(OUT/'attempt-001/report.json')
    for name in TRAIN:
        meta=old['trace_metadata'][name]
        paths+=[Path(meta['trace']),OUT/'attempt-001'/(name+'-trace.json')]
        assert q.sha(meta['trace'])==meta['trace_sha256']
    for c in cases:
        paths += [Path(c['audio']),Path(c['patch']),
                  OUT/'native-controls-001'/(c['id']+'.json')]
        if include_targets:paths.append(ROOT/c['original_case']['reference'])
    return {str(p):q.sha(p) for p in sorted(set(paths))}


def matrix(preset,stage):
    s=net.STAGE[stage]; rs=net.PRESET[preset][stage]; gp=1/net.RP
    gs=0. if rs is None else 1/rs
    D=gp+1/net.RANK['B'][stage]+1/s['Rbr']+3/s['Rmod']+1/s['Reg']+1/net.RBIAS
    a=np.zeros((5,5));a[0,0]=gs+1/net.R0+4*gp
    a[0,1:]=-gp;a[1:,0]=-gp
    a[np.arange(1,5),np.arange(1,5)]=D
    return a,gs,s['slope']*D/gp


class Cache:
    """Frozen phase/source/window factors; retain every board's EG for control."""
    def __init__(self,case,metadata,stride):
        self.case=case;self.clip=case['original_case'];self.name=self.clip['id']
        self.times,self.duration,self.valid=q.masks_and_windows(self.clip)
        complete,self.p=q.read_trace(metadata);raw=complete[::stride]
        complete_v=complete[:,5:].reshape(-1,4,10)
        complete_eg=complete_v[:,:,5]
        # Both frozen factory attacks target AL+(AL-IL)/expm1(5), inside this
        # conservative box. Every startup/held/release sample must remain in it.
        assert np.min(complete_eg)>=-10. and np.max(complete_eg)<=10.
        self.stride=stride
        # Crop only points that cannot enter any declared measurement window.
        t=raw[:,0]+31/q.FS
        start=min(self.times)+self.clip['offset_seconds']-self.duration/2
        end=max(self.times)+self.clip['offset_seconds']+self.duration/2
        raw=raw[(t>=start-stride/16000)&(t<=end+stride/16000)]
        self.trace=raw;t=raw[:,0]+31/q.FS;self.time=t
        v=raw[:,5:].reshape(-1,4,10);self.eg=v[:,:,5]
        p=self.p
        assert p['filterLfo']==p['pressureFilter']==p['brilliance']==0
        assert np.all(v[:,:,8:10]>q.FLOOR) and np.max(v[:,:,8:10])<.22*q.INTERNAL_FS
        assert np.all(complete_v[:,:,8:10]>q.FLOOR) and np.max(complete_v[:,:,8:10])<.22*q.INTERNAL_FS
        self.full_key=np.log2((complete_v[:,:,8]-q.FLOOR)/(p['hpCutoff']-q.FLOOR))-complete_eg*p['hpEnvelopeScale']
        full_lp_key=np.log2((complete_v[:,:,9]-q.FLOOR)/(p['lpCutoff']-q.FLOOR))-complete_eg*p['lpEnvelopeScale']
        assert np.max(abs(self.full_key-full_lp_key))<1e-11
        hp_key=np.log2((v[:,:,8]-q.FLOOR)/(p['hpCutoff']-q.FLOOR))-self.eg*p['hpEnvelopeScale']
        lp_key=np.log2((v[:,:,9]-q.FLOOR)/(p['lpCutoff']-q.FLOOR))-self.eg*p['lpEnvelopeScale']
        self.key_disagreement=float(np.max(abs(hp_key-lp_key)))
        assert self.key_disagreement<1e-11
        self.key=hp_key
        self.active=np.flatnonzero(np.any(v[:,:,4]!=0,axis=0))
        assert len(self.active)==(1 if self.clip['family']=='flute' else 3)
        self.notes=v[:,self.active,0].astype(int)
        f0=v[:,self.active,7,None]
        h=np.arange(1,65 if self.clip['family']=='flute' else 9)
        h=h[np.any(f0*h[None,None,:]<q.FS*.45,axis=(0,1))]
        self.h=h;self.f=f0*h[None,None,:]
        saw=q.source_coefficients(f0,h[None,None,:],p['resetMicroseconds'])
        self.saw=q.mix_branches(saw,h[None,None,:],1.,q.coupling(self.f,.1),0.,p['filterLevel'])
        direct=np.broadcast_to((h[None,None,:]==1)*(.5*p['sine']),self.f.shape).astype(complex).copy()
        tail=np.ones(self.f.shape,complex)
        for tau in [.127,4.7e-6*(1e3+4*47e3),33e-6*(4.7e3+560)]:tail*=q.coupling(self.f,tau)
        grid=np.linspace(0,q.FS/2,12001)
        tail*=np.interp(np.clip(self.f,0,q.FS/2),grid,abs(q.decimator(grid)))
        tail*=v[:,self.active,4,None]*raw[:,1,None,None]*p['volume']*10**(p['outputTrimDb']/20)
        tail=np.where(self.f<q.FS*.45,tail,0.)
        self.saw*=tail;self.direct=direct*tail
        self.legacy_cutoffs={stage:v[:,self.active,index] for stage,index in [('HP',8),('LP',9)]}
        self.stage_cache={}
        for stage,gs,column in [('HP',3/150e3,2),('LP',1/150e3,3)]:
            s=2j*q.INTERNAL_FS*np.tan(np.pi*self.f/q.INTERNAL_FS)
            ys=gs*s*q.CS/(gs+s*q.CS)
            node=ys+q.GG+q.GQ+s*q.CQ
            gb=(2-1.8*raw[:,column,None,None])*(gs+q.GG+q.GQ)/2-q.GQ
            feedback=gb+q.GQ+s*q.CQ
            self.stage_cache[stage]=(node*s*s,node,2*feedback*s,2*ys*(s*s if stage=='HP' else 1))
        self.network={}
        br=0. if self.clip['preset']=='flute' else -15.
        for stage in ['HP','LP']:
            a,gs,kappa=matrix(self.clip['preset'],stage);s=net.STAGE[stage]
            rhs=np.zeros((len(raw),5));rhs[:,0]=gs*9.4
            sub_voltage=p['filterLfo']*raw[:,4]/(.65*39000/47000)
            rhs[:,1:]=br/s['Rbr']+self.eg/s['Reg']+sub_voltage[:,None]/s['Rmod']
            solved=np.linalg.solve(a,rhs.T).T
            bias_rhs=np.r_[0.,np.full(4,1/net.RBIAS)]
            bias=np.linalg.solve(a,bias_rhs)
            scale=np.maximum(np.max(abs(a))*np.maximum(np.max(abs(solved),axis=1),1.),np.max(abs(rhs),axis=1))
            kcl=float(np.max(np.max(abs(solved@a.T-rhs),axis=1)/scale))
            assert kcl<1e-12
            # Entire engineering bias interval must retain the declared diode states.
            margins=[];currents=[]
            for b in [-15.,15.]:
                bus=solved[:,0]+b*bias[0];margins.append(float(np.min(bus+.6)))
                if gs:currents.append(float(np.min((9.4-bus)*gs)))
            assert min(margins)>=0 and (not currents or min(currents)>=0)
            self.network[stage]=dict(pin=solved[:,1:],bias=bias[1:],kappa=kappa,
                KCL=kcl,minimum_manual_margin=min(margins),
                minimum_selected_current=min(currents) if currents else None)
            full_rhs=np.zeros((len(complete),5));full_rhs[:,0]=gs*9.4
            full_sub=p['filterLfo']*complete[:,4]/(.65*39000/47000)
            full_rhs[:,1:]=br/s['Rbr']+complete_eg/s['Reg']+full_sub[:,None]/s['Rmod']
            self.network[stage]['full_pin']=np.linalg.solve(a,full_rhs.T).T[:,1:]
            corner_margins=[];corner_currents=[]
            for eg,b in itertools.product(itertools.product([-10.,10.],repeat=4),[-15.,15.]):
                corner_rhs=np.r_[gs*9.4,br/s['Rbr']+np.asarray(eg)/s['Reg']+b/net.RBIAS]
                corner=np.linalg.solve(a,corner_rhs)
                corner_margins.append(float(corner[0]+.6))
                if gs:corner_currents.append(float((9.4-corner[0])*gs))
            assert min(corner_margins)>=0 and (not corner_currents or min(corner_currents)>=0)
            self.network[stage].update(EG_box_V=[-10.,10.],diode_corner_cases=32,
                corner_minimum_manual_margin=min(corner_margins),
                corner_minimum_selected_current=min(corner_currents) if corner_currents else None,
                full_trace_EG_range_V=[float(np.min(complete_eg)),float(np.max(complete_eg))])
        rows=[];cols=[];weights=[];rect=[];self.windows=[]
        for i,center in enumerate(self.times+self.clip['offset_seconds']):
            a,b=center-self.duration/2,center+self.duration/2
            ix=np.flatnonzero((t>=a)&(t<b));assert len(ix)>100
            phase=(t[ix]-a)/self.duration;w=(.5-.5*np.cos(2*np.pi*phase))**2
            rows.extend([i]*len(ix));cols.extend(ix.tolist())
            weights.extend((w/w.sum()).tolist());rect.extend([1/len(ix)]*len(ix))
            notes=np.median(self.notes[ix],axis=0).astype(int)
            assert all(np.all(self.notes[ix,j]==notes[j]) for j in range(len(self.active)))
            assert len(set(notes))==len(notes)
            self.windows.append(dict(center=float(center),points=len(ix),notes=notes.tolist()))
        self.W=sparse.csr_matrix((weights,(rows,cols)),shape=(len(self.times),len(t)))
        self.R=sparse.csr_matrix((rect,(rows,cols)),shape=(len(self.times),len(t)))
        self.level_valid=np.ones((len(self.times),1 if self.clip['family']=='flute' else 3),bool)
        self.cells=[]
        if self.clip['family']=='brass':
            lookup={int(n+12*p['octave']):i for i,n in enumerate(self.notes[0])}
            assert set(q.m.NOTES)==set(lookup)
            self.lookup=lookup
            for j,note in enumerate(q.m.NOTES):
                columns=[i for i,pair in enumerate(q.m.PAIRS) if pair[0]==note]
                self.level_valid[:,j]=self.valid[:,columns].any(axis=1)
                for label,early in [('early',True),('release',False)]:
                    selected=(self.times+136.4<=137.08+1e-8)==early
                    self.cells.append((note,label,selected,columns,j))

    def stage_response(self,stage,cutoff):
        a,b,c,numerator=self.stage_cache[stage]
        fc=np.clip(cutoff,q.FLOOR,.22*q.INTERNAL_FS)
        omega=np.where(fc<=q.FLOOR,q.GF/q.CI,2*q.INTERNAL_FS*np.tan(np.pi*fc/q.INTERNAL_FS))[:,:,None]
        den=a+b*omega**2+c*omega
        response=numerator*(omega**2 if stage=='LP' else 1)/den
        d_dw=response*((2/omega if stage=='LP' else 0)-(2*b*omega+c)/den)
        d_wdf=2*np.pi/np.cos(np.pi*fc/q.INTERNAL_FS)**2
        d_wdf=np.where((cutoff>q.FLOOR)&(cutoff<.22*q.INTERNAL_FS),d_wdf,0.)
        return response,d_dw*d_wdf[:,:,None]

    def evaluate(self,theta=None,jacobian=False):
        responses={};derivatives={}
        for j,stage in enumerate(['HP','LP']):
            if theta is None:fc=self.legacy_cutoffs[stage]
            else:
                model=self.network[stage];u=model['pin']+theta[j]*model['bias'][None,:]
                exponent=self.key+theta[2]*model['kappa']*u
                fc=q.FLOOR+net.STAGE[stage]['gm_anchor_Hz']*np.exp2(exponent[:,self.active])
            response,d_response=self.stage_response(stage,fc);responses[stage]=response
            if jacobian:
                de=np.zeros((len(self.time),len(self.active),3))
                de[:,:,j]=theta[2]*model['kappa']*model['bias'][self.active][None,:]
                de[:,:,2]=model['kappa']*u[:,self.active]
                derivatives[stage]=d_response[:,:,:,None]*(LN2*(fc-q.FLOOR)[:,:,None]*de)[:,:,None,:]
        coefficient=self.saw*responses['HP']*responses['LP']+self.direct
        power=2*abs(coefficient)**2
        dp=None
        if jacobian:
            dc=self.saw[:,:,:,None]*(derivatives['HP']*responses['LP'][:,:,:,None]+
                                    responses['HP'][:,:,:,None]*derivatives['LP'])
            dp=4*np.real(np.conj(coefficient)[:,:,:,None]*dc)
        return self.integrate(power,dp)

    def integrate(self,power,derivative=None):
        shape=(len(self.times),len(self.active),len(self.h))
        bands=np.asarray(self.W@power.reshape(len(self.time),-1)).reshape(shape)
        dbands=None
        if derivative is not None:
            dbands=np.asarray(self.W@derivative.reshape(len(self.time),-1)).reshape(shape+(3,))
        if self.clip['family']=='flute':
            p=bands[:,0,:]
            ratio=10*np.log10(np.maximum(p[:,1:7],1e-30)/np.maximum(p[:,0,None],1e-30))
            level_power=np.asarray(self.R@power.sum(axis=(1,2)))[:,None]
            level=10*np.log10(np.maximum(level_power,1e-30))
            if derivative is None:return ratio,level
            d=dbands[:,0,:,:]
            dr=DB_FACTOR*(d[:,1:7,:]*(p[:,1:7,None]>1e-30)/np.maximum(p[:,1:7,None],1e-30)-d[:,0,None,:]*(p[:,0,None,None]>1e-30)/np.maximum(p[:,0,None,None],1e-30))
            dl_power=np.asarray(self.R@derivative.sum(axis=(1,2)))[:,None,:]
            dl=DB_FACTOR*dl_power*(level_power[:,:,None]>1e-30)/np.maximum(level_power[:,:,None],1e-30)
            return ratio,level,dr,dl
        ratios=[];levels=[];dr=[];dl=[]
        for note,h in q.m.PAIRS:
            j=self.lookup[note];p1=bands[:,j,0];ph=bands[:,j,h-1]
            ratios.append(10*np.log10(np.maximum(ph,1e-30)/np.maximum(p1,1e-30)))
            if derivative is not None:
                dr.append(DB_FACTOR*(dbands[:,j,h-1,:]*(ph[:,None]>1e-30)/np.maximum(ph[:,None],1e-30)-dbands[:,j,0,:]*(p1[:,None]>1e-30)/np.maximum(p1[:,None],1e-30)))
        for note in q.m.NOTES:
            j=self.lookup[note];p1=bands[:,j,0]
            levels.append(10*np.log10(np.maximum(p1,1e-30)))
            if derivative is not None:dl.append(DB_FACTOR*dbands[:,j,0,:]*(p1[:,None]>1e-30)/np.maximum(p1[:,None],1e-30))
        result=(np.asarray(ratios).T,np.asarray(levels).T)
        return result if derivative is None else result+(np.stack(dr,axis=1),np.stack(dl,axis=1))

    def vector(self,ratio,level):
        """Weights square to .25 per clip/domain, six equal Brass cells."""
        if self.clip['family']=='flute':
            return np.concatenate([ratio[self.valid]/np.sqrt(4*self.valid.sum()),
                                   level[self.level_valid]/np.sqrt(4*self.level_valid.sum())],axis=0)
        result=[]
        for note,label,rows,columns,j in self.cells:
            mask=self.valid[np.ix_(rows,columns)]
            values=ratio[np.ix_(rows,columns)][mask]
            result.append(values/np.sqrt(24*len(values)))
        for note,label,rows,columns,j in self.cells:
            mask=rows&self.level_valid[:,j];values=level[mask,j]
            result.append(values/np.sqrt(24*len(values)))
        return np.concatenate(result,axis=0)

    def description(self):
        return dict(clip=self.name,stride=self.stride,time_points=len(self.time),
            active_voices=self.active.tolist(),harmonics=self.h.tolist(),
            key_recovery_max_octave_disagreement=self.key_disagreement,
            network={k:{a:b for a,b in v.items() if a not in ['pin','full_pin','bias']} for k,v in self.network.items()},
            windows=self.windows)

    def cutoff_state(self,theta):
        result={}
        for j,stage in enumerate(['HP','LP']):
            model=self.network[stage];u=model['full_pin']+theta[j]*model['bias'][None,:]
            fc=q.FLOOR+net.STAGE[stage]['gm_anchor_Hz']*np.exp2(self.full_key+theta[2]*model['kappa']*u)
            result[stage]=dict(minimum_Hz=float(np.min(fc)),maximum_Hz=float(np.max(fc)),
                upper_clamped_sample_boards=int(np.count_nonzero(fc>=.22*q.INTERNAL_FS)),
                lower_clamped_sample_boards=int(np.count_nonzero(fc<=q.FLOOR)),
                scope='All saved16kHz trace samples, including startup and tails; not192kHz internal sample counts.')
        return result


def caches(cases,stride):
    report=q.read(OUT/'attempt-001/report.json');result={}
    for name in TRAIN:
        case=next(c for c in cases if c['original_case']['id']==name and c['kind']=='disabled')
        result[name]=Cache(case,report['trace_metadata'][name],stride)
    return result


def gate(cache,native_delta,predicted_delta,fine_delta):
    error=native_delta[0]-predicted_delta[0];level_error=native_delta[1]-predicted_delta[1]
    ratio=q.summary(error[cache.valid]);level=q.summary(level_error[cache.level_valid])
    cells=[]
    for note,label,rows,columns,j in cache.cells:
        selected=cache.valid[np.ix_(rows,columns)]
        cells.append(dict(note=note,phase=label,**q.summary(error[np.ix_(rows,columns)][selected])))
    convergence=max(float(np.max(abs(a-b))) for a,b in zip(predicted_delta,fine_delta))
    t=q.THRESHOLDS
    checks=dict(ratio_rms=ratio['rms_db']<=t['ratio_rms_db'],ratio_p95=ratio['p95_abs_db']<=t['ratio_p95_abs_db'],
        level_rms=level['rms_db']<=t['level_rms_db'],level_p95=level['p95_abs_db']<=t['level_p95_abs_db'],
        brass_cells=all(abs(c['signed_mean_db'])<=t['brass_cell_signed_mean_abs_db'] for c in cells),
        quadrature=convergence<=t['quadrature_delta_max_db'])
    return dict(clip=cache.name,passed=all(checks.values()),checks=checks,ratio_delta_error=ratio,
        level_delta_error=level,brass_cells=cells,quadrature_delta_max_db=convergence,
        native_delta_ratios_db=native_delta[0].tolist(),native_delta_levels_db=native_delta[1].tolist(),
        analytical_delta_ratios_db=predicted_delta[0].tolist(),analytical_delta_levels_db=predicted_delta[1].tolist(),
        ratio_error_db=error.tolist(),level_error_db=level_error.tolist(),source_mask_changed=False,native_exclusions=0)


def require_gate(path):
    report=q.read(path)
    for name,digest in report['input_sha256'].items():assert q.sha(name)==digest,name
    assert report['passed'],str(path)+' did not pass'
    return report


def declare(folder,cases,targets=False,preconditions=()):
    folder.mkdir()
    shutil.copy2(Path(__file__),folder/'joint_fit.py')
    inputs=bindings(cases,targets)
    inputs.update({str(path):q.sha(path) for path in preconditions})
    q.write(folder/'declaration.json',dict(input_sha256=inputs,runtime=runtime(),
        source_target_access=targets,transfer_metrics_access=False,new_audio_renders=0))
    return inputs


def dynamic_qualification(folder):
    plan=load_plan();cases=training_cases();inputs=declare(folder,cases)
    coarse=caches(cases,2);fine=caches(cases,1);old=q.read(OUT/'attempt-001/report.json')
    rows=[];oracles=[]
    for name in TRAIN:
        cache=coarse[name];legacy=cache.evaluate();full=cache.evaluate(X0)
        fine_legacy=fine[name].evaluate();fine_full=fine[name].evaluate(X0)
        disabled=next(c for c in cases if c['original_case']['id']==name and c['kind']=='disabled')
        unfitted=next(c for c in cases if c['original_case']['id']==name and c['kind']=='unfitted')
        probe=dict(disabled,transformed_values={})
        pinned=q.surrogate(probe,old['trace_metadata'][name],cache.times,cache.duration,2)
        oracle=max(float(np.max(abs(legacy[k]-pinned[k]))) for k in (0,1))
        assert oracle<=1e-9,'Cache does not preserve qualified model'
        oracles.append(dict(clip=name,cache_vs_pinned_legacy_max_db=oracle))
        native0=q.native_measurement(disabled,cache.times,cache.duration)
        native1=q.native_measurement(unfitted,cache.times,cache.duration)
        rows.append(gate(cache,[a-b for a,b in zip(native1,native0)],
            [a-b for a,b in zip(full,legacy)],[a-b for a,b in zip(fine_full,fine_legacy)]))
        rows[-1]['theta0_cutoff_state']=cache.cutoff_state(X0)
        print(name,'dynamic',rows[-1]['passed'],flush=True)
    report=dict(status='dynamic_qualification_pass' if all(r['passed'] for r in rows) else 'dynamic_qualification_failed',
        passed=all(r['passed'] for r in rows),cases=rows,oracles=oracles,
        cache=[c.description() for c in coarse.values()],thresholds=q.THRESHOLDS,
        input_sha256=inputs,source_targets_read=False,parameter_fits=0,new_audio_renders=0,
        scope='Two retained theta0 trajectories only; candidate needs native replay. Not hardware calibration or bitwise reproducibility.')
    assert inputs==bindings(cases)
    q.write(folder/'report.json',report)
    return report


def feature_jacobian(cache,theta):
    value=cache.evaluate(theta,True)
    return cache.vector(value[0],value[1]),cache.vector(value[2],value[3])


def jacobian_gate(folder):
    plan=load_plan();dynamic_path=OUT/'dynamic-qualification-001/report.json';require_gate(dynamic_path)
    cases=training_cases();inputs=declare(folder,cases,preconditions=[dynamic_path]);cached=caches(cases,2)
    columns=[]
    for j,step in enumerate(STEPS):
        high=X0.copy();low=X0.copy();high[j]+=step;low[j]-=step
        columns.append(np.concatenate([(c.vector(*c.evaluate(high))-c.vector(*c.evaluate(low)))/(2*step) for c in cached.values()]))
    J=np.column_stack(columns);norms=np.linalg.norm(J,axis=0)
    hp_fixed=bool(norms[0]<.02);active=[1,2] if hp_fixed else [0,1,2]
    normalized=J[:,active]/np.maximum(norms[active],1e-300)
    singular=np.linalg.svd(normalized,compute_uv=False)
    ratio=float(singular[-1]/singular[0]) if singular[0]>0 else 0.
    # Separate small-step numerical derivative oracle; never target residuals.
    analytic=np.concatenate([feature_jacobian(c,X0)[1] for c in cached.values()],axis=0)
    tiny=np.array([1e-4,1e-4,1e-5]);differences=[]
    for j,step in enumerate(tiny):
        high=X0.copy();low=X0.copy();high[j]+=step;low[j]-=step
        central=np.concatenate([(c.vector(*c.evaluate(high))-c.vector(*c.evaluate(low)))/(2*step) for c in cached.values()])
        differences.append(float(np.max(abs(analytic[:,j]-central))))
    assert max(differences)<1e-6,'Analytic derivative oracle failed'
    passed=ratio>=1e-3 and all(norms[j]>1e-12 for j in active)
    report=dict(status='jacobian_pass' if passed else 'jacobian_failed',passed=passed,
        parameter_order=['HP_bias_V','LP_bias_V','alpha'],central_steps=STEPS.tolist(),
        objective_weighted_derivative_norms=norms.tolist(),hp_fixed_at_zero=hp_fixed,
        active_parameter_indices=active,normalized_singular_values=singular.tolist(),
        normalized_smallest_to_largest_ratio=ratio,normalized_ratio_threshold=1e-3,
        hp_sensitivity_threshold_db_per_V=.02,small_step_analytic_jacobian_max_error=differences,
        weighted_jacobian=J.tolist(),input_sha256=inputs,source_targets_read=False,
        parameter_fits=0,new_audio_renders=0)
    assert all(q.sha(path)==digest for path,digest in inputs.items());q.write(folder/'report.json',report)
    return report


def targets(cache):
    """Only called after frozen fit permission + both gates; training cases only."""
    clip=cache.clip
    if clip['family']=='flute':
        rate,data=wavfile.read(ROOT/clip['reference'])
        if data.dtype.kind in 'iu':data=data.astype(float)/float(2**(data.dtype.itemsize*8-1))
        else:data=data.astype(float)
        if data.ndim==2:data=data.mean(axis=1)
        rows=[q.m.measure_frame(data,rate,float(t),clip['expected_f0'],count=16,duration=.128) for t in cache.times]
        return np.asarray([r['relative_db'][1:7] for r in rows]),20*np.log10(np.maximum([r['rms'] for r in rows],1e-15))[:,None]
    source=q.read(ROOT/'Docs/accuracy/inputs/research/brass1-release-spectra-160.json')['source']
    ratios=np.asarray(source['ratio_db'])
    levels=np.asarray([[q.m.band(row,note,1)['band_dbFS'] for note in q.m.NOTES] for row in source['rows']])
    assert ratios.shape==cache.valid.shape
    return ratios,levels


def fit(folder):
    plan=load_plan();assert plan['fit']['authorized'] is True
    dynamic_path=OUT/'dynamic-qualification-001/report.json';rank_path=OUT/'jacobian-001/report.json'
    dynamic=require_gate(dynamic_path);rank=require_gate(rank_path)
    cases=training_cases();inputs=declare(folder,cases,True,[dynamic_path,rank_path]);cached=caches(cases,2)
    active=rank['active_parameter_indices'];target=[];anchors={};zero={}
    for name,cache in cached.items():
        unfitted=next(c for c in cases if c['original_case']['id']==name and c['kind']=='unfitted')
        anchors[name]=q.native_measurement(unfitted,cache.times,cache.duration)
        zero[name]=cache.evaluate(X0)
        target.append(cache.vector(*targets(cache)))
    target=np.concatenate(target);evaluations=[];memo={}
    def evaluate(x):
        theta=X0.copy();theta[active]=x
        key=tuple(theta)
        if key==memo.get('theta'):return memo['residual'],memo['jacobian']
        predicted=[];jac=[]
        for name,cache in cached.items():
            value=cache.evaluate(theta,True)
            ratios=anchors[name][0]+value[0]-zero[name][0]
            levels=anchors[name][1]+value[1]-zero[name][1]+20*np.log10(cache.clip['recording_gain'])
            predicted.append(cache.vector(ratios,levels));jac.append(cache.vector(value[2],value[3]))
        residual=np.concatenate(predicted)-target;J=np.concatenate(jac,axis=0)[:,active]
        assert np.isfinite(residual).all() and np.isfinite(J).all()
        evaluations.append(dict(theta=theta.tolist(),weighted_MSE_db2=float(residual@residual)))
        memo.update(theta=key,residual=residual,jacobian=J)
        return residual,J
    initial=evaluate(X0[active])[0].copy()
    solution=least_squares(lambda x:evaluate(x)[0],X0[active],jac=lambda x:evaluate(x)[1],
        bounds=(BOUNDS[0][active],BOUNDS[1][active]),method='trf',loss='linear',
        max_nfev=80,ftol=1e-8,xtol=1e-8,gtol=1e-8,x_scale=1.)
    theta=X0.copy();theta[active]=solution.x
    # solution.fun is the accepted point's already-computed residual. A final
    # evaluate() could cause an81st call if the last bounded trial was rejected.
    final=np.asarray(solution.fun).copy()
    assert len(evaluations)<=80
    report=dict(status='one_candidate_frozen',candidate=theta.tolist(),parameter_order=['HP_bias_V','LP_bias_V','alpha'],
        hp_fixed_at_zero=rank['hp_fixed_at_zero'],initial_weighted_MSE_db2=float(initial@initial),
        final_weighted_MSE_db2=float(final@final),improved=bool(final@final<initial@initial),
        optimizer=dict(success=bool(solution.success),status=int(solution.status),message=solution.message,
            nfev=solution.nfev,njev=solution.njev,optimality=float(solution.optimality)),
        model_forward_evaluations=len(evaluations),evaluations=evaluations,
        initial_weighted_residual=initial.tolist(),final_weighted_residual=final.tolist(),
        input_sha256=inputs,source_targets_read=TRAIN,transfer_metrics_read=False,
        candidate_cutoff_state={name:c.cutoff_state(theta) for name,c in cached.items()},
        native_validation_pending=True,production_changed=False,new_audio_renders=0,
        interpretation='Conditional fixed-gauge candidate, not identified component calibration; no transfer tuning.')
    assert all(q.sha(path)==digest for path,digest in inputs.items());q.write(folder/'candidate.json',report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['qualify','jacobian','fit'])
    parser.add_argument('--attempt')
    args=parser.parse_args();default={'qualify':'dynamic-qualification-001','jacobian':'jacobian-001','fit':'fit-001'}
    name=args.attempt or default[args.command]
    assert Path(name).name==name and name not in ['.','..']
    folder=OUT/name
    assert not folder.exists(),'Preserve completed and incomplete attempts: '+str(folder)
    try:
        result={'qualify':dynamic_qualification,'jacobian':jacobian_gate,'fit':fit}[args.command](folder)
        print(result['status'],folder,flush=True)
    except BaseException:
        if folder.exists() and not (folder/'failure.json').exists():
            q.write(folder/'failure.json',dict(traceback=traceback.format_exc(),new_audio_renders=0))
        raise


if __name__=='__main__':main()
