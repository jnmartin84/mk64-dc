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

#define DMEM_BUF_SIZE 2560
//0x1000
//0x17E0
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

static struct  __attribute__((aligned(32)))  {
//    int16_t __attribute__((aligned(32))) after[512];;
    union  __attribute__((aligned(32))) {
        int16_t __attribute__((aligned(32))) as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t __attribute__((aligned(32))) as_u8[DMEM_BUF_SIZE];
    } buf;
//    int16_t __attribute__((aligned(32))) before[512];;

    ADPCM_STATE* adpcm_loop_state;
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;
    uint16_t vol_wet;
    uint16_t rate_wet;
    uint16_t vol[2];
    uint16_t rate[2];
#if 1
    float  __attribute__((aligned(32))) adpcm_table[8][2][8];
#else
    int16_t  __attribute__((aligned(32))) adpcm_table[8][2][8];
#endif
} rspa = {0};

#define MEM_BARRIER()         asm volatile("": : : "memory");
#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r" ((ptr)) : "memory")

static int16_t __attribute__((aligned(32))) resample_table[64][4] = {
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
void n64_memset(void *dst, uint8_t val, size_t size);
void aClearBufferImpl(uint16_t addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    /* n64_ */memset(BUF_U8(addr), 0, nbytes);
}

#include <stdio.h>
void n64_memcpy(void *dst, const void *src, size_t size);

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
#if 0
    static int called = 0;
    if (!called) {
        for (int i=0;i<512;i++) {
            rspa.before[i] = 0;
            rspa.after[i] = 0;
        }
        for (int i=0;i<2560;i++) {
            rspa.buf.as_u8[i] = i&0xf;
        }
    }
    if (called == 32768) {
        for (int i=0;i<4096;i++) {
            printf("%02x ", rspa.buf.as_u8[i]);
            if (i % 16 == 0) {
                printf("\n");
            }
        }
        printf("\n");
    }
    called++;
#endif
    n64_memcpy(BUF_U8(dest_addr), source_addr, ROUND_DOWN_16(nbytes));
}

void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    n64_memcpy(dest_addr, BUF_S16(source_addr), ROUND_DOWN_16(nbytes));
}

#if 0
void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
#if 1
    float *aptr = (float*)rspa.adpcm_table;
    for (int i=0;i<num_entries_times_16 / 2;i++) {
        aptr[i] = (float)(s32)__builtin_bswap16(book_source_addr[i]);    
    }
#else
    int16_t *aptr = (int16_t*)rspa.adpcm_table;
    for (int i=0;i<num_entries_times_16 / 2;i++) {
        aptr[i] = /* (float)(s32) */(int16_t)__builtin_bswap16(book_source_addr[i]);    
    }

#endif
}
#endif
void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    float *aptr = (float*)rspa.adpcm_table;
    short tmp[8];

    __builtin_prefetch(book_source_addr);

    for (int i=0;i<num_entries_times_16 / 2; i += 8) {
        __builtin_prefetch(&aptr[i]);
#if 1
        tmp[0] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 0]);
        tmp[1] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 1]);
        tmp[2] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 2]);
        tmp[3] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 3]);
        tmp[4] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 4]);
        tmp[5] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 5]);
        tmp[6] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 6]);
        tmp[7] = (short)/* __builtin_bswap16 */BSWAP16(book_source_addr[i + 7]);
#endif
        MEM_BARRIER_PREF(&book_source_addr[i + 8]);
        aptr[i + 0] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 0]; */tmp[0];
        aptr[i + 1] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 1]; */tmp[1];
        aptr[i + 2] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 2]; */tmp[2];
        aptr[i + 3] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 3]; */tmp[3];
        aptr[i + 4] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 4]; */tmp[4];
        aptr[i + 5] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 5]; */tmp[5];
        aptr[i + 6] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 6]; */tmp[6];
        aptr[i + 7] = 0.00048828f * (f32)(s32)/* book_source_addr[i + 7]; */tmp[7];
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
#if 1
void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);

    //int16_t* d = BUF_S16(rspa.out);
    int32_t* d = (int32_t*)(((uintptr_t)BUF_S16(rspa.out)+3) & ~3);
