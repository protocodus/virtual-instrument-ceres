#!/usr/bin/env python3
"""Source-only screen, frozen grid; no DSP fit or independently scaled variants."""
import argparse,hashlib,json,math,subprocess
from pathlib import Path
import numpy as np
from scipy import optimize,signal
RATE=48000
SEGMENTS={'saw':(50.3,61.7),'square':(63.3,82.7),'both':(143.3,149.7)}

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--audio',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','a:0','-show_entries','stream=codec_name,sample_rate,channels','-of','json',str(args.audio)],text=True))
 stream=probe['streams'][0]
 if int(stream['sample_rate'])!=RATE or stream['channels'] not in [1,2]:
  raise RuntimeError('Source contract requires original 48kHz mono/stereo audio; resampling is not implicit.')
 if (args.output/'source-report.json').exists() or (args.output/'source-declaration.json').exists():
  raise RuntimeError('Output attempt exists; choose a new directory to preserve evidence.')
 declaration={'source_audio_format':stream,'segments_seconds':SEGMENTS,'window_seconds':.2,'hop_seconds':.2,
 'eligibility':'Source-only conditional screen. Stationarity <=0.75dB first/second half rawRMS;16-harmonic explained energy>=98%; rawRMS>=.03. These reject speech,polyphony,fastmotion but do not prove controls/drycapture.',
 'pitch_estimation':'Hann FFT dominant peak90..600Hz, boundedfundamental frequency±4Hz maximizingHann fundamental. Native pitch/gain/phase not fitted.16harmonic+DC jointLS then evaluated in absolute rawgain.',
 'recording_gain':'No gain changes or normalization in source measurements; source creator did not explicitly prove capturegain held.',
 'source_sha256':digest(args.audio),'script_sha256':digest(__file__)}
 args.output.mkdir(parents=True,exist_ok=True);(args.output/'source-declaration.json').write_text(json.dumps(declaration,indent=2)+'\n')
 rows=[]
 for label,(start,end)in SEGMENTS.items():
  data=np.frombuffer(subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-ss',str(start),'-i',str(args.audio),'-t',str(end-start+.2),'-ac','1','-f','f32le','-']),dtype='<f4').astype(float)
  for offset in np.arange(0,end-start+1e-5,.2):
   x=data[round(offset*RATE):round((offset+.2)*RATE)]
   if len(x)!=9600:continue
   rms=float(np.sqrt(np.mean(x*x)));change=float(20*np.log10((np.sqrt(np.mean(x[4800:]**2))+1e-15)/(np.sqrt(np.mean(x[:4800]**2))+1e-15)))
   f,p=signal.periodogram(x,RATE,window='hann',nfft=131072);sel=(f>=90)&(f<=600);peak=float(f[sel][np.argmax(p[sel])]);t=np.arange(len(x))/RATE;w=np.hanning(len(x));xw=x*w
   def loss(hz):return -abs(np.dot(xw,np.exp(-2j*np.pi*hz*t)))**2
   f0=float(optimize.minimize_scalar(loss,bounds=(peak-4,peak+4),method='bounded',options={'xatol':1e-8}).x)
   h=np.arange(1,17);a=2*np.pi*f0*t[:,None]*h[None,:];d=np.column_stack((np.ones(len(x)),np.cos(a),np.sin(a)));c,*_=np.linalg.lstsq(d,x,rcond=None);z=c[1:17]-1j*c[17:33];res=float(np.mean((x-d@c)**2));coverage=1-res/(np.mean(x*x)+1e-30);phase=np.angle(np.exp(1j*(np.angle(z)-h*np.angle(z[0]))));amp=abs(z)
   reasons=[]
   if rms<.03:reasons.append('low-level')
   if abs(change)>.75:reasons.append('nonstationary-level')
   if coverage<.98:reasons.append('not-isolated-stationary-tone')
   row={'label':label,'start_seconds':round(start+offset,3),'rms':rms,'half_rms_change_db':change,'f0_hz':f0,'midi_float':69+12*math.log2(f0/440),'explained_energy':float(coverage),'eligible_source_shape':not reasons,'exclusion_reasons':reasons,'harmonic_amplitude':amp.tolist(),'harmonic_db_relative_h1':(20*np.log10(np.maximum(amp,1e-20)/max(amp[0],1e-20))).tolist(),'relative_phase_degrees':np.degrees(phase).tolist()}
   rows.append(row)
 summary=[]
 for label in SEGMENTS:
  for note in sorted({round(r['midi_float']) for r in rows if r['label']==label and r['eligible_source_shape']}):
   rs=[r for r in rows if r['label']==label and r['eligible_source_shape']and round(r['midi_float'])==note]
   summary.append({'label':label,'note':note,'windows':len(rs),'window_starts':[r['start_seconds']for r in rs],'rms_median':float(np.median([r['rms']for r in rs])),'fundamental_median':float(np.median([r['harmonic_amplitude'][0]for r in rs])),'h2_db_median':float(np.median([r['harmonic_db_relative_h1'][1]for r in rs])),'h3_db_median':float(np.median([r['harmonic_db_relative_h1'][2]for r in rs])),'h2_relative_phase_circular_mean_degrees':float(np.degrees(np.angle(np.mean(np.exp(1j*np.radians([r['relative_phase_degrees'][1]for r in rs]))))))})
 report=dict(declaration,rows=rows,summary=summary,hardware_mixing_accuracy='Conditional common-path shape evidence only; not a matched-original-MIDI or complete-control calibration.')
 (args.output/'source-report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
