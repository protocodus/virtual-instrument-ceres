#!/usr/bin/env python3
"""Independent stored-evidence audit only; no producer imports/audio decoding."""
from pathlib import Path
import hashlib, itertools, json, math, struct

ROOT=Path('/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018')
BASE=ROOT/'Docs/accuracy'; OUT=BASE/'pitch-route'; RUN=OUT/'run-001'
PLAN_SHA='ed455b13daf60509851c856c973d9d2029c80b481c61161975a34ae28bb0d2d7'
REPORT_SHA='edf435a7ece847beb3f052c7ad2d658c7e87e49d31983cf57f1766b97db112eb'
OUTPUT=OUT/'review-001/report.json'

def path(s):
    p=Path(s); return p if p.is_absolute() else ROOT/p

def read(p): return json.loads(Path(p).read_text())
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def patch(p):
    result={}
    for line in Path(p).read_text().splitlines():
        line=line.split('#',1)[0].strip()
        if line:
            key,value=line.split('=');assert key.strip() not in result
            result[key.strip()]=float(value)
    return result

def wav_identity(p):
    """Parse integer container metadata and hash raw data bytes; no sample decode."""
    data=Path(p).read_bytes()
    assert data[:4]==b'RIFF' and data[8:12]==b'WAVE'
    assert struct.unpack_from('<I',data,4)[0]+8==len(data)
    offset=12;fmt=None;payload=None
    while offset<len(data):
        tag=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0]
        chunk=data[offset+8:offset+8+size];assert len(chunk)==size
        if tag==b'fmt ':assert fmt is None;fmt=struct.unpack_from('<HHIIHH',chunk)
        if tag==b'data':assert payload is None;payload=chunk
        offset+=8+size+(size&1)
    assert offset==len(data) and fmt is not None and payload is not None
    encoding,channels,rate,byte_rate,align,bits=fmt
    assert (encoding,channels,align,bits)==(3,1,4,32) and byte_rate==4*rate
    assert len(payload)%4==0
    return dict(file_sha256=hashlib.sha256(data).hexdigest(),
                data_sha256=hashlib.sha256(payload).hexdigest(),
                data_bytes=len(payload),frames=len(payload)//4,rate=rate,
                encoding='IEEE float32 mono; bytes hashed without decoding')

assert not OUTPUT.exists(),'Review output immutable; preserve prior run'
assert sha(OUT/'plan.json')==PLAN_SHA and sha(RUN/'report.json')==REPORT_SHA
p=read(OUT/'plan.json');report=read(RUN/'report.json')
assert report['plan_sha256']==PLAN_SHA and report['input_sha256']==p['input_sha256']
assert report['status']=='completed_diagnostic_not_promotion'
verified={}
for group in (p['input_sha256'],report['artifact_sha256']):
    for name,digest in group.items():
        resolved=path(name).resolve();assert sha(resolved)==digest,name
        verified[str(resolved)]=digest
assert len(p['input_sha256'])==104 and len(report['artifact_sha256'])==76
assert sha(RUN/'declaration.json')==PLAN_SHA
assert sha(RUN/'producer.py')==sha(ROOT/'Tools/accuracy/pitch_route.py')
original={c['id']:c for c in read(BASE/'plan.json')['cases']}
raw=('flute-high-factory-physical','flute-lower-factory-physical')
contexts=('frozen','network-br0','network-brminus15');routes=('shared','vco-only','static-master')
assert p['contexts']==list(contexts) and p['routes']==list(routes)
assert p['fixed_calibration']==[0.,0.,1.] and p['common_master_semitones']==.367919921875
assert p['native_render_count']==report['native_render_count']==24
assert p['known_byte_exact_reference_count']==report['known_byte_exact_reference_count']==11
assert p['score_only_after_all_renders'] and not p['production_adoption']
assert p['gain_fits']==p['parameter_fits']==p['source_event_or_MIDI_changes']==0
assert len(p['cases'])==len({c['id'] for c in p['cases']})==24
assert len({c['audio'] for c in p['cases']})==24
assert set(RUN.glob('*.wav'))=={path(c['audio']) for c in p['cases']}

