//#include <stdbool.h>
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

#define recip8192 0.00012207f
#define recip2048 0.00048828f
#define recip2560 0.00039062f


#define ROUND_UP_64(v) (((v) + 63) & ~63)
#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)
#define ROUND_DOWN_16(v) ((v) & ~0xf)

#define DMEM_BUF_SIZE 2048
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

static struct  __attribute__((aligned(32)))  {
    union  __attribute__((aligned(32))) {
        int16_t __attribute__((aligned(32))) as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t __attribute__((aligned(32))) as_u8[DMEM_BUF_SIZE];
    } buf;

    float  __attribute__((aligned(32))) adpcm_table[8][2][8];
    uint16_t vol[2];
    uint16_t rate[2];

    int16_t adpcm_loop_state[16];
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;
} rspa = {0};

#define MEM_BARRIER()         asm volatile("": : : "memory");
#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r" ((ptr)) : "memory")

static float __attribute__((aligned(32))) resample_table[64][4] = {
{ (f32)3129, (f32)26285, (f32)3398, (f32)-33, },
{ (f32)2873, (f32)26262, (f32)3679, (f32)-40, },
{ (f32)2628, (f32)26217, (f32)3971, (f32)-48, },
{ (f32)2394, (f32)26150, (f32)4276, (f32)-56, },
{ (f32)2173, (f32)26061, (f32)4592, (f32)-65, },
{ (f32)1963, (f32)25950, (f32)4920, (f32)-74, },
{ (f32)1764, (f32)25817, (f32)5260, (f32)-84, },
{ (f32)1576, (f32)25663, (f32)5611, (f32)-95, },
{ (f32)1399, (f32)25487, (f32)5974, (f32)-106, },
{ (f32)1233, (f32)25291, (f32)6347, (f32)-118, },
{ (f32)1077, (f32)25075, (f32)6732, (f32)-130, },
{ (f32)932, (f32)24838, (f32)7127, (f32)-143, },
{ (f32)796, (f32)24583, (f32)7532, (f32)-156, },
{ (f32)671, (f32)24309, (f32)7947, (f32)-170, },
{ (f32)554, (f32)24016, (f32)8371, (f32)-184, },
{ (f32)446, (f32)23706, (f32)8804, (f32)-198, },
{ (f32)347, (f32)23379, (f32)9246, (f32)-212, },
{ (f32)257, (f32)23036, (f32)9696, (f32)-226, },
{ (f32)174, (f32)22678, (f32)10153, (f32)-240, },
{ (f32)99, (f32)22304, (f32)10618, (f32)-254, },
{ (f32)31, (f32)21917, (f32)11088, (f32)-268, },
{ (f32)-30, (f32)21517, (f32)11564, (f32)-280, },
{ (f32)-84, (f32)21104, (f32)12045, (f32)-293, },
{ (f32)-132, (f32)20679, (f32)12531, (f32)-304, },
{ (f32)-173, (f32)20244, (f32)13020, (f32)-314, },
{ (f32)-210, (f32)19799, (f32)13512, (f32)-323, },
{ (f32)-241, (f32)19345, (f32)14006, (f32)-330, },
{ (f32)-267, (f32)18882, (f32)14501, (f32)-336, },
{ (f32)-289, (f32)18413, (f32)14997, (f32)-340, },
{ (f32)-306, (f32)17937, (f32)15493, (f32)-341, },
{ (f32)-320, (f32)17456, (f32)15988, (f32)-340, },
{ (f32)-330, (f32)16970, (f32)16480, (f32)-337, },
{ (f32)-337, (f32)16480, (f32)16970, (f32)-330, },
{ (f32)-340, (f32)15988, (f32)17456, (f32)-320, },
{ (f32)-341, (f32)15493, (f32)17937, (f32)-306, },
{ (f32)-340, (f32)14997, (f32)18413, (f32)-289, },
{ (f32)-336, (f32)14501, (f32)18882, (f32)-267, },
{ (f32)-330, (f32)14006, (f32)19345, (f32)-241, },
{ (f32)-323, (f32)13512, (f32)19799, (f32)-210, },
{ (f32)-314, (f32)13020, (f32)20244, (f32)-173, },
{ (f32)-304, (f32)12531, (f32)20679, (f32)-132, },
{ (f32)-293, (f32)12045, (f32)21104, (f32)-84, },
{ (f32)-280, (f32)11564, (f32)21517, (f32)-30, },
{ (f32)-268, (f32)11088, (f32)21917, (f32)31, },
{ (f32)-254, (f32)10618, (f32)22304, (f32)99, },
{ (f32)-240, (f32)10153, (f32)22678, (f32)174, },
{ (f32)-226, (f32)9696, (f32)23036, (f32)257, },
{ (f32)-212, (f32)9246, (f32)23379, (f32)347, },
{ (f32)-198, (f32)8804, (f32)23706, (f32)446, },
{ (f32)-184, (f32)8371, (f32)24016, (f32)554, },
{ (f32)-170, (f32)7947, (f32)24309, (f32)671, },
{ (f32)-156, (f32)7532, (f32)24583, (f32)796, },
{ (f32)-143, (f32)7127, (f32)24838, (f32)932, },
{ (f32)-130, (f32)6732, (f32)25075, (f32)1077, },
{ (f32)-118, (f32)6347, (f32)25291, (f32)1233, },
{ (f32)-106, (f32)5974, (f32)25487, (f32)1399, },
{ (f32)-95, (f32)5611, (f32)25663, (f32)1576, },
{ (f32)-84, (f32)5260, (f32)25817, (f32)1764, },
{ (f32)-74, (f32)4920, (f32)25950, (f32)1963, },
{ (f32)-65, (f32)4592, (f32)26061, (f32)2173, },
{ (f32)-56, (f32)4276, (f32)26150, (f32)2394, },
{ (f32)-48, (f32)3971, (f32)26217, (f32)2628, },
{ (f32)-40, (f32)3679, (f32)26262, (f32)2873, },
{ (f32)-33, (f32)3398, (f32)26285, (f32)3129, },
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t) v;
}

