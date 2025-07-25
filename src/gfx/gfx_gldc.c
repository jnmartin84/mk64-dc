#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glkos.h>

#define FOR_WINDOWS 0

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "macros.h"
#include "gl_fast_vert.h"

enum MixType {
    SH_MT_NONE,
    SH_MT_TEXTURE,
    SH_MT_COLOR,
    SH_MT_TEXTURE_TEXTURE,
    SH_MT_TEXTURE_COLOR,
    SH_MT_COLOR_COLOR,
};

struct ShaderProgram {
    uint8_t enabled;
    uint32_t shader_id;
    struct CCFeatures cc;
    enum MixType mix;
    uint8_t texture_used[2];
    int texture_ord[2];
    int num_inputs;
};

struct SamplerState {
    GLuint srcaddr;
    GLenum min_filter;
    GLenum mag_filter;
    GLenum wrap_s;
    GLenum wrap_t;
    GLuint tex;
};

int in_intro;

uint32_t shaderlist[64];
uint8_t shaderidx;

extern int blend_fuck;

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static struct ShaderProgram* cur_shader = NULL;

static struct SamplerState tmu_state[2];

static const dc_fast_t* cur_buf = NULL;
static uint8_t gl_blend = 0;
static uint8_t gl_depth = 0;

static void resample_16bit(const unsigned short* in, int inwidth, int inheight, unsigned short* out, int outwidth,
                           int outheight) {
    int i, j;
    const unsigned short* inrow;
    unsigned int frac, fracstep;

    __builtin_prefetch(in);
    fracstep = inwidth * 0x10000 / outwidth;
    for (i = 0; i < outheight; i++, out += outwidth) {
        inrow = in + inwidth * (i * inheight / outheight);
        frac = fracstep >> 1;
        for (j = 0; j < outwidth; j += 4) {
            uint16_t p1,p2,p3,p4;
            if ((j & 7) == 0)
                __builtin_prefetch(inrow + 16);
            p1 = inrow[frac >> 16];
            frac += fracstep;
            p2 = inrow[frac >> 16];
            frac += fracstep;
            p3 = inrow[frac >> 16];
            frac += fracstep;
            p4 = inrow[frac >> 16];
            frac += fracstep;
#if 1
            asm volatile("": : : "memory");
#endif
            out[j] = p1;
            out[j + 1] = p2;
            out[j + 2] = p3;
            out[j + 3] = p4;
        }
    }
}

static inline uint32_t next_pot(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static inline uint32_t is_pot(const uint32_t v) {
    return (v & (v - 1)) == 0;
}

static uint8_t gfx_opengl_z_is_from_0_to_1(void) {
    return 1;
}

static inline GLenum texenv_set_color(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}

static inline GLenum texenv_set_texture(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}

static inline GLenum texenv_set_texture_color(struct ShaderProgram* prg) {
    GLenum mode = GL_MODULATE;

    // only set DECAL for the player animations in menu
//    if (blend_fuck) {
        if (prg->shader_id == 0x01200A00) {
            mode = GL_DECAL;
            if (!blend_fuck) blend_fuck = 2;
        }
//    }

    return mode;
}

static inline GLenum texenv_set_texture_texture(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}
extern int16_t fog_mul;
extern int16_t fog_ofs;

extern s16 gCurrentCourseId;

static void gfx_opengl_apply_shader(struct ShaderProgram* prg) {
    // vertices are always there
    glVertexPointer(3, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].vert);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].texture);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, sizeof(dc_fast_t), &cur_buf[0].color);

    // have texture(s), specify same texcoords for every active texture
    if (prg->texture_used[0] || prg->texture_used[1]) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    if (prg->shader_id & SHADER_OPT_FOG) {
        glEnable(GL_FOG);

        float fogmin, fogmax;

        fogmin = 500.0f - ((128000.0f * (float) fog_ofs) / (256.0f * (float) fog_mul));
        fogmax = fogmin + (128000.0f / (float) fog_mul);

        // choco mountain
        if (gCurrentCourseId == 1) {
            fogmin *= 0.4f;
            fogmax *= 0.6f;
        } else {
            fogmin *= 1.2f;
            fogmax *= 1.4f;
        }

        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, fogmin);
        glFogf(GL_FOG_END, fogmax);
    } else {
        glDisable(GL_FOG);
    }

