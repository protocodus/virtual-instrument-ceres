#!/usr/bin/env python3
"""Exercise benchmark source provenance with tiny repositories; no timing runs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('benchmark_cpu', Path(__file__).parents[1] / 'Tools/benchmark_cpu.py')
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


class BenchmarkSourceProvenance(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='ceres-provenance-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'repo'
        self.root.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Provenance Test')
        self.git('config', 'user.email', 'provenance@example.invalid')
        self.original = {}
        for index, relative in enumerate(BENCHMARK.INPUT_PATHS):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            content = ('// input ' + str(index) + '; UTF-8: č\r\n').encode('utf-8')
            path.write_bytes(content)
            self.original[relative] = content
        self.git('add', '.')
        self.git('commit', '-qm', 'Four clean benchmark inputs')
        self.parent = self.git('rev-parse', 'HEAD').strip()

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, text=True)

    def reconstruct(self, state):
        # A JSON roundtrip models a report recovered without the build directory.
        recovered = json.loads(json.dumps(state))
        destination = Path(self.temporary.name) / 'reconstructed'
        for relative, item in recovered['inputs'].items():
            path = destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            BENCHMARK.write_snapshot(path, item['text_utf8'].encode('utf-8'), item['sha256'])
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), item['sha256'])
        return destination

    def test_clean_inputs_identify_parent_and_reconstruct_exact_bytes(self):
        state = BENCHMARK.capture_candidate_state(self.root)
        self.assertEqual(state['parent_commit'], self.parent)
        self.assertTrue(state['inputs_match_parent_commit'])
        self.assertFalse(state['compiled_inputs_dirty'])
        self.assertEqual(state['status_porcelain_v1'], '')
        rebuilt = self.reconstruct(state)
        for relative, content in self.original.items():
            self.assertTrue(state['inputs'][relative]['matches_parent_commit'])
            self.assertEqual((rebuilt / relative).read_bytes(), content)

    def test_staged_and_unstaged_optimization_keeps_parent_and_frozen_contents(self):
        cpp, header = BENCHMARK.INPUT_PATHS[:2]
        candidate_cpp = self.original[cpp] + b'// staged optimization\n'
        (self.root / cpp).write_bytes(candidate_cpp)
        self.git('add', cpp)
        (self.root / header).write_bytes(self.original[header] + b'// unstaged cache\n')
        state = BENCHMARK.capture_candidate_state(self.root)
        self.assertEqual(state['parent_commit'], self.parent)
        self.assertTrue(state['compiled_inputs_dirty'])
        self.assertFalse(state['inputs_match_parent_commit'])
        self.assertIn('staged optimization', state['diff_from_parent'])
        self.assertIn('unstaged cache', state['diff_from_parent'])
        self.assertNotEqual(state['inputs'][cpp]['sha256'], state['inputs'][cpp]['parent_sha256'])
        # Later live edits must not change the compiler inputs reconstructed from the report.
        (self.root / cpp).write_bytes(b'// subsequent edit\n')
        rebuilt = self.reconstruct(state)
        self.assertEqual((rebuilt / cpp).read_bytes(), candidate_cpp)
        self.assertEqual(self.git('rev-parse', 'HEAD').strip(), self.parent)

    def test_untracked_driver_is_embedded_without_claiming_a_parent_blob(self):
        driver = BENCHMARK.INPUT_PATHS[-1]
        self.git('rm', '--cached', '-q', driver)
        self.git('commit', '-qm', 'Driver is not tracked yet')
        state = BENCHMARK.capture_candidate_state(self.root)
        item = state['inputs'][driver]
        self.assertTrue(state['compiled_inputs_dirty'])
        self.assertFalse(item['matches_parent_commit'])
        self.assertIsNone(item['parent_sha256'])
        self.assertIn('?? ' + driver, state['status_porcelain_v1'])
        self.assertEqual((self.reconstruct(state) / driver).read_bytes(), self.original[driver])

    def test_changed_compiler_snapshot_is_rejected_before_compilation(self):
        state = BENCHMARK.capture_candidate_state(self.root)
        rebuilt = self.reconstruct(state)
        report = {'sources': {'candidate': {}}, 'drivers': {}}
        for relative, item in state['inputs'].items():
            entry = {'sha256': item['sha256'], 'snapshot': str(rebuilt / relative)}
            target = report['drivers'] if relative.startswith('Tools/') else report['sources']['candidate']
            target[relative] = entry
        BENCHMARK.verify_frozen_inputs(report)
        (rebuilt / BENCHMARK.INPUT_PATHS[0]).write_bytes(b'// tampered snapshot\n')
        with self.assertRaisesRegex(RuntimeError, 'Frozen compiler input changed'):
            BENCHMARK.verify_frozen_inputs(report)


if __name__ == '__main__':
    unittest.main()
