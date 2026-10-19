#!/usr/bin/env python3
"""Predeclared source-only Brass harmonic-ratio diagnostic; no DSP/audio renders.

All input audio is existing source evidence or synthetic estimator controls.
The synthetic signals are analytic test fixtures, not instrument emulations.
"""
import os
for _key in ('OPENBLAS_NUM_THREADS', 'OMP_NUM_THREADS', 'MKL_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS'):
    os.environ[_key] = '1'
import argparse
import hashlib
import json
from pathlib import Path
import platform
import sys
import time
import numpy as np
import scipy
from scipy import linalg, signal
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Docs/accuracy/brass-ratio-modulation'
ARC = Path('/tmp/ceres-cs50-1x8LGd')
PLAN = OUT / 'plan.json'
SCRIPT = Path(__file__).resolve()
NOTES = [57, 60, 64]
TARGET_H = [2, 3, 4]
TARGET_COL = [12+h-1 for h in TARGET_H]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def write_json(path, value):
    assert not Path(path).exists(), f'Refuse to overwrite {path}'
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False)+'\n')

def load_plan():
    d = json.loads(PLAN.read_text())
    for p, expected in d['input_sha256'].items():
        assert sha(p) == expected, f'Input changed: {p}'
    assert d['status'] == 'declared_before_source_estimates_reviewed'
    assert d['synthetic']['expected_control_count'] == 60
    return d

def identity():
    load_plan()
    p = OUT / 'implementation.json'
    write_json(p, {'schema':'ceres-source-ratio-implementation-v1',
        'plan_sha256':sha(PLAN), 'script_sha256':sha(SCRIPT),
        'python':sys.version, 'numpy':np.__version__, 'scipy':scipy.__version__,
        'platform':platform.platform(),
        'command':[sys.executable,str(SCRIPT),'run'],
        'native_render_calls':0, 'frozen_before_estimates':True})
    print('Implementation declaration:', sha(p), flush=True)

def verify_identity():
    d=json.loads((OUT/'implementation.json').read_text())
    assert d['script_sha256']==sha(SCRIPT)
    assert d['plan_sha256']==sha(PLAN)
    return d

def pitch_trajectory(plan, n):
    old = json.loads((ARC/'calibration/whitmore-brass1-source.json').read_text())
    lo,hi=plan['pitch_frames_relative_context_seconds']
    rows=[r for r in old['frames'] if lo-1e-9 <= r['time'] <= hi+1e-9]
    ft=np.array([r['time'] for r in rows]); ff=np.array([r['f0_hz'] for r in rows])
    t=np.arange(n)/plan['source_rate_Hz']
    f=np.stack([np.interp(t,ft,ff[:,i]) for i in range(3)],axis=1)
    phase=np.zeros_like(f)
    phase[1:]=2*np.pi*np.cumsum((f[1:]+f[:-1])*.5/plan['source_rate_Hz'],axis=0)
    return t,f,phase,rows

def make_sos(fc,q,kind,fs):
    # Bilinear transform of a prewarped analog two-pole section.
    w=2*fs*np.tan(np.pi*fc/fs)
    b=[1,0,0] if kind=='hp' else [w*w]
    bz,az=signal.bilinear(b,[1,w/q,w*w],fs=fs)
    return signal.tf2sos(bz,az)

