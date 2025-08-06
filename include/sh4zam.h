#ifndef SH4ZAM_H
#define SH4ZAM_H

#define SHZ_NOEXCEPT
#define SHZ_ALIGNAS(n)          __attribute__((aligned(n)))
#define SHZ_INLINE              inline static
#define SHZ_FORCE_INLINE        __attribute__((always_inline)) SHZ_INLINE
#define SHZ_NO_INLINE           __attribute__((noinline))
#define SHZ_ALIASING            __attribute__((__may_alias__))

#define SHZ_FSCA_RAD_FACTOR     10430.37835f

#define TRIG_ARG_SCALE 0.00009587f
#define SHZ_ANGLE(a) (((float)((uint16_t)a)) * TRIG_ARG_SCALE)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shz_sincos {
    float sin;  
    float cos; 
} shz_sincos_t;

typedef struct shz_vec2 {
    union {
        struct {
            float x; 
            float y; 
        };
        float e[2]; 
    };
} shz_vec2_t;

typedef struct shz_vec3 {
    union {
        struct {
            union {
                struct {
                    float x; 
                    float y; 
                };
                shz_vec2_t vec2;
            };
            float z;  
        };
        float e[3];   
    };
} shz_vec3_t;

typedef struct shz_vec4 {
    union {
        struct {
            union {
                struct {
                    union {
                        struct {
                            float x; 
                            float y;  
                        };
                        shz_vec2_t vec2; 
                    };
                    float z;             
                };
                shz_vec3_t vec3;         
            };
            float w; 
        };
        float e[4];     
    };
} shz_vec4_t;


typedef SHZ_ALIGNAS(8) union shz_matrix_2x2 {
    float       elem[4];
    float       elem2D[2][2];
    shz_vec2_t  col[2];
} shz_matrix_2x2_t;

typedef union shz_matrix_3x3 {
    float      elem[9];
    float      elem2D[3][3];
    shz_vec3_t col[3];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
    };
} shz_matrix_3x3_t;

typedef union shz_matrix_3x4 {
    float      elem[12];
    float      elem2D[3][4];
    shz_vec3_t col[4];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
        shz_vec3_t pos;
    };
} shz_matrix_3x4_t;

typedef SHZ_ALIGNAS(8) union shz_matrix_4x4 {
    float      elem[16];
    float      elem2D[4][4];
    shz_vec4_t col[4];
    struct {
        shz_vec4_t left;
        shz_vec4_t up;
        shz_vec4_t forward;
        shz_vec4_t pos;
    };
} shz_matrix_4x4_t;


SHZ_FORCE_INLINE float shz_mag_sqr4f(float x, float y, float z, float w) {
    register float rx asm("fr8") = x;
    register float ry asm("fr9") = y;
    register float rz asm("fr10") = z;
    register float rw asm("fr11") = w;

    asm("fipr fv8, fv8"
        : "+f" (rw)
        : "f" (rx), "f" (ry), "f" (rz));

    return rw;
}

SHZ_FORCE_INLINE float shz_dot8f(float x1, float y1, float z1, float w1,
                                 float x2, float y2, float z2, float w2) {
    register float rx1 asm("fr8")  = x1;
    register float ry1 asm("fr9")  = y1;
    register float rz1 asm("fr10") = z1;
    register float rw1 asm("fr11") = w1;
    register float rx2 asm("fr12") = x2;
    register float ry2 asm("fr13") = y2;
    register float rz2 asm("fr14") = z2;
    register float rw2 asm("fr15") = w2;

    asm("fipr fv8, fv12"
        : "+f" (rw2)
        : "f" (rx1), "f" (ry1), "f" (rz1), "f" (rw1),
          "f" (rx2), "f" (ry2), "f" (rz2));

    return rw2;
}

//! Calculates 1.0f/sqrtf( \p x ), using a fast approximation.
SHZ_FORCE_INLINE float shz_inverse_sqrtf(float x) {
    asm("fsrra %0" : "+f" (x));
    return x;
}

//! Takes the inverse of \p p using a very fast approximation, returning a positive result.
SHZ_FORCE_INLINE float shz_inverse_posf(float x) {
    return shz_inverse_sqrtf(x * x);
}

//! Divides \p num by \p denom using a very fast approximation, returning a positive result.
SHZ_FORCE_INLINE float shz_div_posf(float num, float denom) {
    return num * shz_inverse_posf(denom);
}