//for(int i=0;i<512;i+=64) {
//    printf("before %d after %d\n", rspa.before[0], rspa.after[511]);
//}
    __builtin_prefetch(r);

    while (count > 0) {
        __builtin_prefetch(r+16);
        int32_t lr0 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr1 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr2 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr3 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr4 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr5 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr6 = (*r++ << 16) | (*l++ & 0xffff);
        int32_t lr7 = (*r++ << 16) | (*l++ & 0xffff);
#if 1
            asm volatile("": : : "memory");
#endif
        *d++ = lr0;
        *d++ = lr1;
        *d++ = lr2;
        *d++ = lr3;
        *d++ = lr4;
        *d++ = lr5;
        *d++ = lr6;
        *d++ = lr7;

        --count;
    }
}
#endif
#if 0
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
#endif

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}

void aDMEMCopyImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    n64_memcpy(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}


void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}
#if 1
#include <kos.h>

//https://fgiesen.wordpress.com/2024/10/23/zero-or-sign-extend/
static inline int extend_nyb(int n) {
    return (n ^ 8) - 8;
}
#if 1
#include "sh4zam.h"

inline static  void shz_xmtrx_load_3x4_rows(const shz_vec4_t *r1,
                                            const shz_vec4_t *r2,
                                            const shz_vec4_t *r3) {
    asm volatile(R"(
        pref    @%0
        frchg

        fldi0   fr12
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15

        pref    @%1
        fmov.s  @%0+, fr0
        fmov.s  @%0+, fr1
        fmov.s  @%0+, fr2
        fmov.s  @%0,  fr3

        pref    @%2
        fmov.s  @%1+, fr4
        fmov.s  @%1+, fr5
        fmov.s  @%1+, fr6
        fmov.s  @%1,  fr7

        fmov.s  @%2+, fr8
        fmov.s  @%2+, fr9
        fmov.s  @%2+, fr10
        fmov.s  @%2,  fr11

        frchg
    )"
    : "+&r" (r1), "+&r" (r2), "+&r" (r3));
}

#define SHZ_FORCE_INLINE __attribute__((always_inline))
SHZ_FORCE_INLINE void shz_copy_16_shorts(void *restrict dst, const void *restrict src) {
    asm volatile(R"(
        mov.w   @%[s]+, r0
        mov.w   @%[s]+, r1
        mov.w   @%[s]+, r2
        mov.w   @%[s]+, r3
        mov.w   @%[s]+, r4
        mov.w   @%[s]+, r5
        mov.w   @%[s]+, r6
        mov.w   @%[s]+, r7
        add     #16, %[d]
        mov.w   r7, @-%[d]
        mov.w   r6, @-%[d]
        mov.w   r5, @-%[d]
        mov.w   r4, @-%[d]
        mov.w   r3, @-%[d]
        mov.w   r2, @-%[d]
        mov.w   r1, @-%[d]
        mov.w   r0, @-%[d]
        mov.w   @%[s]+, r0
        mov.w   @%[s]+, r1
        mov.w   @%[s]+, r2
        mov.w   @%[s]+, r3
        mov.w   @%[s]+, r4
        mov.w   @%[s]+, r5
        mov.w   @%[s]+, r6
        mov.w   @%[s]+, r7
        add     #32, %[d]
        mov.w   r7, @-%[d]
        mov.w   r6, @-%[d]
        mov.w   r5, @-%[d]
        mov.w   r4, @-%[d]
        mov.w   r3, @-%[d]
        mov.w   r2, @-%[d]
        mov.w   r1, @-%[d]
        mov.w   r0, @-%[d]
    )"
    : [d] "+r" (dst), [s] "+r" (src)
    :
    : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "memory");
}

