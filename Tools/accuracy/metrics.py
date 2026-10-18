"""Frozen benchmark measurement kernels; public source is analysis-only.
Extracted from the independently checked research helpers listed in origins.json.
Only new numerical guard: silent harmonic peak interpolation uses zero offset.
"""
import math, pathlib, struct
import numpy as np
from scipy import signal
from scipy.signal import butter,hilbert,sosfiltfilt
FS,ORIGIN=48000,136.4
NOTES=[57,60,64]
EXPECTED=440*2**((np.array(NOTES)-69)/12)
PAIRS=[(57,2),(57,4),(57,5),(57,8),(60,2),(60,3),(60,4),(60,7),(60,8),(64,3),(64,5)]


def spectral_rows(values, times, duration):
    """NumPy rFFT oracle, independent of scipy.signal.periodogram/helper."""
    count = round(duration * FS)
    nfft = 131072
    window = .5 - .5 * np.cos(2 * np.pi * np.arange(count) / count)
    scale = 1 / (FS * np.sum(window * window))
    f = np.arange(nfft // 2 + 1) * FS / nfft
    result = []
    for center in times:
        start = round(center * FS) - count // 2
        x = values[start:start + count]
        assert start >= 0 and len(x) == count
        silent = not np.any(x)
        transformed = np.fft.rfft((x - np.mean(x)) * window, n=nfft)
        power = (transformed.real ** 2 + transformed.imag ** 2) * scale
        power[1:-1] *= 2
        fundamentals = []
        for target in EXPECTED:
            if silent:
                fundamentals.append(float(target))
                continue
            indices = np.flatnonzero((f > target * 2 ** (-60 / 1200)) &
                                     (f < target * 2 ** (60 / 1200)))
            k = indices[np.argmax(power[indices])]
            y = np.log(np.maximum(power[k - 1:k + 2], 1e-30))
            denominator = y[0] - 2 * y[1] + y[2]
            frac = .5 * (y[0] - y[2]) / denominator if abs(denominator) > 1e-15 else 0
            fundamentals.append(float((k + np.clip(frac, -1, 1)) * FS / nfft))
        bands = []
        for i, fundamental in enumerate(fundamentals):
            for h in range(1, 9):
                target = h * fundamental
                separation = min(abs(target - other_h * other_f)
                    for j, other_f in enumerate(fundamentals) if j != i for other_h in range(1, 13))
                energy = 0. if silent else float(power[np.abs(f - target) <= 2 / duration].sum() * FS / nfft)
                bands.append({'note': NOTES[i], 'harmonic': h, 'frequency_hz': target,
                    'nearest_other_ladder_hz': separation, 'isolated_main_lobe': separation >= 4 / duration,
                    'band_power': energy, 'band_dbFS': float(10 * np.log10(max(energy, 1e-30)))})
        result.append({'time': float(center), 'f0_hz': fundamentals, 'bands': bands,
                       'rms': float(np.sqrt(np.mean(x * x))), 'exact_silence': silent})
    return result

def band(row, note, harmonic):
    return next(x for x in row['bands'] if x['note'] == note and x['harmonic'] == harmonic)

def comparison(rows, source, gain):
    partials = []
    for j, (note, h) in enumerate(PAIRS):
        records, errors, weak = [], [], []
        for i, row in enumerate(rows):
            p1, ph = band(row, note, 1)['band_power'], band(row, note, h)['band_power']
            # Difference of log powers supplies an algebraically independent
            # check of the production helper's log of their ratio.
            ratio = 10 * (np.log10(max(ph, 1e-30)) - np.log10(max(p1, 1e-30)))
            target = source['ratio_db'][i][j]
            flags = [bool(p1 * gain ** 2 < source['thresholds'][j]['H1_power']),
                     bool(ph * gain ** 2 < source['thresholds'][j]['Hh_power'])]
            valid = source['valid'][i][j]
            records.append({'source_seconds': ORIGIN + row['time'], 'source_valid': valid,
                'source_ratio_db': target, 'native_ratio_db': float(ratio),
                'raw_ratio_residual_db': float(ratio - target), 'native_weak_H1_Hh': flags,
                'separate_weakness_penalty_db': 30. if any(flags) else 0.,
                'ratio_uninformative_both_zero': bool(p1 <= 1e-30 and ph <= 1e-30)})
            if valid:
                errors.append(float(ratio - target)); weak.append(any(flags))
        partials.append({'note': note, 'harmonic': h, 'source_valid_frames': len(errors),
            'source_valid_ratio_MAE_db': float(np.mean(np.abs(errors))) if errors else None,
            'source_valid_ratio_MSE_db2': float(np.mean(np.square(errors))) if errors else None,
            'source_valid_native_weak_count': sum(weak),
            'separate_weakness_penalty_db_per_failed_frame': 30.,
            'separate_weakness_penalty_mean_db': 30 * float(np.mean(weak)) if weak else None,
            'all_frames': records})
    return {'per_partial': partials, 'native_exclusion_used': False,
        'composite_spectral_and_weakness_score': None,
        'interpretation': 'Ratio residuals and weak-native failures are separate diagnostics, not a joint objective.'}

def envelopes(values):
    # Same declared linear band definitions, independently invoked rather than
    # importing either the root band wrapper or its tail-metric implementation.
    output = []
    for lo, hi in [(190, 242), (244, 295), (303, 370)]:
        sos = butter(4, [lo, hi], btype='bandpass', fs=FS, output='sos')
        output.append(np.abs(hilbert(sosfiltfilt(sos, values))))
    return np.asarray(output)

def tails(native, source, gain):
    early_indices = np.arange(22080, 32640 + 1, 96)
    tail_indices = np.arange(32640, 60000 + 1, 96)
    floor = np.median(source[:, early_indices], axis=1) * .01
    db_source = 20 * np.log10(np.maximum(source[:, tail_indices], floor[:, None]))
    db_native = 20 * np.log10(np.maximum(native[:, tail_indices] * gain, floor[:, None]))
    error = db_native - db_source
    valid = source[:, tail_indices] > floor[:, None]
    rms = np.sqrt(np.mean(error ** 2, axis=1))
    return {'source_seconds': [137.08, 137.65], 'sampling_step_ms': 2,
        'floor_below_early_source_median_db': 40, 'source_floor_amplitudes': floor.tolist(),
        'per_note_floored_RMS_db': rms.tolist(), 'A3_C4_joint_floored_RMS_db': float(np.sqrt(np.mean(error[:2] ** 2))),
        'E4_floored_RMS_db': float(rms[2]),
        'per_note_source_above_floor_RMS_db': [float(np.sqrt(np.mean(error[k, valid[k]] ** 2))) for k in range(3)],
        'source_above_floor_sample_counts': valid.sum(axis=1).tolist(),
        'per_note_mean_error_db': np.mean(error, axis=1).tolist()}

def six_cells(comp):
    cells=[]
    for note in [57,60,64]:
        for phase in ['early','release']:
            records=[]
            for part in comp['per_partial']:
                if part['note'] != note: continue
                for frame in part['all_frames']:
                    if not frame['source_valid']: continue
                    early=frame['source_seconds']<=137.08+1e-8
                    if early == (phase=='early'): records.append(frame)
            assert records
            cells.append({'note':note,'phase':phase,'partial_frame_count':len(records),
                'ratio_MAE_db':float(sum(abs(x['raw_ratio_residual_db']) for x in records)/len(records)),
                'native_weak_partial_frames':sum(any(x['native_weak_H1_Hh']) for x in records)})
    return {'six_cell_mean_ratio_MAE_db':float(sum(x['ratio_MAE_db'] for x in cells)/6),
            'cells':cells,'total_native_weak_partial_frames':sum(x['native_weak_partial_frames'] for x in cells)}

def higher_ratios(measured, source_report, windows, gain):
    """Direct Hh/H2 band-power ratios, independently avoiding H1 subtraction."""
    source = source_report['source']
    pairs = [(p['note'], p['harmonic']) for p in source_report['partial_summary']]
    columns = [pairs.index((60,h)) for h in [2,3,4]]
    valid = np.asarray(source['valid'], dtype=bool)[:,columns].all(axis=1)
    thresholds = {h:source['thresholds'][pairs.index((60,h))]['Hh_power'] for h in [2,3,4]}
    results = []
    for h in [3,4]:
        parts = []
        for window in ['upper_lobe_1','upper_lobe_2']:
            low, high = windows[window]
            records = []
            for i, (native, original) in enumerate(zip(measured, source['rows'])):
                t = original['time']+136.4
                if valid[i] and low-1e-8 <= t <= high+1e-8:
                    sp = [band(original,60,k)['band_power'] for k in [h,2]]
                    npow = [band(native,60,k)['band_power'] for k in [h,2]]
                    records.append(dict(source_seconds=t,
                        source_ratio_db=float(10*math.log10(sp[0]/sp[1])),
                        native_ratio_db=float(10*math.log10(max(npow[0],1e-30)/max(npow[1],1e-30))),
                        native_weak_Hh_H2=[bool(gain*gain*p < thresholds[k]) for p,k in zip(npow,[h,2])]))
            assert len(records) == 4
            parts.append(dict(window=window, records=records,
                source_median_db=float(np.median([r['source_ratio_db'] for r in records])),
                native_median_db=float(np.median([r['native_ratio_db'] for r in records])),
                native_weak_frames=sum(any(r['native_weak_Hh_H2']) for r in records)))
        sd = parts[1]['source_median_db']-parts[0]['source_median_db']
        nd = parts[1]['native_median_db']-parts[0]['native_median_db']
        results.append(dict(numerator=h, denominator=2, windows=parts,
                            source_change_db=sd, native_change_db=nd, change_error_db=nd-sd))
    return results

def measure_frame(audio, rate, center, expected, count=24, duration=.064):
    n = round(duration * rate)
    start = round(center * rate) - n // 2
    x = audio[start:start + n]
    if start < 0 or len(x) != n:
        raise ValueError("measurement frame is outside file")
    nfft = 1 << int(np.ceil(np.log2(n * 8)))
    f, power = signal.periodogram(x, fs=rate, window="hann", nfft=nfft,
                                 detrend="constant", scaling="density")
    bins = np.flatnonzero((f > expected * .965) & (f < expected * 1.035))
    i = bins[np.argmax(power[bins])]
    logp = np.log(np.maximum(power[i - 1:i + 2], 1e-30))
    denom = logp[0] - 2 * logp[1] + logp[2]
    delta = .5 * (logp[0] - logp[2]) / denom if abs(denom) > 1e-15 else 0.
    fundamental = (i + delta) * rate / nfft
    amplitudes = []
    for k in range(1, count + 1):
        target = k * fundamental
        if target > min(16000, rate * .45):
            amplitudes.append(np.nan)
            continue
        # The widening band captures weak vibrato sidebands at high harmonics.
        width = 35 + .004 * target
        band = (f > target - width) & (f < target + width)
        amplitudes.append(float(np.sqrt(np.sum(power[band]) * rate / nfft)))
    a = np.array(amplitudes)
    return {"time": float(center), "f0": float(fundamental), "rms": float(np.sqrt(np.mean(x*x))),
            "harmonic_rms": a.tolist(), "relative_db": (20 * np.log10(np.maximum(a, 1e-12) / max(a[0], 1e-12))).tolist()}

def vlq(data, pos):
    value = 0
    for _ in range(4):
        if pos >= len(data):
            raise ValueError("truncated MIDI variable-length integer")
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 127)
        if b < 128:
            return value, pos
    raise ValueError("MIDI VLQ exceeds four bytes")