//! Takes the inverse of \p p using a faster approximation than doing a full division.
SHZ_FORCE_INLINE float shz_fast_invf(float x) {
    float inv;

    if (__builtin_constant_p(x))
        return 1.0f / x;

    inv = shz_inverse_posf(x);

    if (x < 0.0f)
        inv = -inv;

    return inv;
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_scale(shz_vec3_t vec, float factor) SHZ_NOEXCEPT {
    return (shz_vec3_t) { vec.x * factor, vec.y * factor, vec.z * factor };
}

SHZ_FORCE_INLINE float shz_vec3_magnitude_sqr(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_mag_sqr4f(vec.x, vec.y, vec.z, 0.0f);
}

SHZ_FORCE_INLINE float shz_vec3_magnitude_inv(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_inverse_sqrtf(shz_vec3_magnitude_sqr(vec));
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_normalize(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_scale(vec, shz_vec3_magnitude_inv(vec));
}

SHZ_FORCE_INLINE shz_vec4_t shz_xmtrx_trans_vec4(shz_vec4_t vec) {
    register float rx asm("fr8") = vec.x;
    register float ry asm("fr9") = vec.y;
    register float rz asm("fr10") = vec.z;
    register float rw asm("fr11") = vec.w;

    asm volatile("ftrv xmtrx, fv8"
                 : "+f" (rx), "+f" (ry), "+f" (rz), "+f" (rw));

    return (shz_vec4_t) { .x = rx, .y = ry, .z = rz, .w = rw };
}

SHZ_FORCE_INLINE shz_vec3_t shz_xmtrx_trans_vec3(shz_vec3_t vec) {
    return shz_xmtrx_trans_vec4((shz_vec4_t) { .vec3 = vec }).vec3;
}

SHZ_FORCE_INLINE shz_vec2_t shz_xmtrx_trans_vec2(shz_vec2_t vec) {
    return shz_xmtrx_trans_vec3((shz_vec3_t) { .vec2 = vec }).vec2;
}

SHZ_FORCE_INLINE shz_sincos_t shz_sincosu16(uint16_t radians16) {
    register float rsin asm("fr0");
    register float rcos asm("fr1");

    asm("fsca fpul, dr0"
        : "=f" (rsin), "=f" (rcos)
        : "y" (radians16));

    return (shz_sincos_t){ rsin, rcos };
}

SHZ_FORCE_INLINE shz_sincos_t shz_sincosf(float radians) {
    return shz_sincosu16(radians * SHZ_FSCA_RAD_FACTOR);
}

SHZ_FORCE_INLINE float shz_sincos_tanf(shz_sincos_t sincos) {
    return sincos.sin / sincos.cos;
}

SHZ_FORCE_INLINE float shz_sinf(float radians) {
    return shz_sincosf(radians).sin;
}

SHZ_FORCE_INLINE float shz_cosf(float radians) {
    return shz_sincosf(radians).cos;
}

SHZ_FORCE_INLINE float shz_tanf(float radians) {
    return shz_sincos_tanf(shz_sincosf(radians));
}

SHZ_FORCE_INLINE void shz_dcache_alloc_line(void *src) {
    SHZ_ALIASING uint32_t *src32 = (SHZ_ALIASING uint32_t *)src;

    asm volatile("movca.l r0, @%8"
     : "=m"(src32[0]),
       "=m"(src32[1]),
       "=m"(src32[2]),
       "=m"(src32[3]),
       "=m"(src32[4]),
       "=m"(src32[5]),
       "=m"(src32[6]),
       "=m"(src32[7])
     : "r" (src32));
}

SHZ_INLINE void shz_xmtrx_store_4x4(shz_matrix_4x4_t *matrix) {
    asm volatile(R"(
        fschg
        add     #64, %[mtx]
        fmov.d	xd14, @-%[mtx]
        add     #-32, %[mtx]
        pref    @%[mtx]
        add     #32, %[mtx]
        fmov.d	xd12, @-%[mtx]
        fmov.d	xd10, @-%[mtx]
        fmov.d	xd8, @-%[mtx]
        fmov.d	xd6, @-%[mtx]
        fmov.d	xd4, @-%[mtx]
        fmov.d	xd2, @-%[mtx]
        fmov.d	xd0, @-%[mtx]
        fschg
    )"
    : [mtx] "+&r" (matrix), "=m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_store_4x4_unaligned(float matrix[16]) {
    asm volatile(R"(
        frchg
        add     #64, %[mtx]
        fmov.s	fr15, @-%[mtx]
        add     #-32, %[mtx]
        pref    @%[mtx]
        add     #32, %[mtx]
        fmov.s	fr14, @-%[mtx]
        fmov.s	fr13, @-%[mtx]
        fmov.s	fr12, @-%[mtx]
        fmov.s	fr11, @-%[mtx]
        fmov.s	fr10, @-%[mtx]
        fmov.s	fr9, @-%[mtx]
        fmov.s	fr8, @-%[mtx]
        fmov.s	fr7, @-%[mtx]
        fmov.s	fr6, @-%[mtx]
        fmov.s	fr5, @-%[mtx]
        fmov.s	fr4, @-%[mtx]
        fmov.s	fr3, @-%[mtx]
        fmov.s	fr2, @-%[mtx]
        fmov.s	fr1, @-%[mtx]
        fmov.s	fr0, @-%[mtx]        
        frchg
    )"
    : "=m" (*matrix)
    : [mtx] "r" (matrix));
}


SHZ_INLINE void shz_xmtrx_store_4x4_transpose(shz_matrix_4x4_t *matrix) {
    asm volatile(R"(
        frchg
        add     #64-8, %[mtx]
        fmov.s	fr15, @%[mtx]
        add     #-32, %[mtx]
        pref    @%[mtx]
        add     #32, %[mtx]
        fmov.s	fr11, @-%[mtx]
        fmov.s  fr7, @-%[mtx]
        fmov.s  fr3, @-%[mtx]
        fmov.s  fr14, @-%[mtx]
        fmov.s  fr10, @-%[mtx]
        fmov.s  fr6, @-%[mtx]
        fmov.s  fr2, @-%[mtx]
        fmov.s  fr13, @-%[mtx]
        fmov.s  fr9, @-%[mtx]
        fmov.s  fr5, @-%[mtx]
        fmov.s  fr1, @-%[mtx]
        fmov.s  fr12, @-%[mtx]
        fmov.s  fr8, @-%[mtx]
        fmov.s  fr4, @-%[mtx]
        fmov.s  fr0, @-%[mtx]
        frchg
    )"
    : [mtx] "+&r" (matrix), "=m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4(const shz_matrix_4x4_t *matrix) {
    asm volatile(R"(
        fschg
        fmov.d	@%[mtx], xd0
        add     #32, %[mtx]
        pref	@%[mtx]
        add     #-(32-8), %[mtx]
        fmov.d	@%[mtx]+, xd2
        fmov.d	@%[mtx]+, xd4
        fmov.d	@%[mtx]+, xd6
        fmov.d	@%[mtx]+, xd8
        fmov.d	@%[mtx]+, xd10
        fmov.d	@%[mtx]+, xd12
        fmov.d	@%[mtx]+, xd14
        fschg
    )"
    : [mtx] "+r" (matrix)
    : "m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4_unaligned(const float matrix[16]) {
    asm volatile(R"(
        frchg
        fmov.s	@%[mtx]+, fr0
        add     #32, %[mtx]
        pref	@%[mtx]
        add     #-32, %[mtx]
        fmov.s	@%[mtx]+, fr1
        fmov.s	@%[mtx]+, fr2
        fmov.s	@%[mtx]+, fr3
        fmov.s	@%[mtx]+, fr4
        fmov.s	@%[mtx]+, fr5
        fmov.s	@%[mtx]+, fr6
        fmov.s	@%[mtx]+, fr7
        fmov.s	@%[mtx]+, fr8
        fmov.s	@%[mtx]+, fr9
        fmov.s	@%[mtx]+, fr10
        fmov.s	@%[mtx]+, fr11
        fmov.s	@%[mtx]+, fr12
        fmov.s	@%[mtx]+, fr13
        fmov.s	@%[mtx]+, fr14
        fmov.s	@%[mtx]+, fr15
        frchg
    )"
    : [mtx] "+r" (matrix)
    :  "m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4_transpose(const shz_matrix_4x4_t *matrix) {
    asm volatile(R"(
        frchg

        fmov.s  @%[mtx]+, fr0

        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-32, %[mtx]

        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr8
        fmov.s  @%[mtx]+, fr12

        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr9
        fmov.s  @%[mtx]+, fr13

        fmov.s  @%[mtx]+, fr2
        fmov.s  @%[mtx]+, fr6
        fmov.s  @%[mtx]+, fr10
        fmov.s  @%[mtx]+, fr14

        fmov.s  @%[mtx]+, fr3
        fmov.s  @%[mtx]+, fr7
        fmov.s  @%[mtx]+, fr11
        fmov.s  @%[mtx]+, fr15

        frchg
    )"
    : [mtx] "+r" (matrix)
    : "m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4_rows(const shz_vec4_t *r1,
                                        const shz_vec4_t *r2,
                                        const shz_vec4_t *r3,
                                        const shz_vec4_t *r4) {
    asm volatile (R"(
        frchg

        pref    @%1
        fmov.s  @%0+, fr0
        fmov.s  @%0+, fr4
        fmov.s  @%0+, fr8
        fmov.s  @%0,  fr12

        pref    @%2
        fmov.s  @%1+, fr1
        fmov.s  @%1+, fr5
        fmov.s  @%1+, fr9
        fmov.s  @%1,  fr13

        pref    @%3
        fmov.s  @%2+, fr2
        fmov.s  @%2+, fr6
        fmov.s  @%2+, fr10
        fmov.s  @%2,  fr14

        fmov.s  @%3+, fr3
        fmov.s  @%3+, fr7
        fmov.s  @%3+, fr11
        fmov.s  @%3,  fr15

        frchg
    )"
    : "+&r" (r1), "+&r" (r2), "+&r" (r3), "+&r" (r4)
    : "m" (*r1), "m" (*r2), "m" (*r3), "m" (*r4));    
}

