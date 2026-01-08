#ifndef SH4ZAM_H
#define SH4ZAM_H

#include <math.h>
#include <stdint.h>
#define true 1
#define false 0
#define SHZ_NOEXCEPT
#define SHZ_ALIGNAS(n) __attribute__((aligned(n)))
#define SHZ_INLINE inline static
#define SHZ_FORCE_INLINE __attribute__((always_inline)) SHZ_INLINE
#define SHZ_NO_INLINE __attribute__((noinline))
#define SHZ_ALIASING __attribute__((__may_alias__))
#ifdef __cplusplus
#define SHZ_RESTRICT __restrict__
#else
#define SHZ_RESTRICT restrict
#endif

#define SHZ_FSCA_RAD_FACTOR 10430.37835f

#define SHZ_F_PI 3.1415926f
#define SHZ_F_PI_2 1.57079633f
#define TRIG_ARG_SCALE 0.00009587f
#define SHZ_ANGLE(a) (((float) ((uint16_t) a)) * TRIG_ARG_SCALE)

#define SHZ_FSCHG(pairwise_mode) \
    do {                         \
        asm volatile("fschg");   \
    } while (false)
#define SHZ_LIKELY(e) __builtin_expect(!!(e), 1)
#define SHZ_UNLIKELY(e) __builtin_expect(!!(e), 0)
#define SHZ_PREFETCH(a) __builtin_prefetch(a)

#ifdef __cplusplus
extern "C" {
#endif

typedef SHZ_ALIASING int16_t shz_alias_int16_t;
typedef SHZ_ALIASING uint16_t shz_alias_uint16_t;
typedef SHZ_ALIASING int32_t shz_alias_int32_t;
typedef SHZ_ALIASING uint32_t shz_alias_uint32_t;
typedef SHZ_ALIASING float shz_alias_float_t;
typedef SHZ_ALIASING int64_t shz_alias_int64_t;
typedef SHZ_ALIASING uint64_t shz_alias_uint64_t;
typedef SHZ_ALIASING double shz_alias_double_t;

typedef struct shz_sincos {
    float sin;
    float cos;
} shz_sincos_t;

/*! 2D Vector type
 *
 *  Structure for holding coordinates of a 2-dimensional vector.
 *
 * \sa shz_vec3_t, shz_vec4_t
 */
typedef struct shz_vec2 {
    union {
        float e[2]; //!< <X, Y> coordinates as an array
        struct {
            float x; //!< X coordinate
            float y; //!< Y coordinate
        };
    };
} shz_vec2_t;

/*! 3D Vector type
 *
 *  Structure for holding coordinates of a 3-dimensional vector.
 *
 * \sa shz_vec2_t, shz_vec4_t
 */
typedef struct shz_vec3 {
    union {
        float e[3]; //!< <X, Y, Z> coordinates as an array
        struct {
            union {
                struct {
                    float x; //!< X coordinate
                    float y; //!< Y coordinate
                };
                shz_vec2_t xy; //!< Inner 2D vector containing <X, Y> coords
            };
            float z; //!< Z coordinate
        };
    };
} shz_vec3_t;

/*! 4D Vector type
 *
 *  Structure for holding coordinates of a 4-dimensional vector.
 *
 *  \sa shz_vec2_t, shz_vec3_t
 */
typedef struct shz_vec4 {
    union {
        float e[4]; //!< <X, Y, Z, W> coordinates as an array.
        struct {
            union {
                struct {
                    float x; //!< X coordinate
                    float y; //!< Y coordinate
                    float z; //!< Z coordinate
                };
                shz_vec3_t xyz; //!< <X, Y, Z> coordinates as a 3D vector
            };
            float w; //!< W coordinate
        };
        struct {
            shz_vec2_t xy; //!< <X, Y> coordinates as a 2D vector
            shz_vec2_t zw; //!< <Z, W> coordinates as a 2D vector
        };
    };
} shz_vec4_t;

typedef SHZ_ALIGNAS(8) union shz_matrix_2x2 {
    float elem[4];
    float elem2D[2][2];
    shz_vec2_t col[2];
} shz_matrix_2x2_t;

typedef union shz_matrix_3x3 {
    float elem[9];
    float elem2D[3][3];
    shz_vec3_t col[3];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
    };
} shz_matrix_3x3_t;

typedef union shz_matrix_3x4 {
    float elem[12];
    float elem2D[3][4];
    shz_vec3_t col[4];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
        shz_vec3_t pos;
    };
} shz_matrix_3x4_t;

typedef SHZ_ALIGNAS(8) union shz_matrix_4x4 {
    float elem[16];
    float elem2D[4][4];
    shz_vec4_t col[4];
    struct {
        shz_vec4_t left;
        shz_vec4_t up;
        shz_vec4_t forward;
        shz_vec4_t pos;
    };
} shz_matrix_4x4_t;

#if 0
static inline void sincoss(s16 arg0, f32* s, f32* c) {
    register float __s __asm__("fr2");
    register float __c __asm__("fr3");

    asm("lds    %2,fpul\n\t"
        "fsca    fpul,dr2\n\t"
        : "=f"(__s), "=f"(__c)
        : "r"(arg0)
        : "fpul");

    *s = __s;
    *c = __c;
}

static inline void scaled_sincoss(s16 arg0, f32* s, f32* c, f32 scale) {
    register float __s __asm__("fr2");
    register float __c __asm__("fr3");

    asm("lds    %2,fpul\n\t"
        "fsca    fpul,dr2\n\t"
        : "=f"(__s), "=f"(__c)
        : "r"(arg0)
        : "fpul");

    *s = __s * scale;
    *c = __c * scale;
}
#endif

SHZ_FORCE_INLINE float shz_fmaf(float a, float b, float c) SHZ_NOEXCEPT {
    return a * b + c;
}

SHZ_FORCE_INLINE float shz_copysignf(float x, float y) SHZ_NOEXCEPT {
    x = fabsf(x);
    return (y < 0.0f) ? -x : x;
}

SHZ_FORCE_INLINE float shz_inv_sqrtf(float x) SHZ_NOEXCEPT {
    asm volatile("fsrra %0" : "+f"(x));
    return x;
}

SHZ_FORCE_INLINE float shz_invf_fsrra(float x) SHZ_NOEXCEPT {
    return shz_inv_sqrtf(x * x);
}

#include <math.h>
SHZ_FORCE_INLINE float shz_sqrtf_fsrra(float x) {
    return x == 0.0f ? 0.0f : shz_inv_sqrtf(x) * x;
}

SHZ_FORCE_INLINE float shz_mag_sqr3f(float x, float y, float z) SHZ_NOEXCEPT {
    register float rx asm("fr8") = x;
    register float ry asm("fr9") = y;
    register float rz asm("fr10") = z;
    register float rw asm("fr11") = 0.0f;

    asm("fipr fv8, fv8" : "+f"(rw) : "f"(rx), "f"(ry), "f"(rz));

    return rw;
}

