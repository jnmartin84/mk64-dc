#include "libultra_internal.h"
#include "sh4zam.h"

void guNormalize(f32* x, f32* y, f32* z) {
    shz_vec3_t tmp = shz_vec3_normalize((shz_vec3_t) { .x = *x, .y = *y, .z = *z});
    *x = tmp.x;
    *y = tmp.y;
    *z = tmp.z;
}