SHZ_FORCE_INLINE void shz_zero_16_shorts(void *dst) {
    asm volatile(R"(
        xor     r0, r0
        add     #32 %0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
    )"
    : 
    : "r" (dst)
    : "r0", "memory");
}


#define recip8192 0.00012207f
#define recip2048 0.00048828f

#if 0
static inline float clamp16f(float v) {
//    float tv = v * 0.00048828f;
//    float temp = tv + 32767.0f - fabsf(tv - 32767.0f);
//    return (temp + (-65536.0f) + fabsf(temp + 65536.0f)) * 0.25f;
    float tv = v * recip8192;
    float temp = tv + 8191.0f - fabsf(tv - 8191.0f);
    return (temp + (-16384.0f) + fabsf(temp + 16384.0f));
}
#endif
#if 1
static inline float clamp16f(float v) {
//    v *= 0.00048828f;
    if (v < -32768.0f) {
        return -32768.0f;
    } else if (v > 32767.0f) {
        return 32767.0f;
    }
    return v;
}

static inline int16_t clamp16fs(float v) {
//    v *= 0.00048828f;
    if (v < -32768.0f) {//-67108864.0f) {
        v = -32768.0f;//-67108864.0f;
    } else if (v > 32767.0f) {//67106816.0f) {
        v = 32767.0f;//67106816.0f;
    }
    return (int16_t)(((s32)v));// >> 11);
}
#endif
void __attribute__((optimization("-Os"))) aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    int16_t* out = BUF_S16(rspa.out);
    __builtin_prefetch(out);
    uint8_t* in = BUF_U8(rspa.in);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        shz_zero_16_shorts(out);
    } else if (flags & A_LOOP) {
        shz_copy_16_shorts(out, rspa.adpcm_loop_state);
    } else {
        shz_copy_16_shorts(out, state);
    }   
    __builtin_prefetch(in);
    out += 16;
    float prev1 = (f32)(s32)out[-1];
    float prev2 = (f32)(s32)out[-2];

    while (nbytes > 0) {
        uint8_t si_in = *in++;
        MEM_BARRIER_PREF(out);
        int shift = (si_in >> 4) & 0xf;          // should be in 0..12 or 0..14
        int table_index = si_in & 0xf; // should be in 0..7
        float(*tbl)[8] = rspa.adpcm_table[table_index];
        int i;
        __builtin_prefetch(tbl);

        for (i = 0; i < 2; i++) {
            float ins[8];

            int j, k;
            shz_vec4_t acc_vec[2];
            float *accf = (float *)acc_vec;
            const shz_vec4_t in_vec = { .x = prev2, .y = prev1, .z = 1.0f/* 2048.0f */ };
            {
                uint8_t in81 = *in++;
                uint8_t in82 = *in++;
                ins[0] = (f32)((s32)extend_nyb((in81 >> 4)  & 0xf) << shift);
                ins[1] = (f32)((s32)extend_nyb( in81        & 0xf) << shift);
                ins[2] = (f32)((s32)extend_nyb((in82 >> 4)  & 0xf) << shift);
                ins[3] = (f32)((s32)extend_nyb( in82        & 0xf) << shift);
            }
            
            shz_xmtrx_load_3x4_rows(&tbl[0][0],
                                    &tbl[1][0],
                                    &ins[0]);
        
            acc_vec[0] = shz_xmtrx_trans_vec4(in_vec);
            {
                uint8_t in83 = *in++;
                uint8_t in84 = *in++;
                ins[4] = (f32)((s32)extend_nyb((in83 >> 4)  & 0xf) << shift);
                ins[5] = (f32)((s32)extend_nyb( in83        & 0xf) << shift);
                ins[6] = (f32)((s32)extend_nyb((in84 >> 4)  & 0xf) << shift);
                ins[7] = (f32)((s32)extend_nyb( in84        & 0xf) << shift);
            }

            shz_xmtrx_load_3x4_rows(&tbl[0][4],
                                    &tbl[1][4],
                                    &ins[4]);

            acc_vec[1] = shz_xmtrx_trans_vec4(in_vec);           
#if 0
            for(j = 0; j < 8; ++j) {
                for(k = 0; k < j; ++k)
                    accf[j] += tbl[1][((j - k) - 1)] * ins[k];

//                accf[j] *= 0.00048828f; // /= 2048.0f;
                accf[j] = clamp16f(accf[j]);
                *out++ = accf[j];
            }
#else
            //float accaccf = accf[0];
            //accf[0] = clamp16f(accaccf);
//            *out++ = clamp16f(accaccf);

            accf[1] = accf[1] + (tbl[1][0] * ins[0]);
//            *out++ = clamp16f(accaccf);

            accf[2] = fipr(accf[2], tbl[1][1], tbl[1][0], 0, 1, ins[0], ins[1], 0);
            //*out++ = clamp16f(accaccf);

            accf[3] = fipr(accf[3], tbl[1][2], tbl[1][1], tbl[1][0], 1, ins[0], ins[1], ins[2]);
            //*out++ = clamp16f(accaccf);

            accf[4] = fipr(accf[4], tbl[1][3], tbl[1][2], tbl[1][1], 1, ins[0], ins[1], ins[2]);
            accf[4] += (tbl[1][0] * ins[3]);
            //*out++ = clamp16f(accaccf);

            accf[5] = fipr(accf[5], tbl[1][4], tbl[1][3], tbl[1][2], 1, ins[0], ins[1], ins[2]);
            accf[5] += (tbl[1][1] * ins[3]) + (tbl[1][0] * ins[4]);
// if you leave this in it gets spooky
            ///            *out++ = clamp16f(accaccf);

            accf[6] = fipr(accf[6], tbl[1][5], tbl[1][4], tbl[1][3], 1, ins[0], ins[1], ins[2]);  
            accf[6] += fipr(tbl[1][2], tbl[1][1], tbl[1][0], 0, ins[3], ins[4], ins[5], 0);   
            //prev2 = clamp16f(accaccf);
            //*out++ = prev2;//clamp16f(accaccf);

            accf[7] = fipr(accf[7], tbl[1][6], tbl[1][5], tbl[1][4], 1, ins[0], ins[1], ins[2]);
            accf[7] += fipr(tbl[1][3], tbl[1][2], tbl[1][1], tbl[1][0], ins[3], ins[4], ins[5], ins[6]);

//MEM_BARRIER();

            //prev1 = clamp16f(accaccf);//accf[7];
            //*out++ = prev1;
            *out++ = clamp16f(accf[0]);
            *out++ = clamp16f(accf[1]);
            *out++ = clamp16f(accf[2]);
            *out++ = clamp16f(accf[3]);
            *out++ = clamp16f(accf[4]);
            *out++ = clamp16f(accf[5]);
            prev2 = *out++ = clamp16f(accf[6]);
            prev1 = *out++ = clamp16f(accf[7]);


            #endif
//            prev1 = accf[7];
  //          prev2 = accf[6];
        }
        MEM_BARRIER_PREF(in);
        nbytes -= 16 * sizeof(int16_t);
    }

    shz_copy_16_shorts(state, (out - 16));
}
#else
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        n64_memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        n64_memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        n64_memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        uint8_t si_in = *in++;
        int shift = (si_in >> 4);          // should be in 0..12 or 0..14
        int table_index = si_in & 0xf; // should be in 0..7
        int16_t(*tbl)[8] = rspa.adpcm_table[table_index];
        int i;

        for (i = 0; i < 2; i++) {
            int16_t ins[8];
            float prev1 = (f32)(s32)out[-1];
            float prev2 = (f32)(s32)out[-2];
            int j, k;
//            for (j = 0; j < 8; j+=2) {
            {
                uint8_t in81 = *in++;
                uint8_t in82 = *in++;
                uint8_t in83 = *in++;
                uint8_t in84 = *in++;
                ins[0] = extend_nyb((in81 >> 4)  & 0xf) << shift;
                ins[1] = extend_nyb( in81        & 0xf) << shift;
                ins[2] = extend_nyb((in82 >> 4)  & 0xf) << shift;
                ins[3] = extend_nyb( in82        & 0xf) << shift;
                ins[4] = extend_nyb((in83 >> 4)  & 0xf) << shift;
                ins[5] = extend_nyb( in83        & 0xf) << shift;
                ins[6] = extend_nyb((in84  >> 4) & 0xf) << shift;
                ins[7] = extend_nyb( in84        & 0xf) << shift;
            }
//            }
            for (j = 0; j < 8; j++) {
                int32_t acc = 
                (s32)fipr((f32)(s32)tbl[0][j], (f32)(s32)tbl[1][j], (f32)(s32)ins[j], 0, prev2, prev1, 2048.0f, 0);

                for (k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }
                acc >>= 11;

                *out++ = clamp16(acc);
            }
        }
        nbytes -= 16 * sizeof(int16_t);
    }