SHZ_FORCE_INLINE float shz_mag_sqr4f(float x, float y, float z, float w) {
    register float rx asm("fr8") = x;
    register float ry asm("fr9") = y;
    register float rz asm("fr10") = z;
    register float rw asm("fr11") = w;

    asm("fipr fv8, fv8" : "+f"(rw) : "f"(rx), "f"(ry), "f"(rz));

    return rw;
}

SHZ_FORCE_INLINE float shz_dot6f(float x1, float y1, float z1, float x2, float y2, float z2) {
    register float rx1 asm("fr8") = x1;
    register float ry1 asm("fr9") = y1;
    register float rz1 asm("fr10") = z1;
    register float rw1 asm("fr11") = 0.0f;
    register float rx2 asm("fr12") = x2;
    register float ry2 asm("fr13") = y2;
    register float rz2 asm("fr14") = z2;
    register float rw2 asm("fr15");

    asm("fipr fv8, fv12" : "=f"(rw2) : "f"(rx1), "f"(ry1), "f"(rz1), "f"(rw1), "f"(rx2), "f"(ry2), "f"(rz2));

    return rw2;
}

SHZ_FORCE_INLINE float shz_dot8f(float x1, float y1, float z1, float w1, float x2, float y2, float z2, float w2) {
    register float rx1 asm("fr8") = x1;
    register float ry1 asm("fr9") = y1;
    register float rz1 asm("fr10") = z1;
    register float rw1 asm("fr11") = w1;
    register float rx2 asm("fr12") = x2;
    register float ry2 asm("fr13") = y2;
    register float rz2 asm("fr14") = z2;
    register float rw2 asm("fr15") = w2;

    asm("fipr fv8, fv12" : "+f"(rw2) : "f"(rx1), "f"(ry1), "f"(rz1), "f"(rw1), "f"(rx2), "f"(ry2), "f"(rz2));

    return rw2;
}
//! Creates a hardware memory barrier beyond which any loads or stores may not be reordered
#define SHZ_MEMORY_BARRIER_HARD()   __sync_synchronize()

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_dot3(shz_vec3_t l, shz_vec3_t r1, shz_vec3_t r2, shz_vec3_t r3) SHZ_NOEXCEPT {
    shz_vec3_t res;

    register float lx asm("fr8")  = l.x;
    register float ly asm("fr9")  = l.y;
    register float lz asm("fr10") = l.z;
    register float lw asm("fr11") = 0.0f;

    register float r1x asm("fr12") = r1.x;
    register float r1y asm("fr13") = r1.y;
    register float r1z asm("fr14") = r1.z;
    register float r1w asm("fr15");

    asm("fipr   fv8, fv12\n"
        : "=f" (r1w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r1x), "f" (r1y), "f" (r1z));

    register float r2x asm("fr0") = r2.x;
    register float r2y asm("fr1") = r2.y;
    register float r2z asm("fr2") = r2.z;
    register float r2w asm("fr3");

    asm("fipr   fv8, fv0\n"
        : "=f" (r2w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r2x), "f" (r2y), "f" (r2z));

    register float r3x asm("fr4") = r3.x;
    register float r3y asm("fr5") = r3.y;
    register float r3z asm("fr6") = r3.z;
    register float r3w asm("fr7");

    SHZ_MEMORY_BARRIER_HARD();

    res.x = r1w;

    asm("fipr   fv8, fv4\n"
        : "=f" (r3w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r3x), "f" (r3y), "f" (r3z));

    SHZ_MEMORY_BARRIER_HARD();
    res.y = r2w;
    SHZ_MEMORY_BARRIER_HARD();
    res.z = r3w;

    return res;
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec4_dot3(shz_vec4_t l, shz_vec4_t r1, shz_vec4_t r2, shz_vec4_t r3) SHZ_NOEXCEPT {
    shz_vec3_t res;

    register float lx asm("fr8")  = l.x;
    register float ly asm("fr9")  = l.y;
    register float lz asm("fr10") = l.z;
    register float lw asm("fr11") = l.w;

    register float r1x asm("fr12") = r1.x;
    register float r1y asm("fr13") = r1.y;
    register float r1z asm("fr14") = r1.z;
    register float r1w asm("fr15") = r1.w;

    asm("fipr   fv8, fv12\n"
        : "+f" (r1w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r1x), "f" (r1y), "f" (r1z));

    register float r2x asm("fr0") = r2.x;
    register float r2y asm("fr1") = r2.y;
    register float r2z asm("fr2") = r2.z;
    register float r2w asm("fr3") = r2.w;

    asm("fipr   fv8, fv0\n"
        : "+f" (r2w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r2x), "f" (r2y), "f" (r2z));

    register float r3x asm("fr4") = r3.x;
    register float r3y asm("fr5") = r3.y;
    register float r3z asm("fr6") = r3.z;
    register float r3w asm("fr7") = r3.w;

    SHZ_MEMORY_BARRIER_HARD();

    res.x = r1w;

    asm("fipr   fv8, fv4\n"
        : "+f" (r3w)
        : "f" (lx), "f" (ly), "f" (lz), "f" (lw),
          "f" (r3x), "f" (r3y), "f" (r3z));

    SHZ_MEMORY_BARRIER_HARD();
    res.y = r2w;
    SHZ_MEMORY_BARRIER_HARD();
    res.z = r3w;

    return res;
}

//! Calculates 1.0f/sqrtf( \p x ), using a fast approximation.
SHZ_FORCE_INLINE float shz_inverse_sqrtf(float x) {
    if (x == 0.0f)
        return 0.0f;
    asm("fsrra %0" : "+f"(x));
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

    inv = shz_inverse_posf(x);

    if (x < 0.0f)
        inv = -inv;

    return inv;
}

//! Divides \p num by \p denom using a very fast approximation
SHZ_FORCE_INLINE float shz_divf(float num, float denom) {
    return num * shz_fast_invf(denom);
}

SHZ_FORCE_INLINE float shz_atanf_unit(float x) SHZ_NOEXCEPT {
    const float n1 = 0.97239411f;
    const float n2 = -0.19194795f;

    return shz_fmaf(n2, x * x, n1) * x;
}

SHZ_INLINE float shz_atanf_q1(float x) SHZ_NOEXCEPT {
    return (SHZ_F_PI_2) - shz_atanf_unit(shz_invf_fsrra(x));
}

SHZ_INLINE float shz_atanf(float x) SHZ_NOEXCEPT {
    if (x > 1.0f)
        return shz_atanf_q1(x);
    else if (x < -1.0f)
        return -shz_atanf_q1(x);
    else
        return shz_atanf_unit(x);
}

#define SHZ_F_PI_4 0.78539816f

SHZ_INLINE float shz_atan2f(float y, float x) SHZ_NOEXCEPT {
    if (__builtin_expect((x == 0.0f && y == 0.0f),0))
        return 0.0f;

    float abs_y = fabsf(y);
    float abs_sum = abs_y + fabsf(x);
    float inv_abs_sum = shz_invf_fsrra(abs_sum);

    float angle = SHZ_F_PI_2;
    float r = x;
 
    if(x < 0.0f) {
        angle += SHZ_F_PI_4;
        r += abs_y;
    } else {
        angle -= SHZ_F_PI_4;
        r -= abs_y;
    }

    r *= inv_abs_sum;
    angle += shz_fmaf(0.1963f, r * r, -0.9817f) * r;

    return shz_copysignf(angle, y);
}

SHZ_INLINE float shz_asinf(float x) SHZ_NOEXCEPT {
    return shz_atanf(x * shz_inv_sqrtf(1.0f - (x * x)));
}

SHZ_INLINE float shz_acosf(float x) SHZ_NOEXCEPT {
    return (SHZ_F_PI_2) - shz_asinf(x);
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_scale(shz_vec3_t vec, float factor) SHZ_NOEXCEPT {
    return (shz_vec3_t) { vec.x * factor, vec.y * factor, vec.z * factor };
}

SHZ_FORCE_INLINE shz_vec2_t shz_vec2_init(float x, float y) SHZ_NOEXCEPT {
    return (shz_vec2_t) { .x = x, .y = y };
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_init(float x, float y, float z) SHZ_NOEXCEPT {
    return (shz_vec3_t) { .x = x, .y = y, .z = z };
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec4_init(float x, float y, float z, float w) SHZ_NOEXCEPT {
    return (shz_vec4_t) { .x = x, .y = y, .z = z, .w = w };
}

SHZ_FORCE_INLINE shz_vec2_t shz_vec2_fill(float v) SHZ_NOEXCEPT {
    return shz_vec2_init(v, v);
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

//! Dereferences the given pointer to a sequence of 2 floats as a shz_vec2_t.
#define shz_vec2_deref(ptr) (*((shz_vec2_t*) (ptr)))

//! Dereferences the given pointer to a sequence of 3 floats as a shz_vec3_t.
#define shz_vec3_deref(ptr) (*((shz_vec3_t*) (ptr)))

//! Dereferences the given pointer to a sequence of 4 floats as a shz_vec4_t.
#define shz_vec4_deref(ptr) (*((shz_vec4_t*) (ptr)))

SHZ_FORCE_INLINE shz_vec3_t shz_vec2_vec3(shz_vec2_t vec, float z) SHZ_NOEXCEPT {
    return (shz_vec3_t) { .xy = vec, .z = z };
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec2_vec4(shz_vec2_t vec, float z, float w) SHZ_NOEXCEPT {
    return (shz_vec4_t) { .x = vec.x, .y = vec.y, .z = z, .w = w };
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec3_vec4(shz_vec3_t vec, float w) SHZ_NOEXCEPT {
    return (shz_vec4_t) { .xyz = vec, .w = w };
}

SHZ_FORCE_INLINE shz_vec4_t shz_xmtrx_trans_vec4(shz_vec4_t vec) {
    register float rx asm("fr8") = vec.x;
    register float ry asm("fr9") = vec.y;
    register float rz asm("fr10") = vec.z;
    register float rw asm("fr11") = vec.w;

    asm volatile("ftrv xmtrx, fv8" : "+f"(rx), "+f"(ry), "+f"(rz), "+f"(rw));

    return (shz_vec4_t) { .x = rx, .y = ry, .z = rz, .w = rw };
}

SHZ_FORCE_INLINE shz_vec3_t shz_xmtrx_trans_vec3(shz_vec3_t vec) {
    return shz_xmtrx_trans_vec4((shz_vec4_t) { .xyz = vec }).xyz;
}

SHZ_FORCE_INLINE shz_vec2_t shz_xmtrx_trans_vec2(shz_vec2_t vec) {
    return shz_xmtrx_trans_vec3((shz_vec3_t) { .xy = vec }).xy;
}

SHZ_FORCE_INLINE shz_sincos_t shz_sincosu16(uint16_t radians16) {
    register float rsin asm("fr0");
    register float rcos asm("fr1");

    asm("fsca fpul, dr0" : "=f"(rsin), "=f"(rcos) : "y"(radians16));

    return (shz_sincos_t) { rsin, rcos };
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

SHZ_FORCE_INLINE void shz_dcache_alloc_line(void* src) {
    SHZ_ALIASING uint32_t* src32 = (SHZ_ALIASING uint32_t*) src;

    asm volatile("movca.l r0, @%8"
                 : "=m"(src32[0]), "=m"(src32[1]), "=m"(src32[2]), "=m"(src32[3]), "=m"(src32[4]), "=m"(src32[5]),
                   "=m"(src32[6]), "=m"(src32[7])
                 : "r"(src32));
}

SHZ_INLINE void shz_xmtrx_load_apply_store_4x4(shz_matrix_4x4_t* out, const shz_matrix_4x4_t* matrix1,
                                               const shz_matrix_4x4_t* matrix2) SHZ_NOEXCEPT {
    unsigned int prefetch_scratch;

    asm volatile(R"(
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
                 : [m1] "+&r"(matrix1), [m2] "+r"(matrix2), [out] "+&r"(out),
                   "=m"(*out), [prefscr] "=&r"(prefetch_scratch)
                 : "m"(*matrix1), "m"(*matrix2)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_store_4x4(shz_matrix_4x4_t* matrix) {
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
                 : [mtx] "+&r"(matrix), "=m"(*matrix));
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
                 : "=m"(*matrix)
                 : [mtx] "r"(matrix));
}

SHZ_INLINE void shz_xmtrx_store_4x4_transpose(shz_matrix_4x4_t* matrix) {
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
                 : [mtx] "+&r"(matrix), "=m"(*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4(const shz_matrix_4x4_t* matrix) {
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix));
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4_transpose(const shz_matrix_4x4_t* matrix) {
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix));
}

SHZ_INLINE void shz_xmtrx_load_4x4_rows(const shz_vec4_t* r1, const shz_vec4_t* r2, const shz_vec4_t* r3,
                                        const shz_vec4_t* r4) {
    asm volatile(R"(
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
                 : "+&r"(r1), "+&r"(r2), "+&r"(r3), "+&r"(r4)
                 : "m"(*r1), "m"(*r2), "m"(*r3), "m"(*r4));
}

SHZ_INLINE void shz_xmtrx_load_4x4_cols(const shz_vec4_t* c1, const shz_vec4_t* c2, const shz_vec4_t* c3,
                                        const shz_vec4_t* c4) {
    asm volatile(R"(
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
                 : "+&r"(c1), "+&r"(c2), "+&r"(c3), "+&r"(c4)
                 : "m"(*c1), "m"(*c2), "m"(*c3), "m"(*c4));
}

SHZ_INLINE void shz_xmtrx_load_4x4_apply(const shz_matrix_4x4_t* matrix1, const shz_matrix_4x4_t* matrix2) {
    unsigned int prefetch_scratch;

    asm volatile(R"(
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
                 : [m1] "+&r"(matrix1), [m2] "+r"(matrix2), [prefscr] "=&r"(prefetch_scratch)
                 : "m"(*matrix1), "m"(*matrix2)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_load_4x4_apply_store(shz_matrix_4x4_t* out, const shz_matrix_4x4_t* matrix1,
                                               const shz_matrix_4x4_t* matrix2) {
    unsigned int prefetch_scratch;

    asm volatile(R"(
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
                 : [m1] "+&r"(matrix1), [m2] "+r"(matrix2), "=m"(*out), [prefscr] "=&r"(prefetch_scratch)
                 : [out] "r"(out), "m"(*matrix1), "m"(*matrix2)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}

SHZ_INLINE void shz_matrix_4x4_copy(shz_matrix_4x4_t* dst, const shz_matrix_4x4_t* src) {
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
                 : [dst] "+&r"(dst), [src] "+&r"(src), "=m"(*dst)
                 : "m"(*src));
}

SHZ_INLINE void shz_xmtrx_load_3x3(const shz_matrix_3x3_t* matrix) {
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
                 : [mat] "+r"(matrix)
                 : "m"(*matrix));
}

SHZ_INLINE void shz_xmtrx_load_3x3_transpose(const float* matrix) {
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
                 : [mat] "+r"(matrix)
                 : "m"(matrix[0]), "m"(matrix[1]), "m"(matrix[2]), "m"(matrix[3]), "m"(matrix[4]), "m"(matrix[5]),
                   "m"(matrix[6]), "m"(matrix[7]), "m"(matrix[8]));
}

SHZ_INLINE void shz_xmtrx_store_3x3(shz_matrix_3x3_t* matrix) {
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
                 : "=m"(*matrix)
                 : [mtx] "r"(matrix));
}

SHZ_INLINE void shz_xmtrx_store_3x3_transpose(shz_matrix_3x3_t* matrix) {
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
                 : "=m"(*matrix)
                 : [mtx] "r"(matrix));
}

SHZ_INLINE void shz_xmtrx_apply_3x3(const shz_matrix_3x3_t* matrix) {
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_apply_3x3_transpose(const shz_matrix_3x3_t* matrix) {
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}

SHZ_INLINE void shz_xmtrx_apply_4x4(const shz_matrix_4x4_t* matrix) SHZ_NOEXCEPT {
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

        fmov.d  @%[mtx], dr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-(32-8), %[mtx]
        fmov.d  @%[mtx]+, dr2
        fmov.d  @%[mtx]+, dr4
        fmov.d  @%[mtx]+, dr6

        ftrv    xmtrx, fv0

        fmov.d  @%[mtx]+, dr8
        fmov.d  @%[mtx]+, dr10

        ftrv    xmtrx, fv4

        fmov.d  @%[mtx]+, dr12
        fmov.d  @%[mtx], dr14

        ftrv    xmtrx, fv8
        ftrv    xmtrx, fv12

        frchg
        fmov.d  @r15+, dr12
        fmov.d  @r15, dr14

        mov     r7, r15
        fschg
    )"
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix)
                 : "r0", "r7", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11",
                   "fr12");
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
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix)
                 : "r0", "r7", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11",
                   "fr12");
}

SHZ_INLINE void shz_xmtrx_init_identity_safe(void) {
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

SHZ_INLINE void shz_xmtrx_init_identity(void) SHZ_NOEXCEPT {
    asm volatile(R"(
        frchg
        fldi0   fr1
        fldi1   fr0
        fmul    fr1, fr2
        fldi0   fr3
        fmul    fr1, fr4
        fldi1   fr5
        fmul    fr1, fr11
        fldi0   fr6
        fmul    fr1, fr7
        fldi0   fr8
        fmul    fr1, fr9
        fldi1   fr10
        fmul    fr1, fr12
        fldi0   fr13
        fmul    fr1, fr14
        fldi1   fr15
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
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z));
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
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), [w] "r"(&w), "m"(x), "m"(y), "m"(z), "m"(w));
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
                 : "f"(x)
                 : "fpul");
}

