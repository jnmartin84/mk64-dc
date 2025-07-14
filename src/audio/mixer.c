#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef bool

#include <PR/ultratypes.h>
#include <align_asset_macro.h>
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#include "mixer.h"
#define BSWAP16(x) (((x) & 0xff) << 8 | (((x) >> 8) & 0xff))

#define ROUND_UP_64(v) (((v) + 63) & ~63)
#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)
#define ROUND_DOWN_16(v) ((v) & ~0xf)

#define DMEM_BUF_SIZE 0x17E0
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

static struct  __attribute__((aligned(32)))  {
    union  __attribute__((aligned(32))) {
        int16_t __attribute__((aligned(32))) as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t __attribute__((aligned(32))) as_u8[DMEM_BUF_SIZE];
    } buf;

    ADPCM_STATE* adpcm_loop_state;
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;
    uint16_t filter_count;
    uint16_t vol_wet;
    uint16_t rate_wet;
    uint16_t vol[2];
    uint16_t rate[2];

    int16_t  __attribute__((aligned(32))) adpcm_table[8][2][8];
    int16_t  __attribute__((aligned(32))) filter[8];

} rspa;

static int16_t resample_table[64][4] = {
    { 0x0c39, 0x66ad, 0x0d46, 0xffdf }, { 0x0b39, 0x6696, 0x0e5f, 0xffd8 }, { 0x0a44, 0x6669, 0x0f83, 0xffd0 },
    { 0x095a, 0x6626, 0x10b4, 0xffc8 }, { 0x087d, 0x65cd, 0x11f0, 0xffbf }, { 0x07ab, 0x655e, 0x1338, 0xffb6 },
    { 0x06e4, 0x64d9, 0x148c, 0xffac }, { 0x0628, 0x643f, 0x15eb, 0xffa1 }, { 0x0577, 0x638f, 0x1756, 0xff96 },
    { 0x04d1, 0x62cb, 0x18cb, 0xff8a }, { 0x0435, 0x61f3, 0x1a4c, 0xff7e }, { 0x03a4, 0x6106, 0x1bd7, 0xff71 },
    { 0x031c, 0x6007, 0x1d6c, 0xff64 }, { 0x029f, 0x5ef5, 0x1f0b, 0xff56 }, { 0x022a, 0x5dd0, 0x20b3, 0xff48 },
    { 0x01be, 0x5c9a, 0x2264, 0xff3a }, { 0x015b, 0x5b53, 0x241e, 0xff2c }, { 0x0101, 0x59fc, 0x25e0, 0xff1e },
    { 0x00ae, 0x5896, 0x27a9, 0xff10 }, { 0x0063, 0x5720, 0x297a, 0xff02 }, { 0x001f, 0x559d, 0x2b50, 0xfef4 },
    { 0xffe2, 0x540d, 0x2d2c, 0xfee8 }, { 0xffac, 0x5270, 0x2f0d, 0xfedb }, { 0xff7c, 0x50c7, 0x30f3, 0xfed0 },
    { 0xff53, 0x4f14, 0x32dc, 0xfec6 }, { 0xff2e, 0x4d57, 0x34c8, 0xfebd }, { 0xff0f, 0x4b91, 0x36b6, 0xfeb6 },
    { 0xfef5, 0x49c2, 0x38a5, 0xfeb0 }, { 0xfedf, 0x47ed, 0x3a95, 0xfeac }, { 0xfece, 0x4611, 0x3c85, 0xfeab },
    { 0xfec0, 0x4430, 0x3e74, 0xfeac }, { 0xfeb6, 0x424a, 0x4060, 0xfeaf }, { 0xfeaf, 0x4060, 0x424a, 0xfeb6 },
    { 0xfeac, 0x3e74, 0x4430, 0xfec0 }, { 0xfeab, 0x3c85, 0x4611, 0xfece }, { 0xfeac, 0x3a95, 0x47ed, 0xfedf },
    { 0xfeb0, 0x38a5, 0x49c2, 0xfef5 }, { 0xfeb6, 0x36b6, 0x4b91, 0xff0f }, { 0xfebd, 0x34c8, 0x4d57, 0xff2e },
    { 0xfec6, 0x32dc, 0x4f14, 0xff53 }, { 0xfed0, 0x30f3, 0x50c7, 0xff7c }, { 0xfedb, 0x2f0d, 0x5270, 0xffac },
    { 0xfee8, 0x2d2c, 0x540d, 0xffe2 }, { 0xfef4, 0x2b50, 0x559d, 0x001f }, { 0xff02, 0x297a, 0x5720, 0x0063 },
    { 0xff10, 0x27a9, 0x5896, 0x00ae }, { 0xff1e, 0x25e0, 0x59fc, 0x0101 }, { 0xff2c, 0x241e, 0x5b53, 0x015b },
    { 0xff3a, 0x2264, 0x5c9a, 0x01be }, { 0xff48, 0x20b3, 0x5dd0, 0x022a }, { 0xff56, 0x1f0b, 0x5ef5, 0x029f },
    { 0xff64, 0x1d6c, 0x6007, 0x031c }, { 0xff71, 0x1bd7, 0x6106, 0x03a4 }, { 0xff7e, 0x1a4c, 0x61f3, 0x0435 },
    { 0xff8a, 0x18cb, 0x62cb, 0x04d1 }, { 0xff96, 0x1756, 0x638f, 0x0577 }, { 0xffa1, 0x15eb, 0x643f, 0x0628 },
    { 0xffac, 0x148c, 0x64d9, 0x06e4 }, { 0xffb6, 0x1338, 0x655e, 0x07ab }, { 0xffbf, 0x11f0, 0x65cd, 0x087d },
    { 0xffc8, 0x10b4, 0x6626, 0x095a }, { 0xffd0, 0x0f83, 0x6669, 0x0a44 }, { 0xffd8, 0x0e5f, 0x6696, 0x0b39 },
    { 0xffdf, 0x0d46, 0x66ad, 0x0c39 }
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t) v;
}

