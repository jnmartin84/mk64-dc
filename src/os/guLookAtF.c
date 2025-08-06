/**************************************************************************
 *									  *
 *		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "libultra_internal.h"
#include "sh4zam.h"

void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp,
               float yUp, float zUp) {
    float len, xLook, yLook, zLook, xRight, yRight, zRight;

    //guMtxIdentF(mf);

    xLook = xAt - xEye;
    yLook = yAt - yEye;
    zLook = zAt - zEye;

    /* Negate because positive Z is behind us: */
    len = -1.0 / sqrtf(shz_mag_sqr4f(xLook, yLook, zLook, 0.0f));
    xLook *= len;
    yLook *= len;
    zLook *= len;

    /* Right = Up x Look */

    xRight = yUp * zLook - zUp * yLook;
    yRight = zUp * xLook - xUp * zLook;
    zRight = xUp * yLook - yUp * xLook;
    len = 1.0 / sqrtf(shz_mag_sqr4f(xRight, yRight, zRight, 0.0f));
    xRight *= len;
    yRight *= len;
    zRight *= len;

    /* Up = Look x Right */

    xUp = yLook * zRight - zLook * yRight;
    yUp = zLook * xRight - xLook * zRight;
    zUp = xLook * yRight - yLook * xRight;
    len = 1.0 / sqrtf(shz_mag_sqr4f(xUp, yUp, zUp, 0.0f));
    xUp *= len;
    yUp *= len;
    zUp *= len;

    mf[0][0] = xRight;
    mf[1][0] = yRight;
    mf[2][0] = zRight;

    mf[0][1] = xUp;
    mf[1][1] = yUp;
    mf[2][1] = zUp;

    mf[0][2] = xLook;
    mf[1][2] = yLook;
    mf[2][2] = zLook;

    mf[0][3] = 0;
    mf[1][3] = 0;
    mf[2][3] = 0;
    mf[3][3] = 1;
#if 0 // I DO NOT UNDERSTAND WHY THIS DOESN'T WORK!!!
    shz_vec3_t out = shz_matrix4x4_trans_vec3((SHZ_ALIASING const shz_matrix_4x4_t *)mf, (shz_vec3_t) { .x = xEye, .y = yEye, .z = zEye });
    mf[3][0] = -out.x;
    mf[3][1] = -out.y;
    mf[3][2] = -out.z;
#else
    mf[3][0] = -shz_dot8f(xEye,   yEye,   zEye,   0.0f,
                          xRight, yRight, zRight, 0.0f);
    mf[3][1] = -shz_dot8f(xEye, yEye, zEye, 0.0f,
                          xUp,  yUp,  zUp,  0.0f);
    mf[3][2] = -shz_dot8f(xEye,  yEye,  zEye,  0.0f,
                          xLook, yLook, zLook, 0.0f);
#endif
}

#ifndef GBI_FLOATS
void guLookAt(Mtx* m, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp,
              float zUp) {
    float mf[4][4];

    guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

    guMtxF2L(mf, m);
}
#else
void guLookAt(Mtx* m, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp,
              float zUp) {
    guLookAtF(m->m, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
}
#endif