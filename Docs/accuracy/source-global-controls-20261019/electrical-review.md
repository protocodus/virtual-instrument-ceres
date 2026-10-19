# Electrical review of the shared BRILLIANCE and RESONANCE controls

19 October 2026. Local primary-source and circuit audit only. No recording fit, DSP change, default change, or new audio render. This note supplements the retained control/loading notes; it does not revise them.

## Result and mechanical interpretation

**The functional MIN brilliance endpoint maps to the negative BR rail. A visible low cap does not establish its actual voltage or an exact endpoint.** Yamaha's CS Series Servicing Guide PDF14/printed5 explicitly says BRILLIANCE toward MAX raises both HPF and LPF cutoff frequencies, whereas MIN removes higher harmonics. The direct CS-50 PN2 circuit, service2 PDF20/printed26, gives TVR7 a +15 V end, a −15 V end and a separate grounded center tap. R7 injects BR into the cutoff nodes through positive resistances, without an inverter. With the documented increasing cutoff-control direction, the negative side is therefore the darker side. This conclusion combines the functional description and electrical circuit; it does not infer lug direction from the drawing's vertical orientation.

The ideal MIN electrical stop is −15 V and the grounded center is 0 V. The actual pot taper, resistance split about its center tap, contact/end resistance and mechanical travel-to-resistance map are not specified. A cap merely *near* MIN supports a negative/dark hypothesis, not a measured −15 V setting. The separate visual review in `independent-review.md` reports comparable lower/forward cap regions in Brass and both Flute excerpts; the present electrical audit supplies no basis for asserting neutral Flute but dark Brass from their preset names.

RESONANCE is different. The Guide's printed5 description does not map visible lever travel to voltage. Direct service1 PDF9/printed19 explicitly gives **chip pin7 VQ=0 V → Q=5** and **VQ=10 V → Q=.5**. These are chip-coordinate endpoints, not a cap-position calibration or upstream preset-table voltages. No new primary evidence resolves which physical RES lever end is 0 V. The existing `whitmore-modulation-controls-erratum.md` remains applicable.

## The two wiper circuits

| Source | Direct PN2 connections | Shared destination and load |
|---|---|---|
| BRILLIANCE TVR7 C100k | lug1→−15 V; lug3→+15 V; fixed tap5→ground; wiper2→BR29 | All four HP cutoff branches through 680k and all four LP branches through 220k |
| RESONANCE TVR6 C100k | +10 V and ground ends; one wiper→RES51; no center tap | All eight Q-control nodes through 220k each; each also has its own preset branch through 220k and a 10µF pin7 capacitor |

The PN2 wire labels, not an assumed pot convention, establish those BR lug connections. Neither wiper is buffered on PN2. There are not separate global HP/LP brilliance wipers. The distinct touch BRILLIANCE source TBR69 is another modulation path, not a second global BR control; it remains a declared zero source in the current calibration network. The four-voice connections are visible in service1 PDF8/printed14. The Q capacitors and independent pin6 Q trims are visible in service2 PDF12/printed16.

For one half of the BR pot, let `beta∈[0,1]` be the **electrical divider fraction from grounded tap toward that rail**, and let that segment's total resistance be `Rs`. With ideal supplies:

```text
B0 = sign * 15 * beta
RB = Rs * beta * (1-beta)
Rs_negative + Rs_positive = 100k
```

These are Thevenin quantities before loading. The inactive half is tied between stiff ground and its other rail; it does not form an additional wiper branch. The conservative bound is `RB≤25k`; assuming two equal 50k halves would tighten it to 12.5k, but equal halves are not established. **RB=0 at the ideal grounded center and either ideal rail stop.** A finite RB must not be independently assigned to B0=−15 V or B0=0 V while claiming it comes from this ideal tapped pot.

For RES, the ordinary 100k divider instead gives `R0=10*a`, `RR=100k*a*(1-a)`. The grounded midpoint of BR is not a property of RES: at its electrical midpoint RES has its maximum 25k Thevenin resistance. Neither `a` nor `beta` is a measured mechanical travel fraction.

## Exact extension of the current cutoff network

