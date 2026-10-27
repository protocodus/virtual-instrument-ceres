#!/usr/bin/env python3
"""Constant-only examples; no synth import, audio, targets, or optimization."""
import cmath
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
ARC = Path('/tmp/ceres-cs50-1x8LGd/research')
CORE = Path('/tmp/ceres-pitch-route-20261019/Source/DSP')
FILES = [CORE / 'ceres.cpp', CORE / 'ceres.hpp'] + [ARC / name for name in [
    'service1.pdf', 'service2.pdf', 'yamaha-ic-guide.pdf', 'cs-servicing-guide.pdf',
    'cs-servicing-guide.txt', 'ic-guide-033.png', 'envelope-audit-crop.png',
    'q-m-filter-full.png', 'envelopes.png', 'global-resonance-loading.md',
    'resonance-control-dynamics.md', 'q-feedback-derivation.md',
    'q-control-loading-followup.md', 'envelope-timing-audit.md',
    'filter-envelope-derivation.md', 'input-coupling-review.md',
    'output-coupling-integration-review.md']]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def lp_transfer(resonance):
    """Existing linearized circuit, physical source/loading and both capacitors."""
    s = 2j * math.pi * 1000
    a = 2 * math.pi * 1000
    ys = 1 / (150000 + 1 / (s * 1e-6))
    yg, g, c = 1 / 33000, 1 / 1200000, 180e-12
    d0 = 1 / 150000 + yg + g
    damping = 2 - 1.8 * resonance
    n = damping * d0 / 2
    d = ys + yg + g
    denominator = c * s**3 + (d + 2*a*c) * s**2 + (2*a*n + c*a*a) * s + d*a*a
    return 2 * ys * a*a / denominator


def build():
    tau = 10e-6 * (220000 / 2)
    initial, final = .2, .8
    times = [0, .05, tau * math.log(2), tau * math.log(10)]
    rows = []
    for t in times:
        r = final + (initial-final) * math.exp(-t/tau)
        rows.append({'time_seconds': t, 'resonance': r,
                     'lp_1khz_gain_vs_instant_final_db': 20*math.log10(abs(lp_transfer(r)/lp_transfer(final)))})
    release_t60, elapsed, old_il, new_il = 1., .05, 0., -5.
    release_tau = release_t60 / math.log(1000)
    retained = new_il + (old_il-new_il)*math.exp(-elapsed/release_tau)
    difference = retained-new_il
    hp_oct, lp_oct = difference*.7*39/220, difference*.65*39/47
    return {
        'scope': 'Constant-only conditional examples. No model/audio execution or hardware calibration.',
        'inputs': {str(p): sha(p) for p in FILES},
        'producer': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))},
        'q': {'assumption': 'stiff source voltages and zero incremental pin-7 current',
              'tau_seconds': tau, 'initial_resonance': initial, 'final_resonance': final,
              'bare_integrator_hz': 1000., 'rows': rows},
        'idle_eg': {'assumption': 'existing normalized release law retained while idle',
                    'release_t60_seconds': release_t60, 'release_tau_seconds': release_tau,
                    'elapsed_seconds': elapsed, 'old_il_volts': old_il, 'new_il_volts': new_il,
                    'retained_voltage': retained, 'snap_voltage': new_il,
                    'difference_volts': difference, 'existing_hp_gm_octave_difference': hp_oct,
                    'existing_lp_gm_octave_difference': lp_oct,
                    'existing_lp_gm_ratio': 2**lp_oct,
                    'fixed_filter_floor_is_additive': True},
    }


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    value = build()
    path = HERE / 'analytic-examples.json'
    if args.check:
        assert json.loads(path.read_text()) == value
        print('PASS: exact constant-only JSON recomputation and input hashes')
    else:
        with path.open('x') as out:
            json.dump(value, out, indent=2, sort_keys=True)
            out.write('\n')
        print(path)
