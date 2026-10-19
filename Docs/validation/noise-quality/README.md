# Audio-noise density across processing qualities

This corrects a regression in the unreleased 0.2.0 configuration extension. With the same uniform random amplitude at every internal rate, lowering 4× to 1× increased in-band audio-noise density by approximately 6 dB (2× by 3 dB). No preset, gain trim, filter setting or hardware calibration is fitted here.

For a uniform sample in `[-1,1]`, variance is `1/3`. At host rate `F`, the original 4× source has two-sided white density `(1/3)/(4F) = 1/(12F)`. At factor `m`, multiplying the audio sample by `sqrt(m/4)` gives variance `m/12`, hence the same density `(m/12)/(mF)`. Factors 1, 2 and 4 therefore use amplitudes .5, sqrt(.5) and exactly 1. This preserves the existing host-rate convention; it does not establish equal noise level between different host sample rates.

The multiplier is applied only to the audio NO input of the waveform mixer. The 4× branch returns the raw sample directly. SUB-N still receives the original raw sample through its existing 22k/.1µF RC and independent RMS normalization. No random draws, drift logic, oscillator state or noise-free source terms change. Noise remains common to all active boards, as before. Nonlinear drive, filter discretization and output alias rejection can still produce differences between processing qualities; equal source density is not an assertion of universal output RMS equality.

`Tests/NoiseQualityTests.cpp` measures the complete low-drive engine after a one-second warmup, with 262,144 output samples per quality at 48 and 96 kHz. Its independent radix-2 FFT/Hann/Parseval calculation checks 200–800, 800–2,000 and 2,000–4,000 Hz band powers. An integer-bin sine verifies the estimator's normalization and band separation. The fixed .6 dB tolerance accommodates finite-sequence scatter and filter-discretization differences while rejecting the original 3/6 dB defect. Observed maximum difference from 4× was .265 dB. Peaks are not used to estimate noise density.

The complete Release suite, targeted ASan/UBSan, configured native/embedded audio and post-construction allocation checks cover the final source. Original default 4× audio is compared byte-for-byte with immutable `2ee6cf22deead7358d6b4ee7c294da955225af56` over 768,000 samples. Parent binaries and synthetic audio for this parity check are temporary; their hashes and reproduction commands are retained. The separate peer audit compares this change against immediate parent `652e3722eb40f67001f0467103f9f31a957ec363`, including noise-free SUB/drift scenes and unchanged control/oscillator traces.

Reproduce the local checks from the repository root:

```sh
python3 Tools/record_noise_quality_validation.py --run
```

This writes only this new validation directory. The earlier `poly-quality` reports, archived source and pre-ring CPU measurements remain historical and unchanged. No new timing, Reason SDK or host validation is claimed here. The unchanged product version remains the unreleased 0.2.0 draft.

## Linux test-header correction

The preceding PR's Linux Release and sanitizer builds failed because `Tests/EmbeddedContract.cpp` used brace-list range loops without explicitly including `<initializer_list>`. The compiler reported this at lines77 and97; macOS's transitive headers had hidden the omission. The exact failure logs are retained as `prior-linux-ci.log` and `prior-sanitizer-ci.log`. The new configuration/noise/ring test files and benchmark now include the required standard header explicitly. No assertion or test is disabled or weakened. The local checks below cover the corrected sources; a fresh Linux CI run remains a separate result.