The research engine currently accepts **actual stiff BR-node voltage**. That is a well-defined conditional input; it is not a complete pot-position model. Its two five-node networks already contain each stage's selected preset bus and all four instantaneous envelope voltages, including idle and released boards. The next implementable extension is one common BR node coupled to both networks, rather than an independent attenuation applied to each stage.

For stage `j∈{H,L}` and board `i`, retain the current rank-B, fixed-diode and fixed-bias-source assumptions:

```text
gp = 1/39k; gj = 1/RBR,j
Dj = gp + 1/Rrank,j + gj + 3/Rmod,j + 1/REG,j + 1/355k
(gs,j + 1/10k + 4gp) Pj - gp Σi uj,i = gs,j*(10-.6)
Dj uj,i - gp Pj - gj B = Ei/REG,j + biasj/355k + SUB/Rmod,j
(1/RB + 4gH + 4gL) B - Σj,i gj uj,i = B0/RB
```

Use `B=B0` directly when RB=0. This is an 11-node passive linear solve under the declared diode states. It preserves the current distinction between bias-source volts, physical node volts and the provisional octave conversion. Alpha still multiplies the entire `kappa_j*uj,i`; key voltage and the fixed integrator conductance floor remain outside that exponent.

An inexpensive exact reduction is available. Write the two existing networks together as `A*x + c*B = r`, where `A` is block diagonal 10×10 and `c` has −gj at each corresponding voice node. Let `y=A^-1*r`, `z=−A^-1*c`, and `d=1/RB+4gH+4gL`. Then:

```text
B = (B0/RB - cᵀy) / (d + cᵀz)
x = y + z*B
```

The coefficients can be precomputed for fixed component/source resistances; all four EG values enter `r` each sample. This adds shared cross-stage/voice backfeed. It must be solved in the **absolute network**. Merely changing pot resistance inside an anchored delta-only model omits the pre-existing preset/bias backfeed. A full implementation must retain selected-diode current and inactive-diode margin checks. No pot dynamics or unspecified chip input law is recovered by this algebra.

## Size of the conditional interior-loading effect

The following calculations use the current rank B (HP2.2k/LP2.4k), Vd=.6 V, bias source0 V through355k, four EGs at0 V, other sources0 V, and alpha1. The octave changes compare a finite source with the **same unloaded B0 held stiff**. They are not recording measurements or new calibration values.

| Preset | B0 (V) | RB (Ω) | Loaded B (V) | HP variable-gm shift (oct) | LP variable-gm shift (oct) |
|---|---:|---:|---:|---:|---:|
| Flute | −7.5 | 12,500 | −5.745472 | +.072166 | +.205402 |
| Flute | −7.5 | 25,000 | −4.647000 | +.117348 | +.334000 |
| Flute | −14.7 | 980 | −14.360745 | +.013954 | +.039717 |
| Brass I | −7.5 | 12,500 | −5.743570 | +.072361 | +.205396 |
| Brass I | −7.5 | 25,000 | −4.643909 | +.117665 | +.333989 |
| Brass I | −14.7 | 980 | −14.360554 | +.013984 | +.039695 |

The 12.5k and 980Ω rows use an illustrative 50k negative segment at beta=.5 and .98. The 25k row is the conservative segment-resistance limit at beta=.5, not a measured pot. Under these assumptions, interior loading makes negative brilliance **less dark** than its unloaded voltage suggests. It cannot give extra darkening beyond the stiff −15 V endpoint. At a fixed illustrative −7.5 V/12.5k source, taking all four EGs across the preset IL/AL corners moves actual B by .022283 V for Flute and .020708 V for Brass. Thus time-varying backfeed exists, but this example does not establish a large missing envelope effect.

The existing declared BR=0 V and BR=−15 V endpoint cases are exact RB=0 limits of this pot model. **Finite pot resistance cannot change those endpoint cases.** Replacing a near-MIN visual observation by a finite interior source is a different declared control hypothesis, not a correction to an already fixed actual BR voltage. These results do not justify choosing an intermediate position from audio error.

