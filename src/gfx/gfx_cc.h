#ifndef GFX_CC_H
#define GFX_CC_H

#include <stdint.h>
//#include <stdbool.h>

enum {
    CC_0,
    CC_TEXEL0,
    CC_TEXEL1,
    CC_PRIM,
    CC_SHADE,
    CC_ENV,
    CC_TEXEL0A,
    CC_LOD
};

enum {
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1
};

#define SHADER_OPT_ALPHA (1 << 24)
#define SHADER_OPT_FOG (1 << 25)
#define SHADER_OPT_TEXTURE_EDGE (1 << 26)
#define SHADER_OPT_NOISE (1 << 27)

struct CCFeatures {
    uint8_t c[2][4];
    uint8_t opt_alpha;
    uint8_t opt_fog;
    uint8_t opt_texture_edge;
    uint8_t opt_noise;
    uint8_t used_textures[2];
    int num_inputs;
    uint8_t do_single[2];
    uint8_t do_multiply[2];
    uint8_t do_mix[2];
    uint8_t color_alpha_same;
};

#ifdef __cplusplus
extern "C" {
#endif

void gfx_cc_get_features(uint32_t shader_id, struct CCFeatures *cc_features);

#ifdef __cplusplus
}
#endif

#endif