SHZ_INLINE void shz_xmtrx_load_4x4_cols(const shz_vec4_t *c1,
                                        const shz_vec4_t *c2,
                                        const shz_vec4_t *c3,
                                        const shz_vec4_t *c4) {
    asm volatile (R"(
        frchg

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

        pref    @%3
        fmov.s  @%2+, fr8
        fmov.s  @%2+, fr9
        fmov.s  @%2+, fr10
        fmov.s  @%2,  fr11

        fmov.s  @%3+, fr12
        fmov.s  @%3+, fr13
        fmov.s  @%3+, fr14
        fmov.s  @%3,  fr15

        frchg
    )"
    : "+&r" (c1), "+&r" (c2), "+&r" (c3), "+&r" (c4)
    : "m" (*c1), "m" (*c2), "m" (*c3), "m" (*c4));
}

SHZ_INLINE void shz_xmtrx_load_4x4_apply(const shz_matrix_4x4_t *matrix1,
                                         const shz_matrix_4x4_t *matrix2)
{
    unsigned int prefetch_scratch;

    asm volatile (R"(
        mov     %[m1], %[prefscr]
        add     #32, %[prefscr]
        fschg
        pref    @%[prefscr]

        fmov.d  @%[m1]+, xd0
        fmov.d  @%[m1]+, xd2
        fmov.d  @%[m1]+, xd4
        fmov.d  @%[m1]+, xd6
        pref    @%[m1]
        fmov.d  @%[m1]+, xd8
        fmov.d  @%[m1]+, xd10
        fmov.d  @%[m1]+, xd12
        mov     %[m2], %[prefscr]
        add     #32, %[prefscr]
        fmov.d  @%[m1], xd14
        pref    @%[prefscr]

        fmov.d  @%[m2]+, dr0
        fmov.d  @%[m2]+, dr2
        fmov.d  @%[m2]+, dr4
        ftrv    xmtrx, fv0

        fmov.d  @%[m2]+, dr6
        fmov.d  @%[m2]+, dr8
        ftrv    xmtrx, fv4

        fmov.d  @%[m2]+, dr10
        fmov.d  @%[m2]+, dr12
        ftrv    xmtrx, fv8

        fmov.d  @%[m2], dr14
        fschg
        ftrv    xmtrx, fv12
        frchg
    )"
    : [m1] "+&r" (matrix1), [m2] "+r" (matrix2),
      [prefscr] "=&r" (prefetch_scratch)
    : "m" (*matrix1), "m" (*matrix2)
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
      "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15");
}