Independent dense 11-node versus reduced solves were checked for 1,280 cases: both presets, beta0/.1/.5/.9/1, segment resistance50k/100k, all16 EG endpoint combinations, and four ±15 V bias-source pairs. Maximum node discrepancy was 7.11e−15 V and normalized KCL residual 1.34e−16. Center/rail cases were exactly equal to their stiff solves. This sweep checks algebra; it does not certify every assumed diode state in those extreme bias combinations.

## RES extension and remaining identification limit

Unlike the quasistatic BR calculation, the known Q-node capacitors require eight continuous voltage states. For the actual shared RES wiper `r`, preset bus `Pj`, source resistance `RPj`, and unknown chip input current `Ij,i`:

```text
(Pj-Ej)/RPj + Σi(Pj-qj,i)/220k = 0
(r-R0)/RR + Σj,i(r-qj,i)/220k = 0
10µF * dqj,i/dt = (Pj-qj,i)/220k + (r-qj,i)/220k - Ij,i
```

At RR=0 impose r=R0. Finite RR couples HP and LP charge histories; it is not eight independent extra poles. The original preset-source loading, unknown Q pin input law, independent trims and missing interior VQ→damping calibration still prevent identifying an absolute Q map from cap position. The existing zero-input-current calculation remains an explicit limit, not a measured IC load. Switching from stiff actual RES to an unloaded pot source must not silently retain the same voltage name/meaning or initialize all capacitors afresh on every note.

**Recommendation:** the 11-node/Schur BR extension is source-supported as a separately declared interior-pot hypothesis, with explicit electrical fraction/segment-resistance assumptions. It is unnecessary for ideal center/rail tests and supplies no new fitted setting. The analogous RES network is source-supported as a dynamic circuit structure, but its absolute chip current/damping law remains unidentified. No production correction or new default is selected by this review.

## Reproduction of the new numerical check

Run this snippet with Python and NumPy; it performs no audio or file mutation. `solve` builds the full nodal system and compares its Schur reduction. The component table is deliberately explicit rather than importing the implementation being checked.

```python
import itertools
import numpy as np
gp = 1/39000
stages = [(2200,680000,150000,220000,.7),
          (2400,220000,47000,47000,.65)]
def solve(preset, B0, RB, E=(0,)*4, bias=(0,0)):
    Rs = {'flute':(75000,9100),'brass1':(None,7500)}[preset]
    A=np.zeros((11,11)); v=np.zeros(11); k=[]
    for j,(rank,rbr,rmod,reg,slope) in enumerate(stages):
        b=5*j; gs=0 if Rs[j] is None else 1/Rs[j]
        D=gp+1/rank+1/rbr+3/rmod+1/reg+1/355000
        k.append(slope*D/gp)
        A[b,b]=gs+1/10000+4*gp; v[b]=gs*9.4
        for i in range(4):
            a=b+i+1
            A[b,a]=A[a,b]=-gp; A[a,a]=D
            A[a,10]=A[10,a]=-1/rbr; A[10,10]+=1/rbr
            v[a]=E[i]/reg+bias[j]/355000
    if RB:
        A[10,10]+=1/RB; v[10]=B0/RB
    else:
        A[10,:]=0; A[10,10]=1; v[10]=B0
    x=np.linalg.solve(A,v)
    y=np.linalg.solve(A[:10,:10],v[:10])
    z=np.linalg.solve(A[:10,:10],-A[:10,10])
    B=B0 if not RB else (B0/RB-A[10,:10]@y)/(A[10,10]+A[10,:10]@z)
    error=np.max(abs(x-np.r_[y+z*B,B]))
    norm=max(np.max(abs(A))*max(1,np.max(abs(x))),np.max(abs(v)))
    residual=np.max(abs(A@x-v))/norm
    return x,np.array(k),error,residual
worst=np.zeros(2); count=0
for p,(il,al) in {'flute':(-2.62,.315),'brass1':(-.945,1.785)}.items():
    for beta,Rs,E,bias in itertools.product(
            (0,.1,.5,.9,1),(50000,100000),
            itertools.product((il,al),repeat=4),
            itertools.product((-15,15),repeat=2)):
        x,k,e,r=solve(p,-15*beta,Rs*beta*(1-beta),E,bias)
        count+=1; worst=np.maximum(worst,[e,r])
        if beta in (0,1):
            assert np.array_equal(x,solve(p,-15*beta,0,E,bias)[0])
print(count, worst)
for p in ('flute','brass1'):
    for beta,Rs in ((.5,50000),(.5,100000),(.98,50000)):
        x,k,_,_=solve(p,-15*beta,Rs*beta*(1-beta))
        ref=solve(p,-15*beta,0)[0]
        print(p, -15*beta, Rs*beta*(1-beta), x[10],
              k*(x[[1,6]]-ref[[1,6]]))
    il,al={'flute':(-2.62,.315),'brass1':(-.945,1.785)}[p]
    b=[solve(p,-7.5,12500,E)[0][10]
       for E in itertools.product((il,al),repeat=4)]
    print(p, 'EG-induced B span', max(b)-min(b))
```

