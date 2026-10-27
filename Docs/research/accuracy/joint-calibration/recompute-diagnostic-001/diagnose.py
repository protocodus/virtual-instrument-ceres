#!/usr/bin/env python3
"""Capture unchanged check() calculation and recursive exact differences."""
from contextlib import redirect_stdout
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import platform
import sys
import traceback

import numpy as np
import scipy

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
BASE=HERE.parent
ATTEMPT=BASE/'attempt-001'
HELPER=ROOT/'Tools/accuracy/joint_calibration.py'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(name,value):
    path=HERE/name
    assert not path.exists(),'Do not overwrite '+str(path)
    path.write_text(json.dumps(value,indent=2,sort_keys=True,allow_nan=False)+'\n')


def diff(expected,actual,path='$'):
    if type(expected)!=type(actual):
        return [dict(path=path,kind='type',expected_type=type(expected).__name__,
                     actual_type=type(actual).__name__,expected=expected,actual=actual)]
    if isinstance(expected,dict):
        result=[]
        for key in sorted(set(expected)|set(actual)):
            if key not in expected or key not in actual:
                result.append(dict(path=path+'.'+key,kind='key',
                    expected_present=key in expected,actual_present=key in actual))
            else:
                result+=diff(expected[key],actual[key],path+'.'+key)
        return result
    if isinstance(expected,list):
        result=[]
        if len(expected)!=len(actual):
            result.append(dict(path=path,kind='length',expected=len(expected),actual=len(actual)))
        for i,(a,b) in enumerate(zip(expected,actual)):
            result+=diff(a,b,path+f'[{i}]')
        return result
    if expected==actual:
        return []
    value=dict(path=path,kind='value',expected=expected,actual=actual)
    if isinstance(expected,(int,float)) and not isinstance(expected,bool):
        value.update(absolute_difference=abs(actual-expected),
                     relative_difference=abs(actual-expected)/max(abs(expected),1e-300))
    return [value]


def main():
    assert sha(HELPER)==sha(ATTEMPT/'joint_calibration.py')
    saved=json.loads((ATTEMPT/'report.json').read_text())
    stream=io.StringIO()
    with redirect_stdout(stream):
        np.__config__.show()
    try:
        import threadpoolctl
        pools=threadpoolctl.threadpool_info()
    except ImportError:
        pools='threadpoolctl unavailable; no package installed for diagnostic'
    runtime=dict(python=sys.version,numpy=np.__version__,scipy=scipy.__version__,
        platform=platform.platform(),machine=platform.machine(),cpu_count=os.cpu_count(),
        numpy_configuration=stream.getvalue(),threadpools=pools,
        thread_environment={key:os.environ.get(key) for key in [
            'OPENBLAS_NUM_THREADS','OMP_NUM_THREADS','OMP_DYNAMIC','OMP_PROC_BIND',
            'MKL_NUM_THREADS','MKL_DYNAMIC','VECLIB_MAXIMUM_THREADS','NUMEXPR_NUM_THREADS']})
    write('declaration.json',dict(purpose=__doc__,runtime=runtime,
        input_sha256={str(path):sha(path) for path in [Path(__file__),HELPER,
            BASE/'plan.json',ATTEMPT/'report.json',ATTEMPT/'declaration.json']},
        pinned_inputs_mutated=False,tolerances_changed=False,fits=0,new_audio_renders=0,
        action='Call original check(), capture its returned calculation object without altering it.'))
    spec=importlib.util.spec_from_file_location('frozen_joint_calibration',HELPER)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original=module.qualify_calculation
    captured=[]
    def capture(*args,**kwargs):
        actual=original(*args,**kwargs)
        captured.append(actual)
        return actual
    module.qualify_calculation=capture
    exception=None
    try:
        module.check('attempt-001')
    except BaseException:
        exception=traceback.format_exc()
    assert len(captured)==1,'Failure before completed calculation: '+str(exception)
    actual=captured[0]
    write('actual.json',actual)
    differences=diff(saved,actual)
    write('exact-diff.json',differences)
    numeric=[d for d in differences if 'absolute_difference' in d]
    nonnumeric=[d for d in differences if 'absolute_difference' not in d]
    summary=dict(status='exact_match' if not differences else 'exact_difference',
        check_exception=exception,changed_leaf_count=len(differences),
        numeric_changed_leaf_count=len(numeric),nonnumeric_changed_leaf_count=len(nonnumeric),
        first_difference=differences[0] if differences else None,
        maximum_numeric_difference=max(numeric,key=lambda d:d['absolute_difference']) if numeric else None,
        nonnumeric_differences=nonnumeric,
        saved_qualification_status=saved['status'],actual_qualification_status=actual['status'],
        source_helper_sha256=sha(HELPER),original_report_sha256=sha(ATTEMPT/'report.json'),
        actual_sha256=sha(HERE/'actual.json'),exact_diff_sha256=sha(HERE/'exact-diff.json'),
        declaration_sha256=sha(HERE/'declaration.json'),fits=0,new_audio_renders=0)
    write('summary.json',summary)
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':
    main()