//    if (!prg->enabled) {
        // we only need to do this once
        // ^-- uhhh yeah no I think that's wrong and a bug
        //        prg->enabled = 1;
#if 0
        if (prg->shader_id & SHADER_OPT_TEXTURE_EDGE) {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.8f);
        } else {
            glDisable(GL_ALPHA_TEST);
        }
#endif
        // configure texenv
        GLenum mode;
        switch (prg->mix) {
            case SH_MT_TEXTURE:
                mode = texenv_set_texture(prg);
                break;
            case SH_MT_TEXTURE_TEXTURE:
                mode = texenv_set_texture_texture(prg);
                break;
            case SH_MT_TEXTURE_COLOR:
                mode = texenv_set_texture_color(prg);
                break;
            default:
                mode = texenv_set_color(prg);
                break;
        }
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
//    }
}

static void gfx_opengl_unload_shader(struct ShaderProgram* old_prg) {
    if (cur_shader && (cur_shader == old_prg || !old_prg)) {
        cur_shader->enabled = 0;
        cur_shader = NULL;
    }
}

static void gfx_opengl_load_shader(struct ShaderProgram* new_prg) {
    cur_shader = new_prg;
    if (cur_shader)
        cur_shader->enabled = 0;
}

static struct ShaderProgram* gfx_opengl_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures ccf;
    gfx_cc_get_features(shader_id, &ccf);

    struct ShaderProgram* prg = &shader_program_pool[shader_program_pool_size++];

    prg->shader_id = shader_id;
    prg->cc = ccf;
    prg->num_inputs = ccf.num_inputs;
    prg->texture_used[0] = ccf.used_textures[0];
    prg->texture_used[1] = ccf.used_textures[1];

    if (ccf.used_textures[0] && ccf.used_textures[1]) {
        prg->mix = SH_MT_TEXTURE_TEXTURE;
        if (ccf.do_single[1]) {
            prg->texture_ord[0] = 1;
            prg->texture_ord[1] = 0;
        } else {
            prg->texture_ord[0] = 0;
            prg->texture_ord[1] = 1;
        }
    } else if (ccf.used_textures[0] && ccf.num_inputs) {
        prg->mix = SH_MT_TEXTURE_COLOR;
    } else if (ccf.used_textures[0]) {
        prg->mix = SH_MT_TEXTURE;
    } else if (ccf.num_inputs > 1) {
        prg->mix = SH_MT_COLOR_COLOR;
    } else if (ccf.num_inputs) {
        prg->mix = SH_MT_COLOR;
    }

    prg->enabled = 0;

    gfx_opengl_load_shader(prg);

    return prg;
}

static struct ShaderProgram* gfx_opengl_lookup_shader(uint32_t shader_id) {
    size_t i;

    for (i = 0; i < shader_program_pool_size; i++)
        if (shader_program_pool[i].shader_id == shader_id)
            return &shader_program_pool[i];
    return NULL;
}

static void gfx_opengl_shader_get_info(struct ShaderProgram* prg, uint8_t* num_inputs, uint8_t used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->texture_used[0];
    used_textures[1] = prg->texture_used[1];
}

GLuint newest_texture;

static void gfx_clear_all_textures(void) {
    GLuint index = 0;
    if (newest_texture != 0) {
        for (index = 1; index <= newest_texture; index++) {
            glDeleteTextures(0, &index);
        }
        tmu_state[0].tex = 0;
        tmu_state[1].tex = 0;
    }
}

void gfx_clear_texidx(GLuint texidx) {
    GLuint index = texidx;
    glDeleteTextures(0, &index);
    if (tmu_state[0].tex == texidx)
        tmu_state[0].tex = 0;
    if (tmu_state[1].tex == texidx)
        tmu_state[1].tex = 0;
}

#if 0
static uint16_t bg_staging[512*256];
void setup_the_bg_texture(void) {
    GLuint ret;
    glGenTextures(1, &ret);
    if (ret != 1) {
        printf("fucked up other texid is %d\n", ret);
        return;
    }
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB1555_KOS, 512, 256, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, bg_staging);
}
#endif

static uint32_t gfx_opengl_new_texture(void) {
    GLuint ret;
    glGenTextures(1, &ret);
    newest_texture = ret;
    return (uint32_t) ret;
}