renderer=p['renderer'];assert sha(path(renderer['binary']))==renderer['binary_sha256']
for name,digest in renderer['compiled_source_sha256'].items():
    assert sha(path(renderer['source_root'])/name)==digest
assert len(renderer['compiled_source_sha256'])==5
statefiles=p['state_invariance_evidence'];assert len(statefiles)==7
for name in statefiles:assert str(path(name)) in p['input_sha256']
validation=read(path(statefiles[0]))
assert sha(path(statefiles[0]))==renderer['implementation_validation_sha256']
assert validation['compiled_sources']==renderer['compiled_source_sha256']
assert validation['checks']['release_ctest_passed']==validation['checks']['release_ctest_total']==11
assert validation['checks']['targeted_asan_ubsan_passed']==validation['checks']['targeted_asan_ubsan_total']==3
for name in statefiles[1:]:
    marker='/implementation/';rel=name.split(marker,1)[1]
    assert sha(path(name))==validation['input_sha256'][rel]

baseline={c['id']:c for c in read(BASE/'runs/expression-55869df8/report.json')['cases']}
jointplan=read(BASE/'joint-calibration/plan.json')
jointrows={c['id']:c for c in read(BASE/'joint-calibration/native-controls-001/report.json')['cases']}
expectedrefs={}
for oid,c in baseline.items():expectedrefs[(oid,'frozen')]=(str(path(c['audio'])),c['audio_sha256'])
for c in jointplan['native_controls']['cases']:
    if c['kind']=='unfitted' and c['original_case']['id'] in raw:
        saved=jointrows[c['id']];assert path(c['audio'])==path(saved['audio'])
        expectedrefs[(c['original_case']['id'],'network-br0')]=(str(path(saved['audio'])),saved['audio_sha256'])
br=read(BASE/'brightness-boundary/qualify-001/report.json');assert br['passed']
anchor=br['native_anchor_case'];assert anchor['theta']==[0.,0.,1.]
audio=str(path(anchor['audio']));dig=br['input_sha256'].get(audio,br['artifact_sha256'].get(audio))
expectedrefs[(raw[0],'network-brminus15')]=(audio,dig)
assert len(expectedrefs)==11

