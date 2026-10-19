#!/usr/bin/env python3
"""Archive the exact research build and declare the fixed native route matrix."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = Path('/tmp/ceres-pitch-route-20261019').resolve()
COMMIT = '7e82a5ba212ebe5bf9cbf41967839a4aa3fbaf7d'
sys.path.insert(0, str(ROOT / 'Tools/accuracy'))
import run_battery as b


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    plan_path = HERE / 'plan.json'
    identity = HERE / 'identity'
    assert not plan_path.exists() and not identity.exists(), 'Preserve every declaration/identity'
    assert not list(HERE.rglob('*.wav')), 'No reference-linked render before declaration'
    assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=SOURCE, text=True).strip() == COMMIT
    assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=SOURCE, text=True).strip()
    binary = SOURCE / 'build/ceres_render'
    compiled = b.read_identity(binary, SOURCE)
    validation_path = SOURCE / 'Docs/research/pitch-route-validation/validation.json'
    validation = b.read(validation_path)
    assert validation['compiled_sources'] == compiled
    assert sha(binary) == validation['binaries']['build/ceres_render']
    assert validation['checks']['release_ctest_passed'] == validation['checks']['release_ctest_total'] == 11
    assert validation['checks']['targeted_asan_ubsan_passed'] == validation['checks']['targeted_asan_ubsan_total'] == 3
    assert validation['checks']['parent_default_shared_audio_byte_exact']
    # Copy all implementation evidence as well as the five compiled inputs.
    implementation = identity / 'implementation'
    for relative, digest in validation['input_sha256'].items():
        source = SOURCE / relative
        assert sha(source) == digest, relative
        target = implementation / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert sha(target) == digest
    target = implementation / 'Docs/research/pitch-route-validation/validation.json'
    shutil.copy2(validation_path, target)
    for relative, digest in compiled.items():
        target = identity / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(SOURCE / relative, target)
        assert sha(target) == digest
    retained = identity / 'ceres_render'
    shutil.copy2(binary, retained)
    assert sha(retained) == sha(binary)
    assert b.read_identity(retained, identity) == compiled
    spec = importlib.util.spec_from_file_location('new_pitch_route_runner', ROOT / 'Tools/accuracy/pitch_route.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    # This function only reads input metadata and writes case patches/commands.
    cases = runner.prepare_cases(retained)
    assert len(cases) == 24 and sum(c['byte_exact_reference'] is not None for c in cases) == 11
    inputs = {}
    old = b.read(ROOT / 'Docs/accuracy/joint-calibration/plan.json')
    # All old controls/data remain exact; no scoring happens during declaration.
    inputs.update(old['input_sha256'])
    for prior in ['joint-calibration/plan.json', 'joint-calibration/native-controls-001/report.json',
                  'brightness-boundary/plan.json', 'brightness-boundary/qualify-001/report.json',
                  'brightness-boundary/RESULT.md', 'brightness-boundary/checkpoint.json',
                  'runs/expression-55869df8/report.json']:
        inputs[str(ROOT / 'Docs/accuracy' / prior)] = sha(ROOT / 'Docs/accuracy' / prior)
    paths = [Path(__file__), ROOT / 'Tools/accuracy/pitch_route.py', HERE / 'source-review.md',
             HERE / 'method-review.md', HERE / 'code-review.md', HERE / 'runner-review.md',
             ROOT / 'Tools/accuracy/run_battery.py', ROOT / 'Tools/accuracy/metrics.py',
             ROOT / 'Tools/accuracy/preset_dc_loading.py', ROOT / 'Docs/accuracy/plan.json']
    paths += [p for p in identity.rglob('*') if p.is_file()]
    research = Path('/tmp/ceres-cs50-1x8LGd/research')
    for name in ['pitch-modulation-routing.md', 'pitch-modulation-routing.json',
                 'keyboard-tracking-and-master-pitch.md', 'whitmore-modulation-controls.md',
                 'whitmore-modulation-controls-erratum.md', 'service1.pdf', 'service2.pdf',
                 'cs-servicing-guide.pdf', 'yamaha-ic-guide.pdf']:
        paths.append(research / name)
    for case in cases:
        paths += [Path(case['patch'])]
        paths += [ROOT / case['original_case'][k] for k in ['events', 'midi', 'reference', 'patch']]
        if case['byte_exact_reference'] is not None:
            paths.append(Path(case['byte_exact_reference']['audio']))
    for path in paths:
        digest = sha(path)
        assert str(path) not in inputs or inputs[str(path)] == digest, str(path)
        inputs[str(path)] = digest
    for path, digest in inputs.items():
        assert sha(path) == digest, path
    plan = dict(schema='ceres-pitch-route-v1',
        declared_utc=datetime.now(timezone.utc).isoformat(),
        status='declared_before_any_new_reference_render_or_target_score',
        routes=['shared', 'vco-only', 'static-master'],
        contexts=['frozen', 'network-br0', 'network-brminus15'],
        common_master_semitones=0.367919921875,
        common_master_origin='First chronological saved high-Flute bend; same fixed reference for both passages, not measured tuning.',
        fixed_calibration=[0., 0., 1.], native_render_count=24,
        known_byte_exact_reference_count=11, score_only_after_all_renders=True,
        state_invariance_evidence=[str(implementation / relative) for relative in [
            'Docs/research/pitch-route-validation/validation.json',
            'Tests/PitchRouteTests.cpp', 'Tests/PitchRouteParityDriver.cpp',
            'Docs/research/pitch-route-validation/source.patch',
            'Docs/research/pitch-route-validation/release.log',
            'Docs/research/pitch-route-validation/sanitize.log',
            'Docs/research/pitch-route-validation/shared-parent-parity.json']],
        numeric_replay_tolerance=1e-10, parameter_fits=0, gain_fits=0,
        source_event_or_MIDI_changes=0, production_adoption=False,
        renderer=dict(binary=str(retained), binary_sha256=sha(retained), source_root=str(identity),
                      compiled_source_sha256=compiled, source_commit=COMMIT,
                      implementation_validation_sha256=sha(validation_path)),
        cases=cases, input_sha256=dict(sorted(inputs.items())),
        limits=['Two fixed routing sensitivities, not recovered hardware controls.',
                'Raw Flute and transfer clips are reused development evidence.',
                'No optimizer, aggregate winner selection, production-default change or sample synthesis.'])
    with plan_path.open('x') as f:
        f.write(json.dumps(plan, indent=2, sort_keys=True, allow_nan=False) + '\n')
    print('DECLARED', sha(plan_path), 'bound_files', len(inputs))


if __name__ == '__main__':
    main()