void aClearBufferImpl(uint16_t addr, int nbytes) {
    memset(BUF_U8(addr), 0, ROUND_UP_16(nbytes));
}

void n64_memcpy(void *dst, const void *src, size_t size);

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    n64_memcpy(BUF_U8(dest_addr), source_addr, ROUND_DOWN_16(nbytes));
}

void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    n64_memcpy(dest_addr, BUF_S16(source_addr), ROUND_DOWN_16(nbytes));
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    float *aptr = (float*)rspa.adpcm_table;
    short tmp[8];

    __builtin_prefetch(book_source_addr);

    for (int i=0;i<num_entries_times_16 / 2; i += 8) {
        __builtin_prefetch(&aptr[i]);

        tmp[0] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 0]);
        tmp[1] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 1]);
        tmp[2] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 2]);
        tmp[3] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 3]);
        tmp[4] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 4]);
        tmp[5] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 5]);
        tmp[6] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 6]);
        tmp[7] = (short)__builtin_bswap16((uint16_t)book_source_addr[i + 7]);

        MEM_BARRIER_PREF(&book_source_addr[i + 8]);

        aptr[i + 0] = recip2048 * (f32)(s32)tmp[0];
        aptr[i + 1] = recip2048 * (f32)(s32)tmp[1];
        aptr[i + 2] = recip2048 * (f32)(s32)tmp[2];
        aptr[i + 3] = recip2048 * (f32)(s32)tmp[3];
        aptr[i + 4] = recip2048 * (f32)(s32)tmp[4];
        aptr[i + 5] = recip2048 * (f32)(s32)tmp[5];
        aptr[i + 6] = recip2048 * (f32)(s32)tmp[6];
        aptr[i + 7] = recip2048 * (f32)(s32)tmp[7];
    }
}