void gfx_opengl_set_tile_addr(int tile, GLuint addr) {
    tmu_state[tile].srcaddr = (GLuint) addr;
}

static void gfx_opengl_select_texture(int tile, uint32_t texture_id) {
    tmu_state[tile].tex = texture_id; // remember this for multitexturing later
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

/* Used for rescaling textures ROUGHLY into pow2 dims */
static unsigned int __attribute__((aligned(32))) scaled[64 * 64 * 4];//sizeof(unsigned int)]; /* 16kb */

#define LET_GLDC_TWIDDLE 0

static void gfx_opengl_upload_texture(const uint8_t* rgba32_buf, int width, int height, unsigned int type) {
    GLint intFormat;

#if LET_GLDC_TWIDDLE
    if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_TWID_KOS;
    } else {
        intFormat = GL_ARGB4444_TWID_KOS;
    }
#else
    if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_KOS;
    } else {
        intFormat = GL_ARGB4444_KOS;
    }
#endif

    // we don't support non power of two textures, scale to next power of two if necessary
    if ((!is_pot(width) || !is_pot(height)) || (width < 8) || (height < 8)) {
        int pwidth = next_pot(width);
        int pheight = next_pot(height);

        /* Need texture min sizes */
        if (pwidth < 8) {
            pwidth = 8;
        }
        if (pheight < 8) {
            pheight = 8;
        }

        resample_16bit((const uint16_t*) rgba32_buf, width, height, (uint16_t*) scaled, pwidth, pheight);
        rgba32_buf = (uint8_t*) scaled;
        width = pwidth;
        height = pheight;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, intFormat, width, height, 0, GL_BGRA, type, rgba32_buf);
}

static inline GLenum gfx_cm_to_opengl(uint32_t val) {
    if (val & G_TX_CLAMP)
        return GL_CLAMP;

    return (val & G_TX_MIRROR) ? GL_MIRRORED_REPEAT : GL_REPEAT;
}

static inline void gfx_opengl_apply_tmu_state(const int tile) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tmu_state[tile].min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tmu_state[tile].mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tmu_state[tile].wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tmu_state[tile].wrap_t);
}

static void gfx_opengl_set_sampler_parameters(int tile, uint8_t linear_filter, uint32_t cms, uint32_t cmt) {
    const GLenum filter = linear_filter ? GL_LINEAR : GL_NEAREST;
    const GLenum wrap_s = gfx_cm_to_opengl(cms);
    const GLenum wrap_t = gfx_cm_to_opengl(cmt);

    tmu_state[tile].min_filter = filter;
    tmu_state[tile].mag_filter = filter;
    tmu_state[tile].wrap_s = wrap_s;
    tmu_state[tile].wrap_t = wrap_t;

    // set state for the first texture right away
    if (!tile)
        gfx_opengl_apply_tmu_state(tile);
}