def read_midi(path, channel=None, bend_range=2.0):
    """Read synchronous SMF 0/1, PPQ time; retain tempo changes and track order.

    Return (seconds, event, note, value). MIDI channels share the four-voice
    instrument unless --channel is supplied. Fixed +/-2 semitone pitch bend;
    reject RPN/NRPN so altered tuning is never silently accepted.
    """
    data = pathlib.Path(path).read_bytes()
    if len(data) < 14 or data[:4] != b"MThd":
        raise ValueError("not a Standard MIDI File")
    size = struct.unpack_from(">I", data, 4)[0]
    if size < 6 or len(data) < 8 + size:
        raise ValueError("truncated MIDI header")
    fmt, tracks, division = struct.unpack_from(">HHH", data, 8)
    if fmt not in (0, 1) or division == 0 or division & 0x8000:
        raise ValueError("only SMF 0/1 with PPQ timing is supported")
    if tracks == 0 or (fmt == 0 and tracks != 1):
        raise ValueError("invalid MIDI track count")
    pos = 8 + size
    raw = []
    warnings = set()
    active_channels = set()
    for track in range(tracks):
        if pos + 8 > len(data) or data[pos:pos + 4] != b"MTrk":
            raise ValueError("missing MIDI track")
        length = struct.unpack_from(">I", data, pos + 4)[0]
        pos += 8
        end = pos + length
        if end > len(data):
            raise ValueError("truncated MIDI track")
        chunk = data[pos:end]
        pos = end
        p = tick = order = 0
        running = None
        while p < len(chunk):
            delta, p = vlq(chunk, p)
            tick += delta
            if p >= len(chunk):
                raise ValueError("missing MIDI event")
            status = chunk[p]
            if status >= 128:
                p += 1
                if status < 0xF0:
                    running = status
                else:
                    running = None
            elif running is not None:
                status = running
            else:
                raise ValueError("MIDI running status without prior status")
            if status == 0xFF:
                if p >= len(chunk):
                    raise ValueError("truncated MIDI meta event")
                meta = chunk[p]
                n, p = vlq(chunk, p + 1)
                payload = chunk[p:p + n]
                if len(payload) != n:
                    raise ValueError("truncated MIDI meta payload")
                p += n
                if meta == 0x51:
                    if n != 3 or int.from_bytes(payload, "big") == 0:
                        raise ValueError("invalid MIDI tempo")
                    raw.append((tick, track, order, "tempo", 0, int.from_bytes(payload, "big")))
                if meta == 0x2F:
                    break
            elif status in (0xF0, 0xF7):
                n, p = vlq(chunk, p)
                p += n
                if p > len(chunk):
                    raise ValueError("truncated MIDI SysEx")
                warnings.add("SysEx ignored; tuning/patch SysEx is not supported")
            elif status >= 0xF0:
                raise ValueError(f"unsupported MIDI system status {status:#x}")
            else:
                kind, ch = status >> 4, status & 15
                count = 1 if kind in (0xC, 0xD) else 2
                args = chunk[p:p + count]
                if len(args) != count or any(a >= 128 for a in args):
                    raise ValueError("invalid MIDI channel payload")
                p += count
                if channel is not None and ch != channel:
                    order += 1
                    continue
                a = args[0]
                b = args[1] if count == 2 else 0
                event = None
                if kind in (8, 9):
                    active_channels.add(ch)
                    event = ("on", a, b / 127) if kind == 9 and b else ("off", a, 0)
                elif kind == 0xD:
                    event = ("pressure", 0, a / 127)
                elif kind == 0xA:
                    warnings.add("Polyphonic pressure ignored: CS-50 uses channel pressure")
                elif kind == 0xE:
                    event = ("bend", 0, ((a | (b << 7)) - 8192) / 8192 * bend_range)
                elif kind == 0xB:
                    if a in (98, 99, 100, 101, 6, 38):
                        raise ValueError("RPN/NRPN unsupported; export fixed-tuning note MIDI")
                    if a == 64:
                        event = ("sustain", 0, float(b >= 64))
                    elif a == 123:
                        event = ("alloff", 0, 0)
                    elif a == 120:
                        event = ("panic", 0, 0)
                    elif a == 121:
                        raw.extend((tick, track, order, k, 0, 0) for k in ("sustain", "pressure", "bend"))
                    else:
                        warnings.add(f"CC{a} ignored; patch file fixes synthesis settings")
                elif kind == 0xC:
                    warnings.add("Program changes ignored; CLI selects one explicit patch")
                if event:
                    raw.append((tick, track, order, *event))
            order += 1
    raw.sort(key=lambda x: x[:3])
    tempo, last_tick, seconds = 500000, 0, 0.0
    result = []
    for tick, _, _, kind, note, value in raw:
        seconds += (tick - last_tick) * tempo / (division * 1e6)
        last_tick = tick
        if kind == "tempo":
            tempo = value
        else:
            result.append((seconds, kind, note, value))
    if len(active_channels) > 1:
        warnings.add("Multiple MIDI channels consolidated into one four-voice synth")
    return result, sorted(warnings)