SHZ_INLINE void shz_xmtrx_load_4x4_apply_store(shz_matrix_4x4_t *out,
                                               const shz_matrix_4x4_t *matrix1,
                                               const shz_matrix_4x4_t *matrix2)
{
    unsigned int prefetch_scratch;

    asm volatile (R"(
        mov     %[m1], %[prefscr]
        add     #32, %[prefscr]
        fschg
        pref    @%[prefscr]

        fmov.d  @%[m1]+, xd0
        fmov.d  @%[m1]+, xd2
        fmov.d  @%[m1]+, xd4
        fmov.d  @%[m1]+, xd6
        pref    @%[m1]
        fmov.d  @%[m1]+, xd8
        fmov.d  @%[m1]+, xd10
        fmov.d  @%[m1]+, xd12
        mov     %[m2], %[prefscr]
        add     #32, %[prefscr]
        fmov.d  @%[m1], xd14
        pref    @%[prefscr]

        fmov.d  @%[m2]+, dr0
        fmov.d  @%[m2]+, dr2
        fmov.d  @%[m2]+, dr4
        ftrv    xmtrx, fv0

        fmov.d  @%[m2]+, dr6
        fmov.d  @%[m2]+, dr8
        ftrv    xmtrx, fv4

        fmov.d  @%[m2]+, dr10
        fmov.d  @%[m2]+, dr12
        ftrv    xmtrx, fv8

        add     #16, %[out]
        fmov.d  dr2, @-%[out]
        fmov.d  dr0,  @-%[out]

        fmov.d  @%[m2], dr14
        ftrv    xmtrx, fv12

        add     #32, %[out]
        fmov.d  dr6, @-%[out]
        fmov.d  dr4, @-%[out]

        add     #32, %[out]
        fmov.d  dr10, @-%[out]
        fmov.d  dr8, @-%[out] 
        
        add     #32, %[out]
        fmov.d  dr14, @-%[out]
        fmov.d  dr12, @-%[out]

        fschg
    )"
    : [m1] "+&r" (matrix1), [m2] "+r" (matrix2), "=m" (*out),
      [prefscr] "=&r" (prefetch_scratch)
    : [out] "r" (out), "m" (*matrix1), "m" (*matrix2)
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
      "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15");
}

SHZ_INLINE void shz_matrix_4x4_copy(shz_matrix_4x4_t *dst, const shz_matrix_4x4_t *src) {
    asm volatile(R"(
        fschg

        pref    @%[dst]
        fmov.d  @%[src]+, xd0
        fmov.d  @%[src]+, xd2
        fmov.d  @%[src]+, xd4
        fmov.d  @%[src]+, xd6

        pref    @%[src]
        add     #32, %[dst]

        fmov.d  xd6, @-%[dst]
        fmov.d  xd4, @-%[dst]
        fmov.d  xd2, @-%[dst]
        fmov.d  xd0, @-%[dst]

        add     #32, %[dst]
        pref    @%[dst]

        fmov.d  @%[src]+, xd0
        fmov.d  @%[src]+, xd2
        fmov.d  @%[src]+, xd4
        fmov.d  @%[src]+, xd6

        add     #32, %[dst]
        fmov.d  xd6, @-%[dst]
        fmov.d  xd4, @-%[dst]
        fmov.d  xd2, @-%[dst]
        fmov.d  xd0, @-%[dst]

        fschg
    )"
    : [dst] "+&r" (dst), [src] "+&r" (src), "=m" (*dst)
    : "m" (*src));
}

