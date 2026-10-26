#!/usr/bin/env python3
"""Source-only local cutoff-control identifiability audit; no audio or fitting."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import traceback

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'Docs/research/accuracy/calibration-identifiability'
NETWORK = ROOT/'Docs/research/accuracy/preset-dc-loading/network.py'
EVIDENCE = ROOT/'Docs/research/accuracy/preset-dc-loading/source-evidence.json'
PINS = {
    'Docs/research/accuracy/preset-dc-loading/network.py': 'b03cdb48f68c73cc70312cb2c8061d69a1d81fea1f6e0330464f8e052f631297',
    'Docs/research/accuracy/preset-dc-loading/source-evidence.json': '3e4965c2a3cf6b0e0252d48439578b38f58e11274138f143d5fb9b40f70a98c3',
    'Source/DSP/ceres.cpp': '3478c02cc7c134a698bf0a32b708b2bda8e7eb0e38f97d27ee458a98c481baaa',
    'Source/DSP/ceres.hpp': '577cbd5909705d7db3a1522edbbfc71ec056d03751bbb97390413cf8cd6375f8',
}
COLUMNS = ['shared_HP_bias_V', 'shared_LP_bias_V', 'Flute_BR_V', 'Brass_BR_V',
           'Flute_touch_source_V', 'Brass_touch_source_V',
           'common_fractional_exponential_scale', 'common_fractional_EG_amplitude']
PRESETS, STAGES = ('flute', 'brass1'), ('HP', 'LP')
RANK, DROP = 'B', .6
RCOND, FD_STEP = 1e-10, 1e-5


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, data):
    assert not path.exists(), 'Never overwrite an attempt or report: '+str(path)
    path.write_text(json.dumps(data, indent=2, sort_keys=True, allow_nan=False)+'\n')


def inputs():
    for relative, digest in PINS.items():
        assert sha(ROOT/relative) == digest, 'Pinned source changed: '+relative
    evidence = json.loads(EVIDENCE.read_text())
    external = {}
    for name, digest in evidence['input_sha256'].items():
        path = Path(name)
        assert sha(path) == digest, 'Original source evidence changed: '+name
        external[name] = digest
    local = {name: sha(ROOT/name) for name in (*PINS,
        'Docs/research/accuracy/preset-dc-loading/source-review.md',
        'Tools/accuracy/calibration_identifiability.py')}
    return {'repository_files_sha256': local, 'inherited_source_files_sha256': external,
            'source_evidence_sha256': sha(EVIDENCE)}


def load_network():
    spec = importlib.util.spec_from_file_location('declared_cutoff_network', NETWORK)
    network = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(network)
    evidence = json.loads(EVIDENCE.read_text())['parameters']
    assert network.RANK == evidence['rank_ohms']
    assert network.STAGE == evidence['stage_parameters']
    assert network.PRESET == evidence['preset_resistors_ohms_and_EG_bounds']
    assert network.RBIAS == evidence['bias_branch_ohms'] == 355000.
    return network


def state(network, preset, stage, eg):
    """Independent five-node linear equation and analytic control derivatives."""
    s = network.STAGE[stage]
    gs = 0. if network.PRESET[preset][stage] is None else 1/network.PRESET[preset][stage]
    gp = 1/network.RP
    D = gp+1/network.RANK[RANK][stage]+1/s['Rbr']+3/s['Rmod']+1/s['Reg']+1/network.RBIAS
    A = np.zeros((5, 5))
    A[0, 0] = gs+1/network.R0+4*gp
    A[0, 1:] = A[1:, 0] = -gp
    A[1:, 1:] = np.eye(4)*D
    rhs = np.r_[gs*(10-DROP), np.full(4, eg/s['Reg'])]
    x = np.linalg.solve(A, rhs)
    kappa = s['slope']*D/gp
    # All three sources fan to every voice; output coordinate is voice 1.
    derivatives = {name: float(kappa*np.linalg.solve(A, np.r_[0., np.full(4, 1/R)])[1])
                   for name, R in [('bias', network.RBIAS), ('BR', s['Rbr']), ('touch', s['Rmod'])]}
    reference = network.solve(preset, stage, RANK, DROP, 0., eg=(eg,)*4)
    discrepancy = float(np.max(np.abs(x-np.r_[reference['bus_V'], reference['pin4_V']])))
    assert discrepancy < 1e-13
    assert reference['manual_off_margin_V'] > 0
    assert reference['selected_branch_current_A'] is None or reference['selected_branch_current_A'] > 0
    return float(kappa*x[1]), derivatives, reference, discrepancy


def evaluate(network, rows, theta):
    """Independent finite-difference path through the retained network helper."""
    result = []
    for row in rows:
        pi, si = PRESETS.index(row['preset']), STAGES.index(row['stage'])
        s = network.STAGE[row['stage']]
        # One physical touch feed uses the same Rmod as the existing source audit.
        # Equivalent bias is valid only in this declared linear, fixed-R network.
        bias = theta[si]+theta[4+pi]*network.RBIAS/s['Rmod']
        value = network.solve(row['preset'], row['stage'], RANK, DROP, theta[2+pi],
            eg=(row['EG_V']*(1+theta[7]),)*4, bias=bias)['log2_gm_shift'][0]
        result.append((1+theta[6])*value)
    return np.asarray(result)


def svd_result(J, count):
    raw = J[:, :count]
    norms = np.linalg.norm(raw, axis=0)
    assert np.all(norms > 0)
    normalized = raw/norms
    U, singular, Vt = np.linalg.svd(normalized, full_matrices=False)
    rank = int(np.sum(singular > singular[0]*RCOND))
    return {'columns': COLUMNS[:count], 'shape': list(raw.shape), 'rank': rank,
        'nullity': count-rank, 'relative_rank_threshold': RCOND,
        'raw_column_L2_norm': norms.tolist(), 'singular_values_column_normalized': singular.tolist(),
        'identifiable_subspace_condition_number': float(singular[0]/singular[rank-1]),
        'svd_reconstruction_max_abs_error': float(np.max(np.abs((U*singular)@Vt-normalized)))}


def calculate():
    bindings = inputs()
    n = load_network()
    rows, matrix, discrepancies, margins = [], [], [], []
    for pi, preset in enumerate(PRESETS):
        for si, stage in enumerate(STAGES):
            z0, _, _, _ = state(n, preset, stage, 0.)
            for label, E in [('zero', 0.), ('initial', n.PRESET[preset]['IL']), ('peak', n.PRESET[preset]['AL'])]:
                z, d, reference, mismatch = state(n, preset, stage, E)
                row = np.zeros(8)
                row[si], row[2+pi], row[4+pi] = d['bias'], d['BR'], d['touch']
                row[6], row[7] = z, z-z0
                matrix.append(row)
                rows.append({'preset': preset, 'stage': stage, 'EG_state': label, 'EG_V': E,
                    'all_four_voices_equal_EG': True, 'reference_log2_variable_gm_shift': z,
                    'reference_pin4_V': reference['pin4_V'], 'reference_bus_V': reference['bus_V']})
                discrepancies.append(mismatch)
                margins.append(reference['manual_off_margin_V'])
    J = np.asarray(matrix)
    assert J.shape == (12, 8)
    finite = np.empty_like(J)
    for column in range(8):
        step = np.zeros(8)
        step[column] = FD_STEP
        finite[:, column] = (evaluate(n, rows, step)-evaluate(n, rows, -step))/(2*FD_STEP)
    fd_error = float(np.max(np.abs(finite-J)))
    assert fd_error < 1e-8
    cases = [svd_result(J, count) for count in (4, 6, 7, 8)]
    assert [c['rank'] for c in cases] == [3, 4, 5, 5]
    # Known symbolic gauges; these are tangent directions, not proposed settings.
    br = np.array([-n.RBIAS/n.STAGE['HP']['Rbr'], -n.RBIAS/n.STAGE['LP']['Rbr'], 1, 1, 0, 0, 0, 0.])
    touch = np.array([-n.RBIAS/n.STAGE['HP']['Rmod'], -n.RBIAS/n.STAGE['LP']['Rmod'], 0, 0, 1, 1, 0, 0.])
    scale = np.zeros(8)
    scale[6], scale[7] = 1., -1.
    # Cancel only the remaining neutral offsets, using BR/touch per preset.
    # This 2x2 algebraic solve is not a recording fit or a recommended control value.
    for pi in range(2):
        ids = [pi*6, pi*6+3]
        cols = [2+pi, 4+pi]
        scale[cols] = np.linalg.solve(J[np.ix_(ids, cols)], -J[ids, 6])
    gauges = []
    for name, vector in [('BR_and_bias', br), ('touch_and_bias', touch), ('exponential_and_EG_scale', scale)]:
        residual = J@vector
        relative = float(np.linalg.norm(residual)/(np.linalg.norm(J)*np.linalg.norm(vector)))
        assert relative < 1e-12
        gauges.append({'name': name, 'coefficients_in_column_order': vector.tolist(),
            'max_abs_residual_octaves': float(np.max(np.abs(residual))),
            'relative_residual': relative})
    assert np.linalg.matrix_rank(np.asarray([g['coefficients_in_column_order'] for g in gauges]), tol=1e-10) == 3
    nuisance = J[:, [0, 1, 2, 3, 4, 5, 7]]
    projection = nuisance@np.linalg.lstsq(nuisance, J[:, 6], rcond=RCOND)[0]
    projected = float(np.linalg.norm(J[:, 6]-projection)/np.linalg.norm(J[:, 6]))
    assert projected < 1e-10
    return {'schema': 'ceres-local-calibration-identifiability-v1', 'status': 'checks_pass',
        'decision': 'The declared shared cutoff calibration is structurally non-identifiable when these nuisance controls and EG amplitude are unknown.',
        'input_bindings': bindings, 'numpy_version': np.__version__,
        'model': {'rank': RANK, 'diode_drop_V': DROP, 'baseline_bias_BR_touch_V': 0.,
            'bias_branch_ohms': n.RBIAS, 'chip_incremental_input_current': 0.,
            'exponential_reference': 'Existing .7/.65 actual-bus slopes converted by the retained network, both multiplied by one common factor.',
            'output_coordinate': 'log2 variable-gm shift for voice 1; not audio amplitude, total cutoff Hz, or a measured chip voltage law.',
            'EG_amplitude': 'One fractional multiplier on all four known E values, including both initial and peak; held zero stays zero.',
            'row_construction': 'Two presets x two stages x zero/initial/peak; all four voices use the same known E in each row.',
            'bias_reference': '355k fixed Thevenin branch; real trim resistance is not varied.',
            'touch_reference': 'One of the existing Rmod source branches per stage; other modulation branches stay at zero.',
            'parameter_units': ['V']*6+['fraction', 'fraction']},
        'columns': COLUMNS, 'rows': rows, 'analytic_J_octaves_per_parameter': J.tolist(),
        'finite_difference_J': finite.tolist(), 'subset_SVD': cases, 'independent_null_directions': gauges,
        'checks': {'analytic_vs_original_network_max_node_error_V': max(discrepancies),
            'minimum_reference_manual_diode_off_margin_V': min(margins),
            'finite_difference_step': FD_STEP, 'analytic_vs_central_difference_max_abs_error': fd_error,
            'exponential_column_relative_residual_after_nuisance_projection': projected,
            'all_three_symbolic_null_directions_independent': True},
        'audio_renders': 0, 'recordings_read': 0, 'recording_parameter_fits': 0,
        'DSP_changes': False, 'original_plans_modified': False,
        'limits': [
            'A local algebraic audit of a declared linear fixed-impedance control model, not a hardware parameter measurement or an audio fidelity result.',
            'BR, touch and bias are independent voltage coordinates here; large gauge vectors are not feasible operating-point recommendations.',
            'The actual BR/touch controls, voice trims, chip input current, diode law and real trim impedances are not identified by these assumptions.',
            'No claim of global non-identifiability for every nonlinear hardware model; nonlinear loading, known voltages or other measurements can break these model identities.',
            'The common exponential scale and EG-amplitude columns are hypotheses; their variation is not established component tolerance.',
            'Zero/IL/AL are optimistic known control states, not estimates from the public recordings; no temporal sampling or independent holdout is asserted.',
            'Fixing nuisance values can produce a conditional fit, but does not identify their actual hardware values or justify a production mapping change.',
            'No gains, Q, MIDI, gates, phase, source masks or recording scores are optimized.']}


def run(attempt):
    assert __debug__
    assert attempt and Path(attempt).name == attempt and attempt not in ('.', '..')
    assert not (OUT/'report.json').exists(), 'Preserve completed audit'
    OUT.mkdir(parents=True, exist_ok=True)
    folder = OUT/attempt
    folder.mkdir()
    shutil.copy2(Path(__file__), folder/'calibration_identifiability.py')
    write(folder/'declaration.json', {'kind': 'source-only local algebra check, no audio or fitting',
        'tool_sha256': sha(Path(__file__)), 'expected_subset_ranks': [3, 4, 5, 5],
        'rank_threshold': RCOND, 'finite_difference_step': FD_STEP,
        'input_pins': PINS, 'attempt': attempt})
    try:
        report = calculate()
        report['attempt'] = attempt
        report['declaration_sha256'] = sha(folder/'declaration.json')
        write(folder/'report.json', report)
        shutil.copy2(folder/'report.json', OUT/'report.json')
    except BaseException:
        write(folder/'failure.json', {'tool_sha256': sha(Path(__file__)), 'traceback': traceback.format_exc(),
            'audio_renders': 0, 'recordings_read': 0, 'original_evidence_modified': False})
        raise
    print('PASS:', OUT/'report.json')
    print('Ranks:', [row['rank'] for row in report['subset_SVD']], 'of [4, 6, 7, 8]; zero audio renders')


def check():
    saved = json.loads((OUT/'report.json').read_text())
    folder = OUT/saved['attempt']
    assert sha(folder/'declaration.json') == saved['declaration_sha256']
    assert sha(folder/'calibration_identifiability.py') == sha(Path(__file__))
    assert (folder/'report.json').read_bytes() == (OUT/'report.json').read_bytes()
    actual = calculate()
    actual.update(attempt=saved['attempt'], declaration_sha256=saved['declaration_sha256'])
    assert actual == saved, 'Recomputation differs; preserve old evidence and inspect numerical environment/input changes'
    print('VERIFIED:', sha(OUT/'report.json'), '(exact deterministic recomputation; zero audio)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('run', 'check'))
    parser.add_argument('--attempt', default='attempt-001')
    args = parser.parse_args()
    if args.command == 'run':
        run(args.attempt)
    else:
        check()
