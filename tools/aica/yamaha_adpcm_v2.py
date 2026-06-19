#!/usr/bin/env python3
"""
AICA Yamaha-ADPCM encoder v2: analysis-by-synthesis + beam search.

Decoder is byte-identical to yamaha_adpcm (same DIFF/SCALE/clamps/init/packing) --
we import it so the encoder optimizes for the exact playback model. Only the
ENCODE side is smarter: instead of greedily picking the nearest step for the
current sample, it keeps the best `beam` decoder-state trajectories and, at each
sample, expands every trajectory by all 16 nibbles, scores by cumulative squared
error, and prunes. This lets a higher-step-size path survive through a quiet
lead-in so the step is already large enough to track a later full-scale excursion
(the failure mode greedy can't fix -- see sample 188c0).

Optional step-growth penalty (lambda_step) gently discourages needless quant
inflation, which tends to reduce grit in quiet/tonal material; default 0 so the
pure beam-search gain can be measured in isolation.
"""

from yamaha_adpcm import DIFF, SCALE, QMIN, QMAX, _trunc_div, decode  # exact decoder + tables


def encode(pcm, beam=32, lambda_step=0.0):
    """pcm: iterable of int16 -> (bytes, nsamples). Decoder-exact beam search."""
    pcm = list(pcm)
    n = len(pcm)
    if n == 0:
        return b"", 0

    # Each beam node: (score, cur, quant). back[t] holds (parent_idx, code) per
    # surviving node so we can reconstruct the chosen nibble stream at the end.
    beams = [(0.0, 0, 127)]
    back = []

    for x in pcm:
        cand = []
        for pi, (sc, cur, quant) in enumerate(beams):
            for code in range(16):
                m = code & 7
                delta = _trunc_div(quant * DIFF[code], 8)
                ncur = cur + delta
                if ncur < -32768:
                    ncur = -32768
                elif ncur > 32767:
                    ncur = 32767
                nq = (quant * SCALE[m]) >> 8
                if nq < QMIN:
                    nq = QMIN
                elif nq > QMAX:
                    nq = QMAX
                e = x - ncur
                ns = sc + e * e
                if lambda_step:
                    ns += lambda_step * (nq - quant) * (nq - quant)
                cand.append((ns, ncur, nq, pi, code))

        cand.sort(key=lambda c: c[0])

        # Dedup identical (cur, quant) states (keep best score) so the beam holds
        # genuinely distinct trajectories rather than near-duplicates.
        seen = set()
        beams = []
        chosen = []
        for ns, ncur, nq, pi, code in cand:
            k = (ncur, nq)
            if k in seen:
                continue
            seen.add(k)
            beams.append((ns, ncur, nq))
            chosen.append((pi, code))
            if len(beams) >= beam:
                break
        back.append(chosen)

    # Best final trajectory, then backtrack to recover the nibble stream.
    bi = min(range(len(beams)), key=lambda i: beams[i][0])
    codes = [0] * n
    for t in range(n - 1, -1, -1):
        pi, code = back[t][bi]
        codes[t] = code
        bi = pi

    out = bytearray((n + 1) // 2)
    for i, c in enumerate(codes):
        if i & 1:
            out[i >> 1] |= c << 4
        else:
            out[i >> 1] |= c
    return bytes(out), n
