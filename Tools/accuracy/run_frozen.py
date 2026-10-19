#!/usr/bin/env python3
"""Retain a renderer/source snapshot, then invoke the unchanged frozen battery."""
from pathlib import Path
import argparse
import re
import shutil
import run_battery as b


def run(label, binary, source):
    if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_-]*', label):
        raise ValueError('Use a simple, unique run label')
    b.verify_plan()
    binary, source = binary.resolve(), source.resolve()
    identity = b.read_identity(binary, source)
    digest = b.sha(binary)
    snapshot = b.BASE / 'run-identities' / label
    if snapshot.exists() or (b.BASE / 'runs' / label).exists():
        raise FileExistsError('Refuse to reuse a completed or unfinished run')
    snapshot.mkdir(parents=True)
    for name, expected in identity.items():
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        assert b.sha(target) == expected, name
    retained = snapshot / 'ceres_render'
    shutil.copy2(binary, retained)
    assert b.sha(retained) == digest
    assert b.read_identity(retained, snapshot) == identity
    b.save(snapshot / 'identity.json', dict(
        origin_binary=str(binary), origin_source_root=str(source),
        binary_sha256=digest, compiled_source_sha256=identity,
        wrapper_sha256=b.sha(__file__), battery_sha256=b.sha(b.__file__),
        plan_sha256=b.sha(b.BASE / 'plan.json')))
    b.run(label, retained, snapshot)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('label')
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    args = parser.parse_args()
    run(args.label, args.binary, args.source_root)