## Bound local evidence

Paths below are relative to `/tmp/ceres-cs50-1x8LGd/research` unless absolute. Hashes were recomputed. The PN2 crop, R7 sheet and direct pin7 endpoint crop were visually inspected. Guide printed5 was freshly rasterized from the bound PDF and visually checked; its temporary image is not a new source. Existing notes were read before deriving the extension.

| Artifact | SHA-256 |
|---|---|
| `service1.pdf` | `c3a4b0d4e27e9af90c88258d03a1c09c5d080ea162e562d9bf267fc8ffbbadc1` |
| `service2.pdf` | `1102e55b949378e7776555bca1a4d73105ca2d76d6fcf3aa4c2b103566201cfc` |
| `cs-servicing-guide.pdf` | `2738f7ddc46f0bedbab38dcad8e2c112c41b3e6ca34670d471d0cb8c0dbde633` |
| `tracking-cs50-panel.png` | `5584c2deb22625a8b66a7a831f7befb53c42028be2b8b0146dca02e680574801` |
| `pitch-modulation-global-controls.png` | `c9cbb89014b0b6b27ae653c45d3b8feb40b08bc957d277ec348578cae5bcfcfb` |
| `service1-08.png` | `07c59b3a2dd9d1d7f5bff25a0635491da2dace85f2791cfaaa8465858b586668` |
| `global-resonance-pin7-endpoints.png` | `d970bb24d5adc35f3d722f6753c044252c93fd6b2f9640dbe77a3a334adef6c0` |
| `q-m-filter-full.png` | `027b5dad4c0c126b36f94911c0f517f40136fc6636e7ff37334842cc4f2570dc` |
| `cutoff-scale-t71.png` | `c1c3a13d6f44c7959e1186e573635d2382860e6d3978a08261dbced3acaf4cb4` |
| `brilliance-shared-bus-check.md` | `90baf174c023382eeffb2a1bfd621964b303746a3d089925983abbdae0cb20e4` |
| `global-resonance-loading.md` | `34d14e4c46e4527b65f83deb58af2220053850e254c4965010ff99e86891e41f` |
| `pitch-modulation-routing.md` | `362dac857bb5227366b8869259dbc34aeb545731241b5da57361896f41b6e655` |
| `whitmore-modulation-controls-erratum.md` | `064113c7e1ae18d1f8ae00177b78403f5beaf53e3642d8e941b531bc0d9c33b2` |
| `cutoff-voltage-scale.md` | `aff170f4e18a7c00793f00c2321f216dd93a3c50a0ab8ba354104a1324e3cf1f` |
| `/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/preset-dc-loading/network.py` | `b03cdb48f68c73cc70312cb2c8061d69a1d81fea1f6e0330464f8e052f631297` |
| `/tmp/ceres-joint-calibration-20261018/Source/DSP/ceres.cpp` | `f31fe5359c182cda58deda9ee5ca46a3fc78df2ef0fdca7811fa24f2eff6e3f2` |
| `/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/source-global-controls-20261019/independent-review.md` | `73ef15aa07c1aa3295249ff5b9bbf6865dce4364cebd2506d912e284fb24aef6` |

Primary page references: service2 PDF20/printed26 (PN2), PDF12/printed16 (M), PDF8/printed10 (preset matrix); service1 PDF8/printed14 (R7), PDF9/printed19 (IG00156); Servicing Guide PDF14/printed5 (global controls). No remote source was fetched for this audit.