rows=report['analysis']['cases'];assert len(rows)==24
byid={c['id']:c for c in rows};assert len(byid)==24
receipts=report['render_receipts'];assert len(receipts)==24
complete=read(RUN/'all-renders-complete.json')
assert complete['render_count']==24 and complete['known_byte_exact_pass_count']==11
assert complete['hardware_scores_computed'] is False
triples=set();compat=[];known=0;wavids={};receipt_hashes={};max_energy_error=0.
for c,receipt in zip(p['cases'],receipts):
    oid=c['original_case']['id'];o=original[oid]
    assert c['original_case']==o and c['id']==receipt['id']
    assert c['command']==receipt['command'] and c['audio']==receipt['audio']
    assert c['patch_sha256']==sha(path(c['patch']))
    args=dict(zip(c['command'][1::2],c['command'][2::2]));assert len(args)*2+1==len(c['command'])
    assert c['command'][0]==renderer['binary']
    for key,val in [('preset',o['preset']),('patch',c['patch']),('events',str(path(o['events']))),
                    ('output',c['audio']),('rate',str(o['rate'])),('frames',str(o['frames'])),
                    ('seed',str(o['seed'])),('research-pitch-route',c['route'])]:
        assert args['--'+key]==val,(c['id'],key)
    if c['route']=='static-master':assert args['--research-master-semitones']=='0.367919921875'
    else:assert '--research-master-semitones' not in args
    if c['context']=='frozen':assert path(c['patch'])==path(o['patch'])
    else:
        assert patch(path(c['patch']))==dict(octave=-1.,calibration_network=1.,calibration_hp_bias=0.,
             calibration_lp_bias=0.,calibration_alpha=1.,brilliance=0. if c['context']=='network-br0' else -15.)
    if c['role']=='matrix':triples.add((oid,c['context'],c['route']))
    else:compat.append(oid);assert c['role']=='compatibility' and c['context']=='frozen' and c['route']=='shared'
    row=byid[c['id']]
    for k in ('id','role','context','route'):assert row[k]==c[k]
    assert row['original_id']==oid
    rp=RUN/(c['id']+'-render.json');assert read(rp)==receipt
    receipt_hashes[str(rp)]=sha(rp)
    assert complete['receipts_sha256'][str(rp)]==sha(rp)
    log=path(receipt['log']);assert sha(log)==receipt['log_sha256']
    first,rest=log.read_text().split('\nSTDOUT\n',1);stdout,stderr=rest.split('\nSTDERR\n',1)
    assert json.loads(first)==c['command'] and json.loads(stdout)==receipt['statistics'] and not stderr.strip()
    stats=receipt['statistics'];sig=row['signal']
    assert stats['research_pitch_route']==c['route']
    assert stats['research_master_semitones']==(p['common_master_semitones'] if c['route']=='static-master' else 0)
    assert stats['frames']==o['frames'] and stats['sample_rate']==o['rate'] and stats['seed']==o['seed']
    assert stats['preset']==o['preset'] and stats['channels']==1 and stats['sample_format']=='float32'
    assert stats['normalization']=='none' and stats['samples_above_unity']==sig['samples_above_unity']==0
    assert sig['finite'] and sig['peak']==receipt['peak'] and 0<sig['peak']<=1
    assert math.isclose(stats['peak'],sig['peak'],rel_tol=5e-12,abs_tol=5e-12)
    assert math.isclose(stats['rms'],sig['rms'],rel_tol=5e-12,abs_tol=5e-12)
    energy_error=abs(sig['energy']-o['frames']*sig['rms']**2)
    assert energy_error<=1e-10*max(1.,sig['energy']);max_energy_error=max(max_energy_error,energy_error)
    w=wav_identity(path(c['audio']));wavids[c['id']]=w
    assert w['rate']==o['rate'] and w['frames']==o['frames'] and w['file_sha256']==receipt['audio_sha256']
    ref=c['byte_exact_reference'];assert ref==receipt['byte_exact_reference']
    expected=expectedrefs.get((oid,c['context'])) if c['route']=='shared' else None
    if expected:
        assert (ref['audio'],ref['sha256'])==expected
        rw=wav_identity(path(ref['audio']));assert rw==w and receipt['byte_exact_pass'] is True;known+=1
    else:assert ref is None and receipt['byte_exact_pass'] is None
assert triples==set(itertools.product(raw,contexts,routes)) and known==11
assert len(compat)==6 and set(compat)==set(original)-set(raw)
assert receipt_hashes==complete['receipts_sha256']

analysis=report['analysis'];arithmetic_checks=0;differences=[];flags=[]
for field in ('aggregate_winner_selected','production_default_changed','physical_route_identified',
              'strict_eligible','user_fidelity_goal_proven_complete'):assert analysis[field] is False
