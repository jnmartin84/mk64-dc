#ifndef GFX_RENDERING_API_H
#define GFX_RENDERING_API_H

#include <stddef.h>
#include <stdint.h>
//#include <stduint8_t.h>

// Abstract blend factors. The front-end derives these from the N64 blender (other_mode_l,
// game-agnostic) and the backend maps them onto its own blend API. Keeps N64 knowledge in
// the front-end and GPU knowledge in the backend — portable across games and backends.
enum gfx_blend_factor {
    GFX_BLENDF_ZERO = 0,
    GFX_BLENDF_ONE,
    GFX_BLENDF_SRCALPHA,
    GFX_BLENDF_INVSRCALPHA,
    GFX_BLENDF_DSTALPHA,
    GFX_BLENDF_INVDSTALPHA,
};

// Abstract texture-environment mode, derived in the front-end from the N64 color combiner and
// mapped onto the backend's texel<->vertex-color combine. Values intentionally match the PVR
// pvr_txr_shading_mode enum order so the raw-PVR backend maps 1:1. (See texenv derivation notes.)
enum gfx_tex_env {
    GFX_TEXENV_REPLACE = 0,        // px = tex                       (DECALRGBA)
    GFX_TEXENV_MODULATE,           // rgb = col*tex, a = tex.a       (MODULATEIDECALA, texel-free+oargb)
    GFX_TEXENV_DECAL,              // rgb = lerp(col,tex,tex.a), a=col.a  (DECALRGB, BLENDRGBFADEA)
    GFX_TEXENV_MODULATEALPHA,      // rgb = col*tex, a = col.a*tex.a  (MODULATEI/IA)
};

struct ShaderProgram;

struct GfxRenderingAPI {
    uint8_t (*z_is_from_0_to_1)(void);
    void (*unload_shader)(struct ShaderProgram *old_prg);
    void (*load_shader)(struct ShaderProgram *new_prg);
    struct ShaderProgram *(*create_and_load_new_shader)(uint32_t shader_id);
    struct ShaderProgram *(*lookup_shader)(uint32_t shader_id);
    void (*shader_get_info)(struct ShaderProgram *prg, uint8_t *num_inputs, uint8_t used_textures[2]);
    uint32_t (*new_texture)(void);
    void (*select_texture)(int tile, uint32_t texture_id);
#if defined(TARGET_PSP) || defined(TARGET_DC)
    void (*upload_texture)(const uint8_t *rgba32_buf, int width, int height, unsigned int type);
#else 
    void (*upload_texture)(const uint8_t *rgba32_buf, int width, int height);
#endif
    void (*set_sampler_parameters)(int sampler, uint8_t linear_filter, uint32_t cms, uint32_t cmt);
    void (*set_depth_test)(uint8_t depth_test);
    void (*set_depth_mask)(uint8_t z_upd);
    void (*set_zmode_decal)(uint8_t zmode_decal);
    // Texel<->vertex-color combine (enum gfx_tex_env), derived from the N64 combiner. Raw-PVR
    // folds it into the poly header; GLdc no-ops (it keys texenv off shader ids itself).
    void (*set_tex_env)(uint32_t mode);
    void (*set_viewport)(int x, int y, int width, int height);
    void (*set_scissor)(int x, int y, int width, int height);
    void (*set_use_alpha)(uint8_t use_alpha);
    void (*draw_triangles)(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris);
    void (*init)(void);
    void (*on_resize)(void);
    void (*start_frame)(void);
    void (*end_frame)(void);
    void (*finish_render)(void);
};
#ifdef PVR_ONLY
#include <kos.h>
struct PvrRenderingAPI {
    uint8_t (*z_is_from_0_to_1)(void);
    void (*unload_shader)(struct ShaderProgram *old_prg);
    void (*load_shader)(struct ShaderProgram *new_prg);
    struct ShaderProgram *(*create_and_load_new_shader)(uint32_t shader_id);
    struct ShaderProgram *(*lookup_shader)(uint32_t shader_id);
    void (*shader_get_info)(struct ShaderProgram *prg, uint8_t *num_inputs, uint8_t used_textures[2]);
    uint32_t (*new_texture)(void);
    void (*select_texture)(int tile, uint32_t texture_id);
    void (*upload_texture)(const uint16_t *rgba16_buf, int width, int height, unsigned int type);
    void (*set_sampler_parameters)(int sampler, uint8_t linear_filter, uint32_t cms, uint32_t cmt);
    void (*set_depth_test)(uint8_t depth_test);
    void (*set_depth_mask)(uint8_t z_upd);
    void (*set_zmode_decal)(uint8_t zmode_decal);
    void (*set_viewport)(int x, int y, int width, int height);
    void (*set_scissor)(int x, int y, int width, int height);
    void (*set_use_alpha)(uint8_t use_alpha);
    void (*draw_triangle)(pvr_vertex_t *verts);
    void (*draw_triangle)(pvr_vertex_t *verts);
    void (*init)(void);
    void (*on_resize)(void);
    void (*start_frame)(void);
    void (*end_frame)(void);
    void (*finish_render)(void);
};
#endif

#endif
