# Manual LP relative-response opportunity — source-only review

18 October 2026. No audio acquired or analyzed, no DSP edit, no render. Source video: https://www.youtube.com/watch?v=ivVAJ13Tzvc (AutomaticGainsay CS50 filter demonstration). Root supplied existing screenshots at `/Users/vojta/Dev/reason-rack-extension/.playwright-mcp/ceres-filter-{060,090,120,150,240,330}.png`.

## Positive qualification

60s directly shows HP low, LP high, both resonance caps low, VCF IL and AL zero, noise zero and sine branch zero. 90s shows a chord and LP high. 120s shows the right hand operating the filter area but obscures exact cap position. 150s shows a lowered LP cap and no hands, but cannot alone establish sounding-note continuity or unchanged controls between90 and150. 240s has HP and HP resonance moved; it is a different condition. 330s has VCF time controls moved. Thus90–145s is a useful qualification region, but the supplied stills do not yet establish two measurable stationary LP positions with the same sounding voice(s). Close inspection between the supplied frames is needed. Do not assign an exact pixel-derived slider coordinate to the hand-covered120s frame.

## Exact manual cutoff path

Primary source: cached `service2.pdf`, SHA2561102e55b949378e7776555bca1a4d73105ca2d76d6fcf3aa4c2b103566201cfc, available at https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf .

- PN1 PDF19/printed25: LPF SVR7 is B10k between selected panel+10V and ground; its wiper goes to PRA fL1(58). HPF SVR5 likewise goes to PRA fH1(60). They are exceptions to the neighboring unbuffered T71 panel controls.
- PRA PDF16/printed22: IC4 contains unity-gain followers fL1(58)→fL0(57) and fH1(60)→fH0(59).
- PRA PCB PDF17/printed23: fL0→T71 input9(23′), fH0→T71 input7(26′). T71 PCB PDF9/printed11 independently names the corresponding PRA returns.
- T71 PDF8/printed10: each of those manual inputs has a diode into i/FL or g/FH, with shared10k return. Small-sheet service manual printed14 R7 gives four39k feeds from FL to voice LP-control nodes.

Inspection images freshly rendered from the cached PDF: `panel1.png`, `panel1-filter.png`, `panel1-filter-bottom.png`, `pra-filter-buffers.png` in this directory. The full cached PRA and T71 board images were also visually checked.

IMPORTANT CORRECTION TO PRELIMINARY MESSAGE: the passive B10k wiper's10k*x*(1−x) source resistance is isolated by the unity follower. It must not be inserted as the manual FL source impedance. For a forward-conducting diode and ideal follower, actual FL≈10*x−Vd, where x is the electrical fraction toward the high-cutoff end. The initial unbuffered-loading suggestion is withdrawn. This correction does not alter the earlier passive factory-preset loading experiment.

A constant diode drop cancels in the difference: ΔFL≈10Δx. Unknown diode current dependence and real follower output impedance create uncertainty, not the large passive preset-divider attenuation. For fixed other source voltages, Δlog2(variable_gm)=s_actual*ΔFL. The current provisional LP slope .65 oct/V therefore predicts6.5 octaves per full slider travel, or:

  fc2 = f_floor + (fc1−f_floor)*2^(6.5*(x2−x1))

at the same key voltage. f_floor is the current fixed-conductance floor, not multiplied by the control. This is a conditional panel-response prediction. Shared unchanged BR removes its direct additive exponent offset; real finite BR/touch source impedances can produce small backfeed-dependent changes of their actual voltages, so this does not uniquely recover physical pin4 or the chip's internal slope. A measured effective panel response remains useful even with those unresolved components.

## Smallest useful conditional measurement

1. Find two non-overlapping stationary LP plateaus during one uninterrupted held note, ideally with a third plateau available for transfer. Require no HP, Q, wave/PW/PWM, sine/noise, BR, SUB/touch-depth or preset actuation between them. Confirm IL/AL remain0. Measure cap travel relative to visible end stops; retain pixel/occlusion uncertainty as an interval on each x. A screenshot with the hand covering the cap cannot provide a point estimate.
2. Prefer the same voice and pitch at both plateaus. An uninterrupted held key avoids reallocation and voice-trim differences; a repeated key is weaker. If only a chord is available, resolve stable note fundamentals and exclude every harmonic that collides with another note's line. A composite chord spectrum is not one oscillator through one filter. If too few isolated partials remain, the simple two-state test is unqualified rather than made monophonic by assumption.
3. Estimate harmonic magnitudes over settled windows, with common estimator and several window lengths. Compare halves of each plateau for stationarity. Use a common nonzero harmonic h0 (often H2, avoiding any residual sine-bypass fundamental), and form D_h=20log10[(A2,h/A2,h0)/(A1,h/A1,h0)]. Fixed source shape, fixed HP, recording EQ and scalar VCA/output gain cancel for the same frequencies. Pressure, PWM, moving cutoff, clipping, changing note allocation, or a noise-floor crossing do not cancel. Pitch drift must be measured; do not silently key-normalize separate notes with an unverified tracking assumption.
4. Test the fixed6.5*Δx law using only a common effective Q and one starting cutoff as nuisance coordinates. Predict D_h from the ratio of the two static LP responses, with fc2 tied tofc1 by the law above. This has no per-window gain, Q, or arbitrary source spectrum. At least several independent partial ratios spanning the bend are needed; a common-Q assumption may be bounded/profiled without calling low slider position measured Q=.5.
5. Report rejection or compatibility after propagating slider uncertainty, frequency error and spectral-estimation error. A free relative slope can be estimated only if the Jacobian columns for starting cutoff, common Q and slope are nondegenerate and its profile interval is finite. Two plateaus whose whole measurable spectrum lies far below both cutoffs cannot identify slope. All-open is useful as a source baseline but often only supplies a lower cutoff bound. Prefer two intermediate positions with their bends in-band. A third stationary plateau can test prediction; it is within-source transfer, not an independent instrument calibration.

## Concrete checks before interpretation

- Estimator oracle: a fixed-filter harmonic signal with changed scalar amplitude must give D_h≈0; a declared two-state LP pair must recover its known relative cutoff within the estimator's declared error. These are proposed diagnostic controls, not runs performed here.
- Harmonic collision check: identify all notes first; require spectral-line separation appropriate to window length, and full rank/well-conditioned design for any joint sinusoid estimate. Reject near-collision partials rather than allowing cross-note energy to masquerade as a changing LP curve.
- Stationarity check: slider cap and relevant controls fixed through each window; harmonic-ratio estimates stable across subwindows and analysis-window choices. A moving sweep is not a static plateau unless a separately declared trajectory model is used.
- Identifiability check: measured partials must span a cutoff bend. Return a bound or inconclusive result if open-position cutoff/profile is unbounded or the cutoff/Q/slope sensitivities are nearly dependent.
- Source/control check: no changed wave/PW/PWM or pressure-correlated shape motion. Global controls need not have known absolute voltages, but unchanged hand positions alone do not prove pressure is unchanged. Known zero touch depth is stronger.

A successful test would constrain or falsify Ceres's provisional effective manual-LP response across visible slider motion. It would not recover absolute cutoff bias, preset loading, hardware Q trim, physical pin4 voltage, or the independent EG scale. That narrower result is still useful predictive evidence and is not blocked by the earlier12×8 calibration degeneracy.
