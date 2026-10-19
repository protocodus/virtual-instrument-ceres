#!/usr/bin/env python3
"""Nominal source-bound cutoff-control pole audit; no renderer or repository writes."""
import argparse,hashlib,json,math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--research-root',type=Path,default=Path('/tmp/ceres-cs50-1x8LGd/research'),
                    help='Directory containing the original cached primary PDFs and inspected crops')
RESEARCH=parser.parse_args().research_root
C=10e-9
rank={'HP':[2700.,2200.,1800.], 'LP':[3000.,2400.,2000.]}
rows=[]
for j,rs in rank.items():
 for label,r in zip('ABC',rs):
  # BrassI Rth from direct T71: HP0 nominal => no selected source, 10k return; LP7.5k||10k.
  rth=10000. if j=='HP' else 7500.*10000./17500.
  eg=220000. if j=='HP' else 47000.
  br=680000. if j=='HP' else 220000.
  mod=150000. if j=='HP' else 47000.
  other=1/r+1/355000.+1/br+3/mod
  dl=other+1/39000.+1/eg
  dc=other+1/(39000.+4*rth)+1/eg
  rows.append({'stage':j,'rank':label,'rank_ohms':r,'rank_only_tau_ceiling_s':C*r,
    'stiff_other_sources_BrassI_tau_common_s':C/dc,
    'stiff_other_sources_BrassI_tau_difference_s':C/dl})
# Independent full passive star meshes: 8 capacitor nodes, four algebraic source nodes.
# Select unequal rank shunts and connect actual R7 paths to shared FH/FL, BR and SUB nodes.
rng=np.random.default_rng(50); max_tau=0.; min_increment=float('inf'); cases=2000
for case in range(cases):
 r=np.array([rng.choice(rank['HP']) for _ in range(4)]+[rng.choice(rank['LP']) for _ in range(4)])
 g=np.zeros((12,12)); g[np.arange(8),np.arange(8)]=1/r
 for node in range(8,12): g[node,node]+=10**rng.uniform(-12,1) # resistive source to fixed voltage
 def branch(a,b,res):
  v=1/res;g[a,a]+=v;g[b,b]+=v;g[a,b]-=v;g[b,a]-=v
 for i in range(8):
  lp=i>=4
  branch(i,9 if lp else 8,39000.)
  branch(i,10,220000. if lp else 680000.)
  branch(i,11,47000. if lp else 150000.)
  # nonnegative unknown incremental chip shunt; draw across many decades
  g[i,i]+=10**rng.uniform(-12,-2)
 reduced=g[:8,:8]-g[:8,8:]@np.linalg.solve(g[8:,8:],g[8:,:8])
 min_increment=min(min_increment,float(np.linalg.eigvalsh(reduced-np.diag(1/r))[0]))
 max_tau=max(max_tau,C/float(np.linalg.eigvalsh(reduced)[0]))
 assert np.linalg.eigvalsh(reduced)[0]>=1/3000.-1e-15
assert min_increment>=-1e-15
worst_tau=30e-6
result={'scope':'Nominal component bound; no hardware calibration or audio improvement claim','components':{'pin4_C_F':C,'rank_ohms':rank},'rows':rows,
 'general_bound':{'assumptions':['Only stored states are eight 10nF pin4 capacitors','Every pin4 retains its documented grounded rank shunt','All eliminated added networks have nonnegative incremental conductance, with fixed-source perturbations zero','Nominal values; no component tolerance or active negative impedance included'],
 'proof':'G_reduced = diag(1/Rrank) + Schur_complement(G_passive) >= (1/3000)*I; C=10nF*I implies slowest natural tau <=30us',
 'random_passive_mesh_cases':cases,'largest_tau_s':max_tau,'minimum_eigenvalue_added_network_S':min_increment,
 'maximum_tau_s':worst_tau,'minimum_pole_Hz':1/(2*math.pi*worst_tau)},
 'worst_single_pole_at_100Hz':{'gain_dB':-10*math.log10(1+(2*math.pi*100*worst_tau)**2),'phase_deg':-math.degrees(math.atan(2*math.pi*100*worst_tau))},
 'factory_attack_comparison':{name:{'current_attack_seconds':10**(-.3*cv),'30us_as_fraction_of_attack':worst_tau/10**(-.3*cv)} for name,cv in [('flute',3.57),('brass1',1.39)]},
 'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Source/DSP/ceres.cpp',RESEARCH/'service1.pdf',RESEARCH/'service2.pdf',RESEARCH/'q-m-rank-table.png',RESEARCH/'cutoff-m-eg-continuity.png',RESEARCH/'lp-bus-coupling-source.md',Path(__file__)]}}
print(json.dumps(result,indent=2,sort_keys=True))