SHZ_INLINE void shz_xmtrx_load_3x3(const shz_matrix_3x3_t *matrix) {
    asm volatile(R"(
        frchg

        fmov.s  @%[mat]+, fr0
        fldi0   fr3
        fmov.s  @%[mat]+, fr1
        fldi0   fr12
        fmov.s  @%[mat]+, fr2

        fmov.s  @%[mat]+, fr4
        fldi0   fr7
        fmov.s  @%[mat]+, fr5
        fldi0   fr13
        fmov.s  @%[mat]+, fr6

        fmov.s  @%[mat]+, fr8
        fldi0   fr11
        fmov.s  @%[mat]+, fr9
        fldi0   fr14
        fmov.s  @%[mat], fr10
        fldi1   fr15

        frchg
    )"
    : [mat] "+r" (matrix)
    : "m" (*matrix));
}

SHZ_INLINE void shz_xmtrx_load_3x3_transpose(const float *matrix) {
    asm volatile(R"(
        frchg

        fmov.s  @%[mat]+, fr0
        fldi0   fr3
        fmov.s  @%[mat]+, fr4
        fldi0   fr12
        fmov.s  @%[mat]+, fr8

        fmov.s  @%[mat]+, fr1
        fldi0   fr7
        fmov.s  @%[mat]+, fr5
        fldi0   fr13
        fmov.s  @%[mat]+, fr9

        fmov.s  @%[mat]+, fr2
        fldi0   fr11
        fmov.s  @%[mat]+, fr6
        fldi0   fr14
        fmov.s  @%[mat], fr10
        fldi1   fr15

        frchg
    )"
    : [mat] "+r" (matrix)
    : "m" (matrix[0]), "m" (matrix[1]), "m" (matrix[2]),
      "m" (matrix[3]), "m" (matrix[4]), "m" (matrix[5]),
      "m" (matrix[6]), "m" (matrix[7]), "m" (matrix[8]));
}

SHZ_INLINE void shz_xmtrx_store_3x3(shz_matrix_3x3_t *matrix) {
    asm volatile(R"(
        frchg
        add     #36, %[mtx]

        fmov.s	fr10, @-%[mtx]
        fmov.s  fr9, @-%[mtx]
        fmov.s  fr8, @-%[mtx]

        fmov.s	fr6, @-%[mtx]
        fmov.s  fr5, @-%[mtx]
        fmov.s  fr4, @-%[mtx]

        fmov.s	fr2, @-%[mtx]
        fmov.s  fr1, @-%[mtx]
        fmov.s  fr0, @-%[mtx]

        frchg
    )"
    : "=m" (*matrix)
    : [mtx] "r" (matrix));
}

SHZ_INLINE void shz_xmtrx_store_3x3_transpose(shz_matrix_3x3_t *matrix) {
    asm volatile(R"(
        frchg
        add     #36, %[mtx]

        fmov.s	fr10, @-%[mtx]
        fmov.s  fr6, @-%[mtx]
        fmov.s  fr2, @-%[mtx]

        fmov.s	fr9, @-%[mtx]
        fmov.s  fr5, @-%[mtx]
        fmov.s  fr1, @-%[mtx]

        fmov.s	fr8, @-%[mtx]
        fmov.s  fr4, @-%[mtx]
        fmov.s  fr0, @-%[mtx]

        frchg
    )"
    : "=m" (*matrix)
    : [mtx] "r" (matrix));
}

