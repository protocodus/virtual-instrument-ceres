# Native performance observation

This first-run probe uses the embedded DSP ABI on an Apple development machine, four held voices, 48 kHz, 64-sample blocks and the fixed 4x oversampling path. After 500 warmup blocks, it measures 10,000 blocks per workload with paired monotonic wall and thread-CPU clocks. Exact source and compiler hashes, commands and machine identity are in `report.json`.

| Workload | Median thread CPU | p99 thread CPU | Maximum wall time | Wall deadline misses | CPU deadline misses |
|---|---:|---:|---:|---:|---:|
| Baseline engineering Init | 0.208 ms | 0.277 ms | 0.880 ms | 0 | 0 |
| Candidate engineering Init | 0.212 ms | 0.343 ms | 3.990 ms | 2 | 0 |
| Candidate active sound paths | 0.219 ms | 0.326 ms | 1.319 ms | 0 | 0 |

The deadline is 1.333 ms. Candidate default median thread cost increased about 1.8%. The two default wall overruns are retained; maximum thread CPU in that run was 0.565 ms. Other task work was running, and this is not a real-time scheduling guarantee. No retry was used to hide those misses. Native DSP excludes the Rack wrapper, target translation and Reason scheduling; shipping host CPU performance remains unmeasured.