static void gfx_opengl_set_depth_test(uint8_t depth_test) {
    if (depth_test != 0)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

static void gfx_opengl_set_depth_mask(uint8_t z_upd) {
    gl_depth = z_upd;
    glDepthMask(z_upd ? GL_TRUE : GL_FALSE);
}

static uint8_t is_zmode_decal = false;
// Polyoffset currently doesn't work so gotta workaround it.
static void gfx_opengl_set_zmode_decal(uint8_t zmode_decal) {
    is_zmode_decal = zmode_decal;
    if (zmode_decal) {
        glDepthFunc(GL_LEQUAL);
    } else {
        glDepthFunc(GL_LESS);
    }
}

static void gfx_opengl_set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

static void gfx_opengl_set_scissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

static void gfx_opengl_set_use_alpha(uint8_t use_alpha) {
    gl_blend = use_alpha;
    if (use_alpha)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

// draws the same triangles as plain fog color + fog intensity as alpha
// on top of the normal tris and blends them to achieve sort of the same effect
// as fog would
static inline void gfx_opengl_pass_fog(void) {
    ;
}

// this assumes the two textures are combined like so:
// result = mix(tex0.rgb, tex1.rgb, vertex.rgb)
static inline void gfx_opengl_pass_mix_texture(int buf_vbo_num_tris) {
    ;
}

// toads turnpike used shaders
// 01200200, 01045200, 07a00a00, 03200045, 05141548, 01045551, 05a00a00
// lakitu sprites
// if (cur_shader->shader_id != 0x05a00a00)
// ????
// if (cur_shader->shader_id != 0x01045551)
// the kart
// if (cur_shader->shader_id != 0x05141548)
// the entire world basically except guardrails
// if (cur_shader->shader_id != 0x03200045)
// the guardrails
// if (cur_shader->shader_id != 0x07a00a00)
// 4 bit font AND the stars, goddamnit
// if (cur_shader->shader_id != 0x01045200)

// prim color
extern int pr, pg, pb, pa;
// env color
extern int er, eg, eb, ea;
// "stars" over Toad's Turnpike/Wario Stadium skybox
extern u8 D_0D0293D8[];
extern void* segmented_to_virtual(void* addr);
// Rainbow Road road surface texture, for blending tricks
//extern u8 gRRTextureRainbow[];

static void gfx_opengl_draw_triangles(float buf_vbo[], UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    cur_buf = (void*) buf_vbo;

    gfx_opengl_apply_shader(cur_shader);

    // if there's two textures, set primary texture first
    if (cur_shader->texture_used[1]) {
        glBindTexture(GL_TEXTURE_2D, tmu_state[cur_shader->texture_ord[0]].tex);
    }

    // skybox
    if (cur_shader->shader_id == 0x01200200) {
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
        glDisable(GL_FOG);
    }

    // clouds over skybox
    if (cur_shader->shader_id == 0x01a00200) { 
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -3500.0f);
    }

    if (is_zmode_decal) {
        // Adjust depth values slightly for zmode_decal objects
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);

        // Push the geometry slightly towards the camera
        glPushMatrix();
        glTranslatef(0.0f, 2.1f, 0.9f); // magic values need fine tuning.
    }

    // this is pretty fine-tuned at this point
    // this shader id covers lots of "smoke" effects
    // some of those effects we would like to brighten
    if (cur_shader->shader_id == 0x01045551) {
        // the outer test here discards kart smoke and other white smokes
        if (!((pr == 251) && (pg == 255) && (pb == 251))) {
            // discard other gray smokes
            if (!((pr == pg) && (pr == pb))) {
                // boost flames
                if( (pb > 50 && pb < 135) || 
                    // flames on the cave wall in DK Jungle
                    ((er == 255) && (eg == 95) && (eb == 0)) ||
                    // green shell trail
                    ((er == 255) && (eg == 0) && (eb == 0)) ||
                    // blue shell trail
                    ((er == 0) && (eg == 0) && (eb == 255))) {

                    // MAKE IT BRIGHT MAKE IT BURN
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                }
            }
        }
    }

    // star item effect
    if (cur_shader->shader_id == 0x09045551) {
        // shades of orange for the player when star item active
        if (pr > 200 && pg > 200 && pb < 150) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else if (pr < 190 && pg < 190 && pb < 190) {
            if (!((eg+60) < er && (4*eb) < eg)) {
                if (!((eg + 30) < er && (eb + 30) < eg)) {
                    // I don't know this actually runs
                    // can't remember why it is here
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                }
            }
        }
    }

    // this fixes the twinkling stars on Toads Turnpike and Wario Stadium
    if (cur_shader->shader_id == 0x01045200 && 
        (cur_shader->texture_used[0] || cur_shader->texture_used[1]) &&
        (tmu_state[0].srcaddr == (GLuint) segmented_to_virtual(D_0D0293D8) ||
         tmu_state[1].srcaddr == (GLuint) segmented_to_virtual(D_0D0293D8))) {

        // like the clouds over skybox
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -3500.0f);
    }

    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);

    if (is_zmode_decal) {
        glPopMatrix();
        glDepthFunc(GL_LESS); // Reset depth function
    }

    // skybox
    if (cur_shader->shader_id == 0x01200200) {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glEnable(GL_FOG);
    }

    // clouds over skybox
    if (cur_shader->shader_id == 0x01a00200) {
        glPopMatrix();
    }

    // twinkling stars on Toads Turnpike and Wario Stadium
    if (cur_shader->shader_id == 0x01045200 &&
        (cur_shader->texture_used[0] || (cur_shader->texture_used[1])) &&
        (tmu_state[0].srcaddr == (GLuint) segmented_to_virtual(D_0D0293D8) ||
         tmu_state[1].srcaddr == (GLuint) segmented_to_virtual(D_0D0293D8))) {
        glPopMatrix();
    }

    // this is pretty fine-tuned at this point
    // this shader id covers lots of "smoke" effects
    // some of those effects we would like to brighten
    if (cur_shader->shader_id == 0x01045551) {
        // boost flames
        if( (pb > 50 && pb < 135) || 
            // flames on the cave wall in DK Jungle
            ((er == 255) && (eg == 95) && (eb == 0)) ||
            // green shell trail
            ((er == 255) && (eg == 0) && (eb == 0)) ||
            // blue shell trail
            ((er == 0) && (eg == 0) && (eb == 255))) {

            // restore default blend mode
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // star effect
    // doesn't matter if we actually changed the blend before
    // just setting it back to default
    if (cur_shader->shader_id == 0x09045551) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

#if 1
extern u8 gTextureMarioFace00[];
extern u8 gTextureBowserFace16_end[];
extern void gfx_opengl_2d_projection(void);
extern void gfx_opengl_reset_projection(void);
void gfx_opengl_draw_triangles_2d(void* buf_vbo, UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    glDisable(GL_FOG);
    gfx_opengl_2d_projection();

    dc_fast_t* tris = buf_vbo;

    gfx_opengl_apply_shader(cur_shader);

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(dc_fast_t), &tris[0].vert);
    glTexCoordPointer(2, GL_FLOAT, sizeof(dc_fast_t), &tris[0].texture);
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, sizeof(dc_fast_t), &tris[0].color);
    glEnable(GL_BLEND);
glDisable(GL_DEPTH_TEST);
    if (buf_vbo_num_tris) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        // if there's two textures, set primary texture first
        if (cur_shader->texture_used[1])
            glBindTexture(GL_TEXTURE_2D, tmu_state[cur_shader->texture_ord[0]].tex);
    } else {
    glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
//    if (!in_intro) {
       
        //        glDisable(GL_BLEND);
  // FRAGILE AS FUCK

  /*
  
  
  someTexture 8c9376e0
someTexture 8c9376e0
someTexture 8c93bce0
someTexture 8c93bce0
someTexture 8c9422e0
someTexture 8c9422e0
someTexture 8c9448e0
someTexture 8c9448e0
someTexture 8c94aee0
someTexture 8c94aee0
someTexture 8c953ae0
someTexture 8c953ae0
someTexture 8c9560e0
someTexture 8c9560e0
  
  */
  //  if (blend_fuck) {
//            glDisable(GL_BLEND);
//            printf("in there\n");
    //} else {
      //  glEnable(GL_BLEND);
    //}
//}
        ////        glDisable(GL_DEPTH_TEST);
    ////    glDepthMask(GL_FALSE);
        ////glDepthFunc(GL_LEQUAL);
  //  }
#if 0
#if 1
    if (in_intro &&
        cur_shader->shader_id == 0x01200200) { // 18874437){ // 0x1200045, skybox  // may need to relook at this
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
        glDisable(GL_FOG);
    }


#if 1
    if (in_intro && cur_shader->shader_id == 0x01a00200) { // clouds over skybox
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -3500.0f);
    }