SHZ_INLINE void shz_xmtrx_apply_3x3(const shz_matrix_3x3_t *matrix) {
    asm volatile(R"(
        fmov.s  @%[mtx], fr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-(32-4), %[mtx]
        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr2
        fldi0   fr3

        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr6
        fldi0   fr7

        ftrv    xmtrx, fv0

        fmov.s  @%[mtx]+, fr8
        fmov.s  @%[mtx]+, fr9
        fmov.s  @%[mtx]+, fr10
        fldi0   fr11

        ftrv    xmtrx, fv4

        fschg
        fmov    xd12, dr12
        fmov    xd14, dr14
        fschg

        ftrv    xmtrx, fv8

        frchg
    )"
    : [mtx] "+r" (matrix)
    : "m" (*matrix)
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
      "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_apply_3x3_transpose(const shz_matrix_3x3_t *matrix) {
    asm volatile(R"(
        fmov.s  @%[mtx]+, fr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-32, %[mtx]
        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr8
        fldi0   fr12

        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr9
        fldi0   fr13

        ftrv    xmtrx, fv0

        fmov.s  @%[mtx]+, fr2
        fmov.s  @%[mtx]+, fr6
        fmov.s  @%[mtx]+, fr10
        fldi1   fr15

        ftrv    xmtrx, fv4

        fschg
        fmov    xd12, dr12
        fmov    xd14, dr14
        fschg

        ftrv    xmtrx, fv8

        frchg
    )"
    : [mtx] "+r" (matrix)
    : "m" (*matrix)
    : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
      "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_apply_4x4_unaligned(const float matrix[16]) {
    asm volatile(R"(
        mov     r15, r0
        pref    @%[mtx]
        or      #0x0f, r0
        xor     #0x0f, r0
        mov     r15, r7
        
        fschg
        mov     r0, r15        
        fmov.d  dr14, @-r15
        fmov.d  dr12, @-r15
        fschg

        fmov.s  @%[mtx]+, fr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-32, %[mtx]
        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr2
        fmov.s  @%[mtx]+, fr3

        ftrv    xmtrx, fv0

        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr6
        fmov.s  @%[mtx]+, fr7

        ftrv    xmtrx, fv4

        fmov.s  @%[mtx]+, fr8
        fmov.s  @%[mtx]+, fr9
        fmov.s  @%[mtx]+, fr10
        fmov.s  @%[mtx]+, fr11

        ftrv    xmtrx, fv8

        fmov.s  @%[mtx]+, fr12
        fmov.s  @%[mtx]+, fr13
        fmov.s  @%[mtx]+, fr14
        fmov.s  @%[mtx]+, fr15        

        ftrv    xmtrx, fv12

        frchg
        
        fschg
        fmov.d  @r15+, dr12
        fmov.d  @r15, dr14
        mov     r7, r15
        fschg
    )"
    : [mtx] "+r" (matrix)
    :  "m" (*matrix)
    : "r0", "r7", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6",
      "fr7", "fr8", "fr9", "fr10", "fr11", "fr12");
}

SHZ_INLINE void shz_xmtrx_init_identity(void) {
    asm volatile(R"(
        frchg
        fldi1	fr0
        fschg
        fldi0	fr1
        fldi0	fr2
        fldi0	fr3
        fldi0	fr4
        fldi1	fr5
        fmov	dr2, dr6
        fmov	dr2, dr8
        fldi1   fr10
        fldi0   fr11
        fmov	dr2, dr12
        fmov	dr4, dr14
        fschg
        frchg
    )");
}

SHZ_INLINE void shz_xmtrx_init_translation(float x, float y, float z) {
    asm volatile(R"(
        frchg
        fldi1	fr0
        fschg
        fldi0	fr1
        fldi0	fr2
        fldi0   fr3
        fldi0	fr4
        fldi1	fr5
        fmov	dr2, dr6
        fmov	dr2, dr8
        fschg
        fldi1   fr10
        fldi0   fr11
        fmov.s  @%[x], fr12
        fmov.s  @%[y], fr13
        fmov.s  @%[z], fr14
        fldi1   fr15
        frchg
    )"
    :
    : [x] "r" (&x), [y] "r" (&y), [z] "r" (&z),
      "m" (x), "m" (y), "m" (z));
}


SHZ_INLINE void shz_xmtrx_init_diagonal(float x, float y, float z, float w) {
    asm volatile(R"(
        frchg
        fldi0	fr1
        fschg
        fldi0	fr2
        fldi0	fr3
        fldi0	fr4
        fmov	dr2, dr6
        fmov	dr2, dr8
        fldi0	fr11
        fmov	dr2, dr12
        fldi0	fr14
        fschg
        fmov.s	@%[x], fr0
        fmov.s	@%[y], fr5
        fmov.s	@%[z], fr10
        fmov.s  @%[w], fr15
        frchg
    )"
    :
    : [x] "r" (&x), [y] "r" (&y), [z] "r" (&z), [w] "r" (&w),
      "m" (x), "m" (y), "m" (z), "m" (w));
}

SHZ_FORCE_INLINE void shz_xmtrx_init_scale(float x, float y, float z) {
    shz_xmtrx_init_diagonal(x, y, z, 1.0f);
}

SHZ_INLINE void shz_xmtrx_init_rotation_x(float x) {
    x *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul
        frchg
        fsca    fpul, dr0
        fldi0   fr2
        fldi0   fr3
        fldi0   fr4
        fmov    fr1, fr5
        fmov    fr0, fr6
        fldi0   fr7
        fldi0   fr8
        fmov    fr0, fr9
        fneg    fr9
        fmov    fr1, fr10
        fldi0   fr11
        fldi0   fr12
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15
        fldi1   fr0
        fldi0   fr1
        frchg
    )"
    :
    : "f" (x)
    : "fpul");
}

SHZ_INLINE void shz_xmtrx_init_rotation_y(float y) {
    y *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul
        frchg
        fsca	fpul, dr0
        fmov    fr0, fr2
        fneg    fr2
        fldi0	fr3
        fldi0	fr4
        fldi1	fr5
        fldi0	fr6
        fldi0	fr7
        fmov    fr0, fr8
        fldi0	fr9
        fmov	fr1, fr10
        fldi0	fr11
        fldi0	fr12
        fldi0	fr13
        fldi0	fr14
        fldi1	fr15
        fmov    fr1, fr0
        fldi0   fr1
        frchg
    )"
    :
    : "f" (y)
    : "fpul");
}

