#!/usr/bin/env python3
"""Frozen public CS-50 battery. Analysis-only references; no parameter or gain fitting."""
from pathlib import Path
import argparse, hashlib, json, math, subprocess, shutil, sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly
import metrics as m
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Docs/research/accuracy'
OLD=Path('/tmp/ceres-cs50-1x8LGd')
COMMIT='10e17fd9ecdc0a5a0354bad4c5a53780885a5503'
IDENTITY=['Source/DSP/ceres.hpp','Source/DSP/ceres.cpp','Tools/render_main.cpp','Tools/build_identity.hpp.in','CMakeLists.txt']

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rel(p): return str(Path(p).resolve().relative_to(ROOT))
def read(p): return json.loads(Path(p).read_text())
def save(p,x):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(x,indent=2,allow_nan=False)+'\n')
def audio(p):
 rate,x=wavfile.read(p)
 if x.dtype.kind=='i': x=x.astype(float)/2**(8*x.dtype.itemsize-1)
 else: x=x.astype(float)
 if x.ndim==2:x=x.mean(axis=1)
 assert np.isfinite(x).all()
 return rate,x

def verify_plan():
 p=read(BASE/'plan.json')
 for path,h in p['input_sha256'].items(): assert sha(ROOT/path)==h,path
 return p

def midi_check(mid,events):
 decoded,warnings=m.read_midi(mid)
 expected=[(math.floor(t*48000+.5),k,n,v) for t,k,n,v in decoded]
 actual=[]
 for line in Path(events).read_text().splitlines():
  if line.strip() and not line.startswith('#'):
   f,k,n,v=line.split();actual.append((int(f),k,int(n),float(v)))
 assert len(actual)==len(expected)
 for a,b in zip(actual,expected):
  assert a[:3]==b[:3] and abs(a[3]-b[3])<1e-10,(a,b)
 return {'event_count':len(actual),'sample_exact_schedule':True,'warnings':warnings,'original_hardware_MIDI':False}