#endif
    if (in_intro && is_zmode_decal) {
        // Adjust depth values slightly for zmode_decal objects
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);

        // Push the geometry slightly towards the camera
        glPushMatrix();
        glTranslatef(0.0f, 2.1f, 0.9f); // magic values need fine tuning.
    }
#endif
#endif

    if (blend_fuck) {
        for (int i = 0; i < 6; i++) {
            tris[i].color.packed = 0xff000000;
        }
        if (blend_fuck == 2) blend_fuck = 0;
    }

    glDrawArrays(GL_TRIANGLES, 0, 3 * 2 /* 2 tri quad */);

    #if 0
    if (!in_intro) {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }
    #if 1
    if (in_intro && is_zmode_decal) {
        glPopMatrix();
        glDepthFunc(GL_LESS); // Reset depth function
    }

    if (in_intro && cur_shader->shader_id == 0x01200200) { // 18874437){ // 0x1200045, skybox
        //        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glEnable(GL_FOG);
    }

    if (in_intro && cur_shader->shader_id == 0x01a00200) {
        glPopMatrix();
    }
#endif
#endif
/////    glEnable(GL_BLEND);
    gfx_opengl_reset_projection();
}
#endif

static inline uint8_t gl_check_ext(const char* name) {
    static const char* extstr = NULL;

    if (extstr == NULL)
        extstr = (const char*) glGetString(GL_EXTENSIONS);

    if (!strstr(extstr, name)) {
        return 0;
    }

    return 1;
}

