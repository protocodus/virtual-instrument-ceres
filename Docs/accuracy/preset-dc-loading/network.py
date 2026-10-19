#!/usr/bin/env python3
"""Conditional CS50 cutoff DC map. Prints evidence; never renders or edits DSP."""
import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[3]
RANK={'A':{'HP':2700.,'LP':3000.},'B':{'HP':2200.,'LP':2400.},'C':{'HP':1800.,'LP':2000.}}
STAGE={'HP':{'slope':.7,'gm_anchor_Hz':12.,'Rbr':680000.,'Rmod':150000.,'Reg':220000.},
       'LP':{'slope':.65,'gm_anchor_Hz':180.,'Rbr':220000.,'Rmod':47000.,'Reg':47000.}}
PRESET={'flute':{'HP':75000.,'LP':9100.,'IL':-2.62,'AL':.315},
        'brass1':{'HP':None,'LP':7500.,'IL':-.945,'AL':1.785}}
RP=39000.; R0=10000.; RBIAS=355000.; FLOOR=1/(2*math.pi*4.7e6*1.5e-9)
DROPS=(.4,.6); BR_VALUES=(0.,-15.)

def solve(preset,stage,rank,diode_drop,br,eg=(0.,0.,0.,0.),bias=0.):
    """Selected branch on (if present); inactive branches must be checked separately."""
    p=PRESET[preset]; s=STAGE[stage]; rs=p[stage]; gp=1/RP
    gs=0. if rs is None else 1/rs
    D=gp+1/RANK[rank][stage]+1/s['Rbr']+3/s['Rmod']+1/s['Reg']+1/RBIAS
    # kappa calibrated ONLY from declared actual-bus slope, never preset-dependent.
    kappa=s['slope']*D/gp
    a=np.zeros((5,5)); rhs=np.zeros(5)
    a[0,0]=gs+1/R0+4*gp; rhs[0]=gs*(10-diode_drop)
    for i in range(4):
        a[0,i+1]=a[i+1,0]=-gp; a[i+1,i+1]=D
        rhs[i+1]=br/s['Rbr']+eg[i]/s['Reg']+bias/RBIAS
    x=np.linalg.solve(a,rhs)
    denominator=gs+1/R0+4*gp-4*gp*gp/D
    reduced_bus=(gs*(10-diode_drop)+gp/D*sum(rhs[1:]))/denominator
    reduced_u=(gp*reduced_bus+rhs[1:])/D
    assert np.max(abs(x-np.r_[reduced_bus,reduced_u]))<1e-14
    normalization=max(np.max(abs(a))*max(np.max(abs(x)),1.),np.max(abs(rhs)),1e-20)
    selected_current=None if rs is None else (10-diode_drop-x[0])/rs
    # Full dE→log2(gm) matrix. DC-only audio experiment deliberately freezes it out.
    jac=np.linalg.solve(a,np.r_[np.zeros((1,4)),np.eye(4)/s['Reg']])[1:]*kappa
    common=float(sum(jac[0])); own=float(jac[0,0]); cross=float(jac[0,1])
    return {'bus_V':float(x[0]),'pin4_V':[float(v) for v in x[1:]],
            'kappa_octaves_per_pin_V':kappa,'local_D_S':D,
            'log2_gm_shift':[float(kappa*v) for v in x[1:]],
            'reference_Hz':[float(FLOOR+s['gm_anchor_Hz']*2**(kappa*v)) for v in x[1:]],
            'selected_branch_current_A':selected_current,
            'manual_off_margin_V':float(x[0]+diode_drop),
            'normalized_KCL_residual':float(np.max(abs(a@x-rhs))/normalization),
            'full_network_EG_octaves_per_V':{'own':own,'each_other':cross,'common':common,
                 'differential':s['slope']*RP/s['Reg']},
            'nominal_unloaded_ideal_divider_V':0. if rs is None else 10*R0/(rs+R0)}

