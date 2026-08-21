/*
    ya2beam - full-buffer beam-search AICA-ADPCM encoder, callable from Python.

    This is the byte-exact C equivalent of tools/aica/yamaha_adpcm_v2.py::encode
    (the SF64 build's "ya2" beam encoder). It reuses wavbeam.c's verified beam_step
    (identical DIFF/SCALE tables, (score,parent,code) tie-break == Python's stable
    sort, (cur,quant) dedup, lowest-score backtrack), but runs ONE block spanning
    the whole sample -- i.e. a full-buffer beam, matching ya2 exactly (ya2 keeps a
    per-sample `back` array over the entire buffer and backtracks from the global
    best final node). Memory is O(n * width * 2) bytes for the backtrack table,
    same growth as the Python list.

    Built on demand by ya2beam_c.py (cc -O3 -shared -fPIC). Pure integer inner loop
    -> ~100x the pure-Python encoder, which is the whole point: the SF64 audio build
    runs from scratch (no cache) in low-compute Colab and was timing out.
*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BEAM_MAX_WIDTH 256

static const int DIFF[16]  = {1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15};
static const int SCALE[8]  = {0xE6,0xE6,0xE6,0xE6,0x133,0x199,0x200,0x266};

typedef struct { double score; int cur, quant, parent, code; } bcand_t;

/* Total order matching Python: primary score, then insertion order (parent,code).
   Python builds candidates as for pi: for code, then does a STABLE sort by score,
   so equal scores retain (parent,code) order -- exactly this comparator. */
static inline int cand_less(const bcand_t *a, const bcand_t *b) {
    if (a->score < b->score) return 1;
    if (a->score > b->score) return 0;
    if (a->parent != b->parent) return a->parent < b->parent;
    return a->code < b->code;
}

static inline void sift_down(bcand_t *h, int i, int m) {
    for (;;) {
        int l = 2*i+1, r = 2*i+2, s = i;
        if (l < m && cand_less(&h[l], &h[s])) s = l;
        if (r < m && cand_less(&h[r], &h[s])) s = r;
        if (s == i) break;
        bcand_t tmp = h[i]; h[i] = h[s]; h[s] = tmp; i = s;
    }
}

/* One beam step: expand `nb` beams over all 16 codes, keep the `width` lowest-score
   UNIQUE (cur,quant) states via a partial min-heap select. Records the chosen
   nibble + parent into bt[] for backtracking. Returns the new beam count.
   (Verbatim from wavbeam.c -- the standalone-verified core.) */
static int beam_step(bcand_t *beams, int nb, int x, bcand_t *cand,
                     uint16_t *bt, uint32_t *hkey, int hsize, int width) {
    int nc = 0, kept = 0, pi, code;
    for (pi = 0; pi < nb; pi++) {
        int cur = beams[pi].cur, quant = beams[pi].quant;
        double sc = beams[pi].score;
        for (code = 0; code < 16; code++) {
            int m = code & 7, delta = (quant*DIFF[code])/8, ncur = cur + delta, nq = (quant*SCALE[m])>>8;
            double e;
            if (ncur < -32768) ncur = -32768; else if (ncur > 32767) ncur = 32767;
            if (nq < 127) nq = 127; else if (nq > 24576) nq = 24576;
            e = (double)(x - ncur);
            cand[nc].score = sc + e*e; cand[nc].cur = ncur; cand[nc].quant = nq;
            cand[nc].parent = pi; cand[nc].code = code; nc++;
        }
    }
    for (int i = nc/2 - 1; i >= 0; i--) sift_down(cand, i, nc);
    memset(hkey, 0, (size_t)hsize*sizeof(*hkey));
    int m = nc;
    while (m > 0 && kept < width) {
        bcand_t top = cand[0];
        cand[0] = cand[m-1]; m--; sift_down(cand, 0, m);
        uint32_t key = ((uint32_t)(top.cur + 32768) << 15) | (uint32_t)top.quant;
        uint32_t h = (key*2654435761u) & (uint32_t)(hsize - 1);
        int dup = 0;
        while (hkey[h]) { if (hkey[h] == key + 1) { dup = 1; break; } h = (h+1) & (uint32_t)(hsize - 1); }
        if (dup) continue;
        hkey[h] = key + 1;
        beams[kept].score = top.score; beams[kept].cur = top.cur; beams[kept].quant = top.quant;
        bt[kept] = (uint16_t)((top.parent << 4) | top.code); kept++;
    }
    return kept;
}

/* Full-buffer beam encode: `n` int16 PCM -> (n+1)/2 packed ADPCM bytes in `out`
   (even sample -> low nibble, odd -> high). Byte-identical to ya2.encode(pcm,beam).
   Returns 0 on success, -1 on allocation failure. `out` must hold (n+1)/2 bytes. */
int ya2beam_encode(const int16_t *pcm, int n, int width, uint8_t *out) {
    if (n <= 0) return 0;
    if (width < 1) width = 1; else if (width > BEAM_MAX_WIDTH) width = BEAM_MAX_WIDTH;

    int hsize = 1; while (hsize < width*16*2) hsize <<= 1;
    bcand_t  *beams = malloc((size_t)width * sizeof(*beams));
    bcand_t  *cand  = malloc((size_t)width * 16 * sizeof(*cand));
    uint16_t *back  = malloc((size_t)n * width * sizeof(*back));
    uint8_t  *codes = malloc((size_t)n);
    uint32_t *hkey  = malloc((size_t)hsize * sizeof(*hkey));
    if (!beams || !cand || !back || !codes || !hkey) {
        free(beams); free(cand); free(back); free(codes); free(hkey);
        return -1;
    }

    memset(out, 0, (size_t)((n + 1) / 2));
    int nb = 1;
    beams[0].score = 0; beams[0].cur = 0; beams[0].quant = 127; beams[0].parent = 0; beams[0].code = 0;

    for (int j = 0; j < n; j++)
        nb = beam_step(beams, nb, pcm[j], cand, back + (size_t)j*width, hkey, hsize, width);

    /* beams[0] is the lowest-score surviving trajectory (heap-select pops ascending),
       == Python's min(beams, key=score). Backtrack the nibble stream from it. */
    int bi = 0;
    for (int j = n - 1; j >= 0; j--) { uint16_t e = back[(size_t)j*width + bi]; codes[j] = e & 0xF; bi = e >> 4; }
    for (int j = 0; j < n; j++) { if (j & 1) out[j >> 1] |= codes[j] << 4; else out[j >> 1] |= codes[j]; }

    free(beams); free(cand); free(back); free(codes); free(hkey);
    return 0;
}