SHZ_INLINE void shz_xmtrx_rotate_x(float x) {
    float register x_ asm("fr4") = x * SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    fr4, fpul
        fsca    fpul, dr0

        fldi0   fr4
        fmov    fr1, fr5
        fmov    fr0, fr6
        fldi0   fr7
        fldi0   fr8
        ftrv    xmtrx, fv4

        fmov    fr0, fr9
        fneg    fr9
        fmov    fr1, fr10
        fldi0   fr11
        fldi0   fr12
        ftrv    xmtrx, fv8
        
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15
        fldi1   fr0
        ftrv    xmtrx, fv12

        fldi0   fr1
        fldi0   fr2
        fldi0   fr3
        ftrv    xmtrx, fv0

        frchg
    )"
                 : "+f"(x_)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15", "fpul");
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
                 : "f"(y)
                 : "fpul");
}

SHZ_INLINE void shz_xmtrx_rotate_y(float y) {
    float register y_ asm("fr4") = y * SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul
        fsca	fpul, dr0

        fldi0	fr4
        fldi1	fr5
        fldi0	fr6
        fldi0	fr7
        fmov    fr0, fr8
        ftrv    xmtrx, fv4

        fldi0	fr9
        fmov	fr1, fr10
        fldi0	fr11
        fldi0	fr12
        ftrv    xmtrx, fv8

        fldi0	fr13
        fldi0	fr14
        fldi1	fr15
        fmov    fr0, fr2
        ftrv    xmtrx, fv12

        fneg    fr2
        fldi0	fr3
        fmov    fr1, fr0
        fldi0   fr1
        ftrv    xmtrx, fv0

        frchg
    )"
                 : "+f"(y_)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15", "fpul");
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
                 : "f"(z)
                 : "fpul");
}

