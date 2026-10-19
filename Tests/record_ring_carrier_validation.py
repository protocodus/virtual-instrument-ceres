#!/usr/bin/env python3
"""Bind completed ring-carrier validation; no test/audio reruns."""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Docs/ring-carrier'
PARENT='d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    identity=json.loads(subprocess.check_output([str(ROOT/'build/ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    parity=json.loads((OUT/'parent-parity.json').read_text())
    assert parity['status']=='pass' and parity['parent_commit']==PARENT and parity['samples_per_variant']==768000
    assert parity['compiled_source_sha256']==identity
    for name,digest in parity['verified_file_sha256'].items():assert sha(ROOT/name)==digest,name
    for log,count in [('release.log',8),('sanitize.log',3)]:assert f'100% tests passed, 0 tests failed out of {count}' in (OUT/log).read_text(),log
    def table(source):return source.split('const Parameter parameters[] = {',1)[1].split('\n};',1)[0]
    parent=subprocess.check_output(['git','show',PARENT+':Source/DSP/ceres.cpp'],cwd=ROOT,text=True)
    assert table(parent)==table((ROOT/'Source/DSP/ceres.cpp').read_text())
    keys=re.findall(r'\{"([a-z_]+)",',table(parent));assert len(keys)==57
    code=['CMakeLists.txt','Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','Tests/RingCarrierTests.cpp',
          'Tests/ParityDriver.cpp','Tests/check_ring_bypass_parent.py','Tests/record_ring_carrier_validation.py']
    diff=subprocess.check_output(['git','diff',PARENT,'--binary','--unified=0','--',*code],cwd=ROOT);assert diff
    (OUT/'source.patch').write_bytes(diff)
    audit=Path('/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-osc-ring')
    assert sha(audit/'evidence.json')=='afa33dd2d74d3352167a00d28d00d1134ab173cfcba52f738cf647a146f3286f'
    # Bind the exact source-only circuit oracle; do not rerun hardware fits.
    expected=json.loads((audit/'passive-carrier-result.json').read_text())
    actual=json.loads(subprocess.check_output(['python3',str(audit/'passive_carrier.py')],text=True))
    assert expected==actual,'Frozen passive oracle differs'
    files=[ROOT/p for p in code]+[ROOT/'Docs/DSP_SOURCE.md']+[OUT/p for p in [
        'README.md','source.patch','configure.log','build.log','release.log','sanitize-configure.log',
        'sanitize-build.log','sanitize.log','parent-parity.json','parent-parity.log']]
    files += [audit/p for p in ['evidence.json','source-review.md','passive_carrier.py','passive-carrier-result.json',
        'evidence/pra-ring-carrier-bias.png','evidence/guide-ring-carrier-62.png']]
    files += [Path('/tmp/ceres-cs50-1x8LGd/research')/p for p in ['service2.pdf','cs-servicing-guide.pdf']]
    binaries=[ROOT/'build'/p for p in ['ceres_render','libceres.a','libceres_embedded.a','ceres_ring_carrier_tests','ceres_native_parity','ceres_embedded_parity']]
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'parent_commit':PARENT,
        'compiled_source_sha256':identity,'release_ctest_passed':8,'asan_ubsan_ctest_passed':3,'asan_options':'detect_leaks=0',
        'embedded_keys':57,'parameter_table_byte_identical':True,'parent_bypass_audio_exact_samples':768000,
        'native_embedded_active_parity':'Existing native/embedded, active-soundpath and expression 384000-sample comparisons passed; ring uses multiple rates/mixes/depths.',
        'circuit_tests':{'processing_rates_hz':[8000,192000],'frequencies_hz':[.2,1,6.6692178847,30,220],
            'max_allowed_complex_response_error':2e-10,'max_allowed_phase_error_rad':2e-8,'impulse_tail_limit':1e-14,
            'independent_transient_rates_hz':[32000,192000],'physical_state_max_error':5e-13,'kcl_residual_limit':8e-15},
        'scope':'Four passive 100uF carrier capacitors only, ideal source / zero incremental IC current; no frequency normalization, oscillator/envelope/parameter/UI change or recording fit. Explicit panic clears coupling but preserves phase.',
        'allocation_check':'Existing embedded all 57-key contract passed with zero post-construction new/new[] allocations.',
        'binary_sha256':{str(p.relative_to(ROOT)):sha(p) for p in binaries},
        'verified_file_sha256':{str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):sha(p) for p in files}}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,digest in report['verified_file_sha256'].items():assert sha(ROOT/name)==digest,name
    print('PASS',len(report['verified_file_sha256']),'bindings; renderer',sha(ROOT/'build/ceres_render'))
if __name__=='__main__':main()
