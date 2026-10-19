#!/usr/bin/env python3
"""Bounded periodic nonlinear-stage alias comparison; no hardware audio or fit."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--build-dir',type=Path,default=ROOT/'build');ap.add_argument('--report',type=Path,required=True);args=ap.parse_args()
    build=args.build_dir.resolve();identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    signals={};records=[]
    with tempfile.TemporaryDirectory(prefix='ceres-ring-switch-alias-') as temp:
        binary=Path(temp)/'probe'
        command=[os.environ.get('CXX','c++'),'-O3','-std=c++17','-I',str(ROOT/'Source/DSP'),str(ROOT/'Tests/RingSwitchingAlias.cpp'),str(build/'libceres.a'),'-o',str(binary)]
        subprocess.run(command,check=True);binary_hash=sha(binary)
        for fs in [192000,3072000]:
            raw=subprocess.check_output([str(binary),str(fs)])
            data=np.frombuffer(raw,dtype=np.float64).reshape(-1,2)
            assert len(data)==fs//20 and np.all(np.isfinite(data))
            error=float(np.max(np.abs(data[:,0]-data[:,1])))
            assert error<5e-16,(fs,error)
            signals[fs]=data
            records.append({'processing_rate_hz':fs,'samples':len(data),'raw_probe_sha256':hashlib.sha256(raw).hexdigest(),'max_native_vs_independent_exp_current_error':error})
    comparisons=[]
    for signal_hz in [0,440,15000]:
        vectors={}
        for fs,data in signals.items():
            # Reference uses independent exponential BJT currents, while the
            #192k result is the production kernel. Both share source volts.
            waveform=data[:,0 if fs==192000 else 1].copy()
            if signal_hz:waveform*=np.sin(2*np.pi*signal_hz*np.arange(len(waveform))/fs)
            vectors[fs]=np.fft.rfft(waveform)/len(waveform)
        count=1201 #20Hz bins; include24k boundary explicitly.
        actual=vectors[192000][:count];reference=vectors[3072000][:count]
        weight=np.full(count,2.);weight[0]=1
        rms=lambda v:float(np.sqrt(np.sum(weight*np.abs(v)**2)))
        difference=rms(actual-reference);reference_rms=rms(reference)
        high_reference=float(np.sqrt(2*np.sum(np.abs(vectors[3072000][count:])**2)))
        assert difference<2e-11,(signal_hz,difference)
        comparisons.append({'audio_input_sine_hz':signal_hz,'band_hz':[0,24000],'sampled_vs_reference_band_error_rms':difference,
            'reference_band_rms':reference_rms,'error_relative_db':float(20*np.log10(max(difference,1e-300)/reference_rms)),
            'reference_above24k_rms_before_decimator':high_reference})
    files=[Path(__file__),ROOT/'Tests/RingSwitchingAlias.cpp',ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp',build/'libceres.a']
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'carrier_hz':220,'period_seconds':.05,
        'nominal_source_vpp':3,'temperature_kelvin':298.15,'dc_differential_volts':-.023489437180096218,
        'reference_oversampling_over192k':16,'compiled_source_sha256':identity,'compile_command':command,'temporary_probe_binary_sha256':binary_hash,
        'probe_runs':records,'comparisons':comparisons,'absolute_band_error_limit':2e-11,
        'scope':'Actual production steering kernel at192k versus independent exponential-current evaluation at3.072MHz for a common periodic steady analog carrier input; carrier-only and linear440/15000Hz signal products. This bounds periodic sampling error of this new stage for these inputs. Not a universal dynamic synth alias bound; no FIR/output/VCA nonlinearities or hardware recording involved. Above24k reference energy is reported, not claimed absent; existing decimator handles it. No mean/peak normalization. Temporary probe binaries/streams removed.',
        'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in files}}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',len(comparisons),'bounded periodic alias comparisons; worst RMS',max(x['sampled_vs_reference_band_error_rms'] for x in comparisons))
if __name__=='__main__':main()