for field in ('optimizer_calls','parameter_fits','gain_fits'):assert analysis[field]==0
for category,count in [('altered_vs_matched_shared',12),('static_master_vs_vco_only',6)]:
    contrasts=analysis[category];assert len(contrasts)==count
    expectedpairs=set()
    if category=='altered_vs_matched_shared':
        for oid,context,route in itertools.product(raw,contexts,('vco-only','static-master')):
            expectedpairs.add((f'{oid}--{context}--{route}',f'{oid}--{context}--shared'))
    else:
        for oid,context in itertools.product(raw,contexts):
            expectedpairs.add((f'{oid}--{context}--static-master',f'{oid}--{context}--vco-only'))
    assert {(z['case'],z['control']) for z in contrasts}==expectedpairs
    for z in contrasts:
        a=byid[z['case']]['metrics'];b=byid[z['control']]['metrics'];assert set(a)==set(b)
        expected={k:a[k]-b[k] for k in a if type(a[k]) in (float,int)}
        assert z['metric_difference']==expected;arithmetic_checks+=len(expected)
        spectral=a['median_harmonic_MAE_db']<b['median_harmonic_MAE_db'] and a['trajectory_ratio_MSE_db2']<b['trajectory_ratio_MSE_db2']
        nr=spectral and a['held_gain_RMS_error_db']<=b['held_gain_RMS_error_db']
        assert z['canonical_contrast']==dict(spectral_improves=spectral,no_regression=nr);arithmetic_checks+=2
        wa=wavids[z['case']];wb=wavids[z['control']];wd=z['waveform_difference']
        assert wd['bytes_identical']==(wa['file_sha256']==wb['file_sha256'])
        assert wd['samples_identical']==(wa['data_sha256']==wb['data_sha256'])
        assert wd['frames']==wa['frames']==wb['frames'] and wd['sample_rate']==wa['rate']==wb['rate']
        assert wd['gain_fit'] is False and 0<=wd['different_samples']<=wd['frames']
        assert 0<=wd['RMS_difference']<=wd['max_absolute_difference']
        assert wd['max_absolute_difference']<=byid[z['case']]['signal']['peak']+byid[z['control']]['signal']['peak']
        if category=='static_master_vs_vco_only':assert z['constant_key_exponent_difference']==p['common_master_semitones']/12
        flags.append(dict(category=category,case=z['case'],control=z['control'],spectral_improves=spectral,no_regression=nr))
        differences.append(dict(category=category,case=z['case'],control=z['control'],metric_difference=expected))

summary=[]
for context,route in itertools.product(contexts,('vco-only','static-master')):
    group=[z for z in flags if z['category']=='altered_vs_matched_shared' and
           byid[z['case']]['context']==context and byid[z['case']]['route']==route]
    assert len(group)==2
    summary.append(dict(context=context,route=route,spectral_both_passages=all(z['spectral_improves'] for z in group),
                        no_regression_both_passages=all(z['no_regression'] for z in group)))
result=dict(status='pass',kind='independent-stored-identity-arithmetic-review',
    plan_sha256=PLAN_SHA,report_sha256=REPORT_SHA,producer_sha256=sha(__file__),
    unique_verified_bound_files=len(verified),input_bindings=104,artifact_bindings=76,
    native_render_count=24,matrix_cases=18,compatibility_cases=6,exact_reference_files_and_payloads=11,
    native_state_evidence_bindings=7,compiled_source_bindings=5,render_log_checks=24,
    metric_difference_and_contrast_scalar_checks=arithmetic_checks,maximum_scalar_arithmetic_difference=0.,
    maximum_stored_energy_identity_roundoff=max_energy_error,paired_contrasts=flags,paired_metric_differences=differences,
    paired_passage_summary=summary,wav_container_and_payload_identities=wavids,
    source_event_MIDI_gain_changes=0,optimizer_calls=0,parameter_fits=0,new_render_count=0,
    source_target_measurements=0,audio_decodes=0,model_calls=0,producer_modules_imported=False,
    numerical_replay_reviewed=False,
    evidence_scope=['Full-file/payload hashes and integer WAV headers only; no sample decoding or remeasurement.',
      'Metric differences/decision flags recomputed solely from stored metrics. Metric estimates themselves not independently rerun.',
      'Render-first ordering supported by pinned runner plus all-renders receipt and complete hashes, not by an independent process trace.',
      'Native state tests are retained author evidence; reviewer verified bindings and code without rerunning.',
      'No optimizer, aggregate winner selection, route identification, new holdout, or production promotion.'])
OUTPUT.write_text(json.dumps(result,indent=2,sort_keys=True,allow_nan=False)+'\n')
print('PASS',OUTPUT)
print('SHA256',sha(OUTPUT))
print('SCALAR_CHECKS',arithmetic_checks,'VERIFIED_FILES',len(verified))
print(json.dumps(summary,indent=2))