static inline int32_t clamp32(int64_t v) {
    if (v < -0x7fffffff - 1) {
        return -0x7fffffff - 1;
    } else if (v > 0x7fffffff) {
        return 0x7fffffff;
    }
    return (int32_t) v;
}

//void checked_memeset
uintptr_t DMEM_END = rspa.buf.as_u8 + DMEM_BUF_SIZE;

void aClearBufferImpl(uint16_t addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
 /*    if (((uintptr_t)BUF_U8(addr) + nbytes) >= DMEM_END) {
        printf("attempted to clear past end of DMEM\n");
        memset(BUF_U8(addr), 0, DMEM_END - (uintptr_t)BUF_U8(addr));
    } else { */
        memset(BUF_U8(addr), 0, nbytes);
   // }
}
#include <stdio.h>

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    //printf("source_addr: %08x\n dest_addr; %08x\n nbytes: %d\n", (uintptr_t)source_addr, (uintptr_t)BUF_U8(dest_addr), nbytes);
    /* if (((uintptr_t)BUF_U8(dest_addr) + ROUND_DOWN_16(nbytes)) >= DMEM_END) {
        printf("attempted to clear past end of DMEM\n");
        memcpy(BUF_U8(dest_addr), 0, DMEM_END - (uintptr_t)BUF_U8(dest_addr));
    } else { */
    memcpy(BUF_U8(dest_addr), source_addr, ROUND_DOWN_16(nbytes));
//}
}

void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
//    if (nbytes > 704) {nbytes = 704;}
    //printf("source_addr: %08x\n dest_addr; %08x\n nbytes: %d\n", (uintptr_t)BUF_S16(source_addr), (uintptr_t)dest_addr, nbytes);
    memcpy(dest_addr, BUF_S16(source_addr), ROUND_DOWN_16(nbytes));
}
static inline short SwapShort(short dat)
{
        return ((((dat << 8) | (dat >> 8 & 0xff)) << 16) >> 16);
}
//int16_t tmpstuff[512];
void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
//    memcpy(rspa.adpcm_table, book_source_addr, num_entries_times_16);

//    for (int i=0;i<num_entries_times_16 / 2;i++) {
  //      tmpstuff[i] = (short)SwapShort(book_source_addr[i]);    
   // }

//    memcpy(rspa.adpcm_table, tmpstuff, num_entries_times_16);
    short *aptr = (short*)rspa.adpcm_table;
    for (int i=0;i<num_entries_times_16 / 2;i++) {
        aptr[i] = (short)SwapShort(book_source_addr[i]);    
    }

}