void aSetBufferImpl(UNUSED uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);

    uint32_t* d = (int32_t*)(((uintptr_t)BUF_S16(rspa.out)/* +3 */) & ~3);
    __builtin_prefetch(r);

    while (count > 0) {
        __builtin_prefetch(l);
        uint32_t lr0 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr1 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr2 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr3 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr4 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr5 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr6 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);
        uint32_t lr7 = ((uint16_t)*r++ << 16);// | ((uint16_t)*l++);

        __builtin_prefetch(r);

        lr0 |= ((uint16_t)*l++);
        lr1 |= ((uint16_t)*l++);
        lr2 |= ((uint16_t)*l++);
        lr3 |= ((uint16_t)*l++);
        lr4 |= ((uint16_t)*l++);
        lr5 |= ((uint16_t)*l++);
        lr6 |= ((uint16_t)*l++);
        lr7 |= ((uint16_t)*l++);
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
        __builtin_prefetch(d);

        --count;
    }
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}

void aDMEMCopyImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    n64_memcpy(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}

void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
   // rspa.adpcm_loop_state = adpcm_loop_state;
    n64_memcpy(rspa.adpcm_loop_state, adpcm_loop_state, 16*sizeof(int16_t));
    for (int i=0;i<16;i++) {
        rspa.adpcm_loop_state[i] = __builtin_bswap16(rspa.adpcm_loop_state[i]);
    }
}

//https://fgiesen.wordpress.com/2024/10/23/zero-or-sign-extend/
static inline int extend_nyb(int n) {
    return (n ^ 8) - 8;
}

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

static inline s16 clamp16f(float v) {
    //v *= recip2048;
    if (v < -32768.0f) {
        return -32768.0f;
    } else if (v > 32767.0f) {
        return 32767.0f;
    }
    return (s16)(s32)v;
}


static inline float shift_to_float_multiplier(uint8_t shift) {
    const static float __attribute__((aligned(32))) shift_to_float[16] = {
       1.0f,
       2.0f,
       4.0f, 
       8.0f,
       16.0f,
       32.0f,
       64.0f,
       128.0f,
       256.0f,
       512.0f,
       1024.0f,
       2048.0f,
       4096.0f,
       8192.0f,
       16364.0f, 
       32768.0f
    };
    return shift_to_float[shift];
}