def fixtures(plan, phase, t):
    syn=plan['synthetic']; fmod=plan['ratio_fit']['frequency_Hz']
    abs_t=t+plan['source_context_seconds'][0]
    theta=2*np.pi*fmod*(abs_t-plan['ratio_fit']['time_origin_seconds'])
    am=1-.7109245361481505*np.cos(2*np.pi*fmod*(t-.56)-.4848223278487288)
    amps=syn['base_amplitudes']; hspec=syn['harmonic_amplitudes_H1_H12']
    waves=[]; meta=[]
    def add(x,record):
        assert np.isfinite(x).all()
        waves.append(x); meta.append(record)
    def partials(v):
        p=np.empty((3,16,len(t)))
        for i in range(3):
            for hi in range(16):
                h=hi+1; a=hspec[hi] if h<=12 else .06*12/h
                offset=.19*i+.37*h+v*(.73*i+.11*h*h)
                p[i,hi]=amps[i]*a*np.cos(h*phase[:,i]+offset)
        return p
    for vi,v in enumerate(syn['phase_variants']):
        p=partials(v); pernote=p.sum(axis=1); x=pernote.sum(axis=0)
        for subtype,y in [('static',x),('common_am',x*am),
            ('common_am_envelopes',(pernote*np.exp(-np.array([1.2,2,3])[:,None]*np.maximum(t-.44,0))).sum(axis=0)*am),
            ('common_am_noise',x*am+np.random.default_rng(syn['noise_seed_base']+vi).normal(0,1e-4,len(t)))]:
            add(y,{'id':f'null_p{vi}_{subtype}','family':'flat_ratio_null','phase_variant':v,'subtype':subtype})
        for trig,func in [('sin',np.sin),('cos',np.cos)]:
            pp=p.copy(); factor=10**(3*func(theta)/20)
            pp[1,2]*=factor;pp[1,3]*=factor
            add(pp.sum(axis=(0,1))*am,{'id':f'positive_p{vi}_{trig}','family':'positive','phase_variant':v,
                'input_peak_ratio_dB':3.0,'input_phase_degrees':0 if trig=='sin' else 90,'trig':trig})
    st=syn['static_color_controls']; pernote=partials(0).sum(axis=1)
    for fc in st['LP_middle_C_Hz']:
        for q in st['LP_Q']:
            for order in ['before','after']:
                y=np.zeros(len(t)); sections=[]
                for i,note in enumerate(NOTES):
                    scaling=2**((note-60)/12)
                    hp=make_sos(st['HP_middle_C_Hz']*scaling,st['HP_Q'],'hp',plan['source_rate_Hz'])
                    lp=make_sos(fc*scaling,q,'lp',plan['source_rate_Hz'])
                    sos=np.concatenate([hp,lp]);sections.append(sos.tolist())
                    y+=signal.sosfilt(sos,pernote[i]*(am if order=='before' else 1))
                if order=='after':y*=am
                add(y,{'id':f'static_lp{fc:.9f}_q{q}_am_{order}','family':'static_filter_null',
                    'LP_middle_C_Hz':fc,'LP_Q':q,'AM_order':order,'per_note_sos':sections})
    assert len(waves)==60
    return waves,meta

def phase_degrees(s,c):
    return float(np.degrees(np.arctan2(c,s)))

def phase_delta(a,b):
    return float((a-b+180)%360-180)

def fit_ratio(y, centers, plan):
    x=(centers-137.04)/.16
    th=2*np.pi*plan['ratio_fit']['frequency_Hz']*(centers-plan['ratio_fit']['time_origin_seconds'])
    X=np.column_stack([np.ones(len(x)),x,x*x,np.sin(th),np.cos(th)])
    coef=np.linalg.lstsq(X,y,rcond=None)[0]
    poly=np.linalg.lstsq(X[:,:3],y,rcond=None)[0]
    return {'coefficients_constant_linear_quadratic_sin_cos':coef.tolist(),
        'peak_amplitude_dB':float(np.hypot(coef[3],coef[4])),
        'phase_degrees':phase_degrees(coef[3],coef[4]),
        'residual_RMS_dB':float(np.sqrt(np.mean((y-X@coef)**2))),
        'polynomial_only_residual_RMS_dB':float(np.sqrt(np.mean((y-X[:,:3]@poly)**2))),
        'design_condition':float(np.linalg.cond(X))}

def regress_selfcheck(plan,centers):
    x=(centers-137.04)/.16; th=2*np.pi*plan['ratio_fit']['frequency_Hz']*(centers-plan['ratio_fit']['time_origin_seconds'])
    y=1+2*x-.7*x*x+3*np.sin(th)+.4*np.cos(th)
    r=fit_ratio(y,centers,plan)
    err=float(np.max(np.abs(np.array(r['coefficients_constant_linear_quadratic_sin_cos'])-[1,2,-.7,3,.4])))
    assert err<1e-11
    return {'joint_polynomial_sinusoid_max_coefficient_error':err}