SHZ_INLINE void shz_xmtrx_init_rotation_z(float z) {
    z *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul
        frchg
        fsca    fpul, dr4
        fldi0	fr2
        fldi0	fr3
        fmov    fr5, fr0
        fmov    fr4, fr1
        fneg    fr4
        fschg
        fmov	dr2, dr6
        fmov	dr2, dr8
        fldi1	fr10
        fldi0	fr11
        fmov	dr2, dr12
        fldi0	fr14
        fldi1	fr15
        fschg
        frchg
    )"
    :
    : "f" (z)
    : "fpul");
}

SHZ_INLINE void shz_xmtrx_apply_rotation_x(float x) {
    x *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul

        fldi0	fr8
        fldi0	fr11
        fsca 	fpul, dr4
        fmov	fr4, fr9
        fneg	fr9
        fmov	fr5, fr10
        ftrv	xmtrx, fv8

        fmov	fr4, fr6
        fldi0	fr7
        fldi0	fr4
        ftrv	xmtrx, fv4

        fschg
        fmov	dr8, xd8
        fmov	dr10, xd10
        fmov	dr4, xd4
        fmov	dr6, xd6
        fschg
    )"
    :
    : "f" (x)
    : "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fpul");
}


SHZ_INLINE void shz_xmtrx_apply_rotation_y(float y) {
    y *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul

        fldi0	fr9
        fsca    fpul, dr6
        fldi0	fr11
        fmov	fr6, fr8
        fmov	fr7, fr10
        ftrv	xmtrx, fv8

        fmov	fr7, fr4
        fldi0	fr5
        fneg	fr6
        fldi0	fr7
        ftrv	xmtrx, fv4

        fschg
        fmov	dr8, xd8
        fmov	dr10, xd10
        fmov	dr4, xd0
        fmov	dr6, xd2
        fschg
    )"
    :
    : "f" (y)
    : "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fpul");
}

SHZ_INLINE void shz_xmtrx_apply_rotation_z(float z) {
    z *= SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul

        fldi0	fr11
        fsca    fpul, dr8
        fldi0	fr10
        fmov	fr8, fr5
        fneg	fr8
        ftrv	xmtrx, fv8
        
        fmov	fr9, fr4
        fschg
        fmov	dr10, dr6
        ftrv	xmtrx, fv4

        fmov	dr8, xd4
        fmov	dr10, xd6
        fmov	dr4, xd0
        fmov	dr6, xd2
        fschg
    )"
    :
    : "f" (z)
    : "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fpul");
}

SHZ_FORCE_INLINE void shz_xmtrx_set_translation(float x, float y, float z) {
    asm volatile(R"(
        frchg

        fmov.s  @%[x], fr12
        fmov.s  @%[y], fr13
        fmov.s  @%[z], fr14

        frchg
    )"
    :
    : [x] "r" (&x), [y] "r" (&y), [z] "r" (&z),
      "m" (x), "m" (y), "m" (z)
    );
}

SHZ_INLINE void shz_xmtrx_apply_translation(float x, float y, float z) {
    asm volatile(R"(
        fschg
        fmov	xd0, dr4
        fmov	xd2, dr6
        fschg

        frchg
        fmov.s  @%[x], fr0
        fmov.s  @%[y], fr1
        fmov.s  @%[z], fr2

        fadd	fr0, fr12
        fadd	fr1, fr13
        fadd	fr2, fr14
        frchg

        fschg
        fmov	dr4, xd0
        fmov	dr6, xd2
        fschg
    )"
    :
    : [x] "r" (&x), [y] "r" (&y), [z] "r" (&z),
      "m" (x), "m" (y), "m" (z)
    : "fr4", "fr5", "fr6", "fr7");
}

SHZ_INLINE void shz_xmtrx_apply_scale(float x, float y, float z) {
        asm volatile(R"(
        fschg
        fmov	xd12, dr4
        fmov	xd14, dr6
        fschg

        frchg
        fmov.s  @%[x], fr12
        fmov.s  @%[y], fr13
        fmov.s  @%[z], fr14

        fmul    fr12, fr0
        fmul	fr12, fr1
        fmul	fr12, fr2
   
        fmul    fr13, fr4
        fmul	fr13, fr5
        fmul	fr13, fr6

        fmul    fr14, fr8
        fmul	fr14, fr9
        fmul	fr14, fr10
        frchg

        fschg
        fmov	dr4, xd12
        fmov	dr6, xd14
        fschg
    )"
    :
    : [x] "r" (&x), [y] "r" (&y), [z] "r" (&z),
      "m" (x), "m" (y), "m" (z)
    : "fr4", "fr5", "fr6", "fr7");
}


SHZ_INLINE void shz_xmtrx_init_rotation(float roll, float pitch, float yaw) {
    shz_xmtrx_init_rotation_z(yaw);
    shz_xmtrx_apply_rotation_y(pitch);
    shz_xmtrx_apply_rotation_x(roll);
}