static const float __attribute__((aligned(32))) nyblls_as_floats[256][2] = {
        {  0.0f,  0.0f },
        {  0.0f,  1.0f },
        {  0.0f,  2.0f },
        {  0.0f,  3.0f },
        {  0.0f,  4.0f },
        {  0.0f,  5.0f },
        {  0.0f,  6.0f },
        {  0.0f,  7.0f },
        {  0.0f, -8.0f },
        {  0.0f, -7.0f },
        {  0.0f, -6.0f },
        {  0.0f, -5.0f },
        {  0.0f, -4.0f },
        {  0.0f, -3.0f },
        {  0.0f, -2.0f },
        {  0.0f, -1.0f },
        {  1.0f,  0.0f },
        {  1.0f,  1.0f },
        {  1.0f,  2.0f },
        {  1.0f,  3.0f },
        {  1.0f,  4.0f },
        {  1.0f,  5.0f },
        {  1.0f,  6.0f },
        {  1.0f,  7.0f },
        {  1.0f, -8.0f },
        {  1.0f, -7.0f },
        {  1.0f, -6.0f },
        {  1.0f, -5.0f },
        {  1.0f, -4.0f },
        {  1.0f, -3.0f },
        {  1.0f, -2.0f },
        {  1.0f, -1.0f },
        {  2.0f,  0.0f },
        {  2.0f,  1.0f },
        {  2.0f,  2.0f },
        {  2.0f,  3.0f },
        {  2.0f,  4.0f },
        {  2.0f,  5.0f },
        {  2.0f,  6.0f },
        {  2.0f,  7.0f },
        {  2.0f, -8.0f },
        {  2.0f, -7.0f },
        {  2.0f, -6.0f },
        {  2.0f, -5.0f },
        {  2.0f, -4.0f },
        {  2.0f, -3.0f },
        {  2.0f, -2.0f },
        {  2.0f, -1.0f },
        {  3.0f,  0.0f },
        {  3.0f,  1.0f },
        {  3.0f,  2.0f },
        {  3.0f,  3.0f },
        {  3.0f,  4.0f },
        {  3.0f,  5.0f },
        {  3.0f,  6.0f },
        {  3.0f,  7.0f },
        {  3.0f, -8.0f },
        {  3.0f, -7.0f },
        {  3.0f, -6.0f },
        {  3.0f, -5.0f },
        {  3.0f, -4.0f },
        {  3.0f, -3.0f },
        {  3.0f, -2.0f },
        {  3.0f, -1.0f },
        {  4.0f,  0.0f },
        {  4.0f,  1.0f },
        {  4.0f,  2.0f },
        {  4.0f,  3.0f },
        {  4.0f,  4.0f },
        {  4.0f,  5.0f },
        {  4.0f,  6.0f },
        {  4.0f,  7.0f },
        {  4.0f, -8.0f },
        {  4.0f, -7.0f },
        {  4.0f, -6.0f },
        {  4.0f, -5.0f },
        {  4.0f, -4.0f },
        {  4.0f, -3.0f },
        {  4.0f, -2.0f },
        {  4.0f, -1.0f },
        {  5.0f,  0.0f },
        {  5.0f,  1.0f },
        {  5.0f,  2.0f },
        {  5.0f,  3.0f },
        {  5.0f,  4.0f },
        {  5.0f,  5.0f },
        {  5.0f,  6.0f },
        {  5.0f,  7.0f },
        {  5.0f, -8.0f },
        {  5.0f, -7.0f },
        {  5.0f, -6.0f },
        {  5.0f, -5.0f },
        {  5.0f, -4.0f },
        {  5.0f, -3.0f },
        {  5.0f, -2.0f },
        {  5.0f, -1.0f },
        {  6.0f,  0.0f },
        {  6.0f,  1.0f },
        {  6.0f,  2.0f },
        {  6.0f,  3.0f },
        {  6.0f,  4.0f },
        {  6.0f,  5.0f },
        {  6.0f,  6.0f },
        {  6.0f,  7.0f },
        {  6.0f, -8.0f },
        {  6.0f, -7.0f },
        {  6.0f, -6.0f },
        {  6.0f, -5.0f },
        {  6.0f, -4.0f },
        {  6.0f, -3.0f },
        {  6.0f, -2.0f },
        {  6.0f, -1.0f },
        {  7.0f,  0.0f },
        {  7.0f,  1.0f },
        {  7.0f,  2.0f },
        {  7.0f,  3.0f },
        {  7.0f,  4.0f },
        {  7.0f,  5.0f },
        {  7.0f,  6.0f },
        {  7.0f,  7.0f },
        {  7.0f, -8.0f },
        {  7.0f, -7.0f },
        {  7.0f, -6.0f },
        {  7.0f, -5.0f },
        {  7.0f, -4.0f },
        {  7.0f, -3.0f },
        {  7.0f, -2.0f },
        {  7.0f, -1.0f },
        { -8.0f,  0.0f },
        { -8.0f,  1.0f },
        { -8.0f,  2.0f },
        { -8.0f,  3.0f },
        { -8.0f,  4.0f },
        { -8.0f,  5.0f },
        { -8.0f,  6.0f },
        { -8.0f,  7.0f },
        { -8.0f, -8.0f },
        { -8.0f, -7.0f },
        { -8.0f, -6.0f },
        { -8.0f, -5.0f },
        { -8.0f, -4.0f },
        { -8.0f, -3.0f },
        { -8.0f, -2.0f },
        { -8.0f, -1.0f },
        { -7.0f,  0.0f },
        { -7.0f,  1.0f },
        { -7.0f,  2.0f },
        { -7.0f,  3.0f },
        { -7.0f,  4.0f },
        { -7.0f,  5.0f },
        { -7.0f,  6.0f },
        { -7.0f,  7.0f },
        { -7.0f, -8.0f },
        { -7.0f, -7.0f },
        { -7.0f, -6.0f },
        { -7.0f, -5.0f },
        { -7.0f, -4.0f },
        { -7.0f, -3.0f },
        { -7.0f, -2.0f },
        { -7.0f, -1.0f },
        { -6.0f,  0.0f },
        { -6.0f,  1.0f },
        { -6.0f,  2.0f },
        { -6.0f,  3.0f },
        { -6.0f,  4.0f },
        { -6.0f,  5.0f },
        { -6.0f,  6.0f },
        { -6.0f,  7.0f },
        { -6.0f, -8.0f },
        { -6.0f, -7.0f },
        { -6.0f, -6.0f },
        { -6.0f, -5.0f },
        { -6.0f, -4.0f },
        { -6.0f, -3.0f },
        { -6.0f, -2.0f },
        { -6.0f, -1.0f },
        { -5.0f,  0.0f },
        { -5.0f,  1.0f },
        { -5.0f,  2.0f },
        { -5.0f,  3.0f },
        { -5.0f,  4.0f },
        { -5.0f,  5.0f },
        { -5.0f,  6.0f },
        { -5.0f,  7.0f },
        { -5.0f, -8.0f },
        { -5.0f, -7.0f },
        { -5.0f, -6.0f },
        { -5.0f, -5.0f },
        { -5.0f, -4.0f },
        { -5.0f, -3.0f },
        { -5.0f, -2.0f },
        { -5.0f, -1.0f },
        { -4.0f,  0.0f },
        { -4.0f,  1.0f },
        { -4.0f,  2.0f },
        { -4.0f,  3.0f },
        { -4.0f,  4.0f },
        { -4.0f,  5.0f },
        { -4.0f,  6.0f },
        { -4.0f,  7.0f },
        { -4.0f, -8.0f },
        { -4.0f, -7.0f },
        { -4.0f, -6.0f },
        { -4.0f, -5.0f },
        { -4.0f, -4.0f },
        { -4.0f, -3.0f },
        { -4.0f, -2.0f },
        { -4.0f, -1.0f },
        { -3.0f,  0.0f },
        { -3.0f,  1.0f },
        { -3.0f,  2.0f },
        { -3.0f,  3.0f },
        { -3.0f,  4.0f },
        { -3.0f,  5.0f },
        { -3.0f,  6.0f },
        { -3.0f,  7.0f },
        { -3.0f, -8.0f },
        { -3.0f, -7.0f },
        { -3.0f, -6.0f },
        { -3.0f, -5.0f },
        { -3.0f, -4.0f },
        { -3.0f, -3.0f },
        { -3.0f, -2.0f },
        { -3.0f, -1.0f },
        { -2.0f,  0.0f },
        { -2.0f,  1.0f },
        { -2.0f,  2.0f },
        { -2.0f,  3.0f },
        { -2.0f,  4.0f },
        { -2.0f,  5.0f },
        { -2.0f,  6.0f },
        { -2.0f,  7.0f },
        { -2.0f, -8.0f },
        { -2.0f, -7.0f },
        { -2.0f, -6.0f },
        { -2.0f, -5.0f },
        { -2.0f, -4.0f },
        { -2.0f, -3.0f },
        { -2.0f, -2.0f },
        { -2.0f, -1.0f },
        { -1.0f,  0.0f },
        { -1.0f,  1.0f },
        { -1.0f,  2.0f },
        { -1.0f,  3.0f },
        { -1.0f,  4.0f },
        { -1.0f,  5.0f },
        { -1.0f,  6.0f },
        { -1.0f,  7.0f },
        { -1.0f, -8.0f },
        { -1.0f, -7.0f },
        { -1.0f, -6.0f },
        { -1.0f, -5.0f },
        { -1.0f, -4.0f },
        { -1.0f, -3.0f },
        { -1.0f, -2.0f },
        { -1.0f, -1.0f }
    };