def stiff(preset,stage,diode_drop,br,actual_bus_override=None):
    s=STAGE[stage]; rs=PRESET[preset][stage]
    P=(0. if rs is None else (10-diode_drop)*R0/(rs+R0)) if actual_bus_override is None else actual_bus_override
    exponent=s['slope']*P+s['slope']*RP/s['Rbr']*br
    return {'actual_bus_V':P,'log2_gm_shift':exponent,'reference_Hz':FLOOR+s['gm_anchor_Hz']*2**exponent}

def build_evidence(research):
    scenarios=[]; worst_residual=0.; minimum_margin=math.inf; corner_count=0
    for preset,rank,drop,br in itertools.product(PRESET,RANK,DROPS,BR_VALUES):
        row={'id':f'{preset}-rank{rank}-Vd{drop:g}-BR{br:g}','preset':preset,'rank':rank,
             'diode_drop_V':drop,'actual_BR_V':br,'neutral_EG_V':[0.,0.,0.,0.],'stages':{}}
        for stage in STAGE:
            neutral=solve(preset,stage,rank,drop,br)
            corners=[solve(preset,stage,rank,drop,br,eg) for eg in itertools.product((PRESET[preset]['IL'],PRESET[preset]['AL']),repeat=4)]
            margin=min(v['manual_off_margin_V'] for v in [neutral]+corners)
            selected=[v['selected_branch_current_A'] for v in [neutral]+corners if v['selected_branch_current_A'] is not None]
            assert margin>0,'Inactive panel diode would conduct; partial network invalid'
            assert not selected or min(selected)>0,'Selected diode assumed conducting but reversed'
            worst_residual=max(worst_residual,max(v['normalized_KCL_residual'] for v in [neutral]+corners))
            minimum_margin=min(minimum_margin,margin);corner_count+=len(corners)
            row['stages'][stage]=neutral|{'EG_box_bus_V_range':[min(v['bus_V'] for v in corners),max(v['bus_V'] for v in corners)],
                'minimum_manual_off_margin_across_neutral_and_EG_box_V':margin,
                'minimum_selected_current_across_neutral_and_EG_box_A':None if not selected else min(selected)}
        row['stiff_stages']={stage:stiff(preset,stage,drop,br) for stage in STAGE}
        scenarios.append(row)
    # Retain the discarded ideal-drop question; do not silently admit these rows to render grid.
    zero_cases=[]
    for preset,rank,br,stage in itertools.product(PRESET,RANK,BR_VALUES,STAGE):
        values=[solve(preset,stage,rank,0.,br,eg) for eg in [(0.,)*4]+list(itertools.product((PRESET[preset]['IL'],PRESET[preset]['AL']),repeat=4))]
        minimum=min(v['bus_V'] for v in values)
        zero_cases.append({'preset':preset,'rank':rank,'actual_BR_V':br,'stage':stage,
             'minimum_bus_V':minimum,'assumed_inactive_manual_diode_remains_off':minimum>=0.})
    # Analytic nulls recover the CURRENT core formulas under stiff rounded-table inputs.
    # These are equation checks, not new audio comparisons or hardware calibration.
    rounded={'flute':{'HP':1.18,'LP':5.24},'brass1':{'HP':0.,'LP':5.71}}
    nulls=[]
    for preset,stage,rank,br in itertools.product(PRESET,STAGE,RANK,BR_VALUES):
        s=STAGE[stage]; P=rounded[preset][stage]
        D=solve(preset,stage,rank,.6,0.)['local_D_S']
        kappa=s['slope']*D*RP
        u=(P/RP+br/s['Rbr'])/D
        f_full=FLOOR+s['gm_anchor_Hz']*2**(kappa*u)
        f_core=FLOOR+(s['gm_anchor_Hz']*2**(s['slope']*P))*2**(br*s['slope']*RP/s['Rbr'])
        f_stiff=stiff(preset,stage,0.,br,actual_bus_override=P)['reference_Hz']
        assert abs(f_full-f_core)<1e-10 and abs(f_stiff-f_core)<1e-10
        nulls.append({'preset':preset,'stage':stage,'rank':rank,'brilliance':br,
            'rounded_factory_voltage':P,'stiff_nodal_Hz':f_full,'core_formula_Hz':f_core,
            'absolute_error_Hz':abs(f_full-f_core)})
    rows=[{'id':x['id'],'preset':x['preset'],'rank':x['rank'],'brilliance':x['actual_BR_V'],
         'diode_drop':x['diode_drop_V'],
         'loaded':{'hp_cutoff':x['stages']['HP']['reference_Hz'][0],'lp_cutoff':x['stages']['LP']['reference_Hz'][0]},
         'stiff':{'hp_cutoff':x['stiff_stages']['HP']['reference_Hz'],'lp_cutoff':x['stiff_stages']['LP']['reference_Hz']}}
          for x in scenarios]
    files=[ROOT/'Source/DSP/ceres.cpp',Path(__file__).resolve()]
    for name in ('service1.pdf','service2.pdf','cs-servicing-guide.pdf','presets-72.png','cutoff-scale-t71-detail.png',
                 'q-m-rank-table.png','cutoff-m-eg-continuity.png','service1-08.png','pitch-modulation-global-controls.png',
                 'cutoff-voltage-scale.md','brilliance-shared-bus-check.md','lp-bus-coupling-source.md'):
        files.append(research/name)
    for rel in ('tools/benchmark_whitmore_brilliance_endpoint.py','calibration/brass-filter-grid/case10-LPx1-Qfactory.patch',
                'calibration/brass-filter-grid/case20-LPx1-Qfactory.patch'):
        files.append(research.parent/rel)
    return {'schema':'ceres-preset-dc-loading-source-review-v1','kind':'conditional_DC_intercept_only_hypothesis_not_calibration',
        'grid':{'presets':list(PRESET),'ranks':list(RANK),'diode_drop_V':list(DROPS),'actual_BR_V':list(BR_VALUES),'mapping_cases':len(scenarios)},
        'parameters':{'rank_ohms':RANK,'stage_parameters':STAGE,'preset_resistors_ohms_and_EG_bounds':PRESET,
                      'preset_branch_ohms':RP,'shared_return_ohms':R0,'bias_branch_ohms':RBIAS,'bias_wiper_V':0.,'fixed_floor_Hz':FLOOR},
        'source_model':{'selected_rail_V':10.,'unselected_preset_rails':'open, not grounded; ideal reverse blocking and no leakage',
             'inactive_manual_wipers_V':0.,'other_actual_sources_V':0.,'chip_incremental_input_current':'zero',
             'diode_law':'constant forward drop, zero reverse leakage; valid only while declared states pass',
             'bias_reference':'direct M330k plus centered B100k pot Thevenin25k, zero wiper; not recovered calibration',
             'anchor':'12/180Hz variable gm at stiff actualFH/FL0, E0, BR0, other sources0 and centered bias',
             'scope':'All E0 gives DC intercept only. Audio experiment retains existing own-EG scales/times; computed own/cross Jacobian is diagnostic and is not secretly applied.'},
        'equations':['D=gp+Grank+gBR+3*gmod+gEG+gBias','kappa=s_actual*D/gp',
            '(gs+g0+4*gp)*P-gp*sum(u_i)=gs*(10-Vd)',
            'D*u_i-gp*P=gBR*BR+gEG*E_i+gBias*bias',
            'f_ref_i=floor+anchor*2**(kappa*u_i(E_all_zero))'],
        'rows':rows,'scenarios':scenarios,'analytic_stiff_nulls':nulls,'checks':{'EG_corner_solutions':corner_count,'maximum_normalized_KCL_residual':worst_residual,'maximum_analytic_null_error_Hz':max(x['absolute_error_Hz'] for x in nulls),
             'minimum_manual_off_margin_V':minimum_margin,'full_5node_vs_reduced_tolerance_V':1e-14},
        'excluded_zero_drop_diagnostic':zero_cases,
        'input_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-root',type=Path,default=Path('/tmp/ceres-cs50-1x8LGd/research'))
    args=parser.parse_args()
    print(json.dumps(build_evidence(args.research_root),indent=2,sort_keys=True))
