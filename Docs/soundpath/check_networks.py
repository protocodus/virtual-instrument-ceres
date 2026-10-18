#!/usr/bin/env python3
"""Independent passive-node checks. No audio, fitting, package or DSP edits.

Run from any directory. --check compares against the saved numerical artifact.
Only the Python standard library is required.
"""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def solve(a, b):
    m = [list(row) + [float(v)] for row, v in zip(a, b)]
    n = len(b)
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        assert abs(m[p][c]) > 1e-30
        m[c], m[p] = m[p], m[c]
        d = m[c][c]
        m[c] = [v / d for v in m[c]]
        for r in range(n):
            if r == c:
                continue
            v = m[r][c]
            m[r] = [x - v*y for x, y in zip(m[r], m[c])]
    return [row[-1] for row in m]


def full_nodes(rp, r_shared, rth, g_other, source, shared, r_source):
    """11-node static solve: 8 voice nodes, 2 preset buses, 1 shared source.

    g_other is a grounded incremental shunt. All variables are increments;
    baseline absolute voltages are not inferred.
    """
    a = [[0.0]*11 for _ in range(11)]
    b = [0.0]*11
    for j in range(2):
        gp, gs = 1/rp, 1/r_shared[j]
        for i in range(4):
            n = j*4+i
            a[n][n] = gp+gs+g_other[j]
            a[n][8+j], a[n][10] = -gp, -gs
        if rth[j] == 0:
            a[8+j][8+j], b[8+j] = 1.0, source[j]
        else:
            a[8+j][8+j] = 1/rth[j]+4*gp
            b[8+j] = source[j]/rth[j]
            for i in range(4):
                a[8+j][j*4+i] = -gp
    if r_source == 0:
        a[10][10], b[10] = 1.0, shared
    else:
        a[10][10], b[10] = 1/r_source, shared/r_source
        for j in range(2):
            for i in range(4):
                a[10][j*4+i] = -1/r_shared[j]
                a[10][10] += 1/r_shared[j]
    x = solve(a, b)
    residual = max(abs(sum(v*z for v, z in zip(row, x))-rhs)
                   for row, rhs in zip(a, b))
    return x, residual


def q_rhs(q, er, rth, rr, gq):
    r, c = 220e3, 10e-6
    p = [sum(q[4*j:4*j+4])/r/(1/rth[j]+4/r) if rth[j] else 0
         for j in range(2)]
    w = ((er/rr+sum(q)/r)/(1/rr+8/r)) if rr else er
    return [((p[j]-q[j*4+i])/r+(w-q[j*4+i])/r-gq[j]*q[j*4+i])/c
            for j in range(2) for i in range(4)]


def calculate():
    rth_sets = [(0., 0.), (18e3*10e3/28e3, 33e3*10e3/43e3),
                (7.5e3*10e3/17.5e3, 7.5e3*10e3/17.5e3)]
    cutoff_cases = 0
    ratio_error = residual = 0.0
    for resistors in [(680e3, 220e3), (150e3, 47e3)]:
        for rth in rth_sets:
            for g in [(0., 0.), (1/2700, 1/3000), (1/1800, 1/2000)]:
                y, e = full_nodes(39e3, resistors, rth, g, [0., 0.], 1., 0.)
                residual = max(residual, e)
                for j in range(2):
                    v = [0., 0.]; v[j] = 1.
                    z, e = full_nodes(39e3, resistors, rth, g, v, 0., 0.)
                    expected = (39e3+4*rth[j])/resistors[j]
                    ratio_error = max(ratio_error, abs(y[j*4]/z[j*4]-expected))
                    residual = max(residual, e)
                    cutoff_cases += 1
    assert ratio_error < 1e-12 and residual < 1e-12

    q_cases = 0
    q_steady_error = q_difference_error = 0.
    for rth in rth_sets:
        for rr in [0., 2500., 25000.]:
            for gq in [(0., 0.), (1/100e3, 1/50e3)]:
                q, e = full_nodes(220e3, [220e3]*2, rth, gq, [0., 0.], 2., rr)
                residual = max(residual, e)
                q_steady_error = max(q_steady_error, max(abs(v) for v in q_rhs(q[:8], 2., rth, rr, gq)))
                arbitrary = [.1, .2, -.3, .7, .8, -.6, .4, .2]
                derivative = q_rhs(arbitrary, 2., rth, rr, gq)
                for j in range(2):
                    for i in range(1, 4):
                        expected = -(2/220e3+gq[j])/10e-6*(arbitrary[4*j+i]-arbitrary[4*j])
                        observed = derivative[4*j+i]-derivative[4*j]
                        q_difference_error = max(q_difference_error, abs(expected-observed))
                if rr == 0:
                    for j in range(2):
                        den = 1/(220e3+4*rth[j])+1/220e3+gq[j]
                        assert abs(q[4*j]-(2/220e3)/den) < 1e-12
                q_cases += 1
    assert q_steady_error < 1e-12 and q_difference_error < 1e-12
    examples = []
    for name, rs in [('stiff manual', [0., 0.]), ('Flute', [18e3, 33e3]),
                     ('Brass I LP; HP stiff-source reference', [0., 7.5e3])]:
        rth = [v*10e3/(v+10e3) if v else 0. for v in rs]
        den = [1/(220e3+4*v)+1/220e3 for v in rth]
        examples.append({'name': name, 'Rth_ohms': rth,
                         'delta_pin7_V_per_stiff_RES_V': [(1/220e3)/v for v in den],
                         'common_tau_seconds': [10e-6/v for v in den]})
    return {
        'status': 'pass', 'cutoff_ratio_cases': cutoff_cases, 'q_network_cases': q_cases,
        'max_cutoff_ratio_error': ratio_error, 'max_absolute_KCL_residual': residual,
        'max_Q_steady_derivative_error_V_per_s': q_steady_error,
        'max_Q_difference_mode_error_V_per_s': q_difference_error,
        'stiff_manual_octave_per_control_V': {
            'brilliance_HP': .7*39/680, 'brilliance_LP': .65*39/220,
            'SUB_HP': .7*39/150, 'SUB_LP': .65*39/47,
            'SUB_HP_per_LP_octave': (.7/.65)*(47/150)},
        'conditional_Q_examples': examples,
        'limits': ['All voltages are incremental about fixed baseline/source impedances.',
                   'Zero/nonnegative linear input conductance cases do not identify IC input laws.',
                   '.7/.65 octave slopes and .18 damping/V are existing provisional calibration choices.',
                   'Changing passive pot source resistance requires baseline DC backfeed terms.',
                   'No audio, rendering, fitting or hardware-calibration evidence is produced.']}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--check', action='store_true')
    args = parser.parse_args(); out = ROOT/'nodal-checks.json'
    result = calculate()
    if args.check:
        assert json.loads(out.read_text()) == result, 'saved calculation differs'
    else:
        out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'status': 'pass', 'check_only': args.check,
                      'cutoff_ratio_cases': result['cutoff_ratio_cases'],
                      'q_network_cases': result['q_network_cases']}))


if __name__ == '__main__':
    main()
