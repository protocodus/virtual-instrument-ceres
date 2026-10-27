#!/usr/bin/env python3
"""Bind completed physical ring-steering checks; no test/audio reruns."""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Docs/validation/ring-switching-2026-10-19'
PARENT='8c5a96e75f426dd7854cfee8e76cccd9478bbbfe'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    identity=json.loads(subprocess.check_output([str(ROOT/'build/ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    parity=json.loads((OUT/'parent-parity.json').read_text());alias=json.loads((OUT/'alias.json').read_text())
    assert parity['status']=='pass' and parity['parent_commit']==PARENT and parity['samples_per_variant']==768000
    assert alias['status']=='pass' and len(alias['comparisons'])==3
    for report in [parity,alias]:
        assert report['compiled_source_sha256']==identity
        for name,digest in report['verified_file_sha256'].items():assert sha(ROOT/name)==digest,name
    for log,count in [('release.log',9),('sanitize.log',4)]:assert f'100% tests passed, 0 tests failed out of {count}' in (OUT/log).read_text(),log
    def table(source):return source.split('const Parameter parameters[] = {',1)[1].split('\n};',1)[0]
    parent=subprocess.check_output(['git','show',PARENT+':Source/DSP/ceres.cpp'],cwd=ROOT,text=True)
    assert table(parent)==table((ROOT/'Source/DSP/ceres.cpp').read_text())
    keys=re.findall(r'\{"([a-z_]+)",',table(parent));assert len(keys)==57
    code=['CMakeLists.txt','Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','Tests/dsp_tests.cpp',
        'Tests/RingSwitchingTests.cpp','Tests/RingSwitchingAlias.cpp','Tests/check_ring_switching_alias.py',
        'Tests/check_ring_switching_parent.py','Tests/record_ring_switching_validation.py']
    # New files must first be added with git add -N (or staged), so the exact
    # immutable-parent diff is complete both before and after the final commit.
    for name in code:subprocess.run(['git','ls-files','--error-unmatch',name],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    patch=subprocess.check_output(['git','diff',PARENT,'--binary','--unified=0','--',*code],cwd=ROOT);assert patch
    (OUT/'source.patch').write_bytes(patch)
    audit=Path('/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/ring-carrier-nonlinearity-20261019')
    assert sha(audit/'evidence.json')=='d023d975ce4edaa5ee6adc32f04914b66424e486f6e0431520d0dfdf2dbba030'
    evidence=json.loads((audit/'evidence.json').read_text())
    for name,digest in evidence['verified_sha256'].items():assert sha(name)==digest,name
    constants=evidence['conditional_constants']
    assert constants['carrier_dc_volts']==-.023489437180096218 and constants['source_peak_volts']==1.5
    files=[ROOT/p for p in code]+[ROOT/'Docs/research/dsp-source.md',ROOT/'Tests/RingCarrierTests.cpp',ROOT/'Tests/ParityDriver.cpp',ROOT/'Tests/EmbeddedContract.cpp']
    files+=[OUT/p for p in ['README.md','source.patch','configure.log','build.log','release.log','sanitize-configure.log',
        'sanitize-build.log','sanitize.log','parent-parity.json','parent-parity.log','alias.json','alias.log']]
    files+=[audit/p for p in ['evidence.json','source-audit.md','ElectronicSynth.pdf','fairchild-12.png',
        'yamaha-pwm-49.png','dc_nominal.py','dc-nominal.json']]
    binaries=[ROOT/'build'/p for p in ['ceres_render','libceres.a','libceres_embedded.a','ceres_tests',
        'ceres_ring_carrier_tests','ceres_ring_switching_tests','ceres_native_parity','ceres_embedded_parity']]
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'parent_commit':PARENT,
        'compiled_source_sha256':identity,'release_ctest_passed':9,'asan_ubsan_ctest_passed':4,'asan_options':'detect_leaks=0',
        'parameter_keys':57,'parameter_table_byte_identical':True,'parent_bypass_audio_exact_samples':768000,
        'native_embedded_active_parity':'Native/embedded, active-soundpath and expression384000-sample comparisons passed, including multiple active ring rates/mixes/depths.',
        'physical_constants':constants,
        'tests':['Independent exponential BJT current ratio and small-signal1/(2VT) derivative',
            'Independent full6node DC KCL including signal-input returns',
            'Production raw/passive/physical steering connection, mix0 history, notes/live controls, panic/reset',
            'Explicit nominal DC feedthrough/no mean subtraction,0.2Hz no-crossing boundary',
            'Actual fully wet0th/2nd/3rd carrier sidebands versus independent passive Schur plus exponential-current Fourier integral',
            'Existing passive complex AC/physical-state trapezoids and DSP/allocation contracts'],
        'periodic_alias':{'processing_hz':192000,'reference_hz':3072000,'carrier_hz':220,'audio_product_hz':[0,440,15000],
            'band_hz':[0,24000],'max_rms_difference':max(c['sampled_vs_reference_band_error_rms'] for c in alias['comparisons']),
            'scope':'Stationary physical-carrier kernel and linear signal products only; excludes dynamic/full-synth/FIR/output nonlinearities. Reference above24k energy reported separately.'},
        'allocation_check':'Existing embedded57-key contract passes zero post-construction new/new[] allocations.',
        'scope':'Physically scaled matched upper-pair steering after existing passive network. Fixed25C/nominal3Vpp ideal source, zero all-IC DC loads and incremental carrier input current. No current/gain/temperature fit, normalization, new parameter, GUI, clock/envelope change or hardware-recording render. Signal-port nonlinearization/loading and absolute wet gain remain uncalibrated.',
        'binary_sha256':{str(p.relative_to(ROOT)):sha(p) for p in binaries},
        'verified_file_sha256':{str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):sha(p) for p in files}}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,digest in report['verified_file_sha256'].items():assert sha(ROOT/name)==digest,name
    print('PASS',len(report['verified_file_sha256']),'bindings; renderer',sha(ROOT/'build/ceres_render'))
if __name__=='__main__':main()
