# Source provenance correction for the frozen CPU measurements

The original [2026-10-23 timing report](cpu-efficiency-2026-10-23.json) recorded `candidate_head` as `1cad0810e17ce0a735c50fabf9f9bdd1a1594c6b`. That identifies the working-tree parent at measurement time. The optimized candidate was compiled from dirty working-tree files, so this field does not identify its compiled source revision.

The [separate verified annotation](cpu-efficiency-2026-10-23-source-state.json) matches both candidate core files, both timing drivers and the exact historical runner to commit `87cf835`. Every SHA-256 agrees with the frozen measurement report. Its per-file Git commands provide the raw bytes for independent hash verification. The original report remains byte for byte unchanged; no timing was rerun and no measurement was replaced.

New `Tools/benchmark_cpu.py` reports explicitly call HEAD `candidate_parent_commit`. They embed UTF-8 snapshots and hashes of all four compiled candidate inputs, the input-scoped Git status/diff and whether each input matches the parent commit. Compilation uses those frozen snapshots, verified before and after each compiler invocation. Dirty and untracked optimization inputs remain supported, and the report alone reconstructs their exact bytes even when the temporary build directory is removed.

The lightweight regression command is:

```sh
python3 -B Tests/test_benchmark_cpu_provenance.py
```

It checks clean, staged/unstaged and untracked input states, reconstruction through JSON with exact UTF-8/CRLF bytes, and rejection of a changed compiler snapshot. It performs no compilation or timing measurement.