def prepare():
 assert not (BASE/'plan.json').exists(),'Refuse to overwrite declaration'
 bindings={};origins={};cases=[]
 def copy(name,dest=None):
  src=OLD/name;dst=BASE/'inputs'/(dest or name)
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst)
  assert sha(src)==sha(dst);bindings[rel(dst)]=sha(dst);origins[rel(dst)]={'path':str(src),'sha256':sha(src)}
  return rel(dst)
 for name in ['research/brass1-release-spectra-160.json','research/brass1-release-spectra-160-plan.json',
  'references/whitmore-acquisition.json','research/whitmore-transpose-keyboard.json',
  'references/whitmore-original.info.json']:
  copy(name)
 transpose=read(OLD/'calibration/whitmore-transpose.json')
 for row in transpose['comparisons']:
  if row['kind'] not in ['factory-physical','effective-compensated']:continue
  key=row['case'];meta=row['metadata'];name='flute-'+key+'-'+row['kind']
  ref=f'calibration/whitmore-flute-{key}-source.wav'
  cases.append(dict(id=name,family='flute',preset='flute',frames=meta['statistics']['frames'],
   rate=48000,seed=50,offset_seconds=row['native_age'],duration=1.9 if key=='high' else 1.2,
   expected_f0=670. if key=='high' else 530.,recording_gain=row['gain'],
   reference=copy(ref),historical_audio=copy(meta['audio']),
   patch=copy(meta['patch']),events=copy(meta['events']),midi=copy(meta['midi']),
   source_seconds=[205.75,207.65] if key=='high' else [210.80,212.00],
   source_url='https://www.youtube.com/watch?v=Fj21t8MIS_o',creator='Andy Whitmore',
   settings_status='Named factory Flute and physical note/transpose visually supported; unknown globals, pressure, exact gates and voice count. Effective family is a prior high-window fit, not hardware settings.',
   midi_status='Source-pitch following synthetic fresh held gate; physical notes 88/84, octave -1. Bends also affect filter key tracking.',strict_eligible=False))
 grid=read(OLD/'calibration/brass-filter-grid.json')
 for history in [20,10]:
  row=next(x for x in grid['cases'] if x['id']==f'case{history}-LPx1-Qfactory')
  cases.append(dict(id=f'brass-history{history}',family='brass',preset='brass1',frames=96800,rate=48000,seed=50,
   offset_seconds=10400/48000,duration=1.6,recording_gain=grid['recording_gain_held'],
   reference=copy('references/whitmore-brass1-context.wav'),historical_audio=copy(row['audio']),
   patch=copy(row['patch']),events=copy(row['events']),midi=copy(row['midi']),
   source_seconds=[136.4,138.0],source_url='https://www.youtube.com/watch?v=Fj21t8MIS_o',creator='Andy Whitmore',
   settings_status='Named Brass I; BR -15 boundary, common AM and global release are conditional effective controls; actual control voltages and modulation route unknown.',
   midi_status=f'Reused fixed chord gate hypothesis {history}, not original MIDI; physical chord+octave correction retained.',strict_eligible=False))
 regression=read(OLD/'reports/v09-regression.json')
 for kind in ['sawtooth','square']:
  row=next(x for x in regression['cases'] if x['case']['id']=='dry-'+kind);c=row['case'];meta=row['metadata']
  cases.append(dict(id='dry-'+kind,family='dry',preset='init',frames=c['frames'],rate=48000,seed=50,
   offset_seconds=0.,duration=c['frames']/48000,expected_f0=c['expected_f0'],recording_gain=c['locked_gain'],
   reference=copy('references/ptk_cs50_'+kind+'.wav'),historical_audio=copy('renders/v10/dry-'+kind+'.wav'),
   patch=copy(meta['patch']),events=copy(meta['events']),midi=copy(meta['midi']),
   source_seconds=[0.,c['frames']/48000],source_url='https://www.polynominal.com/yamaha-cs50/',creator='Polynominal/PTK archive recording',
   settings_status='Named waveform, dry creator context; all effective oscillator/filter/envelope values previously fitted. No known factory preset or complete hardware settings.',
   midi_status='Inferred monophonic pitch/onset and estimated gate, not original MIDI.',strict_eligible=False))
 for name in ['calibration/whitmore-transpose.json','calibration/brass-filter-grid.json','reports/v09-regression.json','reports/v10-global-release-regression.json']:
  origins[name]={'path':str(OLD/name),'sha256':sha(OLD/name)}
 for c in cases:c['midi_validation']=midi_check(ROOT/c['midi'],ROOT/c['events'])
 snapshot=BASE/'baseline-source'
 for name in IDENTITY:
  p=snapshot/name;p.parent.mkdir(parents=True,exist_ok=True)
  p.write_bytes(subprocess.check_output(['git','show',COMMIT+':'+name],cwd=ROOT))
  bindings[rel(p)]=sha(p)
 for name in ['Tools/accuracy/run_battery.py','Tools/accuracy/metrics.py','Tools/accuracy/origins.json']:bindings[name]=sha(ROOT/name)
 save(BASE/'plan.json',dict(status='declared_before_audio',baseline_commit=COMMIT,input_sha256=bindings,origins=origins,cases=cases,
  native_case_count=8,parameter_fits=0,gain_fits=0,original_MIDI_available=False,strict_eligible=False,
  metrics={'flute':'128ms Hann/40ms hop, pitch-following H2..7 relative H1, full trajectory and median; fixed-gain RMS level and slope separate.',
   'brass':'Frozen source-defined 160ms masks, 243 valid partial records; six equally weighted note/phase cells, weak counts separately. Fixed common-frame C4 higher ratios and 40dB floored H1 tail error.',
   'dry':'128ms Hann,32 harmonics on fixed early .4-.6/train .8-1.0/late1.2-1.4s windows; all H2..32 and odd subset separately; fixed-gain RMS shape.'},
  limits=['All recordings and windows are reused development evidence, no untouched known-settings holdout.',
   'Audio references are offline analysis/listening only; synthesis uses no sample assets.',
   'No controls, gain, gate, source eligibility, or masking may be fitted by this battery.',
   'Neutral global controls and filter_lfo=0 do not verify newly added control paths.',
   'Public creator audio copyright remains with the creator; local analysis excerpts are not a redistribution license.']))
 print('Declared',rel(BASE/'plan.json'),sha(BASE/'plan.json'))