static inline uint8_t gl_get_version(int* major, int* minor, uint8_t* is_es) {
    const char* vstr = (const char*) glGetString(GL_VERSION);
    if (!vstr || !vstr[0])
        return 0;

    if (!strncmp(vstr, "OpenGL ES ", 10)) {
        vstr += 10;
        *is_es = 1;
    } else if (!strncmp(vstr, "OpenGL ES-CM ", 13)) {
        vstr += 13;
        *is_es = 1;
    }

    return (sscanf(vstr, "%d.%d", major, minor) == 2);
}

#define sys_fatal printf

extern void getRamStatus(void);
static void gfx_opengl_init(void) {
#if FOR_WINDOWS || defined(OSX_BUILD)
    GLenum err;
    if ((err = glewInit()) != GLEW_OK)
        sys_fatal("could not init GLEW:\n%s", glewGetErrorString(err));
#endif
    newest_texture = 0;
    shaderidx = 0;
    memset(shaderlist, 0, sizeof(shaderlist));
    GLdcConfig config;
    glKosInitConfig(&config);
    config.autosort_enabled = GL_TRUE;
    config.fsaa_enabled = GL_FALSE;
    /*@Note: These should be adjusted at some point */
    config.initial_op_capacity = 128;
    config.initial_pt_capacity = 32;
    config.initial_tr_capacity = 256;
    config.initial_immediate_capacity = 0;
    glKosInitEx(&config);
    // glKosInit();

    //    getRamStatus();
    fflush(stdout);

    // check GL version
    int vmajor, vminor;
    uint8_t is_es = 0;
    gl_get_version(&vmajor, &vminor, &is_es);
    if ((vmajor < 2 && vminor < 1) || is_es)
        sys_fatal("OpenGL 1.1+ is required.\nReported version: %s%d.%d\n", is_es ? "ES" : "", vmajor, vminor);

    printf("GL_VERSION = %s\n", glGetString(GL_VERSION));
    printf("GL_EXTENSIONS =\n%s\n", glGetString(GL_EXTENSIONS));

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    static float fog[4] = { 1.f, 1.f, 1.f, 0.5f };

    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.f);
    glFogf(GL_FOG_END, 256.f);
    glFogfv(GL_FOG_COLOR, fog);
}

static void gfx_opengl_on_resize(void) {

}

extern void reset_texcache(void);
extern float screen_2d_z;

void nuke_everything(void) {
    gfx_clear_all_textures();
    reset_texcache();
}

extern s32 D_800DC5D0;
extern s32 D_800DC5D4;
extern s32 D_800DC5D8;

static void gfx_opengl_start_frame(void) {
    screen_2d_z = -1.0f;
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE); // Must be set to clear Z-buffer

    glClearColor((float)D_800DC5D0/255.0f, (float)D_800DC5D4/255.0f,
    (float)D_800DC5D8/255.0f,1.0f);
//    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    newest_texture = 0;
}

static void gfx_opengl_end_frame(void) {
    return;
}

static void gfx_opengl_finish_render(void) {
    return;
}

struct GfxRenderingAPI gfx_opengl_api = { gfx_opengl_z_is_from_0_to_1, gfx_opengl_unload_shader,
                                          gfx_opengl_load_shader,      gfx_opengl_create_and_load_new_shader,
                                          gfx_opengl_lookup_shader,    gfx_opengl_shader_get_info,
                                          gfx_opengl_new_texture,      gfx_opengl_select_texture,
                                          gfx_opengl_upload_texture,   gfx_opengl_set_sampler_parameters,
                                          gfx_opengl_set_depth_test,   gfx_opengl_set_depth_mask,
                                          gfx_opengl_set_zmode_decal,  gfx_opengl_set_viewport,
                                          gfx_opengl_set_scissor,      gfx_opengl_set_use_alpha,
                                          gfx_opengl_draw_triangles,   gfx_opengl_init,
                                          gfx_opengl_on_resize,        gfx_opengl_start_frame,
                                          gfx_opengl_end_frame,        gfx_opengl_finish_render };