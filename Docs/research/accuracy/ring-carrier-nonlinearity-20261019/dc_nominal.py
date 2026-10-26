#!/usr/bin/env python3
"""Passive DC limit and physical thermal-voltage constants; no DSP/audio/fit."""
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
PRIMARY = HERE / 'ElectronicSynth.pdf'
GUIDE = Path('/tmp/ceres-cs50-1x8LGd/research/cs-servicing-guide.pdf')
PRA = Path('/tmp/ceres-cs50-1x8LGd/research/service2.pdf')


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def solve(matrix, rhs):
    a = [list(row) + [v] for row, v in zip(matrix, rhs)]
    n = len(a)
    for k in range(n):
        pivot = max(range(k, n), key=lambda i: abs(a[i][k]))
        a[k], a[pivot] = a[pivot], a[k]
        d = a[k][k]
        assert abs(d) > 1e-15
        a[k] = [v / d for v in a[k]]
        for i in range(n):
            if i != k:
                f = a[i][k]
                a[i] = [v - f*w for v, w in zip(a[i], a[k])]
    return [row[-1] for row in a]


def circuit(balance):
    # x=carrier7, b=carrier8, a=signal bias, s=local supply, i1/i4=signal inputs.
    # Signal/bias/collector IC currents are zero in this declared passive limit.
    n = 6
    g = [[0.]*n for _ in range(n)]
    rhs = [0.]*n
    def resistor(i, j, r, source=0.):
        y = 1/r
        g[i][i] += y
        if j is None:
            rhs[i] += y*source
        else:
            g[j][j] += y
            g[i][j] -= y
            g[j][i] -= y
    r1, r4 = 100000+50000*balance, 100000+50000*(1-balance)
    for i,j,r,source in [(0,None,33000,0),(0,1,100,0),(1,2,820,0),
                          (1,3,1200,0),(2,None,1000,0),(3,None,390,15),
                          (2,4,470,0),(2,5,470,0),(4,None,r1,0),(5,None,r4,0)]:
        resistor(i,j,r,source)
    v = solve(g,rhs)
    ra = 1/(1/1000+1/(470+r1)+1/(470+r4))
    rb = 1/(1/33100+1/(820+ra))
    b = 15*rb/(390+1200+rb)
    schur = [b*33000/33100,b,b*ra/(820+ra),15*(1200+rb)/(390+1200+rb)]
    schur += [schur[2]*r1/(470+r1),schur[2]*r4/(470+r4)]
    residual = max(abs(sum(g[i][j]*v[j] for j in range(n))-rhs[i]) for i in range(n))
    error = max(abs(x-y) for x,y in zip(v,schur))
    assert residual < 1e-15 and error < 1e-11
    return {'balance_fraction':balance,'voltages':dict(zip(['pin7','pin8','audio_bias','local_supply','pin1','pin4'],v)),
            'carrier_dc_volts':v[0]-v[1],'signal_dc_volts':v[4]-v[5],
            'max_kcl_residual_amps':residual,'max_independent_schur_difference_volts':error}


def build():
    k, charge, kelvin = 1.380649e-23, 1.602176634e-19, 298.15
    vt = k*kelvin/charge
    rows = [circuit(z) for z in (0.,.5,1.)]
    dc = rows[1]['carrier_dc_volts']
    # No sine arrays, audio processing or fitting: pointwise physical law only.
    return {'scope':'Conditional nominal ideal-source, zero-IC-current external DC limit plus matched forward-active carrier pair at25C. No hardware fit or complete operating-point calibration.',
            'nodes':rows,'temperature_kelvin':kelvin,'thermal_voltage_volts':vt,'two_thermal_voltages_volts':2*vt,
            'nominal_source_peak_volts':1.5,'nominal_carrier_dc_volts':dc,
            'switch_at_zero_ac':math.tanh(dc/(2*vt)),
            'switch_ac_small_signal_slope_per_normalized_unit':1.5/(2*vt)/math.cosh(dc/(2*vt))**2,
            'switch_law':'tanh((nominal_carrier_dc_volts + 1.5 * passive_differential_carrier)/(2*VT))',
            'test_voltages':[{'differential_volts':x*vt,'switching_factor':math.tanh(x/2)} for x in (-10.,-2.,0.,2.,10.)],
            'inputs':{str(p):digest(p) for p in [PRIMARY,GUIDE,PRA,HERE/'fairchild-12.png',HERE/'yamaha-pwm-49.png',Path(__file__)]}}


if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');args=ap.parse_args()
    value=build();out=HERE/'dc-nominal.json'
    if args.check:
        assert json.loads(out.read_text())==value
        print('PASS exact DC/thermal recomputation; nodal and independent series/parallel reductions agree')
    else:
        with out.open('x') as f:json.dump(value,f,indent=2,sort_keys=True);f.write('\n')
        print(json.dumps({k:value[k] for k in ('nominal_carrier_dc_volts','two_thermal_voltages_volts','switch_at_zero_ac')}))
