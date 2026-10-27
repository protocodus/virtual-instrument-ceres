#!/usr/bin/env python3
"""Bind the completed combined experiment; inspect evidence without rerendering."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
STUDY=Path('/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-circuits-20261019')
DOCS=ROOT/'Docs/validation/audible-circuits-2026-10-19'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text())

def main():
    binary=ROOT/'build/ceres_render'
    identity=json.loads(subprocess.check_output([str(binary),'--build-info'],text=True))
    assert all(sha(ROOT/name)==digest for name,digest in identity.items())
    assert sha(binary)=='59964a09832bb14772976e5dd53e2b61f5f65de6278d46767948800669dc3a3a'
    assert '100% tests passed, 0 tests failed out of 12' in (DOCS/'release-final.log').read_text()
    assert '100% tests passed, 0 tests failed out of 6' in (DOCS/'sanitize-final.log').read_text()
    assert len(re.findall(r'add_test\(NAME ',(ROOT/'CMakeLists.txt').read_text()))==12
    alias=read(DOCS/'ring-alias-final.json');assert alias['status']=='pass'
    assert alias['compiled_source_sha256']==identity
    assert all(row['sampled_vs_reference_band_error_rms']<2e-11 for row in alias['comparisons'])
    render_count=0;audio_bindings={};component_manifests={}
    for group,count in [('capacitor-run-001',10),('oscillator-run-001',6),('ring-run-001',6),('ring-switch-run-001',3)]:
        folder=STUDY/group;manifest=read(folder/'manifest.json')
        assert len(manifest['renders'])==count
        for key,row in manifest['renders'].items():
            name=('switching-'+key if group=='ring-switch-run-001' else key)+'.wav'
            path=folder/name;assert sha(path)==row['wav_sha256']
            audio_bindings[str(path)]=row['wav_sha256']
        component_manifests[str(folder/'manifest.json')]=sha(folder/'manifest.json');render_count+=count
    public=STUDY/'public-frozen';integrity=read(public/'integrity.json')
    assert integrity['status']=='pass' and integrity['input_bindings']==45
    assert integrity['all5_final_source_hashes_match'] and integrity['all16_parent_candidate_metrics_recomputed']
    assert integrity['all8_commands_exact_except_renderer_and_output'] and integrity['all24_listening_segments_exact']
    assert integrity['all_audio_finite'] and integrity['total_candidate_samples_above_unity']==0
    for path,digest in integrity['output_sha256'].items():assert sha(STUDY/path)==digest,path
    comparison=read(public/'comparison.json')
    assert comparison['candidate_compiled_source_sha256']==identity
    assert comparison['parameter_fits']==0 and comparison['gain_fits']==0 and not comparison['original_hardware_MIDI']
    for row in integrity['audio_pairs']:
        path=public/'runs/audible-final'/(row['id']+'.wav');assert sha(path)==row['candidate_sha256']
        audio_bindings[str(path)]=row['candidate_sha256']
    render_count+=integrity['native_candidate_renders'];assert render_count==33
    montage=read(STUDY/'montage.json')
    assert montage['wav_sha256']==sha(STUDY/'comparison-circuits.wav')
    assert montage['seconds']==18.75 and len(montage['segments'])==12 and montage['gain']==1
    assert montage['plan_sha256']==sha(STUDY/'montage-plan.json')
    for row in montage['segments']:assert sha(row['path'])==row['sha256']
    performance=read(DOCS/'performance/comparison.json')
    for key,row in performance['runs'].items():
        assert row['stats']['cpu_deadline_misses']==0
        if key.startswith('candidate'):assert row['compiled_source_sha256']==identity
    external=STUDY.parent
    evidence=[STUDY/'RESULT.md',STUDY/'montage-plan.json',STUDY/'montage.json',STUDY/'assemble_montage.py',
              public/'integrity.json',public/'comparison.json',public/'README.md',STUDY/'evaluate_public.py',STUDY/'public-evaluation-plan.json',
              external/'filter-eg-capacitor-continuity/source-audit.md',external/'filter-eg-capacitor-continuity/implementation-review-rebind-001.json',
              external/'audible-osc-ring/source-review.md',external/'audible-osc-ring/oscillator-implementation-review.json',
              external/'ring-carrier-nonlinearity-20261019/source-audit.md',external/'ring-carrier-nonlinearity-20261019/evidence.json',
              external/'ring-carrier-nonlinearity-20261019/implementation-review.json']
    local=[ROOT/'README.md',ROOT/'Docs/research/dsp-source.md',Path(__file__),DOCS/'README.md',DOCS/'release-final.log',DOCS/'sanitize-final.log',
           DOCS/'ring-alias-final.json',DOCS/'performance/comparison.json']
    requirements=[
      dict(requirement='Research audible oscillator/filter/envelope/ring omissions against primary documentation.',evidence='Yamaha service/IC diagrams plus original Fairchild equivalent circuit; source audits and independent review bindings.',status='proven'),
      dict(requirement='Implement the supported audible gaps in standalone circuit DSP without sample playback.',evidence='Two runtime source files, four integrated circuit groups, physical source/RC/thermal equations,33 native renders; no sample input or GUI in runtime build.',status='proven'),
      dict(requirement='Verify implementation and expose audible consequences.',evidence='12 Release and6 targeted ASan/UBSan entries; native/embedded and allocation tests; fixed-gain before/after scenes; static controls and declared bypass parity.',status='proven'),
      dict(requirement='Evaluate simulation accuracy without replacing the objective with a fitted surrogate.',evidence='Unchanged8-case public plan,45 inputs,16 recomputed metric trees, unchanged gains/gates/masks, retained adverse/mixed results. No broad hardware-match claim.',status='proven'),
      dict(requirement='Preserve the experiment scope and account for unidentified circuit terms.',evidence='Source remains in /tmp; existing reports and parents retained; result lists uncalibrated VCO shaping, chip/source loads, EG idle rate, Q scale, ring audio/DC/gain. These are model limitations, not claimed implemented calibrations.',status='proven')]
    report=dict(status='pass',scope='Completed audible-gap research/implementation/evaluation experiment. This does not certify an exact or fully calibrated CS-50 clone.',
                implementation_commit='28ddadca6b546f2f81b18adc2d060449214c4da3',compiled_source_sha256=identity,
                renderer_sha256=sha(binary),release_tests=12,targeted_asan_ubsan_tests=6,asan_options='detect_leaks=0',
                native_render_count=33,component_native_render_count=25,public_recording_native_render_count=8,
                parameter_key_count=57,parameter_changes='Existing reset_microseconds upper engineering bound50->60 and default6->59.7286us; no added keys.',
                no_runtime_sample_playback=True,requirements=requirements,
                component_manifest_sha256=component_manifests,native_wav_sha256=audio_bindings,
                verified_file_sha256={str(p):sha(p) for p in evidence+local},
                accuracy='Small mixed changes. Raw factory Flute remains19-22dB spectral ratio error; no known-settings/original-MIDI holdout; public ring path untested.',
                final_listening=str(STUDY/'comparison-circuits.wav'))
    (DOCS/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS combined experiment:',render_count,'native WAVs;',len(evidence)+len(local),'report/source bindings;',len(identity),'compiled inputs;',len(requirements),'requirements')
if __name__=='__main__':main()