SHZ_INLINE void shz_xmtrx_apply_rotation(float roll, float pitch, float yaw) {
    shz_xmtrx_apply_rotation_x(roll);
    shz_xmtrx_apply_rotation_y(pitch);
    shz_xmtrx_apply_rotation_z(yaw);
}

SHZ_INLINE void shz_xmtrx_transpose(void) {
    asm volatile (R"(
        frchg

        flds    fr1, fpul
        fmov    fr4, fr1
        fsts    fpul, fr4

        flds    fr2, fpul
        fmov    fr8, fr2
        fsts    fpul, fr8

        flds    fr3, fpul
        fmov    fr12, fr3
        fsts    fpul, fr12

        flds    fr6, fpul
        fmov    fr9, fr6
        fsts    fpul, fr9

        flds    fr7, fpul
        fmov    fr13, fr7
        fsts    fpul, fr13

        flds    fr11, fpul
        fmov    fr14, fr11
        fsts    fpul, fr14

        frchg
    )"
    :
    :
    : "fpul");
}


SHZ_INLINE shz_vec3_t shz_matrix4x4_trans_vec3(const shz_matrix_4x4_t *m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[1][0];
    register float fr6 asm("fr6") = m->elem2D[2][0];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);

    register float fr8  asm("fr8")  = m->elem2D[0][1];
    register float fr9  asm("fr9")  = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[2][1];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" 
        : "+f" (fr11)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr8), "f" (fr9), "f" (fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[0][2];
    fr5 = m->elem2D[1][2];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);      

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3(const shz_matrix_3x3_t *m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[1][0];
    register float fr6 asm("fr6") = m->elem2D[2][0];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);

    register float fr8  asm("fr8")  = m->elem2D[0][1];
    register float fr9  asm("fr9")  = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[2][1];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" 
        : "+f" (fr11)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr8), "f" (fr9), "f" (fr10));


    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[0][2];
    fr5 = m->elem2D[1][2];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);      

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix4x4_trans_vec3_transpose(const shz_matrix_4x4_t *m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[0][1];
    register float fr6 asm("fr6") = m->elem2D[0][2];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);

    register float fr8  asm("fr8")  = m->elem2D[1][0];
    register float fr9  asm("fr9")  = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[1][2];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" 
        : "+f" (fr11)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr8), "f" (fr9), "f" (fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[2][0];
    fr5 = m->elem2D[2][1];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);      

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3_transpose(const shz_matrix_3x3_t *m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[0][1];
    register float fr6 asm("fr6") = m->elem2D[0][2];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);

    register float fr8  asm("fr8")  = m->elem2D[1][0];
    register float fr9  asm("fr9")  = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[1][2];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" 
        : "+f" (fr11)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr8), "f" (fr9), "f" (fr10));


    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[2][0];
    fr5 = m->elem2D[2][1];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" 
        : "+f" (fr7)
        : "f" (fr0), "f" (fr1), "f" (fr2), "f" (fr3),
          "f" (fr4), "f" (fr5), "f" (fr6));

    __atomic_thread_fence(1);      

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE void shz_memcpy4_16(void *dst, const void *src) {
    SHZ_ALIASING const uint32_t (*d)[16] = (SHZ_ALIASING const uint32_t (*)[16])dst;
    SHZ_ALIASING       uint32_t (*s)[16] = (SHZ_ALIASING       uint32_t (*)[16])src;
    asm(R"(        
        mov.l   @%[s]+, r0
        mov.l   @%[s]+, r1
        mov.l   @%[s]+, r2
        mov.l   @%[s]+, r3
        mov.l   r0, @%[d]
        mov.l   r1, @( 4, %[d])
        mov.l   r2, @( 8, %[d])
        mov.l   r3, @(12, %[d])
        mov.l   @%[s]+, r0
        mov.l   @%[s]+, r1
        mov.l   @%[s]+, r2
        mov.l   @%[s]+, r3
        mov.l   r0, @(16, %[d])
        mov.l   r1, @(20, %[d])
        mov.l   r2, @(24, %[d])
        mov.l   r3, @(28, %[d])
        mov.l   @%[s]+, r0
        mov.l   @%[s]+, r1
        mov.l   @%[s]+, r2
        mov.l   @%[s]+, r3
        mov.l   r0, @(32, %[d])
        mov.l   r1, @(36, %[d])
        mov.l   r2, @(40, %[d])
        mov.l   r3, @(44, %[d])
        mov.l   @%[s]+, r0
        mov.l   @%[s]+, r1
        mov.l   @%[s]+, r2
        mov.l   @%[s]+, r3
        mov.l   r0, @(48, %[d])
        mov.l   r1, @(52, %[d])
        mov.l   r2, @(56, %[d])
        mov.l   r3, @(60, %[d])
        add     #-64, %[s]
    )"
    : "=m" (*d)
    : [s] "r" (s), [d] "r" (d), "m" (*s)
    : "r0", "r1", "r2", "r3");
}

#ifdef __cplusplus
}
#endif

#endif