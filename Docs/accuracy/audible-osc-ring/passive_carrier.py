#!/usr/bin/env python3
"""Source-only carrier-network calculation; stdlib, no audio/model/fit imports."""
import cmath
import json
import math


def solve(a, b):
    a = [list(row) + [rhs] for row, rhs in zip(a, b)]
    n = len(b)
    for k in range(n):
        p = max(range(k, n), key=lambda i: abs(a[i][k]))
        a[k], a[p] = a[p], a[k]
        assert abs(a[k][k]) > 1e-20
        v = a[k][k]
        a[k] = [x/v for x in a[k]]
        for i in range(n):
            if i != k:
                v = a[i][k]
                a[i] = [x-v*y for x, y in zip(a[i], a[k])]
    return [row[-1] for row in a]


def network(f, signal_series=None, trim_fraction=.5):
    # x=7,b=8,a=audio-bias,s=local+15; optionally p=1,q=4,t=audio-cap right.
    n = 4 if signal_series is None else 7
    a = [[0j]*n for _ in range(n)]
    b = [0j]*n

    def edge(i, j, y):
        a[i][i] += y
        if j is not None:
            a[j][j] += y
            a[i][j] -= y
            a[j][i] -= y

    for i, j, r in [(0,None,33000),(0,1,100),(1,2,820),
                    (1,3,1200),(2,None,1000),(3,None,390)]:
        edge(i,j,1/r)
    s = 2j*math.pi*f
    for i in range(4):
        edge(i,None,s*100e-6)
    b[0] = s*100e-6  # ideal unit carrier source left of input100uF
    if n == 7:
        edge(4,2,1/470)
        edge(5,2,1/470)
        edge(4,None,1/(100000+50000*trim_fraction))
        edge(5,None,1/(100000+50000*(1-trim_fraction)))
        edge(4,6,1/signal_series)
        edge(6,None,s*10e-6)  # zero audio source for carrier transfer
    v = solve(a,b)
    residual = max(abs(sum(ai*vi for ai,vi in zip(row,v))-bi)
                   for row,bi in zip(a,b))
    h = v[0]-v[1]
    return dict(magnitude=abs(h), db=20*math.log10(abs(h)),
                phase_degrees=math.degrees(cmath.phase(h)),
                kcl_max_abs_amp_per_source_volt=residual)


def calculate():
    rows=[]
    for f in [.2,1,6.6692178847,7,30,50,100,220]:
        basic=network(f)
        # Independent scalar Schur solution for exactly the four-node limit.
        s=2j*math.pi*f
        ya=1/1000+s*100e-6
        ys=1/390+s*100e-6
        yb=s*100e-6+ya/(1+820*ya)+ys/(1+1200*ys)
        zb=1/yb
        yx=1/33000+1/(100+zb)
        vx=(s*100e-6)/(s*100e-6+yx)
        h=vx*100/(100+zb)
        assert abs(abs(h)-basic['magnitude'])<2e-14
        extras=[]
        for rs in [330,330000]:
            for q in [0,.5,1]:
                z=network(f,rs,q)
                extras.append(dict(signal_series_ohms=rs,trim_fraction=q,**z))
        rows.append(dict(frequency_hz=f,four_node=basic,
                         full_external_zero_IC_current=extras,
                         scalar_oracle_magnitude_error=abs(abs(h)-basic['magnitude'])))
    return dict(scope='Small-signal differential carrier pin7-pin8. Ideal IG00150 voltage source; zero incremental IC currents. No multiplier law or normalizing gain.',
                nodes=['pin7','pin8','audio_bias','local_supply','pin1','pin4','audio_cap_right'],
                main_capacitances_f=[.0001,.0001,.0001,.0001],
                expanded_audio_cap_f=.00001,rows=rows,
                audio_read=False,native_render_count=0,fit_count=0)


if __name__=='__main__':
    print(json.dumps(calculate(),indent=2,sort_keys=True,allow_nan=False))