def geometry_and_measure(plan, data, phase, centers):
    fs=plan['analysis_rate_Hz'];ncase=data.shape[1]; nframe=len(centers)
    nwin=len(plan['windows_seconds']);noff=len(plan['carrier_offset_sensitivity_Hz'])
    coefs=np.zeros((noff,nwin,nframe,36,ncase),dtype=np.complex128)
    residual=np.zeros((noff,nwin,nframe,ncase))
    geometries=[]
    for oi,off in enumerate(plan['carrier_offset_sensitivity_Hz']):
        adjusted=phase+2*np.pi*off*np.arange(len(phase))[:,None]/fs
        for wi,wsec in enumerate(plan['windows_seconds']):
            size=round(wsec*fs); hwin=np.hanning(size)
            local_t=(np.arange(size)-(size-1)/2)/(size/2)
            for ci,center_abs in enumerate(centers):
                center=round((center_abs-plan['source_context_seconds'][0])*fs)
                ix=np.arange(center-size//2,center+size//2)
                pp=(adjusted[ix,:,None]*np.arange(1,13)[None,None,:]).reshape(size,36)
                X=np.column_stack([np.cos(pp),np.sin(pp),np.ones(size),local_t])
                Xw=X*hwin[:,None]; norms=np.linalg.norm(Xw,axis=0);Xn=Xw/norms
                U,s,Vt=linalg.svd(Xn,full_matrices=False,check_finite=False)
                good=s>s[0]*1e-12;rank=int(good.sum());sinv=np.where(good,1/s,0)
                inv=(Vt.T*sinv)@U.T
                physinv=inv/norms[:,None]
                coeff=physinv@(data[ix]*hwin[:,None])
                coefs[oi,wi,ci]=coeff[:36]-1j*coeff[36:72]
                err=(data[ix]-X@coeff)*hwin[:,None]
                residual[oi,wi,ci]=np.sqrt(np.sum(err*err,axis=0)/np.sum(hwin*hwin))
                vif=np.sum((Vt.T*sinv)**2,axis=1)
                noisegain=np.linalg.norm(physinv*hwin[None,:],axis=1)
                targets=[]
                for h,col in zip(TARGET_H,TARGET_COL):
                    targets.append({'harmonic':h,'cos_VIF':float(vif[col]),'sin_VIF':float(vif[36+col]),
                        'cos_noise_gain_L2':float(noisegain[col]),'sin_noise_gain_L2':float(noisegain[36+col])})
                geometries.append({'offset_Hz':off,'window_seconds':wsec,'center_seconds':float(center_abs),
                    'rank':rank,'scaled_condition':float(s[0]/s[-1]),'singular_values':s.tolist(),'targets':targets})
            print(f'Estimated offset {off:+g} Hz, window {wsec*1000:g} ms; {ncase} signals × {nframe} frames',flush=True)
    return coefs,residual,geometries

def source_flags(plan, centers, coefs):
    old=json.loads((ARC/'research/brass1-release-spectra-160.json').read_text())['source']
    thresholds={(r['note'],r['harmonic']):r['Hh_power'] for r in old['thresholds']}
    pairs=[(r['note'],r['harmonic']) for r in old['thresholds']]
    oldtimes=np.array([r['time'] for r in old['rows']])
    nearest=[int(np.argmin(abs(oldtimes-(c-plan['source_context_seconds'][0])))) for c in centers]
    flags={}
    for oi,off in enumerate(plan['carrier_offset_sensitivity_Hz']):
        for wi,w in enumerate(plan['windows_seconds']):
            rows=[]
            for ci,c in enumerate(centers):
                targets=[]
                for h,col in zip(TARGET_H,TARGET_COL):
                    z=coefs[oi,wi,ci,col,0];power=float(abs(z)**2/2)
                    oldgood=bool(old['valid'][nearest[ci]][pairs.index((60,h))])
                    targets.append({'harmonic':h,'coefficient_real':float(z.real),'coefficient_imag':float(z.imag),
                        'power_proxy':power,'old_band_power_threshold':thresholds[(60,h)],
                        'below_old_threshold_proxy':power<thresholds[(60,h)],
                        'nearest_old_valid':oldgood,'nearest_old_time_seconds':float(oldtimes[nearest[ci]]+plan['source_context_seconds'][0]),
                        'weak':power<thresholds[(60,h)] or not oldgood})
                rows.append({'time_seconds':float(c),'harmonics':targets})
            flags[f'{off:g}/{w:g}']=rows
    return flags

def summarize(plan, meta, coefs, residual, geometry, centers, flags):
    measures=[]
    for oi,off in enumerate(plan['carrier_offset_sensitivity_Hz']):
        for wi,w in enumerate(plan['windows_seconds']):
            for si,m in enumerate(meta):
                targets=[]
                amp=np.abs(coefs[oi,wi,:,:,si])
                for h in [3,4]:
                    y=20*np.log10(np.maximum(amp[:,12+h-1],1e-300)/np.maximum(amp[:,13],1e-300))
                    fit=fit_ratio(y,centers,plan)
                    item={'name':f'C4_H{h}_H2','ratio_dB':y.tolist(),'fit':fit}
                    if m['family']=='positive':
                        item['recovery_gain']=fit['peak_amplitude_dB']/3
                        item['phase_error_degrees']=phase_delta(fit['phase_degrees'],m['input_phase_degrees'])
                    targets.append(item)
                measures.append({'offset_Hz':off,'window_seconds':w,'case_id':m['id'],
                    'weighted_residual_RMS':residual[oi,wi,:,si].tolist(),'targets':targets})
    idx={(m['offset_Hz'],m['window_seconds'],m['case_id']):m for m in measures}
    g0=[g for g in geometry if g['offset_Hz']==0]
    gp=all(g['rank']==74 and g['scaled_condition']<=1e6 and max(max(t['cos_VIF'],t['sin_VIF']) for t in g['targets'])<=10 for g in g0)
    decision=[]
    for ti,h in enumerate([3,4]):
        rows=[]
        for w in plan['windows_seconds']:
            src=idx[(0,w,'source')]['targets'][ti]['fit']
            null=[idx[(0,w,m['id'])]['targets'][ti]['fit']['peak_amplitude_dB'] for m in meta if m['family']=='flat_ratio_null']
            static=[idx[(0,w,m['id'])]['targets'][ti]['fit']['peak_amplitude_dB'] for m in meta if m['family']=='static_filter_null']
            pos=[idx[(0,w,m['id'])]['targets'][ti] for m in meta if m['family']=='positive']
            recovery=all(.5<=p['recovery_gain']<=1.5 and abs(p['phase_error_degrees'])<=20 for p in pos)
            gains=[p['recovery_gain'] for p in pos]; rg=float(np.sqrt(np.mean(np.square(gains))))
            weak=sum(any(t['weak'] for t in row['harmonics'] if t['harmonic'] in [2,h]) for row in flags[f'0/{w:g}'])
            threshold=max(1,2*max(null),2*max(static))
            rows.append({'window_seconds':w,'source_peak_amplitude_dB':src['peak_amplitude_dB'],
                'source_phase_degrees':src['phase_degrees'],'max_flat_ratio_null_dB':max(null),
                'max_static_filter_null_dB':max(static),'decision_threshold_dB':threshold,
                'weak_source_frame_count':weak,'positive_gain_range':[min(gains),max(gains)],
                'positive_phase_error_max_degrees':max(abs(p['phase_error_degrees']) for p in pos),
                'positive_RMS_gain':rg,'source_gain_adjusted_amplitude_dB':src['peak_amplitude_dB']/rg,
                'recovery_pass':recovery,'flat_null_pass':max(null)<=.5,
                'amplitude_exceeds_declared_threshold':src['peak_amplitude_dB']>threshold})
        spread=max(abs(phase_delta(a['source_phase_degrees'],b['source_phase_degrees'])) for a in rows for b in rows)
        ratio=max(r['source_gain_adjusted_amplitude_dB'] for r in rows)/min(r['source_gain_adjusted_amplitude_dB'] for r in rows)
        passed=gp and spread<=30 and ratio<=1.5 and all(r['weak_source_frame_count']==0 and r['recovery_pass'] and r['flat_null_pass'] and r['amplitude_exceeds_declared_threshold'] for r in rows)
        sensitivity=[]
        for off in plan['carrier_offset_sensitivity_Hz']:
            sensitivity.append({'offset_Hz':off,'source_by_window':[{'window_seconds':w,**idx[(off,w,'source')]['targets'][ti]['fit']} for w in plan['windows_seconds']]})
        decision.append({'target':f'C4_H{h}_H2','windows':rows,'source_phase_spread_degrees':spread,
            'gain_adjusted_amplitude_max_over_min':ratio,'passes_all_predeclared_gates':passed,'carrier_sensitivity':sensitivity})
    overall=gp and all(d['passes_all_predeclared_gates'] for d in decision)
    return measures,{'geometry_pass':gp,'targets':decision,'both_targets_pass':overall,
        'interpretation':('Stable ratio modulation beyond this limited null family, without physical route identification.' if overall else 'The strict predeclared coloration criterion did not pass. Failures remain reported; no physical route is identified or excluded.')}

def plot_results(report,path):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(2,3,figsize=(13,7),sharex=True)
    t=np.array(report['centers_seconds']);meta={m['id']:m for m in report['cases']}
    for ti,h in enumerate([3,4]):
        for wi,w in enumerate(report['plan_settings']['windows_seconds']):
            ax=axes[ti,wi]; rows=[m for m in report['measurements'] if m['offset_Hz']==0 and m['window_seconds']==w]
            null=[]
            for r in rows:
                if meta[r['case_id']]['family']=='static_filter_null':
                    yy=np.array(r['targets'][ti]['ratio_dB']);null.append(yy-np.mean(yy))
            null=np.array(null)
            ax.fill_between(t,np.min(null,axis=0),np.max(null,axis=0),color='#c4c9b7',alpha=.6,label='Static-color controls, mean centered')
            for off,color in [(0,'#222222'),(-2,'#bf792e'),(2,'#4b82aa')]:
                r=next(m for m in report['measurements'] if m['case_id']=='source' and m['offset_Hz']==off and m['window_seconds']==w)
                yy=np.array(r['targets'][ti]['ratio_dB']);ax.plot(t,yy-np.mean(yy),color=color,lw=1.4,label=f'Source carrier {off:+g} Hz')
            ax.set_title(f'C4 H{h}/H2 · {w*1000:g} ms')
            ax.grid(alpha=.2);ax.set_ylabel('Ratio minus own mean (dB)')
            if ti==1:ax.set_xlabel('Source time (s)')
    axes[0,0].legend(fontsize=7,loc='best')
    fig.suptitle('Source-only ratio diagnostic: static-color spread is illustrative, not a universal bound')
    fig.tight_layout();fig.savefig(path,dpi=160);plt.close(fig)

def write_note(report):
    d=report['decision'];lines=['# Brass short-window harmonic-ratio diagnostic','',
        'This source-only study used the existing recording and pitch track. It rendered no Ceres audio and changed no DSP or default. All settings and thresholds were declared before estimates. The excerpt spans 2.134 cycles of the inherited fixed 6.6692178847 Hz rate; every overlapping frame is retained.','',
        '| Target | Window | Source fixed-rate peak | Flat-ratio null max | Static-filter null max | Weak frames | Declared gates |',
        '|---|---:|---:|---:|---:|---:|---|']
    for target in d['targets']:
        for r in target['windows']:
            lines.append(f"| {target['target']} | {r['window_seconds']*1000:.0f} ms | {r['source_peak_amplitude_dB']:.3f} dB | {r['max_flat_ratio_null_dB']:.3f} dB | {r['max_static_filter_null_dB']:.3f} dB | {r['weak_source_frame_count']} | {'pass' if target['passes_all_predeclared_gates'] else 'fail'} |")
    lines+=['',d['interpretation'],'',
        'The 36 static-filter controls include LP corners at 400, 800, 1600 Hz and at 2/3/4 times the previously measured early C4 fundamental, with Q 0.5/2/5 and AM before/after each fixed filter. They are a limited illustration, not a universal bound on unknown coloration. Carrier offsets ±2 Hz are sensitivity cases, not an established error bound.','',
        'Complex coefficients for all 36 carriers, source masks, target powers, singular values, VIFs, residuals, every control curve and positive-recovery errors remain in report.json and arrays.npz. The old power masks came from 160 ms band measurements; their thresholds applied to local coefficient power are explicitly only a proxy. No weak row was dropped.','',
        'Same-note ratios can reject a tested spectrally flat common-gain explanation but cannot identify HP versus LP motion, ring versus VCA, hardware control positions, or the capture chain. A null does not rule out filter modulation: some filter regions act nearly as common gain over these harmonics. No original hardware MIDI is available.','',
        'Reproduce with `python3 Tools/accuracy/brass_ratio_modulation.py check --recompute` from the repository. The immutable plan, implementation declaration and artifact hashes bind this run.']
    (OUT/'README.md').write_text('\n'.join(lines)+'\n')

def compute(plan):
    wav=next(Path(p) for p in plan['input_sha256'] if p.endswith('whitmore-brass1-context.wav'))
    fs,source=wavfile.read(wav);assert fs==48000 and source.shape==(76800,2)
    source=source.astype(np.float64).mean(axis=1);assert np.isfinite(source).all()
    t,f,phase,pitchrows=pitch_trajectory(plan,len(source))
    waves,meta=fixtures(plan,phase,t)
    meta=[{'id':'source','family':'recording'}]+meta
    full=np.column_stack([source]+waves)
    data=signal.resample_poly(full,1,4,axis=0)
    centers=plan['analysis_seconds'][0]+np.arange(plan['centers_count'])*plan['hop_seconds']
    check=regress_selfcheck(plan,centers)
    coefs,residual,geo=geometry_and_measure(plan,data,phase[::4],centers)
    flags=source_flags(plan,centers,coefs)
    measures,decision=summarize(plan,meta,coefs,residual,geo,centers,flags)
    report={'schema':'ceres-brass-ratio-modulation-report-v1','plan_sha256':sha(PLAN),
        'implementation_sha256':sha(OUT/'implementation.json'),'script_sha256':sha(SCRIPT),
        'input_sha256':plan['input_sha256'],'plan_settings':plan,'cases':meta,'centers_seconds':centers.tolist(),
        'pitch_frames_used':pitchrows,'selfchecks':check,'geometry':geo,'source_flags':flags,
        'measurements':measures,'decision':decision,'native_render_calls':0,'DSP_parameter_fits':0,
        'original_hardware_MIDI':False,'strict_eligible':False,
        'amplitude_definition':'sqrt(cos_coefficient²+sin_coefficient²); complex coefficient cos−i*sin; ratio20log10(amplitude_h/amplitude_2)',
        'array_axes':{'complex_harmonics':['offset','window','frame','note-major H1..12','case'],
                      'residual':['offset','window','frame','case']}}
    arrays={'complex_harmonics':coefs,'residual':residual,'source_pitch_Hz_48k':f,'source_phase_radians_48k':phase,
        'centers_seconds':centers,'case_ids':np.array([m['id'] for m in meta]),'note_harmonics':np.array([(n,h) for n in NOTES for h in range(1,13)])}
    return report,arrays

def run():
    plan=load_plan();verify_identity()
    for name in ['report.json','arrays.npz','README.md','ratios.png','artifacts.json']:
        assert not (OUT/name).exists(),f'Refuse to overwrite {name}'
    started=time.monotonic();report,arrays=compute(plan)
    np.savez_compressed(OUT/'arrays.npz',**arrays)
    report['arrays_sha256']=sha(OUT/'arrays.npz')
    write_json(OUT/'report.json',report)
    plot_results(report,OUT/'ratios.png');write_note(report)
    bindings={str((OUT/n).relative_to(ROOT)):sha(OUT/n) for n in ['plan.json','proposal-initial.json','implementation.json','report.json','arrays.npz','README.md','ratios.png']}
    bindings[str(SCRIPT.relative_to(ROOT))]=sha(SCRIPT)
    write_json(OUT/'artifacts.json',{'schema':'ceres-source-ratio-artifacts-v1','sha256':bindings})
    print(json.dumps({'seconds':time.monotonic()-started,'report_sha256':sha(OUT/'report.json'),'decision':report['decision']},indent=2),flush=True)

def check(recompute):
    load_plan();verify_identity()
    artifacts=json.loads((OUT/'artifacts.json').read_text())
    for p,h in artifacts['sha256'].items():assert sha(ROOT/p)==h,p
    report=json.loads((OUT/'report.json').read_text());a=np.load(OUT/'arrays.npz')
    assert len(report['cases'])==61 and len(report['measurements'])==549 and len(report['geometry'])==729
    assert a['complex_harmonics'].shape==(3,3,81,36,61)
    assert np.isfinite(a['complex_harmonics']).all() and np.isfinite(a['residual']).all()
    maxerr=0.0
    if recompute:
        fresh,arrays=compute(load_plan())
        for k,v in arrays.items():
            if v.dtype.kind in 'fc':
                err=float(np.max(np.abs(v-a[k])));maxerr=max(err,maxerr);assert np.allclose(v,a[k],rtol=1e-11,atol=1e-12),(k,err)
            else:assert np.array_equal(v,a[k]),k
        # Deterministic complete JSON evidence apart from the NPZ container hash.
        old=dict(report);old.pop('arrays_sha256');assert old==fresh,'Recomputed report differs'
    print(json.dumps({'status':'pass','bindings':len(artifacts['sha256'])+len(report['input_sha256']),
        'cases':61,'frames_per_window':81,'carrier_offsets':3,'full_recompute':recompute,'maximum_array_difference':maxerr}))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=['freeze','run','check']);ap.add_argument('--recompute',action='store_true');args=ap.parse_args()
    if args.action=='freeze':identity()
    elif args.action=='run':run()
    else:check(args.recompute)