static inline void extend_nyblls_to_floats(uint8_t nybll, float *fp1, float *fp2) {
    const float* fpair = nyblls_as_floats[nybll];
    *fp1 = fpair[0];
    *fp2 = fpair[1];
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    int16_t* out = BUF_S16(rspa.out);
    MEM_BARRIER_PREF(out);
    uint8_t* in = BUF_U8(rspa.in);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        shz_zero_16_shorts(out);
    } else if (flags & A_LOOP) {
        shz_copy_16_shorts(out, rspa.adpcm_loop_state);
    } else {
        shz_copy_16_shorts(out, state);
    }   
    MEM_BARRIER_PREF(in);
    out += 16;
    float prev1 = out[-1];
    float prev2 = out[-2];

    while (nbytes > 0) {
        uint8_t si_in = *in++;
        int table_index = si_in & 0xf; // should be in 0..7
        float(*tbl)[8] = rspa.adpcm_table[table_index];
        float shift = shift_to_float_multiplier(si_in >> 4); // should be in 0..12 or 0..14

        float instr[2][8];
        for(int i = 0; i < 2; ++i) {
            MEM_BARRIER_PREF((void *)((uintptr_t)tbl) + 4 * i);
            const uint8_t in_array[4] = {
                *in++, *in++, *in++, *in++
            };
            {
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[1]]);
                extend_nyblls_to_floats(in_array[0], &instr[i][0], &instr[i][1]);
                instr[i][0] *= shift;
                instr[i][1] *= shift;
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[2]]);
                extend_nyblls_to_floats(in_array[1], &instr[i][2], &instr[i][3]);
                instr[i][2] *= shift;
                instr[i][3] *= shift;
            }
            {    
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[3]]);
                extend_nyblls_to_floats(in_array[2], &instr[i][4], &instr[i][5]);
                instr[i][4] *= shift;
                instr[i][5] *= shift;
                extend_nyblls_to_floats(in_array[3], &instr[i][6], &instr[i][7]);
                instr[i][6] *= shift;
                instr[i][7] *= shift;         
            }
        }
        MEM_BARRIER_PREF(in);

        for (int i = 0; i < 2; i++) {
            float *ins = instr[i];
            shz_vec4_t acc_vec[2];
            float *accf = (float *)acc_vec;
            const shz_vec4_t in_vec = { .x = prev2, .y = prev1, .z = 1.0f };

            shz_xmtrx_load_3x4_rows(&tbl[0][0], &tbl[1][0], &ins[0]);
            acc_vec[0] = shz_xmtrx_trans_vec4(in_vec);
            shz_xmtrx_load_3x4_rows(&tbl[0][4], &tbl[1][4], &ins[4]);
            acc_vec[1] = shz_xmtrx_trans_vec4(in_vec);        

            {
                register float fone asm("fr8")  = 1.0f;
                register float ins0 asm("fr9")  = ins[0];
                register float ins1 asm("fr10") = ins[1];
                register float ins2 asm("fr11") = ins[2];
                accf[2] = shz_dot8f(fone, ins0, ins1, ins2, accf[2], tbl[1][1], tbl[1][0], 0.0f);
                accf[7] = shz_dot8f(fone, ins0, ins1, ins2, accf[7], tbl[1][6], tbl[1][5], tbl[1][4]);
                shz_xmtrx_load_4x4_cols(&accf[3], &tbl[1][2], &tbl[1][1], &tbl[1][0]);
                *(SHZ_ALIASING shz_vec4_t*)&accf[3] = 
                    shz_xmtrx_trans_vec4((shz_vec4_t) { .x = 1.0f, .y = ins[0], .z = ins[1], .w = ins[2] });
                accf[1] += (tbl[1][0] * ins[0]);

            }
            {
                register float ins3 asm("fr8")  = ins[3];
                register float ins4 asm("fr9")  = ins[4];
                register float ins5 asm("fr10") = ins[5];
                register float ins6 asm("fr11") = ins[6];
                accf[7] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][3], tbl[1][2], tbl[1][1], tbl[1][0]);
                accf[6] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][2], tbl[1][1], tbl[1][0], 0.0f);   
                accf[5] += (tbl[1][1] * ins[3]) + (tbl[1][0] * ins[4]);
                accf[4] += (tbl[1][0] * ins[3]);
            }

            for(int j = 0; j < 6; ++j)
                *out++ = clamp16f(accf[j]);

            prev2  = clamp16f(accf[6]);
            *out++ = prev2;
            prev1  = clamp16f(accf[7]);
            *out++ = prev1;
        }
        MEM_BARRIER_PREF(out);
        nbytes -= 16 * sizeof(int16_t);
    }

    shz_copy_16_shorts(state, (out - 16));
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t __attribute__((aligned(32))) tmp[32] = {0};
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator = 0;
    int i = 0;
    float* tbl_f = NULL;
    float sample_f = 0;

    int16_t *dp,*sp;

    if (flags & A_INIT) {
        memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        dp = tmp;
        sp = state;
        for (int l=0;l<16;l++) {
            *dp++ = *sp++;
        }
    }
    if (flags & 2) {
        dp = in[-8];
        sp = tmp[8];
        for (int l=0;l<8;l++) {
            *dp++ = *sp++;
        }
        in -= (tmp[5] >> 1);
    }
    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    
    dp = in;
    sp = tmp;
    for (int l=0;l<4;l++)
        *dp++ = *sp++;

    do {
        for (i = 0; i < 8; i++) {
            tbl_f = resample_table[pitch_accumulator >> 10];
            float in_f[4] = {(float)(int)in[0],(float)(int)in[1],(float)(int)in[2],(float)(int)in[3]};

            sample_f = shz_dot8f(in_f[0],in_f[1],in_f[2],in_f[3],
                tbl_f[0],tbl_f[1],tbl_f[2],tbl_f[3]) * 0.00003052f;

            *out++ = clamp16((s32)(sample_f));

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    dp = (int16_t*)(state);
    sp = in;
    for (int l=0;l<4;l++)
        *dp++ = *sp++;

    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    dp = (int16_t*)(state + 8);
    sp = in;
    for (int l=0;l<8;l++)
        *dp++ = *sp++;
}

void aEnvSetup1Impl(UNUSED uint8_t initial_vol_wet, UNUSED uint16_t rate_wet, uint16_t rate_left, uint16_t rate_right) {
    rspa.rate[0] = rate_left;
    rspa.rate[1] = rate_right;
}

void aEnvSetup2Impl(uint16_t initial_vol_left, uint16_t initial_vol_right) {
    rspa.vol[0] = initial_vol_left;
    rspa.vol[1] = initial_vol_right;
}

void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples, UNUSED int swap_reverb, UNUSED int neg_left, UNUSED int neg_right,
                   uint16_t dry_left_addr, uint16_t dry_right_addr, UNUSED uint16_t wet_left_addr, UNUSED uint16_t wet_right_addr) {
    int16_t* in = BUF_S16(in_addr);
    int16_t* dry[2] = { BUF_S16(dry_left_addr), BUF_S16(dry_right_addr) };
    int n = ROUND_UP_16(n_samples);

    uint16_t vols[2] = { rspa.vol[0], rspa.vol[1] };
    uint16_t rates[2] = { rspa.rate[0], rspa.rate[1] };
    __builtin_prefetch(in);
    do {
        for (int i = 0; i < 8; i++) {
            int16_t sample = *in++;

            int32_t dsampl1 = *dry[0] + (sample * vols[0] >> 16);
            int32_t dsampl2 = *dry[1] + (sample * vols[1] >> 16);
#if 1
            asm volatile("": : : "memory");
#endif
            *dry[0]++ = clamp16(dsampl1); 
            *dry[1]++ = clamp16(dsampl2); 
        }
        __builtin_prefetch(in);
        vols[0] += rates[0];
        vols[1] += rates[1];
        n -= 8;
    } while (n > 0);
}

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
#if 0
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);
    int nbytes = ROUND_UP_32(count);

    do {
        // out and in never overlap as called by `synthesis.c`
        n64_memcpy(out, in, nbytes);
        out += nbytes;
    } while (t-- > 0);
#endif
}

void aDownsampleHalfImpl(
    uint16_t n_samples, uint16_t in_addr, uint16_t out_addr)
{
    uint32_t * __restrict in =  (uint32_t*)(((uintptr_t)BUF_S16(in_addr)/* +3 */) & ~3);
    __builtin_prefetch(in);
    uint32_t * __restrict out = (uint32_t*)(((uintptr_t)BUF_S16(out_addr)/* +3 */) & ~3);
    int n = ROUND_UP_8(n_samples);

    do {
        asm volatile("pref @%0" : : "r" (out) : "memory");
        uint32_t pair0 = *in++;
        uint32_t pair1 = *in++;
        uint32_t pair2 = *in++;
        uint32_t pair3 = *in++;
        asm volatile("pref @%0" : : "r" (in) : "memory");
        // its easier to do this the "wrong" way
        *out++ = (pair0 << 16) | ((uint16_t)pair1); // keep second, discard first
        *out++ = (pair2 << 16) | ((uint16_t)pair3); // keep second, discard first
        n -= 4;
    } while (n > 0);
}