void aSetBufferImpl(UNUSED uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

#if 0
void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);
    int16_t* d = BUF_S16(rspa.out);

    while (count > 0) {
        int16_t l0 = *l++;
        int16_t l1 = *l++;
        int16_t l2 = *l++;
        int16_t l3 = *l++;
        int16_t l4 = *l++;
        int16_t l5 = *l++;
        int16_t l6 = *l++;
        int16_t l7 = *l++;
        int16_t r0 = *r++;
        int16_t r1 = *r++;
        int16_t r2 = *r++;
        int16_t r3 = *r++;
        int16_t r4 = *r++;
        int16_t r5 = *r++;
        int16_t r6 = *r++;
        int16_t r7 = *r++;
//        //printf("%d %d\n", l0, r0);
        *d++ = l0;
        *d++ = r0;
        *d++ = l1;
        *d++ = r1;
        *d++ = l2;
        *d++ = r2;
        *d++ = l3;
        *d++ = r3;
        *d++ = l4;
        *d++ = r4;
        *d++ = l5;
        *d++ = r5;
        *d++ = l6;
        *d++ = r6;
        *d++ = l7;
        *d++ = r7;
        --count;
    }
}
#endif
#if 0
void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 4;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);
    //int16_t* d = BUF_S16(rspa.out);
    int32_t* d = (int32_t*)(((uintptr_t)BUF_S16(rspa.out)+3) & ~3);

    while (count > 0) {
        int32_t l0 = (*l++ << 16) | (*r++ & 0xffff);
        int32_t l1 = (*l++ << 16) | (*r++ & 0xffff);
        int32_t l2 = (*l++ << 16) | (*r++ & 0xffff);
        int32_t l3 = (*l++ << 16) | (*r++ & 0xffff);

        *d++ = l0;
        *d++ = l1;
        *d++ = l2;
        *d++ = l3;
        --count;
    }
}
#endif

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);
    int16_t* d = BUF_S16(rspa.out);

    __builtin_prefetch(l);
    while (count > 0) {
        __builtin_prefetch(r);
        int16_t l0 = *l++;
        int16_t l1 = *l++;
        int16_t l2 = *l++;
        int16_t l3 = *l++;
        int16_t l4 = *l++;
        int16_t l5 = *l++;
        int16_t l6 = *l++;
        int16_t l7 = *l++;
        __builtin_prefetch(d);
        int16_t r0 = *r++;
        int16_t r1 = *r++;
        int16_t r2 = *r++;
        int16_t r3 = *r++;
        int16_t r4 = *r++;
        int16_t r5 = *r++;
        int16_t r6 = *r++;
        int16_t r7 = *r++;
//        //printf("%d %d\n", l0, r0);
        __builtin_prefetch(l);
        *d++ = l0;
        *d++ = r0;
        *d++ = l1;
        *d++ = r1;
        *d++ = l2;
        *d++ = r2;
        *d++ = l3;
        *d++ = r3;
        *d++ = l4;
        *d++ = r4;
        *d++ = l5;
        *d++ = r5;
        *d++ = l6;
        *d++ = r6;
        *d++ = l7;
        *d++ = r7;
        --count;
    }
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}

void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        int shift = 28 - (*in >> 4);          // should be in 0..12 or 0..14
        int table_index = *in++ & 0xf; // should be in 0..7
        int16_t(*tbl)[8] = rspa.adpcm_table[table_index];
        int i;

        for (i = 0; i < 2; i++) {
            int16_t ins[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];
            int j, k;
            for (j = 0; j < 4; j++) {
                ins[j * 2] = (((*in >> 4) << 28) >> shift);//>> 28) << shift;
                ins[j * 2 + 1] = (((*in++ & 0xf) << 28) >> shift);//>> 28) << shift;
            }
            for (j = 0; j < 8; j++) {
                int32_t acc = tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);
                for (k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }
                acc >>= 11;
                *out++ = clamp16(acc);
            }
        }
        nbytes -= 16 * sizeof(int16_t);
    }
    memcpy(state, out - 16, 16 * sizeof(int16_t));
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t __attribute__((aligned(32))) tmp[32] = {0};
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator = 0;
    int i = 0;
    int16_t* tbl = NULL;
    int32_t sample = 0;

    if (flags & A_INIT) {
        memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        memcpy(tmp, state, 16 * sizeof(int16_t));
    }
    if (flags & 2) {
        memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= tmp[5] / sizeof(int16_t);
    }
    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    memcpy(in, tmp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            // tbl = resample_table[pitch_accumulator * 64 >> 16];
            tbl = resample_table[pitch_accumulator >> 10];
            sample = ((in[0] * tbl[0] + 0x4000) >> 15) + ((in[1] * tbl[1] + 0x4000) >> 15) +
                     ((in[2] * tbl[2] + 0x4000) >> 15) + ((in[3] * tbl[3] + 0x4000) >> 15);
            *out++ = clamp16(sample);

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}

void aEnvSetup1Impl(uint8_t initial_vol_wet, /* UNUSED  */uint16_t rate_wet, uint16_t rate_left, uint16_t rate_right) {
    rspa.vol_wet = (uint16_t) (initial_vol_wet << 8);
    rspa.rate_wet = 0;
    rspa.rate[0] = rate_left;
    rspa.rate[1] = rate_right;
}