SHZ_INLINE void shz_xmtrx_rotate_z(float z) {
    float register z_ asm("fr4") = z * SHZ_FSCA_RAD_FACTOR;
    asm volatile(R"(
        ftrc    %0, fpul
        fsca    fpul, dr4

        fmov    fr5, fr0
        fmov    fr4, fr1
        fldi0	fr2
        fldi0	fr3
        ftrv    xmtrx, fv0

        fneg    fr4
        fldi0   fr6
        fldi0   fr7
        ftrv    xmtrx, fv4

        fldi0   fr8
        fldi0   fr9
        fldi1   fr10
        fldi0   fr11
        ftrv    xmtrx, fv8

        fldi0   fr12
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15
        ftrv    xmtrx, fv12

        frchg
    )"
                 : "+f"(z_)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15", "fpul");
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
                 : "f"(x)
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
                 : "f"(y)
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
                 : "f"(z)
                 : "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fpul");
}

SHZ_INLINE void shz_xmtrx_apply_rotation_axis(float angle, float x, float y, float z) {
    register float x_ asm("fr4") = x;
    register float y_ asm("fr5") = y;
    register float z_ asm("fr6") = z;
    register float a_ asm("fr7") = angle * SHZ_FSCA_RAD_FACTOR;

    asm volatile(R"(
        ftrc	fr7, fpul
        fsca	fpul, dr2
        fldi1	fr0
        fsub	fr3, fr0	/* 1-cos */

        fldi0	fr7
        fipr	fv4, fv4
        fsrra	fr7
        fmul	fr7, fr4
        fmul	fr7, fr5
        fmul	fr7, fr6

        fmov	fr4, fr1
        fmul	fr2, fr1	/* xsin */
        fmov	fr5, fr7
        fmul	fr2, fr7	/* ysin */
        fmul	fr6, fr2	/* zsin */

        fmov	fr4, fr8
        fmul	fr0, fr8
        fmov	fr5, fr9
        fmul	fr8, fr9	/* xy(1-cos) */
        fmul	fr6, fr8	/* xz(1-cos) */
        fmov	fr6, fr10
        fmul	fr0, fr6
        fmul	fr6, fr10
        fadd	fr3, fr10	/* zz(1-cos)+cos */
        fmul	fr5, fr6	/* yz(1-cos) */
        fmul	fr5, fr5
        fmul	fr0, fr5
        fadd	fr3, fr5	/* yy(1-cos)+cos */
        fmul	fr4, fr0
        fmul	fr4, fr0
        fadd	fr3, fr0	/* xx(1-cos)+cos */

        fmov	fr8, fr3	/* xz(1-cos) */
        fmov	fr9, fr4	/* xy(1-cos) */
        fadd	fr7, fr8
        fmov	fr6, fr9
        fsub	fr1, fr9
        fldi0	fr11
        ftrv	xmtrx, fv8

        fadd	fr1, fr6
        fmov	fr4, fr1
        fsub	fr2, fr4
        fsub	fr7, fr3
        fldi0	fr7
        ftrv	xmtrx, fv4

        fadd	fr2, fr1
        fmov	fr3, fr2
        fldi0	fr3
        ftrv	xmtrx, fv0

        frchg
    )"
                 : "+f"(x_), "+f"(y_), "+f"(z_), "+f"(a_)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", "fpul");
}