//    n64_memcpy(state, out - 16, 16 * sizeof(int16_t));
    int16_t *sp = state;
    int16_t *op = (int16_t *)(out - 16);
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
    *sp++ = *op++;
}
#endif
#if 0
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);

    if (flags & A_INIT) {
        n64_memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        n64_memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        n64_memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        int shift = 28 - (*in >> 4);      // scale factor
        int table_index = *in++ & 0xF;    // predictor index
        int16_t(*tbl)[8] = rspa.adpcm_table[table_index];

        for (int i = 0; i < 2; i++) {
            int16_t ins[8];
            int32_t acc[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];

            // Expand 4 bytes -> 8 samples
            for (int j = 0; j < 8; j+=2) {
                uint8_t in8 = *in++;
                // Sign extend nibble: shift left into sign bit, then shift back down
                ins[j]     = (((in8 >> 4) & 0x0F) << 28) >> shift;
                ins[j + 1] = (((in8 & 0x0F)) << 28) >> shift;
            }

            // Init each output sample with taps for prev1, prev2, and ins[j]
            for (int j = 0; j < 8; j++) {
                acc[j] = //tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);
                (s32)(fipr((f32)(s32)tbl[0][j], (f32)(s32)tbl[1][j], (f32)(s32)ins[j], 0, (f32)(s32)prev2, (f32)(s32)prev1, 2048.0f, 0));
            }

            // Accumulate FIR taps: k outer, j inner
            for (int k = 0; k < 7; k++) {
                for (int j = k + 1; j < 8; j++) {
                    acc[j] += tbl[1][(j - k) - 1] * ins[k];
                }
            }

            // Finalize, shift, clamp, output
            for (int j = 0; j < 8; j++) {
                int32_t sample = acc[j] >> 11;
                *out++ = clamp16(sample);
            }
        }

        nbytes -= 16 * sizeof(int16_t);
    }

    n64_memcpy(state, out - 16, 16 * sizeof(int16_t));
}
#endif
#endif

