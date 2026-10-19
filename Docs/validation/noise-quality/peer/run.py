#!/usr/bin/env python3
import hashlib,json,platform,subprocess,sys
from pathlib import Path
import numpy as np
import scipy
from scipy.signal import welch
ROOT=Path(__file__).resolve().parent
CORE=Path('/Users/vojta/Dev/virtual-instrument-ceres-poly-quality-20261019')
BASELINE='652e3722eb40f67001f0467103f9f31a957ec363'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def digest(data):return hashlib.sha256(data).hexdigest()
def main():
 assert not (ROOT/'plan.json').exists(),'Never overwrite an attempt'
 inputs={};sources={}
 for label in ('baseline','candidate'):
  folder=ROOT/(label+'-source');folder.mkdir(exist_ok=False)
  for name in ('ceres.cpp','ceres.hpp'):
   origin=CORE/'Source/DSP'/name
   raw=subprocess.check_output(['git','-C',str(CORE),'show',BASELINE+':Source/DSP/'+name]) if label=='baseline' else origin.read_bytes()
   (folder/name).write_bytes(raw);inputs[str(folder/name)]=digest(raw)
   if label=='candidate':sources[str(origin)]=digest(raw)
 profiles=['saw_noise0','sub_noise0','drift_noise0','mixed_noise','noise_only'];bands=[(100,1000),(1000,3000)]
 for p in (Path(__file__),ROOT/'probe.cpp'):inputs[str(p)]=sha(p)
 declaration={'scope':'Independent same-host audio-noise density normalization and exact no-audio-noise/SUB/drift sequence parity; no hardware gain fit or preset calibration','baseline_commit':BASELINE,'sample_rate':48000,'seed':50,'qualities':[1,2,4],'profiles':profiles,'frames':{'noise_only':245760,'other':48000},'noise_psd':{'discard_frames':48000,'method':'Welch density, Hann8192, overlap4096, constant detrend, arithmetic sum of bins times df','bands_hz':bands,'candidate_relative_4x_band_tolerances_db':[.3,.5]},'expected_audio_identity':'all9 noise0 and both4x noisy scenes exact; other4 noisy outputs must change','expected_state_identity':'all15 full sample sequences of SUB output,voice0 oscillator frequency and phase exact within each quality','analytic_density':'two-sided white PSD sigma^2/Fs_internal; gain^2=factor/4 makes sigma^2/(4*Fs_host), preserving historical4x; no host-rate normalization','input_sha256':inputs,'candidate_original_source_sha256':sources,'runtime':{'python':sys.version,'numpy':np.__version__,'scipy':scipy.__version__,'platform':platform.platform()},'limits':['Quality-dependent filter warping/anti-alias bandwidth and nonlinear drive can still change full-band RMS, peaks and timbre.','No assertion of exact audio equivalence across qualities, or identical noise samples across different internal clocks.','SUB has its own rate-dependent RC variance normalization and must continue receiving the unscaled raw draw.']}
 (ROOT/'plan.json').write_text(json.dumps(declaration,indent=2)+'\n');rows=[];builds={}
 dtype=np.dtype([('audio','<f4'),('sub','<f8'),('frequency','<f8'),('phase','<f8')]);assert dtype.itemsize==28
 for label in ('baseline','candidate'):
  source=ROOT/(label+'-source');binary=ROOT/(label+'-probe');cmd=['clang++','-std=c++17','-O2','-I'+str(source),str(ROOT/'probe.cpp'),str(source/'ceres.cpp'),'-o',str(binary)]
  result=subprocess.run(cmd,capture_output=True,text=True);(ROOT/(label+'-compile.log')).write_text(result.stdout+result.stderr);result.check_returncode();builds[label]={'command':cmd,'binary_sha256':sha(binary)}
  for factor in (1,2,4):
   for profile in profiles:
    frames=245760 if profile=='noise_only' else 48000;command=[str(binary),profile,str(factor),str(frames)]
    proc=subprocess.run(command,capture_output=True);(ROOT/(label+'-'+profile+'-'+str(factor)+'x.log')).write_bytes(proc.stderr);proc.check_returncode();assert len(proc.stdout)==frames*28
    x=np.frombuffer(proc.stdout,dtype=dtype);assert np.isfinite(x['audio']).all();row={'build':label,'factor':factor,'profile':profile,'frames':frames,'command':command,'record_bytes':len(proc.stdout),'record_sha256':digest(proc.stdout),'field_sha256':{k:digest(np.ascontiguousarray(x[k]).tobytes()) for k in dtype.names},'peak':float(np.max(np.abs(x['audio'])))}
    if profile=='noise_only':
     freq,psd=welch(x['audio'][48000:].astype(np.float64),fs=48000,window='hann',nperseg=8192,noverlap=4096,detrend='constant',scaling='density');df=freq[1]-freq[0]
     row['band_powers']=[float(np.sum(psd[(freq>=lo)&(freq<hi)])*df) for lo,hi in bands];row['measured_rms']=float(np.sqrt(np.mean(x['audio'][48000:].astype(np.float64)**2)))
    rows.append(row)
 (ROOT/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
 parity=[];spectral=[]
 for factor in (1,2,4):
  for profile in profiles:
   a=next(r for r in rows if r['build']=='baseline' and r['factor']==factor and r['profile']==profile);b=next(r for r in rows if r['build']=='candidate' and r['factor']==factor and r['profile']==profile)
   expected=factor==4 or profile.endswith('noise0');audio=a['field_sha256']['audio']==b['field_sha256']['audio'];states={k:a['field_sha256'][k]==b['field_sha256'][k] for k in ('sub','frequency','phase')}
   parity.append({'factor':factor,'profile':profile,'audio_exact_expected':expected,'audio_exact':audio,'states_exact':states,'passed':audio==expected and all(states.values())})
 for label in ('baseline','candidate'):
  ref=next(r for r in rows if r['build']==label and r['factor']==4 and r['profile']=='noise_only')
  for factor in (1,2):
   row=next(r for r in rows if r['build']==label and r['factor']==factor and r['profile']=='noise_only');db=[float(10*np.log10(a/b)) for a,b in zip(row['band_powers'],ref['band_powers'])]
   spectral.append({'build':label,'factor':factor,'relative_4x_band_db':db,'passed':all(abs(d)<=t for d,t in zip(db,(.3,.5))) if label=='candidate' else None})
 for p,h in inputs.items():assert sha(Path(p))==h
 for p,h in sources.items():assert sha(Path(p))==h
 report={'status':'pass' if all(x['passed'] for x in parity) and all(x['passed'] for x in spectral if x['build']=='candidate') else 'fail','plan_sha256':sha(ROOT/'plan.json'),'builds':builds,'parity':parity,'band_comparisons':spectral,'audio_bit_identity_cases':sum(x['audio_exact'] for x in parity),'state_identity_cases':sum(all(x['states_exact'].values()) for x in parity),'raw_audio_policy':'No WAVs or raw sample files retained; complete stream and each contiguous field SHA256/byte count retained. Reproduction uses retained exact sources and commands.','measurements_sha256':sha(ROOT/'measurements.json')}
 (ROOT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
 if report['status']!='pass':raise SystemExit(1)
if __name__=='__main__':main()
