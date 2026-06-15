#!/usr/bin/env python3
"""
VADPCM (N64 ADPCM) -> PCM16 decoder. Ported from
oot/tools/audio/sampleconv/src/codec/vadpcm.c (CC0).

Handles both codecs:
  CODEC_ADPCM       (0): 4-bit, 9-byte frames (16 samples)
  CODEC_SMALL_ADPCM (3): 2-bit, 5-byte frames (16 samples)
"""

FRAME_BYTES = {0: 9, 3: 5}   # codec -> bytes per 16-sample frame


def expand_codebook(book, order, npredictors):
    """book: flat seq of 8*order*npredictors int16. Returns table[npred][8][order+8]."""
    table = [[[0] * (order + 8) for _ in range(8)] for _ in range(npredictors)]
    bi = 0
    for i in range(npredictors):
        te = table[i]
        for j in range(order):
            for k in range(8):
                te[k][j] = book[bi]; bi += 1
        for k in range(1, 8):
            te[k][order] = te[k - 1][order - 1]
        te[0][order] = 1 << 11
        for k in range(1, 8):
            for j in range(k):
                te[j][k + order] = 0
            for j in range(k, 8):
                te[j][k + order] = te[j - k][order]
    return table


def inner_product(length, v1, v2):
    # C does floor(out / 2048); Python // is floor division for ints.
    out = 0
    for i in range(length):
        out += v1[i] * v2[i]
    return out // 2048


def decode_frame(frame, state, order, coef_tbl, frame_size):
    header = frame[0]
    scale = 1 << ((header >> 4) & 0xF)
    optimalp = header & 0xF
    ix = [0] * 16
    if frame_size == 5:
        for i in range(0, 16, 4):
            c = frame[1 + i // 4]
            ix[i + 0] = (c >> 6) & 0b11
            ix[i + 1] = (c >> 4) & 0b11
            ix[i + 2] = (c >> 2) & 0b11
            ix[i + 3] = (c >> 0) & 0b11
    else:
        for i in range(0, 16, 2):
            c = frame[1 + i // 2]
            ix[i + 0] = (c >> 4) & 0xF
            ix[i + 1] = (c >> 0) & 0xF
    for i in range(16):
        if frame_size == 5:
            if ix[i] >= 2:
                ix[i] -= 4
        else:
            if ix[i] >= 8:
                ix[i] -= 16
        ix[i] *= scale
    coef_page = coef_tbl[optimalp]
    for j in range(2):
        in_vec = [0] * 16
        for i in range(order):
            in_vec[i] = state[(2 - j) * 8 - order + i]
        for i in range(8):
            ind = j * 8 + i
            in_vec[order + i] = ix[ind]
            state[ind] = inner_product(order + i, coef_page[i], in_vec) + ix[ind]


def decode(data, codec, order, npredictors, book):
    """Decode raw VADPCM bytes -> list of PCM16 ints (clamped to s16)."""
    fs = FRAME_BYTES[codec]
    coef_tbl = expand_codebook(book, order, npredictors)
    state = [0] * 16
    out = []
    n = len(data) // fs
    for f in range(n):
        frame = data[f * fs: f * fs + fs]
        decode_frame(frame, state, order, coef_tbl, fs)
        for s in state:
            out.append(-32768 if s < -32768 else 32767 if s > 32767 else s)
    return out