#if 1
void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t __attribute__((aligned(32))) tmp[32] = {0};
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator = 0;
    int i = 0;
    int16_t* tbl = NULL;
    float sample_f = 0;

    int16_t *dp,*sp;

    if (flags & A_INIT) {
        /* n64_ */memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        n64_memcpy(tmp, state, 16 * sizeof(int16_t));
    }
    if (flags & 2) {
        n64_memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= (tmp[5] >> 1);// / sizeof(int16_t);
    }
    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    
//    n64_memcpy(in, tmp, 4 * sizeof(int16_t));
    dp = in;
    sp = tmp;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;

    do {
#if 0
        uint32_t pa[8];
        uint32_t inp[8];
        int16_t tbls[8];
        pa[0] = pitch_accumulator;
        inp[0] = 0;
        tbls[0] = resample_table[pa[0] >> 10];

        pa[1] = pa[0] + (pitch << 1);
        inp[1] = inp[0] + (pa[1] >> 16);
        tbl[1] = resample_table[pa[1] >> 10];

        pa[2] = (pa[1]&0xffff) + (pitch << 1);
        inp[2] = inp[1] + (pa[2] >> 16);
        tbls[2] = resample_table[pa[2] >> 10];

        pa[3] = (pa[2]0xffff) + (pitch << 1);
        inp[3] = inp[2] + (pa[3] >> 16);
        tbls[3] = resample_table[pa[3] >> 10];

        pa[4] = (pa[3]&0xffff) + (pitch << 1);
        inp[4] = inp[3] + (pa[4] >> 16);
        tbls[4] = resample_table[pa[4] >> 10];

        pa[5] = (pa[4]&0xffff) + (pitch << 1);
        inp[5] = inp[4] + (pa[5] >> 16);
        tbls[5] = resample_table[pa[5] >> 10];

        pa[6] = (pa[5]&0xffff) + (pitch << 1);
        inp[6] = inp[5] + (pa[6] >> 16);
        tbls[6] = resample_table[pa[6] >> 10];

        pa[7] = (pa[6]&0xffff) + (pitch << 1);
        inp[7] = inp[6] + (pa[7] >> 16);
        tbls[7] = resample_table[pa[7] >> 10];
#endif

        for (i = 0; i < 8; i++) {
            tbl = resample_table[pitch_accumulator >> 10];
            float in_f[4] = {(float)(int)in[0],(float)(int)in[1],(float)(int)in[2],(float)(int)in[3]};
            float tbl_f[4] = {(float)(int)tbl[0],(float)(int)tbl[1],(float)(int)tbl[2],(float)(int)tbl[3]};

            sample_f = fipr(in_f[0],in_f[1],in_f[2],in_f[3],
                tbl_f[0],tbl_f[1],tbl_f[2],tbl_f[3]) * 0.00003052f;

            *out++ = clamp16((s32)sample_f);

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
//    n64_memcpy(state, in, 4 * sizeof(int16_t));
    dp = (int16_t*)(state);
    sp = in;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
//    n64_memcpy(state + 8, in, 8 * sizeof(int16_t));
    dp = (int16_t*)(state + 8);
    sp = in;

    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
    *dp++ = *sp++;
}
#else
void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t __attribute__((aligned(32))) tmp[32] = {0};
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    MEM_BARRIER_PREF(in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator = 0;
    int i = 0;
    int16_t* tbl = NULL;

    MEM_BARRIER_PREF(state);

    if (flags & A_INIT) {
        n64_memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        n64_memcpy(tmp, state, 16 * sizeof(int16_t));
    }
    if (flags & 2) {
        n64_memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= (tmp[5] >> 1);// / sizeof(int16_t);
    }
    __builtin_prefetch(out);
    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    tbl = resample_table[pitch_accumulator >> 10];
    MEM_BARRIER_PREF(tbl);

    n64_memcpy(in, tmp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            float in_f[4] = {(float)(int)in[0],(float)(int)in[1],(float)(int)in[2],(float)(int)in[3]};
            float tbl_f[4] = {(float)(int)tbl[0],(float)(int)tbl[1],(float)(int)tbl[2],(float)(int)tbl[3]};
            float sample_f = shz_dot8f(in_f[0],in_f[1],in_f[2],in_f[3],
                            tbl_f[0],tbl_f[1],tbl_f[2],tbl_f[3]) * 0.00003052f;

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;

            tbl = resample_table[pitch_accumulator >> 10];
            MEM_BARRIER_PREF(tbl);   
            *out++ = clamp16((s32)sample_f);
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    n64_memcpy(state, in, 4 * sizeof(int16_t));
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    n64_memcpy(state + 8, in, 8 * sizeof(int16_t));
}
#endif

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

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);
    int nbytes = ROUND_UP_32(count);

    do {
        // out and in never overlap as called by `synthesis.c`
        n64_memcpy(out, in, nbytes);
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