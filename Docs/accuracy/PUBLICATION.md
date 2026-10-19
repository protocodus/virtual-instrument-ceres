# Public research archive and local reproduction

19 October 2026. This publication retains authored experiment declarations, circuit calculations, analysis code, reconstructed event schedules and patches, numerical results, review notes, failures, and rejected candidates. It does not distribute third-party source media or install rejected experimental DSP changes in the production engine. The integrated audible-circuit implementation is recorded separately at `aec922d8aa87c1e6cdd9bc8a4b2fc06ef8659c4b`.

The [accuracy index](README.md) explains the experiments and their limitations. In particular, the full-network joint fit failed the native transfer criteria, the brightness-boundary fit exhausted its declared evaluation budget without a feasible point, and pitch-route results do not identify the physical modulation route. All corresponding authored reports and evaluation histories remain published, including large numerical JSON files. These are analysis results, not audio samples. The scientific plots are authored visualizations of those results.

## Files kept local

Scoped ignore rules exclude the original and cropped recordings, listening comparisons containing source recordings, videos and source screenshots; Yamaha and Fairchild document scans and crops; the full extracted Fairchild text; and downloaded DatasheetArchive HTML. Public accessibility is not treated as a redistribution license. Source URLs, page references, hashes, acquisition outcomes and the authors' interpretation remain in the evidence manifests. No local file was deleted or rewritten to prepare this publication.

Authored native demo WAVs are also omitted as rebuildable outputs. Their producers, fixed scene declarations, original gains, timing, segment metadata and output hashes are retained. Rendered executables, local source/binary identity directories, numerical trace arrays and the absolute `audible-circuits-20261019/public-frozen/inputs` symlink remain local. The symlink points at the repository's existing accuracy inputs and is not a portable artifact. Removing these files from the public payload does not change the original experimental evidence or its recorded hashes.

The original reference images and document text are not needed to read the authored conclusions, but are needed for independent visual source inspection. Obtain them from the cited primary sources as permitted, and check the recorded SHA-256 values. Do not substitute a similarly titled PDF, a different video transcode, or a newly cropped recording while claiming to reproduce an original frozen run.

## Reproduction boundaries

A clean checkout can inspect the declarations, code, scalar results and reviews. It cannot immediately rerun every historical verification command. Frozen plans intentionally retain their original absolute paths, source identities, compiler/runtime information and output hashes. They were not rewritten for publication. Their strict checks may require omitted source recordings, screenshots, binaries, generated WAVs, source snapshots and the original research archive at `/tmp/ceres-cs50-1x8LGd`.

The pending-report audit found 891 absolute keyed local file references, all present on the originating machine. Among them are 130 authored historical notes, numerical JSON files, source code and patches in that older `/tmp` archive, totaling about 5.47 MB; those external files are not implicitly supplied by this publication. The newly published studies preserve their identities and conclusions, but the referenced older archive is an additional dependency. Some dependencies are nested in other manifests. Counts describe the audited pending research, not every historical dependency in the repository.

Historical analysis helpers also pin earlier production source hashes. For example, `Tools/accuracy/calibration_identifiability.py` checks the original `Source/DSP/ceres.cpp` and header. Such a check is expected to fail against the later integrated engine. Use the recorded historical source identity in a separate checkout or restored evidence workspace; do not edit the hash to make a newer model appear to be the old one. `run_frozen.py` snapshots a renderer and its five compiled source inputs for a new run, but does not reconstruct all missing past artifacts.

Two isolated research engines are referenced by the new analysis:

| Research engine | Recorded commit | Role |
|---|---|---|
| Full-network cutoff calibration | `ff2dd42e71f545bd52e621bac896d1e46c132d53` | Native-only conditional calibration experiments, including rejected fits |
| Pitch-route diagnostic | `7e82a5ba212ebe5bf9cbf41967839a4aa3fbaf7d` | Native-only shared, oscillator-only and common-static-master comparisons |

These identifiers and the ignored `joint-calibration/identity/` and `pitch-route/identity/` directories preserve the original local evidence. A commit identifier alone does not guarantee that its object is reachable from public main. Reproduction requires the corresponding authored source snapshot or reachable historical commit, plus the exact dependencies identified by its plan. Their experimental equations must not be substituted into the current production DSP merely to make a historical runner work.

For local restoration, keep the original declarations unchanged, restore each missing input by its recorded hash, rebuild from the identified source and documented command, and use a new run directory. Rebuilt executable hashes can differ across compilers and platforms; that is a new build identity, not permission to claim byte identity with a historical binary. A separate relocation manifest may map an unchanged, hash-identical artifact to a new path. Preserve unsuccessful attempts and any numerical reproducibility limits when reporting a replay.

The synthetic circuit demonstrations and public-recording comparisons answer different questions. Fixed-setting differences and source-derived equations establish implementation behavior; they do not by themselves prove improved agreement with hardware recordings. The retained limitations, failed promotion gates and adverse results remain part of the public record.
