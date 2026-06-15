#!/usr/bin/env python3
"""
AICA 4-bit Yamaha ADPCM encoder/decoder.
Tables verified against AICA_E.pdf (ADPCM section, Table 1 & Table 2) and the
canonical BERO wav2adpcm. Continuous nibble stream, no frame headers.

Per nibble `code` (bit3 = sign, bits0..2 = magnitude index m):
    delta = trunc(quant * DIFF[code] / 8)
    cur   = clamp(cur + delta, -32768, 32767)
    quant = clamp((quant * SCALE[code & 7]) >> 8, 127, 24576)
init: cur=0, quant=127.  Packing: even sample -> low nibble, odd -> high nibble.
"""

DIFF = (1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15)
SCALE = (0xE6, 0xE6, 0xE6, 0xE6, 0x133, 0x199, 0x200, 0x266)

QMIN, QMAX = 127, 24576


def _trunc_div(a, b):
    # C-style division truncating toward zero
    q = abs(a) // b
    return q if a >= 0 else -q


def _clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def encode(pcm):
    """pcm: iterable of int16 -> (bytes, nsamples)."""
    cur, quant = 0, 127
    codes = []
    for x in pcm:
        d = x - cur
        sign = 0
        if d < 0:
            sign = 8
            d = -d
        m = (d * 4) // quant          # which quarter-step bucket
        if m > 7:
            m = 7
        code = sign | m
        delta = _trunc_div(quant * DIFF[code], 8)
        cur = _clamp(cur + delta, -32768, 32767)
        quant = _clamp((quant * SCALE[m]) >> 8, QMIN, QMAX)
        codes.append(code)

    out = bytearray((len(codes) + 1) // 2)
    for i, c in enumerate(codes):
        if i & 1:
            out[i >> 1] |= c << 4
        else:
            out[i >> 1] |= c
    return bytes(out), len(codes)


def decode(data, nsamples=None):
    """AICA ADPCM bytes -> list of int16. Mirrors hardware decode."""
    cur, quant = 0, 127
    out = []
    n = len(data) * 2 if nsamples is None else nsamples
    for i in range(n):
        byte = data[i >> 1]
        code = (byte >> 4) & 0xF if (i & 1) else byte & 0xF
        m = code & 7
        delta = _trunc_div(quant * DIFF[code], 8)
        cur = _clamp(cur + delta, -32768, 32767)
        quant = _clamp((quant * SCALE[m]) >> 8, QMIN, QMAX)
        out.append(cur)
    return out
