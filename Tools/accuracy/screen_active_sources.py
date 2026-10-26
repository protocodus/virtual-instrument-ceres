#!/usr/bin/env python3
"""Bounded source-only screen; no synth renders, parameter fitting, or gate inference."""
from pathlib import Path
import argparse,json,hashlib,subprocess
import numpy as np
from scipy.io import wavfile
from scipy.signal import spectrogram
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Docs/research/accuracy';ASSETS=BASE/'active-source'
SOURCE=Path('/tmp/ceres-cs50-1x8LGd/references/whitmore-original.wav')
META=BASE/'inputs/references/whitmore-original.info.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,x):Path(p).write_text(json.dumps(x,indent=2,allow_nan=False)+'\n')
def declare():
 assert not (BASE/'active-source-plan.json').exists()
 save(BASE/'active-source-plan.json',dict(source_url='https://www.youtube.com/watch?v=Fj21t8MIS_o',creator='Andy Whitmore',
  source_audio=str(SOURCE),source_audio_sha256=sha(SOURCE),creator_metadata_sha256=sha(META),script_sha256=sha(__file__),
  windows=[dict(id='sub',seconds=[740,780],chapter='Sculpting Sub-Oscillator Modulation'),dict(id='resonant-noise',seconds=[900,928],chapter='Wind - Open Close Filter with 4 Oscillators - full Resonance'),dict(id='no-resonance',seconds=[950,970],chapter='Filter open - no Resonance')],
  measurement='Source-only mono mean,4096-sample Hann spectral windows,1024-sample hop; RMS, centroid80..8000Hz and spectral-flatness descriptors only. No model scores or extracted MIDI.',
  eligibility='Not eligible for a known-settings hardware accuracy test: absolute manual/global controls, active physical notes and gate/pressure remain unresolved. Video is screened for actual route evidence.',
  prior_exposure='Chapter names and ten frames900..970 visually screened before declaration; no spectral fitting or acoustic parameter selection.',
  source_use='Analysis-only. Creator claims dry; unknown recording chain. Copyright remains with creator; no redistribution license inferred.'))
 print('declared',sha(BASE/'active-source-plan.json'))
def run():
 p=json.loads((BASE/'active-source-plan.json').read_text());assert p['script_sha256']==sha(__file__);assert p['source_audio_sha256']==sha(SOURCE)
 rate,original=wavfile.read(SOURCE,mmap=True);assert rate==48000 and original.dtype==np.float32
 rows=[];fig,axes=plt.subplots(3,1,figsize=(11,9),constrained_layout=True)
 for ax,w in zip(axes,p['windows']):
  a,b=w['seconds'];stereo=np.asarray(original[round(a*rate):round(b*rate)],float);mono=stereo.mean(axis=1)
  excerpt=ASSETS/(w['id']+'-audio.wav');wavfile.write(excerpt,rate,stereo.astype(np.float32))
  f,t,power=spectrogram(mono,fs=rate,nperseg=4096,noverlap=3072,window='hann',scaling='density');mask=(f>=80)&(f<=8000);db=10*np.log10(np.maximum(power[mask],1e-14))
  centroid=(power[mask]*f[mask,None]).sum(axis=0)/np.maximum(power[mask].sum(axis=0),1e-30)
  flatness=np.exp(np.mean(np.log(np.maximum(power[mask],1e-30)),axis=0))/np.maximum(np.mean(power[mask],axis=0),1e-30)
  ax.pcolormesh(t+a,f[mask],db,shading='auto',vmin=-100,vmax=-35,cmap='magma');ax.set_yscale('log');ax.set_ylim(80,8000);ax.set_title(w['chapter']);ax.set_ylabel('Hz');ax.set_xlabel('Original source seconds')
  rows.append(dict(**w,excerpt=str(excerpt.relative_to(ROOT)),excerpt_sha256=sha(excerpt),rms=float(np.sqrt(np.mean(mono**2))),
   centroid_Hz_p10_p50_p90=np.percentile(centroid,[10,50,90]).tolist(),spectral_flatness_p10_p50_p90=np.percentile(flatness,[10,50,90]).tolist(),
   role='Descriptive overview; speech, live control movement, polyphony and processing not separated.'))
 fig.suptitle('Public hardware source overview — not calibrated input/settings',fontsize=13)
 plot=BASE/'active-source-overview.png';fig.savefig(plot,dpi=140);plt.close(fig)
 bindings={str(plot.relative_to(ROOT)):sha(plot)}
 for r in rows:bindings[r['excerpt']]=r['excerpt_sha256']
 for name,times,start in [('whitmore-resonance-900-970',[901,908,915,922,929,936,943,950,957,964],900),('whitmore-sub-740-780',[741,748,755,762,769,777],740)]:
  video=ASSETS/(name+'.webm');assert video.exists();bindings[str(video.relative_to(ROOT))]=sha(video);tiles=[]
  for t in times:
   target=ASSETS/f'frame-{t}.png';subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-ss',str(t-start),'-i',str(video),'-frames:v','1','-y',str(target)],check=True)
   bindings[str(target.relative_to(ROOT))]=sha(target);im=Image.open(target).convert('RGB');im.thumbnail((640,340));tile=Image.new('RGB',(640,365),'white');tile.paste(im,(0,25));ImageDraw.Draw(tile).text((8,5),f'Requested source {t}s; 25fps video section offset {start}s',fill='black');tiles.append(tile)
  sheet=Image.new('RGB',(1280,365*((len(tiles)+1)//2)),'white')
  for i,tile in enumerate(tiles):sheet.paste(tile,((i%2)*640,(i//2)*365))
  target=ASSETS/(name+'-contact.png');sheet.save(target);bindings[str(target.relative_to(ROOT))]=sha(target)
 save(BASE/'active-source-screen.json',dict(plan_sha256=sha(BASE/'active-source-plan.json'),script_sha256=sha(__file__),source_sha256=p['source_audio_sha256'],measurements=rows,artifact_sha256=bindings,
  native_calls=0,parameter_fits=0,strict_eligible=False,original_MIDI_available=False,
  timing='Video requested timestamps have25fps sampling and possible section/keyframe/AV alignment uncertainty; not sample-exact contact timing.',
  verdict='Manual VCF sweep/resonance movement is visible in900..970s; this is not a confirmed isolated global RES or BR sweep. SUB route findings require reading the accompanying visual note. No new accuracy target selected.'))
 print('screened',sha(BASE/'active-source-screen.json'))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['declare','run']);a=p.parse_args();declare() if a.action=='declare' else run()