def build():
 p=verify_plan();commands=[['cmake','-S',str(BASE/'baseline-source'),'-B',str(BASE/'baseline-build'),'-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(BASE/'baseline-build'),'--target','ceres_render','-j','2']]
 logs=[]
 for cmd in commands:
  r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);logs.append('$ '+' '.join(cmd)+'\n'+r.stdout);assert r.returncode==0,r.stdout
 (BASE/'baseline-build.log').write_text('\n'.join(logs))
 binary=BASE/'baseline-build/ceres_render';identity=read_identity(binary,BASE/'baseline-source')
 save(BASE/'baseline-build.json',dict(commit=COMMIT,plan_sha256=sha(BASE/'plan.json'),commands=commands,binary=rel(binary),binary_sha256=sha(binary),compiled_source_sha256=identity,log_sha256=sha(BASE/'baseline-build.log')))
 print('Built baseline',sha(binary))

def read_identity(binary,source):
 identity=json.loads(subprocess.check_output([str(binary),'--build-info'],text=True))
 assert set(identity)==set(IDENTITY),identity
 for path,h in identity.items():assert sha(source/path)==h,path
 return identity

def frames_summary(x,rate,times,expected,count):
 return [m.measure_frame(x,rate,float(t),expected,count,.128) for t in times]

def tonal_metrics(source,native,gain,harmonics):
 s=np.asarray([r['relative_db'] for r in source])[:,harmonics];n=np.asarray([r['relative_db'] for r in native])[:,harmonics]
 error=n-s;sr=np.array([r['rms'] for r in source]);nr=np.array([r['rms'] for r in native])*gain
 db=20*np.log10(np.maximum(nr,1e-15)/np.maximum(sr,1e-15));t=np.array([r['time'] for r in source]);sf=np.array([r['f0'] for r in source]);nf=np.array([r['f0'] for r in native])
 return dict(frame_count=len(source),harmonics=(np.array(harmonics)+1).tolist(),median_harmonic_MAE_db=float(np.mean(np.abs(np.median(n,axis=0)-np.median(s,axis=0)))),
  trajectory_ratio_MAE_db=float(np.mean(np.abs(error))),trajectory_ratio_MSE_db2=float(np.mean(error**2)),
  held_gain_RMS_error_db=float(np.sqrt(np.mean(db**2))),source_RMS_slope_db_per_s=float(np.polyfit(t,20*np.log10(np.maximum(sr,1e-15)),1)[0]),
  native_RMS_slope_db_per_s=float(np.polyfit(t,20*np.log10(np.maximum(nr,1e-15)),1)[0]),
  source_median_f0=float(np.median(sf)),native_median_f0=float(np.median(nf)),median_pitch_error_cents=float(np.median(1200*np.log2(nf/sf))))

def measure(c,path):
 sr,s=audio(ROOT/c['reference']);nr,n=audio(path);assert nr==48000 and len(n)==c['frames']
 offset=round(c['offset_seconds']*48000);gain=c['recording_gain'];d={}
 if c['family']=='brass':
  assert sr==48000;aligned=n[offset:offset+len(s)]
  source=read(BASE/'inputs/research/brass1-release-spectra-160.json');windows=read(BASE/'inputs/research/brass1-release-spectra-160-plan.json')['descriptive_windows_seconds']
  rows=m.spectral_rows(aligned,[r['time'] for r in source['source']['rows']],.160);comp=m.comparison(rows,source['source'],gain)
  d={'spectral_score':m.six_cells(comp),'tail_metrics':m.tails(m.envelopes(aligned),m.envelopes(s),gain),'C4_higher_ratios':m.higher_ratios(rows,source,windows,gain),'source_valid_partial_records':sum(p['source_valid_frames'] for p in comp['per_partial'])}
 elif c['family']=='flute':
  times=np.arange(.064,c['duration']-.064+1e-8,.04)
  ss=frames_summary(s,sr,times,c['expected_f0'],16);nn=frames_summary(n,nr,times+c['offset_seconds'],c['expected_f0'],16)
  d=tonal_metrics(ss,nn,gain,list(range(1,7)))
 else:
  d={'windows':[]}
  for label,a,b in [('early',.4,.6),('prior_train',.8,1.),('late',1.2,1.4)]:
   times=np.arange(a,b+1e-8,.02);ss=frames_summary(s,sr,times,c['expected_f0'],32);nn=frames_summary(n,nr,times,c['expected_f0'],32)
   d['windows'].append({'name':label,'all_H2_H32':tonal_metrics(ss,nn,gain,list(range(1,32))),
    'odd_H3_H31':tonal_metrics(ss,nn,gain,list(range(2,32,2)))})
 return d,dict(finite=bool(np.isfinite(n).all()),peak=float(np.max(np.abs(n))),rms=float(np.sqrt(np.mean(n*n))),energy=float(np.sum(n*n)),samples_above_unity=int(np.sum(np.abs(n)>1)))

def run(label,binary,source):
 p=verify_plan();binary=binary.resolve();source=source.resolve();identity=read_identity(binary,source);bh=sha(binary)
 out=BASE/'runs'/label;assert not (out/'report.json').exists(),'Refuse to overwrite completed run';out.mkdir(parents=True,exist_ok=True)
 rows=[]
 for c in p['cases']:
  midi_check(ROOT/c['midi'],ROOT/c['events']);wav=out/(c['id']+'.wav')
  cmd=[str(binary),'--events',str(ROOT/c['events']),'--output',str(wav),'--preset',c['preset'],'--patch',str(ROOT/c['patch']),'--rate','48000','--frames',str(c['frames']),'--seed','50']
  process=subprocess.run(cmd,text=True,capture_output=True);assert process.returncode==0,process.stderr
  stats=json.loads(process.stdout);metrics,signal=measure(c,wav);hr,hx=audio(ROOT/c['historical_audio']);rate,nx=audio(wav)
  historical=dict(samples_identical=bool(np.array_equal(hx,nx)),byte_identical=sha(wav)==sha(ROOT/c['historical_audio']),max_sample_difference=float(np.max(np.abs(hx-nx))))
  rows.append(dict(id=c['id'],audio=rel(wav),audio_sha256=sha(wav),command=cmd,statistics=stats,metrics=metrics,signal=signal,historical_comparison=historical))
  print(label,c['id'],'finite',signal['finite'],'historical exact',historical['samples_identical'],flush=True)
 assert sha(binary)==bh;read_identity(binary,source);verify_plan()
 save(out/'report.json',dict(status='pass',label=label,plan_sha256=sha(BASE/'plan.json'),binary=str(binary),binary_sha256=bh,source_root=str(source),compiled_source_sha256=identity,cases=rows,render_count=len(rows),parameter_fits=0,gain_fits=0,strict_eligible=False))
 print('Report',rel(out/'report.json'))

def compare(candidate):
 p=verify_plan();base=read(BASE/'runs/baseline/report.json');cand=read(BASE/'runs'/candidate/'report.json');assert base['plan_sha256']==cand['plan_sha256']==sha(BASE/'plan.json')
 segments=[];chunks=[];pairs=[]
 for c,b,a in zip(p['cases'],base['cases'],cand['cases']):
  assert c['id']==a['id']==b['id'];rate,s=audio(ROOT/c['reference'])
  if rate!=48000:s=resample_poly(s,48000//math.gcd(rate,48000),rate//math.gcd(rate,48000))
  count=round(c['duration']*48000);offset=round(c['offset_seconds']*48000);s=s[:count]
  _,bv=audio(ROOT/b['audio']);_,av=audio(ROOT/a['audio']);bv=bv[offset:offset+len(s)]*c['recording_gain'];av=av[offset:offset+len(s)]*c['recording_gain']
  assert len(s)==len(bv)==len(av)
  for name,x in [('hardware',s),('baseline',bv),('candidate',av)]:
   start=sum(len(y) for y in chunks);chunks.append(x);segments.append(dict(case=c['id'],kind=name,start_frame=start,frames=len(x)));chunks.append(np.zeros(12000))
  pairs.append(dict(id=c['id'],baseline_metrics=b['metrics'],candidate_metrics=a['metrics'],samples_identical=np.array_equal(bv,av),max_aligned_sample_difference=float(np.max(np.abs(bv-av)))))
 combined=np.concatenate(chunks);playback_gain=min(1.,.95/max(1e-15,np.max(np.abs(combined))));path=BASE/f'listening-{candidate}.wav';wavfile.write(path,48000,(combined*playback_gain).astype(np.float32))
 save(BASE/f'comparison-{candidate}.json',dict(status='pass',plan_sha256=sha(BASE/'plan.json'),baseline_report_sha256=sha(BASE/'runs/baseline/report.json'),candidate_report_sha256=sha(BASE/'runs'/candidate/'report.json'),comparisons=pairs,
  listening=dict(audio=rel(path),sha256=sha(path),sample_rate=48000,frames=len(combined),duration_seconds=len(combined)/48000,common_playback_gain=float(playback_gain),segments=segments,individual_normalization=False),strict_eligible=False,parameter_fits=0,gain_fits=0))
 print('Comparison',rel(BASE/f'comparison-{candidate}.json'))

if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='command',required=True)
 sub.add_parser('prepare');sub.add_parser('build-baseline')
 r=sub.add_parser('run');r.add_argument('label');r.add_argument('--binary',type=Path,required=True);r.add_argument('--source-root',type=Path,required=True)
 c=sub.add_parser('compare');c.add_argument('candidate')
 args=ap.parse_args()
 if args.command=='prepare':prepare()
 elif args.command=='build-baseline':build()
 elif args.command=='run':run(args.label,args.binary,args.source_root)
 else:compare(args.candidate)
