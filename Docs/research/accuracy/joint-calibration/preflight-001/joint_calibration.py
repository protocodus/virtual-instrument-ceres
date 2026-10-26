#!/usr/bin/env python3
"""Qualify a full-window spectral-change surrogate on retained native renders.

No recording targets, parameter fitting, new WAVs, transfer-case inspection, or
production edits. Native control stepping requires explicit frozen permission.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import traceback

import numpy as np
from scipy.io import wavfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'Docs/accuracy/joint-calibration'
DC = ROOT/'Docs/accuracy/preset-dc-loading'
TRAIN = ['flute-high-factory-physical', 'brass-history20']
FS, INTERNAL_FS = 48000., 192000.
CI, CS, CQ = 1.5e-9, 1e-6, 180e-12
GG, GQ, GF = 1/33e3, 1/1.2e6, 1/4.7e6
FLOOR = GF/(2*np.pi*CI)
THRESHOLDS = dict(ratio_rms_db=1., ratio_p95_abs_db=2., brass_cell_mean_abs_db=.5,
                  level_rms_db=1., level_p95_abs_db=2., quadrature_delta_max_db=.1)
PINS = {
    'Source/DSP/ceres.cpp': '3478c02cc7c134a698bf0a32b708b2bda8e7eb0e38f97d27ee458a98c481baaa',
    'Source/DSP/ceres.hpp': '577cbd5909705d7db3a1522edbbfc71ec056d03751bbb97390413cf8cd6375f8',
}
METRIC_PATH = ROOT/'Tools/accuracy/metrics.py'
spec = importlib.util.spec_from_file_location('retained_measurements', METRIC_PATH)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def write(path, value):
    assert not path.exists(), 'Preserve evidence: '+str(path)
    path.write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False)+'\n')


def stage(frequency, cutoff, resonance, gs, reference_gs, highpass):
    """Canonical small-signal TPT transfer including both physical RC nodes."""
    s = 2j*INTERNAL_FS*np.tan(np.pi*frequency/INTERNAL_FS)
    cutoff = np.clip(cutoff, FLOOR, .22*INTERNAL_FS)
    omega = np.where(cutoff<=FLOOR, GF/CI,
                     2*INTERNAL_FS*np.tan(np.pi*cutoff/INTERNAL_FS))
    ys = gs*s*CS/(gs+s*CS)
    node = ys+GG+GQ+s*CQ
    gb = (2-1.8*resonance)*(reference_gs+GG+GQ)/2-GQ
    feedback = gb+GQ+s*CQ
    return 2*ys*(s*s if highpass else omega*omega)/(node*(s*s+omega*omega)+2*omega*feedback*s)


def independent_nodal(frequency, cutoff, resonance, gs, reference_gs, highpass):
    s = 2j*INTERNAL_FS*np.tan(np.pi*frequency/INTERNAL_FS)
    omega = GF/CI if cutoff<=FLOOR else 2*INTERNAL_FS*np.tan(np.pi*min(cutoff,.22*INTERNAL_FS)/INTERNAL_FS)
    gb = (2-1.8*resonance)*(reference_gs+GG+GQ)/2-GQ
    a = np.array([[1,0,1,-2,0], [omega,s,0,0,0], [0,omega,s,0,0],
        [0,-(gb+GQ+s*CQ),0,gs+GG+GQ+s*CQ,gs], [0,0,0,gs,gs+s*CS]], complex)
    rhs = np.array([0,0,0,gs,gs], complex)
    x = np.linalg.solve(a,rhs)
    residual = np.max(abs(a@x-rhs)/np.maximum(abs(a)@abs(x)+abs(rhs),1e-30))
    return x[0 if highpass else 2], float(residual)


def coupling(frequency, tau):
    s = 2j*INTERNAL_FS*np.tan(np.pi*frequency/INTERNAL_FS)
    return s*tau/(1+s*tau)


def source_coefficients(f0, harmonics, reset_us):
    """Ascending finite-reset saw; c_h convention is x=sum(c_h exp(ih phi)+c.c.)."""
    recharge = np.clip(f0*reset_us*1e-6, 1e-7, .35)
    a = 2*np.pi*harmonics
    coefficient = -(2/(1-recharge)+2/recharge)*(-np.expm1(-1j*a*(1-recharge)))/(a*a)
    return coefficient*np.sinc(f0*harmonics/INTERNAL_FS)**2


def decimator(frequency):
    # Exact 249-tap production FIR magnitude/phase. Evaluate once per frame's
    # slowly changing f0 grid via vectorized finite taps (not a fitted curve).
    taps = np.arange(249)
    impulse = 2*.1125*np.sinc(2*.1125*(taps-124))*np.kaiser(249,10)
    impulse /= impulse.sum()
    return np.exp(-2j*np.pi*frequency[...,None]*taps/INTERNAL_FS)@impulse


def oracles():
    max_transfer = max_kcl = 0.
    for highpass,gs,reference in [(True,3/150e3,3/150e3),(False,1/150e3,1/150e3)]:
        for q in [0., .429, .767, 1.]:
            for fc in [FLOOR, 40., 217., 443., 1931., 12000.]:
                for f in [220., 530., 670., 1340., 4200.]:
                    expected,kcl = independent_nodal(f,fc,q,gs,reference,highpass)
                    actual = stage(f,fc,q,gs,reference,highpass)
                    max_transfer = max(max_transfer,float(abs(actual-expected)/max(abs(expected),1e-30)))
                    max_kcl = max(max_kcl,kcl)
    assert max_transfer<1e-10 and max_kcl<1e-10
    # The ideal shaper sine is cos(phi), hence +0.5 at H1; its phase is not
    # interchangeable with the +imaginary ascending-saw fundamental.
    phases = np.arange(131072)/131072
    triangle = 2*abs(1-2*phases)-1
    sine = np.sin(.5*np.pi*triangle)
    c1 = np.mean(sine*np.exp(-2j*np.pi*phases))
    sine_error = float(abs(c1-.5))
    assert sine_error<1e-10
    # Full-window power integration is invariant to common scalar gain in
    # ratios and predicts its absolute level exactly.
    grid = np.arange(4096)/4096
    weights = (.5-.5*np.cos(2*np.pi*grid))**2
    amplitude = np.array([1., .4, .2])[None,:]*(.3+np.exp(-grid[:,None]))
    power = np.sum(weights[:,None]*amplitude**2,axis=0)/weights.sum()
    scaled = np.sum(weights[:,None]*(amplitude*2.3)**2,axis=0)/weights.sum()
    scalar_error = float(np.max(abs(10*np.log10(scaled/scaled[0])-10*np.log10(power/power[0]))))
    zero_error = float(np.max(abs(10*np.log10(power/power))))
    assert scalar_error<=1e-10 and zero_error<=1e-10
    return dict(transfer_max_relative_error=max_transfer, normalized_KCL_max_error=max_kcl,
                direct_sine_coefficient_error=sine_error, scalar_AM_ratio_error_db=scalar_error,
                zero_change_error_db=zero_error, audio_renders=0)


def declaration():
    plan = read(OUT/'plan.json')
    q = plan['qualification']
    assert q['training_ids']==TRAIN
    assert all(q[k]==v for k,v in THRESHOLDS.items())
    assert q['mode']=='dc_only' and q['fit_allowed'] is False
    assert q['allow_control_trace_stepping'] is True
    return plan


def selected_cases():
    # The immutable DC plan embeds old score fields; none is accessed here.
    # Do not load its aggregate report or any transfer-case result.
    original = read(DC/'plan.json')
    cases = [c for c in original['cases'] if c['original_case']['id'] in TRAIN]
    assert len(cases)==34
    assert sum(c['kind']=='frozen' for c in cases)==2
    assert sum(c['kind']=='stiff' for c in cases)==8
    assert sum(c['kind']=='loaded' for c in cases)==24
    return original,cases


def bindings(cases, dcplan):
    paths = [Path(__file__).resolve(), OUT/'control_trace.cpp', OUT/'plan.json',
        METRIC_PATH, DC/'plan.json', ROOT/'Docs/accuracy/plan.json',
        ROOT/'Docs/accuracy/inputs/research/brass1-release-spectra-160.json']
    identity = DC/'identity'
    for name,digest in dcplan['compiled_source_sha256'].items():
        path = identity/name
        assert sha(path)==digest
        paths.append(path)
    for name,digest in PINS.items():
        assert sha(identity/name)==digest
    for c in cases:
        row = read(DC/(c['id']+'.json'))
        assert row['id']==c['id'] and row['audio']==c['audio']
        assert sha(ROOT/c['audio'])==row['audio_sha256']
        assert sha(ROOT/c['baseline_audio'])==c['baseline_audio_sha256']
        paths += [ROOT/c['audio'],ROOT/c['patch'],DC/(c['id']+'.json'),
                  ROOT/c['original_case']['events'],ROOT/c['original_case']['midi']]
    return {str(p):sha(p) for p in sorted(set(paths))}


def command(args, log):
    result = subprocess.run([str(a) for a in args],capture_output=True,text=True)
    write(log,dict(command=[str(a) for a in args],returncode=result.returncode,
                   stdout=result.stdout,stderr=result.stderr))
    if result.returncode: raise RuntimeError('Command failed; retained '+str(log))
    return result.stdout


def trace_controls(folder, cases):
    compiler = shutil.which('clang++')
    assert compiler
    identity = DC/'identity/Source/DSP'
    binary = folder/'control_trace'
    command([compiler,'-std=c++17','-O2','-fno-fast-math','-I',identity,
             OUT/'control_trace.cpp',identity/'ceres.cpp','-o',binary],folder/'compile.json')
    answer = {}
    for c in cases:
        if c['kind']!='frozen': continue
        clip = c['original_case']; name=clip['id']; path=folder/(name+'.f64')
        output = command([binary,clip['preset'],ROOT/c['patch'],ROOT/clip['events'],
                          clip['frames'],clip['seed'],path],folder/(name+'-trace-command.json'))
        metadata = json.loads(output)
        metadata.update(binary_sha256=sha(binary),trace_sha256=sha(path),
            trace=str(path),dtype='little-endian IEEE float64',
            global_columns=['time_after_host_sample','commonVcaGain','hpResonance','lpResonance','sub'],
            voice_columns=['note','keyNote','held','pedalHeld','ampEnvelope','filterEnvelope',
                           'oscillatorPhase','oscillatorFrequency','hpFrequency','lpFrequency'],
            state_timing='Final internal sample after process; oscillatorPhase includes its increment.',
            audio_discarded=True,audio_file_created=False)
        write(folder/(name+'-trace.json'),metadata)
        answer[name]=metadata
    return answer


def read_trace(metadata):
    assert sys.byteorder=='little'
    path=Path(metadata['trace']); assert sha(path)==metadata['trace_sha256']
    data=np.fromfile(path,dtype='<f8').reshape(-1,45)
    assert data.shape==(metadata['records'],metadata['columns'])
    assert np.isfinite(data).all()
    p=metadata['parameters']
    # Explicitly restrict this surrogate to the two declared saw-based cases.
    # Unsupported sources fail; they are not silently replaced by a saw.
    for key in ['pulse','noise','pwmDepth','filterLfo','brilliance','ringMix','pressureFilter',
                'pressureVolume','pressureVibrato','velocitySensitivity','voiceSpreadCents','driftCents']:
        assert p[key]==0., 'Unsupported frozen source/control: '+key
    assert p['saw']==1. and p['filterLevel']==1. and p['keyTrack']==1.
    assert p['sine'] in (0.,.043) and p['resetMicroseconds']==6.
    return data,p


def masks_and_windows(clip):
    if clip['family']=='flute':
        times=np.arange(.064,clip['duration']-.064+1e-8,.04)
        return times,.128,np.ones((len(times),6),bool)
    raw=read(ROOT/'Docs/accuracy/inputs/research/brass1-release-spectra-160.json')
    # Read only frozen centers/eligibility, never hardware ratios or amplitudes.
    times=np.array([r['time'] for r in raw['source']['rows']])
    valid=np.asarray(raw['source']['valid'],bool)
    assert valid.shape==(len(times),len(m.PAIRS))
    return times,.160,valid


def native_measurement(case, times, duration):
    clip=case['original_case']; rate,x=wavfile.read(ROOT/case['audio'])
    assert rate==48000 and x.ndim==1 and x.dtype.kind=='f' and len(x)==clip['frames']
    x=x.astype(float); assert np.isfinite(x).all()
    if clip['family']=='flute':
        rows=[m.measure_frame(x,rate,float(t+clip['offset_seconds']),clip['expected_f0'],
                             count=32,duration=duration) for t in times]
        ratios=np.array([r['relative_db'][1:7] for r in rows])
        levels=20*np.log10(np.maximum([r['rms'] for r in rows],1e-15))[:,None]
        return ratios,levels
    offset=round(clip['offset_seconds']*rate)
    aligned=x[offset:offset+round(clip['duration']*rate)]
    rows=m.spectral_rows(aligned,times,duration)
    ratios=[]; levels=[]
    for row in rows:
        power={(n,h):m.band(row,n,h)['band_power'] for n,h in [(n,1) for n in m.NOTES]+m.PAIRS}
        ratios.append([10*np.log10(max(power[n,h],1e-30)/max(power[n,1],1e-30)) for n,h in m.PAIRS])
        levels.append([10*np.log10(max(power[n,1],1e-30)) for n in m.NOTES])
    return np.asarray(ratios),np.asarray(levels)


def coefficients(trace, p, hp_base, lp_base, harmonics):
    """Instantaneous complex coefficients, then power is integrated over windows.

    The approximation concerns changing filter state, not the source phase:
    filtered saw and direct cosine remain a coherent complex sum at each h.
    All physical voices are retained, including their silent/released states.
    """
    v=trace[:,5:].reshape(-1,4,10)
    f0=v[:,:,7,None]
    h=np.asarray(harmonics)[None,None,:]
    f=f0*h
    assert np.all(f0>0)
    # Frozen baseline frequencies expose exact key/bend and own-EG schedules.
    # Only reference cutoff differs in the retained DC-only cases.
    hp_factor=(v[:,:,8]-FLOOR)/(p['hpCutoff']-FLOOR)
    lp_factor=(v[:,:,9]-FLOOR)/(p['lpCutoff']-FLOOR)
    hp=FLOOR+(hp_base-FLOOR)*hp_factor
    lp=FLOOR+(lp_base-FLOOR)*lp_factor
    assert np.all(hp>=FLOOR) and np.all(lp>=FLOOR)
    assert np.max(hp)<.22*INTERNAL_FS and np.max(lp)<.22*INTERNAL_FS
    filtered=stage(f,hp[:,:,None],trace[:,2,None,None],3/150e3,3/150e3,True)
    filtered*=stage(f,lp[:,:,None],trace[:,3,None,None],1/150e3,1/150e3,False)
    hp_nominal=3/150e3+GG+GQ; lp_nominal=1/150e3+GG+GQ
    calibration=.8/((2/(150e3*hp_nominal))*(2/(150e3*lp_nominal)))
    c=source_coefficients(f0,h,p['resetMicroseconds'])*filtered*calibration*coupling(f,.1)
    c=c*p['filterLevel']+(h==1)*(.5*p['sine'])
    for tau in [.127,4.7e-6*(1e3+4*47e3),33e-6*(4.7e3+560)]:
        c*=coupling(f,tau)
    # FIR phase cancels from each harmonic's power, so a compact interpolation
    # table of its analytic magnitude avoids a huge time*voice*harmonic*tap array.
    frequencies=np.linspace(0.,FS/2,12001)
    response=abs(decimator(frequencies))
    c*=np.interp(np.clip(f,0,FS/2),frequencies,response)
    c*=v[:,:,4,None]*trace[:,1,None,None]*p['volume']*10**(p['outputTrimDb']/20)
    c=np.where(f<FS*.45,c,0.)
    return c,f0[:,:,0],v[:,:,0],v[:,:,6]


def surrogate(case, metadata, times, duration, stride):
    trace,p=read_trace(metadata)
    trace=trace[::stride]
    # Production's FIR group delay is31 host frames. Other carrier-frequency
    # phase responses are already in the analytic cascade; changing-state
    # memory remains an explicitly tested quasistationary approximation.
    model_time=trace[:,0]+31/FS
    values=case['transformed_values']
    hp=values.get('hp_cutoff',p['hpCutoff']); lp=values.get('lp_cutoff',p['lpCutoff'])
    clip=case['original_case']
    harmonics=np.arange(1,65 if clip['family']=='flute' else 9)
    coeff,f0,notes,phase=coefficients(trace,p,hp,lp,harmonics)
    powers=2*abs(coeff)**2
    ratios=[]; levels=[]; windows=[]
    for center in times+clip['offset_seconds']:
        start,end=center-duration/2,center+duration/2
        indices=np.flatnonzero((model_time>=start)&(model_time<end))
        assert len(indices)>100 and model_time[0]<=start and model_time[-1]>=end
        t=(model_time[indices]-start)/duration
        w=(.5-.5*np.cos(2*np.pi*t))**2
        spectral=np.sum(powers[indices]*w[:,None,None],axis=0)/w.sum()
        # Quadrature treats resolved note ladders incoherently, exactly the
        # isolated-band target family. Coincident active voices are rejected.
        active=np.max(powers[indices],axis=(0,2))>1e-24
        n=np.median(notes[indices],axis=0).astype(int)
        assert all(np.all(notes[indices,i]==n[i]) for i in np.flatnonzero(active)), 'Note change inside window'
        assert len(set(n[active]))==int(active.sum()), 'Coincident active voices unsupported'
        if clip['family']=='flute':
            assert active.sum()==1, 'Flute is not a single frozen active voice'
            band=spectral.sum(axis=0)
            ratios.append(10*np.log10(np.maximum(band[1:7],1e-30)/max(band[0],1e-30)))
            rms2=float(np.mean(powers[indices].sum(axis=(1,2))))
            levels.append([10*np.log10(max(rms2,1e-30))])
        else:
            # Frozen physical note69/72/76 with octave−1 sounds57/60/64.
            lookup={int(note+12*p['octave']):i for i,note in enumerate(n) if active[i]}
            assert set(m.NOTES)<=set(lookup), 'Brass note missing from fixed window'
            band={note:spectral[lookup[note]] for note in m.NOTES}
            ratios.append([10*np.log10(max(band[note][h-1],1e-30)/max(band[note][0],1e-30)) for note,h in m.PAIRS])
            levels.append([10*np.log10(max(band[note][0],1e-30)) for note in m.NOTES])
        windows.append(dict(center=float(center),points=len(indices),
                            active_voices=int(active.sum())))
    return np.asarray(ratios),np.asarray(levels),windows


def summary(error):
    error=np.asarray(error)
    assert error.size and np.isfinite(error).all()
    return dict(count=error.size,rms_db=float(np.sqrt(np.mean(error**2))),
                p95_abs_db=float(np.percentile(abs(error),95)),
                max_abs_db=float(np.max(abs(error))),signed_mean_db=float(np.mean(error)))


def qualify_calculation(folder, cases, traces):
    result=[]; baseline={}; all_pass=True
    for name in TRAIN:
        group=[c for c in cases if c['original_case']['id']==name]
        base=next(c for c in group if c['kind']=='frozen')
        clip=base['original_case']; times,duration,valid=masks_and_windows(clip)
        native0=native_measurement(base,times,duration)
        predicted0=surrogate(base,traces[name],times,duration,2)
        fine0=surrogate(base,traces[name],times,duration,1)
        baseline[name]=dict(id=base['id'],native_ratios_db=native0[0].tolist(),
            native_levels_db=native0[1].tolist(),analytical_ratios_db=predicted0[0].tolist(),
            analytical_levels_db=predicted0[1].tolist(),valid=valid.tolist(),
            times=times.tolist(),duration=duration,full_window_quadrature=True)
        for case in group:
            if case['kind']=='frozen': continue
            native=native_measurement(case,times,duration)
            predicted=surrogate(case,traces[name],times,duration,2)
            fine=surrogate(case,traces[name],times,duration,1)
            native_delta=[a-b for a,b in zip(native,native0)]
            predicted_delta=[predicted[k]-predicted0[k] for k in (0,1)]
            fine_delta=[fine[k]-fine0[k] for k in (0,1)]
            error=native_delta[0]-predicted_delta[0]
            level_error=native_delta[1]-predicted_delta[1]
            level_valid=np.ones_like(level_error,dtype=bool)
            cells=[]
            if clip['family']=='brass':
                for j,note in enumerate(m.NOTES):
                    cols=[i for i,pair in enumerate(m.PAIRS) if pair[0]==note]
                    level_valid[:,j]=valid[:,cols].any(axis=1)
                    for label,early in [('early',True),('release',False)]:
                        rows=(times+136.4<=137.08+1e-8)==early
                        selected=valid[np.ix_(rows,cols)]
                        cells.append(dict(note=note,phase=label,**summary(error[np.ix_(rows,cols)][selected])))
            ratio_stats=summary(error[valid]); level_stats=summary(level_error[level_valid])
            convergence=max(float(np.max(abs(predicted_delta[0]-fine_delta[0]))),
                            float(np.max(abs(predicted_delta[1]-fine_delta[1]))))
            checks=dict(ratio_rms=ratio_stats['rms_db']<=THRESHOLDS['ratio_rms_db'],
                ratio_p95=ratio_stats['p95_abs_db']<=THRESHOLDS['ratio_p95_abs_db'],
                level_rms=level_stats['rms_db']<=THRESHOLDS['level_rms_db'],
                level_p95=level_stats['p95_abs_db']<=THRESHOLDS['level_p95_abs_db'],
                brass_cells=all(abs(c['signed_mean_db'])<=THRESHOLDS['brass_cell_mean_abs_db'] for c in cells),
                quadrature=convergence<=THRESHOLDS['quadrature_delta_max_db'])
            passed=all(checks.values()); all_pass &= passed
            result.append(dict(id=case['id'],kind=case['kind'],clip=name,passed=passed,checks=checks,
                ratio_delta_error=ratio_stats,level_delta_error=level_stats,brass_cells=cells,
                quadrature_delta_max_db=convergence,native_delta_ratios_db=native_delta[0].tolist(),
                native_delta_levels_db=native_delta[1].tolist(),
                analytical_delta_ratios_db=predicted_delta[0].tolist(),
                analytical_delta_levels_db=predicted_delta[1].tolist(),
                anchored_prediction_ratios_db=(native0[0]+predicted_delta[0]).tolist(),
                anchored_prediction_levels_db=(native0[1]+predicted_delta[1]).tolist(),
                ratio_error_db=error.tolist(),level_error_db=level_error.tolist(),
                source_valid_mask_changed=False,native_exclusions=0))
    assert len(result)==32
    return dict(status='qualification_pass' if all_pass else 'qualification_failed',
        qualification_passed=all_pass,baselines=baseline,cases=result,thresholds=THRESHOLDS,
        interpretation='Native baseline plus full-window analytical change; no source target or fit.',
        limitations=['Quasistationary small-signal filter response omits changing capacitor memory and finite drive.',
            'Resolved partial powers add incoherently across distinct voices; direct sine and filtered saw add coherently within each voice.',
            'Only DC cutoff changes are qualified here; qualification does not establish full-network dynamic accuracy.',
            'Hann-squared spectral power integration and rectangular Flute RMS integration approximate the retained finite windows.',
            'No original recording samples enter synthesis, and no audio is written by this tool.'],
        new_audio_renders=0,source_recordings_read=0,parameter_fits=0,transfer_cases_read=0,
        qualification_conditions=32,control_trace_runs=2,fit_allowed=False)


def run(attempt):
    declaration(); dcplan,cases=selected_cases()
    assert attempt and Path(attempt).name==attempt and attempt not in ('.','..')
    folder=OUT/attempt; folder.mkdir()
    shutil.copy2(Path(__file__),folder/'joint_calibration.py')
    shutil.copy2(OUT/'control_trace.cpp',folder/'control_trace.cpp')
    try:
        inputs=bindings(cases,dcplan)
        write(folder/'declaration.json',dict(input_sha256=inputs,thresholds=THRESHOLDS,
            selected_ids=[c['id'] for c in cases],no_transfer_or_source_targets=True))
        checks=oracles(); write(folder/'oracles.json',checks)
        traces=trace_controls(folder,cases)
        report=qualify_calculation(folder,cases,traces)
        report.update(input_sha256=inputs,oracles=checks,trace_metadata=traces,
            declaration_sha256=sha(folder/'declaration.json'),attempt=attempt)
        assert bindings(cases,dcplan)==inputs
        write(folder/'report.json',report)
        print(report['status'],folder/'report.json',flush=True)
    except BaseException:
        write(folder/'failure.json',dict(traceback=traceback.format_exc(),new_audio_files=0,
            parameter_fits=0,transfer_cases_read=0))
        raise


def check(attempt):
    folder=OUT/attempt; saved=read(folder/'report.json')
    assert sha(folder/'joint_calibration.py')==sha(Path(__file__))
    assert sha(folder/'control_trace.cpp')==sha(OUT/'control_trace.cpp')
    assert sha(folder/'declaration.json')==saved['declaration_sha256']
    declaration(); dcplan,cases=selected_cases()
    assert bindings(cases,dcplan)==saved['input_sha256']
    actual=qualify_calculation(folder,cases,saved['trace_metadata'])
    actual.update(input_sha256=saved['input_sha256'],oracles=oracles(),
        trace_metadata=saved['trace_metadata'],declaration_sha256=saved['declaration_sha256'],attempt=attempt)
    assert actual==saved,'Preserve differing result; do not overwrite evidence'
    print('VERIFIED',sha(folder/'report.json'),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=('oracles','qualify','check'))
    parser.add_argument('--attempt',default='attempt-001')
    args=parser.parse_args()
    if args.command=='oracles': print(json.dumps(oracles(),indent=2))
    elif args.command=='qualify': run(args.attempt)
    else: check(args.attempt)
