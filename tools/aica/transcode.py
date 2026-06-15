#!/usr/bin/env python3
"""
Full per-sample transcode: VADPCM -> PCM16 -> (2x decimate if >64K) -> AICA
Yamaha ADPCM, producing the final ADPCM bytes + descriptor. Shared by the
build-time emitter.

Decimation is scipy-optional: scipy.signal.resample_poly is used when available
(the output that was ear-checked/approved), else a stdlib windowed-sinc FIR so a
clean checkout builds with only stock Python 3.
"""

import math

try:
    import numpy as np
    from scipy.signal import resample_poly
    _HAVE_SCIPY = True
except Exception:
    _HAVE_SCIPY = False

import vadpcm
import yamaha_adpcm

AICA_MAX = 65534


def _clamp16(v):
    return -32768 if v < -32768 else 32767 if v > 32767 else v


def _decimate2(pcm):
    """Anti-aliased 2x downsample. scipy if present (matches approved output);
    else a 23-tap Hamming-windowed-sinc lowpass (fc=0.25) + take every 2nd."""
    if _HAVE_SCIPY:
        dec = resample_poly(np.asarray(pcm, dtype=np.float64), 1, 2)
        return [_clamp16(int(round(v))) for v in dec]
    N, fc = 23, 0.25
    mid = (N - 1) // 2
    h = []
    for i in range(N):
        x = i - (N - 1) / 2.0
        s = 2 * fc if x == 0 else math.sin(2 * math.pi * fc * x) / (math.pi * x)
        w = 0.54 - 0.46 * math.cos(2 * math.pi * i / (N - 1))
        h.append(s * w)
    g = sum(h)
    h = [c / g for c in h]
    L = len(pcm)
    out = []
    for n in range(0, L, 2):
        acc = 0.0
        for k in range(N):
            idx = n + k - mid
            idx = 0 if idx < 0 else (L - 1 if idx >= L else idx)
            acc += pcm[idx] * h[k]
        out.append(_clamp16(int(round(acc))))
    return out


def transcode_sample(s):
    """s: albank_parse.Sample (parsed with_data=True). Returns descriptor dict."""
    pcm = vadpcm.decode(s.data, s.codec, s.order, s.npredictors, s.book)
    shift = 0
    loop_start, loop_end = s.loop_start, s.loop_end

    if len(pcm) > AICA_MAX:
        # 2x trick: anti-aliased decimate by 2, play back at half freq at runtime.
        pcm = _decimate2(pcm)
        shift = 1
        loop_start //= 2
        loop_end //= 2

    adpcm, n = yamaha_adpcm.encode(pcm)
    assert n <= AICA_MAX, (s.bank, s.addr, n)

    if not s.has_loop:
        # one-shot: AICA still needs loopend = sample count as the play length
        loop_start, loop_end = 0, n

    return {
        "bank": s.bank,
        "addr": s.addr,
        "adpcm": adpcm,
        "nsamples": n,
        "loop": bool(s.has_loop),
        "loop_start": loop_start,
        "loop_end": min(loop_end, n),
        "downsample_shift": shift,
    }
