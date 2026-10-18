#!/usr/bin/env python3
"""Compare the native research-node solver with the frozen full-matrix Python oracle."""
import hashlib
import importlib.util
import itertools
import json
import math
from pathlib import Path
import subprocess
import sys
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
oracle_path=ROOT/'Tests/oracles/preset_dc_network.py'
spec=importlib.util.spec_from_file_location('network_oracle',oracle_path)
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
build=Path(sys.argv[1]).resolve()
rows=[]
for mode,br,bias,alpha in itertools.product((1,2),(-15.,0.,15.),(-15.,0.,15.),(.1,1.,3.)):
    name='flute' if mode==1 else 'brass1'
    p=oracle.PRESET[name]
    eg=(p['IL'],p['AL'],.2*p['AL'],.7*p['IL'])
    rows.append((mode,br,bias,-bias,alpha,0.,0.,*eg))
# Also exercise the inherited shared SUB source at both signs. The oracle
# below adds its current directly to a full five-node matrix, not to a
# precomputed frequency or independently modified own-envelope scale.
for mode,sub,depth in itertools.product((1,2),(-1.,.37,1.),(.1,1.3,5.)):
    rows.append((mode,-15.,4.,-7.,1.6,depth,sub,-.945,1.785,0.,-.31))
# All four-board IL/AL corners at bias/BR endpoints, including maximum
# declared inherited SUB depth. Alpha does not affect conduction margins.
for mode,br,bias,sub in itertools.product((1,2),(-15.,15.),(-15.,15.),(-1.,1.)):
    name='flute' if mode==1 else 'brass1';p=oracle.PRESET[name]
    for eg in itertools.product((p['IL'],p['AL']),repeat=4):
        rows.append((mode,br,bias,bias,1.,5.,sub,*eg))
text=''.join(' '.join(map(str,row))+'\n' for row in rows)
run=subprocess.run([str(build/'ceres_calibration_tests'),'--probe'],input=text,text=True,capture_output=True,check=True)
lines=run.stdout.splitlines();assert len(lines)==len(rows)
max_node=max_frequency=max_residual=0.;min_margin=math.inf;min_selected=math.inf
for row,line in zip(rows,lines):
    mode,br,bh,bl,alpha,depth,sub,*eg=row
    name='flute' if mode==1 else 'brass1'
    fields=list(map(float,line.split()));assert len(fields)==41
    sub_voltage=depth*sub/(.65*39000/47000)
    assert abs(fields[0]-sub_voltage)<1e-13
    for j,stage in enumerate(('HP','LP')):
        got=fields[1+20*j:1+20*(j+1)]
        bias=bh if stage=='HP' else bl
        expected=oracle.solve(name,stage,'B',.6,br,eg,bias)
        s=oracle.STAGE[stage];rs=oracle.PRESET[name][stage];gp=1/oracle.RP
        gs=0 if rs is None else 1/rs
        D=expected['local_D_S'];kappa=expected['kappa_octaves_per_pin_V']
        A=np.zeros((5,5));rhs=np.zeros(5)
        A[0,0]=gs+1/oracle.R0+4*gp;rhs[0]=gs*9.4
        for i in range(4):
            A[0,i+1]=A[i+1,0]=-gp;A[i+1,i+1]=D
            rhs[i+1]=br/s['Rbr']+eg[i]/s['Reg']+bias/oracle.RBIAS+sub_voltage/s['Rmod']
        full=np.linalg.solve(A,rhs)
        if depth==0:
            assert np.max(np.abs(full-np.r_[expected['bus_V'],expected['pin4_V']]))<1e-14
        native=np.r_[got[0],got[8:12]]
        node_error=float(np.max(abs(native-full)));max_node=max(max_node,node_error)
        assert node_error<2e-14
        np.testing.assert_allclose(got[12:16],alpha*kappa*full[1:],rtol=2e-14,atol=2e-14)
        expected_freq=oracle.FLOOR+s['gm_anchor_Hz']*2**(alpha*kappa*full[1:])
        error=float(np.max(abs(np.array(got[16:20])-expected_freq)));max_frequency=max(max_frequency,error)
        np.testing.assert_allclose(got[16:20],expected_freq,rtol=3e-14,atol=2e-12)
        selected=0 if rs is None else (9.4-full[0])/rs
        assert abs(got[3]-selected)<1e-16 and abs(got[4]-(full[0]+.6))<1e-14
        valid=(full[0]+.6>=0 and (rs is None or selected>=0))
        assert bool(got[6])==(rs is not None) and bool(got[7])==valid
        residual=float(np.max(abs(A@native-rhs))/max(np.max(abs(A))*max(np.max(abs(native)),1.),np.max(abs(rhs)),1e-20))
        assert residual<1e-13 and got[5]<1e-13
        max_residual=max(max_residual,residual);min_margin=min(min_margin,full[0]+.6)
        if rs is not None:min_selected=min(min_selected,selected)
print(json.dumps({'status':'pass','cases':len(rows),'stage_solves':2*len(rows),
    'maximum_node_error_V':max_node,'maximum_frequency_error_Hz':max_frequency,
    'maximum_independent_normalized_KCL_residual':max_residual,
    'minimum_manual_off_margin_V':min_margin,'minimum_selected_current_A':min_selected,
    'oracle_sha256':hashlib.sha256(oracle_path.read_bytes()).hexdigest()},indent=2))