SHZ_INLINE void shz_xmtrx_init_rotation_axis(float angle, float x, float y, float z) {
    register float x_ asm("fr4") = x;
    register float y_ asm("fr5") = y;
    register float z_ asm("fr6") = z;
    register float a_ asm("fr7") = angle * SHZ_FSCA_RAD_FACTOR;

    asm volatile(R"(
        ftrc	fr7, fpul
        fsca	fpul, dr2
        fldi1	fr0
        fsub	fr3, fr0	/* 1-cos */

        fldi0	fr7
        fipr	fv4, fv4
        fsrra	fr7
        fmul	fr7, fr4
        fmul	fr7, fr5
        fmul	fr7, fr6

        fmov	fr4, fr1
        fmul	fr2, fr1	/* xsin */
        fmov	fr5, fr7
        fmul	fr2, fr7	/* ysin */
        fmul	fr6, fr2	/* zsin */

        fmov	fr4, fr8
        fmul	fr0, fr8
        fmov	fr5, fr9
        fmul	fr8, fr9	/* xy(1-cos) */
        fmul	fr6, fr8	/* xz(1-cos) */
        fmov	fr6, fr10
        fmul	fr0, fr6
        fmul	fr6, fr10
        fadd	fr3, fr10	/* zz(1-cos)+cos */
        fmul	fr5, fr6	/* yz(1-cos) */
        fmul	fr5, fr5
        fmul	fr0, fr5
        fadd	fr3, fr5	/* yy(1-cos)+cos */
        fmul	fr4, fr0
        fmul	fr4, fr0
        fadd	fr3, fr0	/* xx(1-cos)+cos */

        fmov	fr8, fr3	/* xz(1-cos) */
        fmov	fr9, fr4	/* xy(1-cos) */
        fadd	fr7, fr8
        fmov	fr6, fr9
        fsub	fr1, fr9
        fldi0	fr11

        fadd	fr1, fr6
        fmov	fr4, fr1
        fsub	fr2, fr4
        fsub	fr7, fr3
        fldi0	fr7

        fadd	fr2, fr1
        fmov	fr3, fr2
        fldi0	fr3

        fldi0   fr12
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15

        frchg
    )"
                 : "+f"(x_), "+f"(y_), "+f"(z_), "+f"(a_)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", "fpul");
}

SHZ_INLINE void shz_xmtrx_apply_lookat(const float* position_3f, const float* target_3f,
                                       const float* up_3f) SHZ_NOEXCEPT {
    asm volatile(R"(
        fmov.s  @%[t]+, fr8
        fmov.s  @%[t]+, fr9
        fmov.s  @%[t]+, fr10

        fmov.s  @%[p]+, fr12
        fmov.s  @%[p]+, fr13
        fmov.s  @%[p]+, fr14
        fldi0   fr15

        /* z = position - target */
        fneg    fr8
        fadd    fr12, fr8
        fneg    fr9
        fadd    fr13, fr9
        fneg    fr10
        fadd    fr14, fr10
        fldi0   fr11
        fipr    fv8, fv8

        fmov.s  @%[u]+, fr4
        fmov.s  @%[u]+, fr5
        fmov.s  @%[u]+, fr6

        fsrra   fr11
        fmul    fr11, fr8
        fmul    fr11, fr9
        fmul    fr11, fr10
        fldi0   fr11
        fipr    fv12, fv8

        /* x = cross(up, z) */
        fmov    fr6, fr15
        fmul    fr9, fr15
        fmov    fr5, fr0
        fmul    fr10, fr0
        fmov    fr4, fr3
        fmul    fr10, fr3
        fsub    fr15, fr0
        fmov    fr6, fr1
        fmul    fr8, fr1
        fmov    fr4, fr2
        fmul    fr9, fr2
        fmov    fr5, fr15
        fmul    fr8, fr15
        fsub    fr3, fr1
        fsub    fr15, fr2
        fldi0   fr3
        fldi0   fr15
        fipr    fv0, fv0

        fsrra   fr3
        fmul    fr3, fr0
        fmul    fr3, fr1
        fmul    fr3, fr2
        fldi0   fr3
        fipr    fv12, fv0

        /* y = cross(z, x) */
        fmov    fr10, fr15
        fmul    fr1, fr15
        fmov    fr9, fr4
        fmul    fr2, fr4
        fmov    fr8, fr7
        fmul    fr2, fr7
        fsub    fr15, fr4
        fmov    fr10, fr5
        fmul    fr0, fr5
        fmov    fr8, fr6
        fmul    fr1, fr6
        fmov    fr9, fr15
        fmul    fr0, fr15
        fsub    fr7, fr5
        fsub    fr15, fr6

        fldi0   fr7
        fldi0   fr15
        fipr    fv12, fv4

        fneg    fr3
        fneg    fr11
        fneg    fr7
        fmov    fr3, fr12
        fmov    fr7, fr13
        fmov    fr11, fr14
        fldi1   fr15
        ftrv    xmtrx, fv12

        fmov    fr1, fr7
        fmov    fr2, fr1
        fmov    fr4, fr1
        fmov    fr8, fr2
        fldi0   fr3
        ftrv    xmtrx, fv0

        fmov    fr7, fr4
        fmov    fr6, fr7
        fmov    fr9, fr6
        fmov    fr7, fr9
        fldi0   fr7
        ftrv    xmtrx, fv4

        fmov    fr11, fr8
        fldi0   fr11
        ftrv    xmtrx, fv8

        frchg
    )"
                 : [p] "+&r"(position_3f), [t] "+&r"(target_3f), [u] "+&r"(up_3f)
                 :
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
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
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z));
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
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z)
                 : "fr4", "fr5", "fr6", "fr7");
}

