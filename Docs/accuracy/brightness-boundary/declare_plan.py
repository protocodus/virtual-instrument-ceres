#!/usr/bin/env python3
"""Freeze this new study before any phase; read/hash only, no model or audio."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OLD = ROOT / 'Docs/accuracy/joint-calibration'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def main():
    target = HERE / 'plan.json'
    assert not target.exists(), 'Preserve the declaration'
    assert not any((HERE / name).exists() for name in
                   ['qualify-001', 'jacobian-001', 'fit-001', 'native-validation-001'])
    old_plan = read(OLD / 'plan.json')
    old_fit = read(OLD / 'fit-plan.json')
    renderer = {k: old_plan['native_controls'][k] for k in
                ['binary', 'binary_sha256', 'source_root', 'compiled_source_sha256']}
    inputs = dict(old_plan['input_sha256'])
    for path, digest in old_fit['input_sha256'].items():
        assert path not in inputs or inputs[path] == digest
        inputs[path] = digest
    paths = [Path(__file__), HERE / 'method.md', ROOT / 'Tools/accuracy/brightness_boundary.py',
             OLD / 'plan.json', OLD / 'fit-plan.json', OLD / 'RESULT.md',
             OLD / 'dynamic-qualification-001/report.json',
             OLD / 'native-controls-001/report.json', OLD / 'attempt-001/report.json',
             ROOT / 'Docs/accuracy/runs/expression-55869df8/report.json',
             ROOT / 'Tools/accuracy/preset_dc_loading.py',
             ROOT / 'Docs/accuracy/source-global-controls-20261019/independent-review.md',
             ROOT / 'Docs/accuracy/source-global-controls-20261019/electrical-review.md',
             ROOT / 'Docs/accuracy/source-global-controls-20261019/method-review.md',
             ROOT / 'Docs/accuracy/source-global-controls-20261019/code-review.md',
             Path(renderer['binary'])]
    research = Path('/tmp/ceres-cs50-1x8LGd/research')
    paths += [research / name for name in [
        'service1.pdf', 'service2.pdf', 'cs-servicing-guide.pdf',
        'whitmore-modulation-controls.md', 'whitmore-modulation-controls-erratum.md',
        'whitmore-brass1-modulation-visual.md', 'whitmore-brass1-modulation-visual.json',
        'whitmore-modulation-controls/controls-contact-sheet.png',
        'whitmore-modulation-controls/high_205.80-global.png',
        'whitmore-modulation-controls/lower_210.90-global.png',
        'whitmore-brass1-modulation-visual/brass-event-controls.png',
        'whitmore-brass1-modulation-visual/137.00.png',
        'pitch-modulation-global-controls.png', 'global-resonance-pin7-endpoints.png']]
    paths += [Path(renderer['source_root']) / p for p in renderer['compiled_source_sha256']]
    for case in old_plan['native_controls']['cases']:
        paths.extend([Path(case['audio']), Path(case['patch'])])
    for path in paths:
        digest = sha(path)
        assert str(path) not in inputs or inputs[str(path)] == digest, str(path)
        inputs[str(path)] = digest
    for path, digest in inputs.items():
        assert sha(path) == digest, path
    assert sha(renderer['binary']) == renderer['binary_sha256']
    for path, digest in renderer['compiled_source_sha256'].items():
        assert sha(Path(renderer['source_root']) / path) == digest
    plan = dict(
        schema='ceres-source-conditioned-dark-boundary-v1',
        status='declared_before_any_new_phase_render_model_or_source_target_access',
        declared_utc=datetime.now(timezone.utc).isoformat(),
        training_ids=['flute-high-factory-physical', 'brass-history20'],
        shared_actual_BR_V=-15,
        BR_interpretation='One common maximum-darkening boundary hypothesis, not a measured voltage.',
        authorized_phases=dict(qualify=True, jacobian=True, fit=True, validate=True),
        qualification=dict(thresholds=dict(
            ratio_rms_db=1., ratio_p95_abs_db=2., level_rms_db=1., level_p95_abs_db=2.,
            brass_cell_signed_mean_abs_db=.5, quadrature_delta_max_db=.1)),
        jacobian=dict(central_steps=[.25, .25, .01], hp_rms_db_per_V_min=.02,
                      normalized_singular_ratio_min=.001, analytic_derivative_max_abs_error=1e-6),
        fit=dict(x0=[0., 0., 1.], lower_bounds=[-15., -15., .5],
                 upper_bounds=[15., 15., 2.], method='SLSQP',
                 max_distinct_forwards=80, maxiter=80, ftol=1e-9,
                 objective='flute_ratio_trajectory_MSE',
                 constraints='frozen_disabled_Flute_level_MSE_and_six_Brass_ratio_MAEs_and_six_Brass_H1_level_MSEs',
                 derivative_steps=[1e-4, 1e-4, 1e-5], derivative_max_abs_error=1e-6,
                 fail_on_MAE_sign_crossing=True,
                 candidate_count=1, bounds_are_engineering_not_measured=True,
                 budget_exception_stops_before_validation=True),
        numeric_tolerance_db=1e-10,
        native_validation=dict(new_WAV_ceiling=10,
            allow_unsuccessful_or_infeasible_solver_return=True,
            failed_solver_return_is_diagnostic_not_adoption=True,
            exact_gates='Unchanged run_battery.measure and preset_dc_loading.contrast against frozen and BR=-15 theta0 controls.',
            transfer_ids=['flute-lower-factory-physical', 'brass-history10'],
            candidate_frozen_before_transfer=True, no_second_candidate=True),
        fixed_nuisance='Rank B; diode .6 V; fixed355k bias branches; zerochipinputcurrent; BR-15bothpresets; touch0; source, Q, EGtime/levels, gates, MIDI, AMphase, masks and gains unchanged.',
        study_limits=['Partial-metric training feasibility; native median, tails and weak counts remain mandatory.',
                      'Reused development excerpts and reconstructed MIDI; no independent hardware holdout.',
                      'No production adoption or GUI change from this tool.'],
        renderer=renderer, input_sha256=dict(sorted(inputs.items())))
    with target.open('x') as f:
        f.write(json.dumps(plan, indent=2, sort_keys=True, allow_nan=False) + '\n')
    print('DECLARED', sha(target), 'bound_files', len(inputs))


if __name__ == '__main__':
    main()
