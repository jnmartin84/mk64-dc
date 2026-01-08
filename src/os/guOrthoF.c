#include "libultra_internal.h"
#include "sh4zam.h"
void guOrthoF(float m[4][4], float left, float right, float bottom, float top, float near, float far, float scale) {
    int row;
    int col;
    f32 rl = shz_fast_invf(right - left);
    f32 tb = shz_fast_invf(top - bottom);
    f32 fn = shz_fast_invf(far - near);
    guMtxIdentF(m);
    m[0][0] = 2.0f * rl; // / (right - left);
    m[1][1] = 2.0f * tb; // / (top - bottom);
    m[2][2] = -2.0f * fn; // / (far - near);
    m[3][0] = -(right + left) * rl; // / (right - left);
    m[3][1] = -(top + bottom) * tb; // / (top - bottom);
    m[3][2] = -(far + near) * fn; // / (far - near);
    m[3][3] = 1;
    if (scale != 1.0f) {
        for (row = 0; row < 4; row++) {
            for (col = 0; col < 4; col++) {
                m[row][col] *= scale;
            }
        }
    }
}

void guOrtho(Mtx* m, float left, float right, float bottom, float top, float near, float far, float scale) {
#ifndef GBI_FLOATS
    float sp28[4][4];
    guOrthoF(sp28, left, right, bottom, top, near, far, scale);
    guMtxF2L(sp28, m);
#else
    guOrthoF(m->m, left, right, bottom, top, near, far, scale);
#endif
}