SHZ_INLINE void shz_xmtrx_translate(float x, float y, float z) {
    asm volatile(R"(
        fldi0   fr1
        fldi1   fr0
        fldi0   fr2
        fldi0   fr3
        fldi0   fr4
        ftrv    xmtrx, fv0

        fldi1   fr5
        fldi0   fr6
        fldi0   fr7
        fldi0   fr8
        ftrv    xmtrx, fv4

        fldi0   fr9
        fldi1   fr10
        fldi0   fr11
        fmov.s  @%[x], fr12
        ftrv    xmtrx, fv8

        fmov.s  @%[y], fr13
        fmov.s  @%[z], fr14
        fldi1   fr15
        ftrv    xmtrx, fv12

        frchg
    )"
                 :
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
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
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z)
                 : "fr4", "fr5", "fr6", "fr7");
}

SHZ_INLINE void shz_xmtrx_scale(float x, float y, float z) {
    asm volatile(R"(
        fmov.s  @%[x], fr0
        fldi0   fr1
        fldi0   fr2
        fldi0   fr3
        fldi0   fr4
        ftrv    xmtrx, fv0

        fmov.s  @%[y], fr5
        fldi0   fr6
        fldi0   fr7
        fldi0   fr8
        ftrv    xmtrx, fv4

        fldi0   fr9
        fmov.s  @%[z], fr10
        fldi0   fr11
        fldi0   fr12
        ftrv    xmtrx, fv8

        fldi0   fr13
        fldi0   fr14
        fldi1   fr15
        ftrv    xmtrx, fv12

        frchg
    )"
                 :
                 : [x] "r"(&x), [y] "r"(&y), [z] "r"(&z), "m"(x), "m"(y), "m"(z)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
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
    asm volatile(R"(
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

SHZ_INLINE shz_vec3_t shz_mat4x4_trans_vec3(const shz_matrix_4x4_t* m, shz_vec3_t v) SHZ_NOEXCEPT {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[1][0];
    register float fr6 asm("fr6") = m->elem2D[2][0];
    register float fr7 asm("fr7");

    asm volatile("fipr fv0, fv4" : "=f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    register float fr8 asm("fr8") = m->elem2D[0][1];
    register float fr9 asm("fr9") = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[2][1];
    register float fr11 asm("fr11");

    asm volatile("fipr fv0, fv8" : "=f"(fr11) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr8), "f"(fr9), "f"(fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[0][2];
    fr5 = m->elem2D[1][2];
    fr6 = m->elem2D[2][2];

    asm volatile("fipr fv0, fv4" : "=f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    out.y = fr11;
    out.z = fr7;

    return out;
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

SHZ_INLINE shz_vec4_t shz_mat4x4_trans_vec4(const shz_matrix_4x4_t* mat, shz_vec4_t in) SHZ_NOEXCEPT {
    SHZ_PREFETCH(mat);

    shz_vec4_t* v = &in;
    const shz_vec4_t* c[4] = { &mat->col[0], &mat->col[1], &mat->col[2], &mat->col[3] };

    asm volatile(R"(
        ! Load input vector into FV12
        fmov.s  @%[v]+, fr12
        fmov.s  @%[v]+, fr13
        fmov.s  @%[v]+, fr14
        fmov.s  @%[v]+, fr15

        ! Prefetch the second half of the matrix
        pref    @%[c2]

        ! Load first column int FV0
        fmov.s  @%[c0]+, fr0
        fmov.s  @%[c1]+, fr1
        fmov.s  @%[c2]+, fr2
        fmov.s  @%[c3]+, fr3
        ! Start loading next column
        fmov.s  @%[c0]+, fr4   ! Vector instructions need 3 cycles between
        fmov.s  @%[c1]+, fr5   ! loading arguments and using them.

        ! Calculate output vector's X component
        fipr    fv12, fv0

        ! Finish loading second column vector
        fmov.s  @%[c2]+, fr6
        fmov.s  @%[c3]+, fr7
        ! Begin loading third column vector
        fmov.s  @%[c0]+, fr8
        fmov.s  @%[c1]+, fr9

        ! Calculate output vector's Y componennt
        fipr    fv12, fv4

        ! Finish loading third column vector
        fmov.s  @%[c2]+, fr10
        add     #-16, %[v]      ! Point v back to the beginning of the input vector
        fmov.s  @%[c3]+, fr11
        fmov.s  fr3, @%[v]      ! Store output vector X component
        ! Start loading fourth column vector
        fmov.s  @%[c0]+, fr0

        ! Calculate output vector's Z component
        fipr    fv12, fv8

        ! Finish loading the fourth column vector
        fmov.s  @%[c1]+, fr1
        fmov.s  @%[c2]+, fr2
        fmov.s  @%[c3]+, fr3
        add     #4, %[v]        ! Advance output vector pointer
        fmov.s  fr7, @%[v]      ! Store output vector Y component

        ! Calculate output vector's W component
        fipr    fv12, fv0       ! FUCKING STALL - 4th column vector is still loading (3 cycle delay)

        ! Store output vector's Z component
        add     #4, %[v]        ! Advance output vector pointer
        fmov.s  fr11, @%[v]
        
        ! Store output vector's W component
        add     #4, %[v]        ! Advance output vector pointer
        fmov.s  fr3, @%[v]      ! FUCKING STALL - previous FIPR still in pipeline!
    )"
                 : [v] "+r"(v), "=m"(in), [c0] "+r"(c[0]), [c1] "+r"(c[1]), [c2] "+r"(c[2]), [c3] "+r"(c[3])
                 : "m"(in), "m"(*c[0]), "m"(*c[1]), "m"(*c[2]), "m"(*c[3])
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");

    return in;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3(const shz_matrix_3x3_t* m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[1][0];
    register float fr6 asm("fr6") = m->elem2D[2][0];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    register float fr8 asm("fr8") = m->elem2D[0][1];
    register float fr9 asm("fr9") = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[2][1];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" : "+f"(fr11) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr8), "f"(fr9), "f"(fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[0][2];
    fr5 = m->elem2D[1][2];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix4x4_trans_vec3_transpose(const shz_matrix_4x4_t* m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[0][1];
    register float fr6 asm("fr6") = m->elem2D[0][2];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    register float fr8 asm("fr8") = m->elem2D[1][0];
    register float fr9 asm("fr9") = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[1][2];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" : "+f"(fr11) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr8), "f"(fr9), "f"(fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[2][0];
    fr5 = m->elem2D[2][1];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    out.y = fr11;
    out.z = fr7;

    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3_transpose(const shz_matrix_3x3_t* m, shz_vec3_t v) {
    shz_vec3_t out;

    register float fr0 asm("fr0") = v.x;
    register float fr1 asm("fr1") = v.y;
    register float fr2 asm("fr2") = v.z;
    register float fr3 asm("fr3") = 0.0f;

    register float fr4 asm("fr4") = m->elem2D[0][0];
    register float fr5 asm("fr5") = m->elem2D[0][1];
    register float fr6 asm("fr6") = m->elem2D[0][2];
    register float fr7 asm("fr7") = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    register float fr8 asm("fr8") = m->elem2D[1][0];
    register float fr9 asm("fr9") = m->elem2D[1][1];
    register float fr10 asm("fr10") = m->elem2D[1][2];
    register float fr11 asm("fr11") = 0.0f;

    asm volatile("fipr fv0, fv8" : "+f"(fr11) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr8), "f"(fr9), "f"(fr10));

    __atomic_thread_fence(1);

    out.x = fr7;

    __atomic_thread_fence(1);

    fr4 = m->elem2D[2][0];
    fr5 = m->elem2D[2][1];
    fr6 = m->elem2D[2][2];
    fr7 = 0.0f;

    asm volatile("fipr fv0, fv4" : "+f"(fr7) : "f"(fr0), "f"(fr1), "f"(fr2), "f"(fr3), "f"(fr4), "f"(fr5), "f"(fr6));

    __atomic_thread_fence(1);

    out.y = fr11;
    out.z = fr7;

    return out;
}
#if 0
SHZ_INLINE void* shz_memcpy(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_FORCE_INLINE void* shz_memcpy1(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_memcpy2(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_memcpy4(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
extern void* shz_memcpy8(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_sq_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_memcpy64(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
SHZ_INLINE void* shz_memcpy128(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
extern void shz_memcpy128_(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT;
#endif
SHZ_INLINE void shz_memcpy4_16(void* dst, const void* src) {
    SHZ_ALIASING const uint32_t (*d)[16] = (SHZ_ALIASING const uint32_t (*)[16]) dst;
    SHZ_ALIASING uint32_t (*s)[16] = (SHZ_ALIASING uint32_t (*)[16]) src;
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
        : "=m"(*d)
        : [s] "r"(s), [d] "r"(d), "m"(*s)
        : "r0", "r1", "r2", "r3");
}
#if 0
SHZ_FORCE_INLINE void shz_memcpy32_store_(uint64_t* SHZ_RESTRICT* dst) SHZ_NOEXCEPT {
    asm volatile(R"(
        add       #32, %[dst]
        fmov.d    dr10, @-%[dst]
        fmov.d    dr8,  @-%[dst]
        fmov.d    dr6,  @-%[dst]
        fmov.d    dr4,  @-%[dst]
    )"
                 : "=m"((*dst)[0]), "=m"((*dst)[1]), "=m"((*dst)[2]), "=m"((*dst)[3])
                 : [dst] "r"(*dst));
}

SHZ_FORCE_INLINE void shz_memcpy32_load_(const uint64_t* SHZ_RESTRICT* src) SHZ_NOEXCEPT {
    asm volatile(R"(
        fmov.d    @%[src]+, dr4
        fmov.d    @%[src]+, dr6
        fmov.d    @%[src]+, dr8
        fmov.d    @%[src]+, dr10
    )"
                 : [src] "+r"(*src)
                 : "m"(src[0]), "m"(src[1]), "m"(src[2]), "m"(src[3]));
}

SHZ_FORCE_INLINE void shz_memcpy64_load_(const uint64_t* SHZ_RESTRICT* src) SHZ_NOEXCEPT {
    asm volatile(R"(
        fmov.d    @%[src]+, dr0
        fmov.d    @%[src]+, dr2
        fmov.d    @%[src]+, dr4
        fmov.d    @%[src]+, dr6
        fmov.d    @%[src]+, dr8
        fmov.d    @%[src]+, dr10
        fmov.d    @%[src]+, dr12
        fmov.d    @%[src]+, dr14
    )"
                 : [src] "+r"(*src)
                 : "m"((*src)[0]), "m"((*src)[1]), "m"((*src)[2]), "m"((*src)[3]), "m"((*src)[4]), "m"((*src)[5]),
                   "m"((*src)[6]), "m"((*src)[7]));
}

SHZ_FORCE_INLINE void shz_memcpy64_store_(uint64_t* SHZ_RESTRICT* dst) SHZ_NOEXCEPT {
    asm volatile(R"(
        add       #32, %[dst]
        movca.l   r0, @%[dst]
        add       #32, %[dst]

        fmov.d    dr14, @-%[dst]
        fmov.d    dr12, @-%[dst]
        fmov.d    dr10, @-%[dst]
        fmov.d    dr8,  @-%[dst]

        add       #-32, %[dst]
        movca.l   r0, @%[dst]
        add       #32, %[dst]

        fmov.d    dr6,  @-%[dst]
        fmov.d    dr4,  @-%[dst]
        fmov.d    dr2,  @-%[dst]
        fmov.d    dr0,  @-%[dst]
    )"
                 : [dst] "+r"(*dst), "=m"((*dst)[0]), "=m"((*dst)[1]), "=m"((*dst)[2]), "=m"((*dst)[3]),
                   "=m"((*dst)[4]), "=m"((*dst)[5]), "=m"((*dst)[6]), "=m"((*dst)[7]));
}

SHZ_FORCE_INLINE void* shz_memcpy1(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    void* ret = dst;
    uint32_t scratch;

    SHZ_PREFETCH(src);

    if (bytes) {
        asm volatile(R"(
        1:
            mov.b   @%[src]+, %[tmp]
            dt      %[cnt]
            mov.b   %[tmp], @%[dst]
            bf/s    1b;
            add     #1, %[dst]
        )"
                     : [src] "+&r"(src), [dst] "+&r"(dst), [cnt] "+&r"(bytes), [tmp] "=r"(scratch)
                     : "m"(*((uint8_t (*)[]) src))
                     : "t", "memory");
    }

    return ret;
}

SHZ_INLINE void* shz_memcpy2(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    const shz_alias_uint16_t* s = (const shz_alias_uint16_t*) src;
    shz_alias_uint16_t* d = (shz_alias_uint16_t*) dst;

    bytes >>= 1;

    size_t blocks = bytes >> 3; // Block size of 16 bytes

    if (blocks) {
        do {
            s += 8;
            SHZ_PREFETCH(s); // Prefetch 16 bytes for next iteration
            d[7] = *(--s);
            d[6] = *(--s);
            d[5] = *(--s);
            d[4] = *(--s);
            d[3] = *(--s);
            d[2] = *(--s);
            d[1] = *(--s);
            d[0] = *(--s);
            d += 8;
            s += 8;
        } while (SHZ_LIKELY(--blocks));
        bytes &= 0xf;
    }

    while (SHZ_LIKELY(bytes--))
        d[bytes] = s[bytes];

    return dst;
}

SHZ_INLINE void* shz_memcpy4(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    const shz_alias_uint32_t* s = (const shz_alias_uint32_t*) src;
    shz_alias_uint32_t* d = (shz_alias_uint32_t*) dst;
    bytes >>= 2;
    size_t blocks = bytes >> 5; // Block size of 32 bytes

    if (blocks) {
        do {
            s += 8;
            SHZ_PREFETCH(s); // Prefetch 32 bytes for next iteration
            d[7] = *(--s);
            d[6] = *(--s);
            d[5] = *(--s);
            d[4] = *(--s);
            d[3] = *(--s);
            d[2] = *(--s);
            d[1] = *(--s);
            d[0] = *(--s);
            d += 8;
            s += 8;
        } while (SHZ_LIKELY(--blocks));
        bytes &= 0x1f;
    }

    while (SHZ_LIKELY(bytes--))
        d[bytes] = s[bytes];

    return dst;
}

SHZ_INLINE void* shz_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    shz_alias_uint64_t* d = (shz_alias_uint64_t*) dst;
    const shz_alias_uint64_t* s = (const shz_alias_uint64_t*) src;

    size_t cnt = (bytes >> 5);

    SHZ_FSCHG(true);

    if (SHZ_LIKELY(cnt >= 8)) {
        shz_memcpy128_(d, s, bytes);
        size_t copied = bytes / 128 * 128;
        cnt -= copied / 32;
        d += copied / sizeof(uint64_t);
        s += copied / sizeof(uint64_t);
    }

    while (SHZ_LIKELY(cnt--)) {
        shz_memcpy32_load_(&s);
        shz_dcache_alloc_line(d);
        shz_memcpy32_store_(&d);
        SHZ_PREFETCH(s);
        d += 4;
    }

    SHZ_FSCHG(false);

    return dst;
}

SHZ_INLINE void* shz_sq_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    void* ret = dst;

    bytes >>= 5;

    SHZ_FSCHG(true);

    asm volatile(R"(
    1:
        fmov.d @%[src]+, xd0
        fmov.d @%[src]+, xd2
        fmov.d @%[src]+, xd4
        fmov.d @%[src]+, xd6
        pref   @%[src]          ! Prefetch 32 bytes for next loop
        dt     %[blks]          ! while(n--)
        add    #32, %[dst]
        fmov.d xd6, @-%[dst]
        fmov.d xd4, @-%[dst]
        fmov.d xd2, @-%[dst]
        fmov.d xd0, @-%[dst]
        add    #32, %[dst]
        bf.s   1b
        pref   @%[dst]          ! Fire off store queue
    )"
                 : [dst] "+r"(dst), [src] "+&r"(src), [blks] "+r"(bytes)
                 : "m"((const char (*)[]) src)
                 : "memory");

    SHZ_FSCHG(false);

    return ret;
}

SHZ_INLINE void* shz_memcpy64(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    const shz_alias_uint64_t* s = (const shz_alias_uint64_t*) src;
    shz_alias_uint64_t* d = (shz_alias_uint64_t*) dst;

    SHZ_FSCHG(true);

    size_t cnt = (bytes >> 6);

    if (SHZ_LIKELY(cnt >= 4)) {
        shz_memcpy128_(d, s, bytes);
        size_t copied = bytes / 128 * 128;
        cnt -= copied / 64;
        d += copied / sizeof(uint64_t);
        s += copied / sizeof(uint64_t);
    }

    while (SHZ_LIKELY(cnt--)) {
        SHZ_PREFETCH(s + 4);
        shz_memcpy64_load_(&s);
        shz_memcpy64_store_(&d);
        SHZ_PREFETCH(s);
        d += 8;
    }

    SHZ_FSCHG(false);

    return dst;
}

SHZ_INLINE void* shz_memcpy128(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {

    if (bytes & ~0x7f) {
        SHZ_FSCHG(true);
        shz_memcpy128_(dst, src, bytes);
        SHZ_FSCHG(false);
    }

    return dst;
}

SHZ_INLINE void* shz_memcpy(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    const uint8_t* s = (const uint8_t*) src;
    uint8_t* d = (uint8_t*) dst;
    size_t copied;

    if (SHZ_UNLIKELY(!bytes))
        return dst;
    else if (SHZ_LIKELY(bytes < 64)) {
        shz_memcpy1(d, s, bytes);
    } else {
        if ((uintptr_t) d & 0x1f) {
            copied = (((uintptr_t) d + 31) & ~0x1f) - (uintptr_t) d;
            shz_memcpy1(d, s, copied);
            bytes -= copied;
            d += copied;
            s += copied;
        }

        if (SHZ_LIKELY(bytes >= 32)) {
            copied = 0;

            if (!(((uintptr_t) s) & 0x7)) {
                copied = bytes - (bytes & ~7);
                shz_memcpy8(d, s, copied);
            } else if (!(((uintptr_t) s) & 0x3)) {
                copied = bytes - (bytes & ~3);
                shz_memcpy4(d, s, copied);
            } else if (!(((uintptr_t) s) & 0x1)) {
                copied = bytes - (bytes & ~1);
                shz_memcpy2(d, s, copied);
            }

            bytes -= copied;
            d += copied;
            s += copied;
        }

        shz_memcpy1(d, s, bytes);
    }

    return dst;
}
#endif

#ifdef __cplusplus
}
#endif

#endif