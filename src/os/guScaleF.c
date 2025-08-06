#include "libultra_internal.h"
#include "sh4zam.h"

void guScaleF(float mf[4][4], float x, float y, float z) {
#if 0    
    guMtxIdentF(mf);
    mf[0][0] = x;
    mf[1][1] = y;
    mf[2][2] = z;
    mf[3][3] = 1.0;
#else
    shz_xmtrx_init_scale(x, y, z);
    shz_xmtrx_store_4x4_unaligned(mf);
#endif
}

void guScale(Mtx* m, float x, float y, float z) {
#ifndef GBI_FLOATS
    float mf[4][4];
    guScaleF(mf, x, y, z);
    guMtxF2L(mf, m);
#else
    guScaleF(m->m, x, y, z);
#endif
}
