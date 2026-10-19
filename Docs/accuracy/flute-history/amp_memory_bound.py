#!/usr/bin/env python3
"""Constant-only retriggered amp-state bound. No audio, synth or optimizer imports."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import traceback

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OLD = Path('/tmp/ceres-cs50-1x8LGd')
CV = OLD / 'research/factory-preset-cv.json'
PARAMETERS = OLD / 'calibration/whitmore-envelope-age.json'
CORE = Path('/tmp/ceres-pitch-route-20261019/Source/DSP/ceres.cpp')
BATTERY = ROOT / 'Docs/accuracy/plan.json'
PLAN = HERE / 'amp-memory-bound-plan.json'
RESULT = HERE / 'amp-memory-bound-result.json'


def read(p):
    return json.loads(Path(p).read_text())


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def write(p, value):
    with Path(p).open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write('\n')


def parameters():
    table = read(CV)
    cv = dict(zip(table['columns'], table['presets']['flute']))
    attack = 10 ** (-.3 * cv['amp_attack_cv'])
    decay = 10 * 10 ** (-.3 * cv['amp_decay_cv']) * math.log(1000.) / 5
    sustain = cv['amp_sustain_cv'] / 10
    archived = read(PARAMETERS)['factory_native_parameters']
    assert attack == archived['attack_seconds']
    assert decay == archived['decay_T60_seconds'] and sustain == archived['sustain']
    tau_d = decay / math.log(1000.)
    assert tau_d == archived['tau_seconds']
    return dict(attack_seconds=attack, headroom=1 / math.expm1(5.),
                decay_T60_seconds=decay, decay_tau_seconds=tau_d, sustain=sustain,
                release_tau_seconds=2 * 10 ** (-.3 * cv['amp_release_cv']),
                internal_sample_rate=192000, initial_residual_interval=[0., 1.])


def calculate():
    p = parameters()
    A, h, tau, S = p['attack_seconds'], p['headroom'], p['decay_tau_seconds'], p['sustain']
    tau_a = A / math.log1p(1 / h)
    C = 20 / math.log(10.)

    def t_peak(u0):
        return tau_a * math.log((1 + h - u0) / h)

    def E(t, peak_time):
        assert t >= peak_time
        return S + (1 - S) * math.exp(-(t - peak_time) / tau)

    def log_slope(t, peak_time):
        state = E(t, peak_time)
        return -C / tau * (state - S) / state

    def state_db(t, peak_time):
        return C * math.log(E(t, peak_time))

    # Independent attack-boundary substitution, not a sampled residual grid.
    peak_times = [t_peak(0.), t_peak(1.)]
    oracle = [abs(peak_times[0] - A), abs(peak_times[1])]
    for u0 in (0., 1.):
        oracle.append(abs((1 + h - (1 + h - u0) * math.exp(-t_peak(u0) / tau_a)) - 1))
    assert max(oracle) < 1e-14
    cases = []
    for c in read(BATTERY)['cases']:
        if c['id'] not in ('flute-high-factory-physical', 'flute-lower-factory-physical'):
            continue
        age, duration = c['offset_seconds'], c['duration']
        assert age >= A
        end = age + duration
        # Delta log-slope = C*S/tau*(1/E_warm - 1/E_fresh).
        # In x=(1-S)*exp(-t/tau), its unique positive maximum is
        # x=S/sqrt(exp(A/tau)); include it only when inside the interval.
        r = math.exp(A / tau)
        critical_time = tau * math.log((1 - S) * math.sqrt(r) / S)
        candidates = [age, end] + ([critical_time] if age <= critical_time <= end else [])
        differences = [log_slope(t, 0.) - log_slope(t, A) for t in candidates]
        deficit_db = state_db(age, 0.) - state_db(age, A)
        fresh_secant = (state_db(end, A) - state_db(age, A)) / duration
        warm_secant = (state_db(end, 0.) - state_db(age, 0.)) / duration
        # For t>=A the maximum amplitude deficit is at the interval start.
        assert deficit_db <= state_db(end, 0.) - state_db(end, A) <= 0
        assert min(differences) >= 0
        cases.append(dict(id=c['id'], gate_age_seconds=age, duration_seconds=duration,
            interval_seconds_after_gate=[age, end],
            fresh_state_at_start=E(age, A), maximum_residual_state_at_start=E(age, 0.),
            largest_envelope_level_difference_db=deficit_db,
            fresh_instant_log_slope_at_start_db_per_s=log_slope(age, A),
            maximum_residual_instant_log_slope_at_start_db_per_s=log_slope(age, 0.),
            maximum_possible_instant_log_slope_flattening_db_per_s=max(differences),
            maximum_flattening_at_seconds_after_gate=candidates[differences.index(max(differences))],
            fresh_endpoint_secant_db_per_s=fresh_secant,
            maximum_residual_endpoint_secant_db_per_s=warm_secant,
            endpoint_secant_flattening_db_per_s=warm_secant - fresh_secant,
            secant_is_not_128ms_windowed_RMS_regression=True))
    assert len(cases) == 2
    return dict(parameters=p, attack_tau_seconds=tau_a,
        continuous_peak_advance_upper_seconds=A,
        conservative_one_internal_step_timing_allowance_seconds=1 / p['internal_sample_rate'],
        continuous_attack_endpoint_oracle_max_error=max(oracle), cases=cases,
        optional_zero_global_release_quarter_second_state_upper=math.exp(-.25 / p['release_tau_seconds']),
        release_bound_assumption='u_off<=1, actual release engaged throughout .25s, frozen global_release=0; physical controls/gate timing not established.',
        scope='Continuous normalized amp coordinate with factory h, D and S; current discrete comparator adds sample quantization. Not a bound on filtered audio, scalar-RMS regression, multiple voices, physical chip calibration or different closure laws.',
        complete_native_audio_accuracy_claim=False, target_audio_read=False, optimizer_calls=0,
        synth_imports=0, native_render_count=0, residual_states_fitted=0)


def run():
    assert not PLAN.exists() and not RESULT.exists(), 'Preserve all attempts'
    files = [Path(__file__).resolve(), CV, PARAMETERS, CORE, BATTERY]
    write(PLAN, dict(status='declared_before_calculation', input_sha256={str(p): sha(p) for p in files},
        operation='Closed-form endpoint bound over u0 in[0,1]; fixed two gate ages from existing battery, no grid or model/audio evaluation.',
        coordinate_assumptions='Ordinary retrigger resets stage to Attack and retains u0; peak1, sustain.524, no closure offset; all times unchanged.'))
    try:
        result = calculate()
        for p, digest in read(PLAN)['input_sha256'].items():
            assert sha(p) == digest
        write(RESULT, dict(plan_sha256=sha(PLAN), result=result))
        print('RESULT', sha(RESULT))
    except Exception:
        write(HERE / 'amp-memory-bound-failure.json', dict(traceback=traceback.format_exc(), plan_sha256=sha(PLAN)))
        raise


def check():
    plan = read(PLAN)
    for p, digest in plan['input_sha256'].items():
        assert sha(p) == digest
    saved = read(RESULT)
    assert saved['plan_sha256'] == sha(PLAN)
    actual = calculate()
    write(HERE / 'amp-memory-bound-recomputed.json', actual)
    exact = actual == saved['result']
    write(HERE / 'amp-memory-bound-check.json', dict(status='pass' if exact else 'fail', exact_equal=exact,
        result_sha256=sha(RESULT), recomputed_sha256=sha(HERE / 'amp-memory-bound-recomputed.json')))
    assert exact
    print('CHECK exact', exact)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('run', 'check'))
    args = parser.parse_args()
    {'run': run, 'check': check}[args.command]()
