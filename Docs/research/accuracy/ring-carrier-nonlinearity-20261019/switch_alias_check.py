#!/usr/bin/env python3
"""Constant-only steady matched-BJT sampling check. No DSP import or audio fit."""
import hashlib
import importlib.util
import json
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('carrier_passive',ROOT.parent/'audible-osc-ring/passive_carrier.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
DC=-.023489437180096218
TWO_VT=2*1.380649e-23*298.15/1.602176634e-19

def main():
    # .05s is coherent at220Hz (11 cycles) and440Hz (22 cycles).
    # Compare retained <24kHz Fourier coefficients before any decimation.
    # This bounds sampled-carrier nonlinear aliases in this declared steady
    # sine440Hz scene; not the complete time-varying instrument.
    rate=220;h=oracle.network(rate)
    amplitude=h['magnitude'];phase=np.deg2rad(h['phase_degrees'])
    spectra={}
    for fs in (192000,1536000):
        n=fs//20;t=np.arange(n)/fs
        differential=DC+1.5*amplitude*np.sin(2*np.pi*rate*t+phase)
        # Independent branch-current exponential ratio rather than np.tanh.
        ratio=np.exp(2*differential/TWO_VT)
        gain=(ratio-1)/(ratio+1)
        signal=np.sin(2*np.pi*440*t)*gain
        spectra[fs]=np.fft.rfft(signal)/n
    bins=np.arange(1,1200)  #20..23980Hz; excludes the boundary itself.
    delta=spectra[192000][bins]-spectra[1536000][bins]
    bound=float(np.max(np.abs(delta)))
    reference_peak=float(np.max(np.abs(spectra[1536000][bins])))
    assert bound<1e-10
    result=dict(status='pass',scope='Steady220Hz source-scaled matched carrier pairs times440Hz sine, ideal analog passive transfer;192kHz versus1.536MHz sampling, in-band Fourier coefficients20..23980Hz.',
                max_coefficient_difference=bound,relative_to_peak_db=float(20*np.log10(bound/reference_peak)),
                physical_constants=dict(dc_volts=DC,two_vt_volts=TWO_VT,carrier_source_peak_volts=1.5),
                normalization='none',limitations='Not a native DSP test, dynamic-FM bound or full-instrument alias-free claim.',
                script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                passive_oracle_sha256=hashlib.sha256((ROOT.parent/'audible-osc-ring/passive_carrier.py').read_bytes()).hexdigest())
    (ROOT/'switch-alias-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