void aEnvSetup2Impl(uint16_t initial_vol_left, uint16_t initial_vol_right) {
    rspa.vol[0] = initial_vol_left;
    rspa.vol[1] = initial_vol_right;
}

void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples, bool swap_reverb, bool neg_left, bool neg_right,
                   uint16_t dry_left_addr, uint16_t dry_right_addr, uint16_t wet_left_addr, uint16_t wet_right_addr) {
    int16_t* in = BUF_S16(in_addr);
    int16_t* dry[2] = { BUF_S16(dry_left_addr), BUF_S16(dry_right_addr) };
    int n = ROUND_UP_16(n_samples);

    uint16_t vols[2] = { rspa.vol[0], rspa.vol[1] };
    uint16_t rates[2] = { rspa.rate[0], rspa.rate[1] };
    __builtin_prefetch(in);
    do {
        for (int i = 0; i < 8; i++) {
            int16_t sample = *in++;
            int16_t dsampl1 = clamp16(*dry[0] + (sample * vols[0] >> 16));
            int16_t dsampl2 = clamp16(*dry[1] + (sample * vols[1] >> 16));
#if 1
            asm volatile("": : : "memory");
#endif
            *dry[0]++ = dsampl1; 
            *dry[1]++ = dsampl2; 
        }
        __builtin_prefetch(in);
        vols[0] += rates[0];
        vols[1] += rates[1];
        n -= 8;
    } while (n > 0);
}

void aSMixImpl(uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    int nbytes = ROUND_UP_32(ROUND_DOWN_16(count));
    int32_t* in = (int32_t *)(((uintptr_t)BUF_S16(in_addr)+3)&~3);
    int32_t* out = (int32_t *)(((uintptr_t)BUF_S16(out_addr)+3)&~3);
    __builtin_prefetch(in);
    int i = 0;
    int32_t sample1 = 0;
    int32_t sample2 = 0;
    int32_t outsamp;
    int32_t insamp;

    while (nbytes > 0) {
        __builtin_prefetch(out);
        for (i = 0; i < 8; i++) {
            insamp = *in++;
            sample1 = (insamp >> 16) & 0xffff;
            sample2 = (insamp & 0xffff);
#if 1
            asm volatile("": : : "memory");
#endif
            outsamp = *out;
            sample1 += (outsamp >> 16) & 0xffff;
            sample2 += (outsamp & 0xffff);
            *out++ = (clamp16(sample1)<<16) | clamp16(sample2);
        }    
        __builtin_prefetch(in);
        nbytes -= 16 * sizeof(int16_t);
    }
}

#if 0
void aMixImpl(int16_t gain, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    int nbytes = ROUND_UP_32(ROUND_DOWN_16(count));
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int i = 0;
    int32_t sample = 0;

    // have never observed this code path being taken
    if (gain == -0x8000) {
        while (nbytes > 0) {
            for (i = 0; i < 16; i++) {
                sample = *out - *in++;
                *out++ = clamp16(sample);
            }
            nbytes -= 16 * sizeof(int16_t);
        }
    }

    while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            sample = ((*out * 0x7fff + *in++ * gain) + 0x4000) >> 15;
            *out++ = clamp16(sample);
        }

        nbytes -= 16 * sizeof(int16_t);
    }
}
#endif

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);
    int nbytes = ROUND_UP_32(count);

    do {
        // out and in never overlap as called by `synthesis.c`
        memcpy(out, in, nbytes);
        out += nbytes;
    } while (t-- > 0);
}

void aDownsampleHalfImpl(
    uint16_t n_samples, uint16_t in_addr, uint16_t out_addr)
{
    int32_t * __restrict in =  (int32_t*)(((uintptr_t)BUF_S16(in_addr)+3) & ~3);
    __builtin_prefetch(in);
    int32_t * __restrict out = (int32_t*)(((uintptr_t)BUF_S16(out_addr)+3) & ~3);
    int n = ROUND_UP_8(n_samples);

    do {
        asm volatile("pref @%0" : : "r" (out) : "memory");
        uint32_t pair0 = *in++;
        uint32_t pair1 = *in++;
        uint32_t pair2 = *in++;
        uint32_t pair3 = *in++;
        asm volatile("pref @%0" : : "r" (in) : "memory");
        *out++ = ((int16_t)pair0 << 16) | (int16_t)pair1; // keep first, discard second
        *out++ = ((int16_t)pair2 << 16) | (int16_t)pair3; // keep first, discard second
        n -= 4;
    } while (n > 0);
}