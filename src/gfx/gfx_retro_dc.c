#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "gfx_pc.h"
#include "gfx_cc.h"
#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"
#include "gfx_screen_config.h"
#include "macros.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glkos.h>
#include "gl_fast_vert.h"
extern int in_intro;

uint32_t last_set_texture_image_width;
int draw_rect;
uint32_t oops_texture_id;

void* segmented_to_virtual(void* addr);

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_3_5(VAL_) ((VAL_) << 2)

#define SCALE_5_8(VAL_) ((VAL_) << 3)
#define SCALE_8_5(VAL_) ((VAL_) >> 3)

#define SCALE_4_8(VAL_) ((VAL_) << 4)
#define SCALE_8_4(VAL_) ((VAL_) >> 4)

#define SCALE_3_8(VAL_) ((VAL_) << 5)
#define SCALE_8_3(VAL_) ((VAL_) >> 5)

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH / 2)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT / 2)

#define RATIO_X (gfx_current_dimensions.width / (2.0f * HALF_SCREEN_WIDTH))
#define RATIO_Y (gfx_current_dimensions.height / (2.0f * HALF_SCREEN_HEIGHT))

// enough to submit the nintendo logo in one go
//#define MAX_BUFFERED (384)
// need to save some memory
#define MAX_BUFFERED 128
#define MAX_LIGHTS 2
#define MAX_VERTICES 64

int blend_fuck=0;

struct ShaderProgram {
	uint8_t enabled;
	uint32_t shader_id;
	struct CCFeatures cc;
	int mix;
	uint8_t texture_used[2];
	int texture_ord[2];
	int num_inputs;
};

struct RGBA {
	uint8_t r, g, b, a;
};

struct XYWidthHeight {
	uint16_t x, y, width, height;
};

struct __attribute__((aligned(32))) LoadedVertex {
	// 16
	float _x, _y, _z, _w;
	// 32
	float u, v;
	// 40
	struct RGBA color;
	// 0
	float x, y, z, w;
	// 44
	uint8_t clip_rej;
	// 45
	uint8_t	wlt0;
};


static inline uint64_t pack_key(uint32_t a, uint16_t b, uint16_t c, uint8_t d, uint8_t e) {
    uint64_t key = 0;
    key |= ((uint64_t)((a >> 5) & 0x7FFFF)) << 42; // 19 bits
    key |= ((uint64_t)b & 0xFFFF) << 26;           // 16 bits
    key |= ((uint64_t)c & 0xFFFF) << 10;           // 16 bits
    key |= ((uint64_t)d & 0xFF)   << 2;            // 8 bits
    key |= ((uint64_t)(e & 0x3));                  // 2 bits
    return key;
}


// exactly one cache-line in size
struct TextureHashmapNode {
	// 0
	struct TextureHashmapNode* next;
	// 4
	uint32_t texture_id;
#if 0
	// 8
	const uint8_t* texture_addr;
	// 16
	uint16_t uls;
	// 18
	uint16_t ult;
	// 20
	uint8_t fmt, siz;
#endif
	// 8
	uint64_t key;
	// 16
	uint32_t tmem;
	// 20
	uint8_t dirty;
	// 21
	uint8_t linear_filter;
	// 22
	uint8_t cms, cmt;
	// 23
	uint8_t pad1;
	// 24
	uint32_t pad2;
	// 28
	uint32_t pad3;
	// 28
//	uint32_t pad3;
};

struct TextureHashmapNode oops_node;

static struct {
	struct TextureHashmapNode* hashmap[1024];
	struct TextureHashmapNode pool[1024];
	uint32_t pool_pos;
} gfx_texture_cache;

struct ColorCombiner {
	uint32_t cc_id;
	struct ShaderProgram* prg;
	uint8_t shader_input_mapping[2][4];
};

static struct ColorCombiner color_combiner_pool[64];
static uint8_t color_combiner_pool_size;

static struct RSP {
	struct LoadedVertex __attribute__((aligned(32))) loaded_vertices[MAX_VERTICES + 4];
	struct __attribute__((aligned(32))) dc_fast_t loaded_vertices_2D[4];

	float modelview_matrix_stack[11][4][4] __attribute__((aligned(32)));

	float MP_matrix[4][4] __attribute__((aligned(32)));

	float P_matrix[4][4] __attribute__((aligned(32)));

	Light_t __attribute__((aligned(32))) current_lights[MAX_LIGHTS + 1];

	float __attribute__((aligned(32))) current_lights_coeffs[MAX_LIGHTS][3];

	float __attribute__((aligned(32))) current_lookat_coeffs[2][3]; // lookat_x, lookat_y

	uint32_t __attribute__((aligned(32))) geometry_mode;
	struct {
		// U0.16
		uint16_t s, t;
	} texture_scaling_factor;
	int16_t fog_mul, fog_offset;
	uint8_t modelview_matrix_stack_size;
	// 866
	uint8_t current_num_lights;        // includes ambient light
	// 867
	uint8_t lights_changed;
} rsp __attribute__((aligned(32)));

static struct RDP {
	const uint8_t* palette;
	struct {
		const uint8_t* addr;
		uint8_t siz;
		uint8_t tile_number;
		uint32_t tmem;
	} texture_to_load;
	struct {
		const uint8_t* addr;
		uint32_t size_bytes;
	} loaded_texture[8];
	struct {
		uint8_t fmt;
		uint8_t siz;
		uint8_t cms, cmt;
		uint16_t uls, ult, lrs, lrt; // U10.2
		uint32_t line_size_bytes;
	} texture_tile;
	uint8_t textures_changed[2];

	uint32_t other_mode_l, other_mode_h;
	uint32_t combine_mode;

	struct RGBA env_color, prim_color, fog_color, fill_color;
	struct XYWidthHeight viewport, scissor;
	uint8_t viewport_or_scissor_changed;
	void* z_buf_address;
	void* color_image_address;
} rdp;

struct RenderingState {
	// 0
	struct ShaderProgram* shader_program;
	// 4
	struct TextureHashmapNode* textures[2];
	// 8
	uint8_t depth_test;
	uint8_t depth_mask;
	uint8_t decal_mode;
	uint8_t alpha_blend;
	// 12
	struct XYWidthHeight viewport;
	// 20
	struct XYWidthHeight scissor;
	// 28
	uint32_t pad;
};

struct RenderingState rendering_state;

struct GfxDimensions gfx_current_dimensions;

static uint8_t dropped_frame;

static dc_fast_t __attribute__((aligned(32))) buf_vbo[MAX_BUFFERED * 3]; // 3 vertices in a triangle
static dc_fast_t __attribute__((aligned(32))) quad_vbo[2 * 3];           // 2 tris make a quad
static size_t buf_vbo_len = 0;
static size_t buf_num_vert = 0;
static size_t buf_vbo_num_tris = 0;

static struct GfxWindowManagerAPI* gfx_wapi;
static struct GfxRenderingAPI* gfx_rapi;

static uint16_t __attribute__((aligned(32))) tlut[256];

int do_fill_rect = 0;

extern s16 gCurrentCourseId;

static  __attribute__((noinline)) void gfx_flush(void) {
	if (buf_vbo_len) {
		gfx_rapi->draw_triangles((void*) buf_vbo, buf_vbo_len, buf_vbo_num_tris);
		buf_vbo_len = 0;
		buf_num_vert = 0;
		buf_vbo_num_tris = 0;
	}
}

static  __attribute__((noinline)) struct ShaderProgram* gfx_lookup_or_create_shader_program(uint32_t shader_id) {
	struct ShaderProgram* prg = gfx_rapi->lookup_shader(shader_id);
	if (prg == NULL) {
		gfx_rapi->unload_shader(rendering_state.shader_program);
		prg = gfx_rapi->create_and_load_new_shader(shader_id);
		rendering_state.shader_program = prg;
	}
	return prg;
}

void n64_memcpy(void *dst, const void *src, size_t size);

static  __attribute__((noinline)) void gfx_generate_cc(struct ColorCombiner* comb, uint32_t cc_id) {
	uint8_t c[2][4];
	uint32_t shader_id = (cc_id >> 24) << 24;
	uint8_t shader_input_mapping[2][4] = { { 0 } };
	int i, j;

	for (i = 0; i < 4; i++) {
		c[0][i] = (cc_id >> (i * 3)) & 7;
		c[1][i] = (cc_id >> (12 + i * 3)) & 7;
	}
	for (i = 0; i < 2; i++) {
		if (c[i][0] == c[i][1] || c[i][2] == CC_0) {
			c[i][0] = c[i][1] = c[i][2] = 0;
		}
		uint8_t input_number[8] = { 0 };
		int next_input_number = SHADER_INPUT_1;
		for (j = 0; j < 4; j++) {
			int val = 0;
			switch (c[i][j]) {
				case CC_0:
					break;
				case CC_TEXEL0:
					val = SHADER_TEXEL0;
					break;
				case CC_TEXEL1:
					val = SHADER_TEXEL1;
					break;
				case CC_TEXEL0A:
					val = SHADER_TEXEL0A;
					break;
				case CC_PRIM:
				case CC_SHADE:
				case CC_ENV:
				case CC_LOD:
					if (input_number[c[i][j]] == 0) {
						shader_input_mapping[i][next_input_number - 1] = c[i][j];
						input_number[c[i][j]] = next_input_number++;
					}
					val = input_number[c[i][j]];
					break;
			}
			shader_id |= val << (i * 12 + j * 3);
		}
	}
	comb->cc_id = cc_id;
	comb->prg = gfx_lookup_or_create_shader_program(shader_id);
	n64_memcpy(comb->shader_input_mapping, shader_input_mapping, sizeof(shader_input_mapping));
}

static  __attribute__((noinline)) struct ColorCombiner* gfx_lookup_or_create_color_combiner(uint32_t cc_id) {
	size_t i;

	static struct ColorCombiner* prev_combiner;
	if (prev_combiner != NULL && prev_combiner->cc_id == cc_id) {
		return prev_combiner;
	}

	for (i = 0; i < color_combiner_pool_size; i++) {
		if (color_combiner_pool[i].cc_id == cc_id) {
			return prev_combiner = &color_combiner_pool[i];
		}
	}
	gfx_flush();
	struct ColorCombiner* comb = &color_combiner_pool[color_combiner_pool_size++];
	gfx_generate_cc(comb, cc_id);
	return prev_combiner = comb;
}
void gfx_clear_texidx(GLuint texidx);

void reset_texcache(void) {
	gfx_texture_cache.pool_pos = 0;
//	memset(&gfx_texture_cache, 0, sizeof(gfx_texture_cache));
}

static inline uint32_t unpack_A(uint64_t key) {
    // Extract 19-bit field
    uint32_t a19 = (uint32_t)((key >> 42) & 0x7FFFF);

    // Shift back to original position (restore the lost low 5 bits as 0s)
//    uint32_t a = (a19 << 5);

    // Restore the constant top 8 bits (0x8C per your earlier note)
//    a |= 0x8C000000;

    return a19;
}

static inline uint64_t hash64(uint64_t key) {
    uint64_t h = 1469598103934665603ULL; // FNV offset basis
    h ^= key; 
    h *= 1099511628211ULL;               // FNV prime
    return h;
}

void gfx_texture_cache_invalidate(void* orig_addr) {
 	void* segaddr = segmented_to_virtual(orig_addr);

	size_t hash = (uintptr_t) segaddr;
	hash = (hash >> 5) & 0x3ff;


	uintptr_t addrcomp = ((uintptr_t)segaddr>>5)&0x7FFFF;
	struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
	uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
	while (*node != NULL && ((uintptr_t)*node < last_node)) {
		__builtin_prefetch((*node)->next);
		uintptr_t unpaddr = (uintptr_t)unpack_A((*node)->key);
		if (unpaddr == addrcomp) {
			(*node)->dirty = 1;
		}
		node = &(*node)->next;
	}
}

void gfx_opengl_replace_texture(const uint8_t* rgba32_buf, int width, int height, unsigned int type);

extern void gfx_opengl_set_tile_addr(int tile, GLuint addr);

//int max_lookup = 0;

static  __attribute__((noinline)) uint8_t gfx_texture_cache_lookup(int tile, struct TextureHashmapNode** n, const uint8_t* orig_addr,
										uint32_t tmem, uint32_t fmt, uint32_t siz, uint16_t uls, uint16_t ult) {
	void* segaddr = segmented_to_virtual((void *)orig_addr);
	size_t hash = (uintptr_t) segaddr;
	hash = (hash >> 5) & 0x3ff;

	uint64_t newkey = pack_key(segaddr,uls,ult,fmt,siz);

	struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
	if ((uintptr_t)rdp.loaded_texture[tile].addr < 0x8c010000u) {
		gfx_rapi->select_texture(tile, oops_texture_id);
		*node = &oops_node;
		return 1;
	}
	__builtin_prefetch(*node);

	uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
	while (*node != NULL && ((uintptr_t)*node < last_node)) {
		__builtin_prefetch((*node)->next);

		if ((*node)->key == newkey
			/* (*node)->texture_addr == segaddr && (*node)->ult == ult && (*node)->uls == uls &&
			(*node)->fmt == fmt && (*node)->siz == siz */) {
			gfx_rapi->select_texture(tile, (*node)->texture_id);
			gfx_opengl_set_tile_addr(tile, segaddr);

			if ((*node)->dirty) {
				(*node)->dirty = 0;
				return 2;
			} else {
				*n = *node;
				return 1;
			}
		}
		node = &(*node)->next;
	}

	if (gfx_texture_cache.pool_pos == sizeof(gfx_texture_cache.pool) / sizeof(struct TextureHashmapNode)) {
// jnmartin84
		printf("FUCK\n");
		exit(-1);
		// Pool is full. We have a backup texture that is empty for this...
		gfx_rapi->select_texture(tile, oops_texture_id);
		*node = &oops_node;
		return 1;
	}

	*node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];
//	if (gfx_texture_cache.pool_pos > max_lookup) {
//		max_lookup = gfx_texture_cache.pool_pos;
//		printf("max pool pos %d\n", max_lookup);
//	}
//	if ((*node)->texture_addr == NULL) {
	if ((*node)->key == 0) {
		(*node)->texture_id = gfx_rapi->new_texture();
	}
	gfx_rapi->select_texture(tile, (*node)->texture_id);
	gfx_opengl_set_tile_addr(tile, segaddr);

	gfx_rapi->set_sampler_parameters(tile, 0, 0, 0);
#if 0
	(*node)->texture_addr = segaddr;
	(*node)->fmt = fmt;
	(*node)->siz = siz;
	(*node)->uls = uls;
	(*node)->ult = ult;
#endif
	(*node)->key = pack_key(segaddr,uls,ult,fmt,siz);

	(*node)->dirty = 0;

	(*node)->tmem = tmem;
	(*node)->cms = 0;
	(*node)->cmt = 0;
	(*node)->linear_filter = 0;
	(*node)->next = NULL;
	*n = *node;

	return 0;
}

// enough for a 64 * 64 16-bit image, no overflow should happen at this point
uint16_t __attribute__((aligned(32))) rgba16_buf[4096 * 8];
uint8_t __attribute__((aligned(32))) xform_buf[8192];

int last_cl_rv;

static void __attribute__((noinline)) import_texture(int tile);
#if 0
static void import_texture_rgba16_block(int tile) {
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
	uint16_t* start = (uint16_t *)rdp.loaded_texture[tile].addr;
	for (i = 0; i < loopcount; i++) {
		uint16_t col16 = __builtin_bswap16(start[i]);
		rgba16_buf[i] = ((col16 & 1) << 15) | (col16 >> 1);
	}
	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void import_texture_rgba16_tile(int tile) {
    uint32_t i;
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

	u32 src_width = last_set_texture_image_width + 1;
    uint32_t somewidth = src_width;
    if (width <= ((src_width >> 1) + 4)) {
        somewidth = width;
    } else {
        if (width == 20 && last_set_texture_image_width == 30) {
            somewidth = width - 4;
        }
    }

    uint16_t* start = (uint16_t*) &rdp.loaded_texture[tile]
                          .addr[(((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)) << 1) +
                                ((((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)) * (src_width)) << 1)];

    uint16_t* tex16 = rgba16_buf;
    for (i = 0; i < height; i++) {
        for (uint32_t x = 0; x < somewidth; x++) {
            uint16_t col16 = __builtin_bswap16(start[x]);
            *tex16++ = ((col16 & 1) << 15) | (col16 >> 1);
        }
        start += src_width;
    }

    width = somewidth;
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif
#if 1
static void import_texture_rgba16(int tile) {
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	
	if (last_set_texture_image_width == 0) {
		uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
		uint16_t* start = (uint16_t *)rdp.loaded_texture[tile].addr;
		for (i = 0; i < loopcount; i++) {
			uint16_t col16 = __builtin_bswap16(start[i]);
			rgba16_buf[i] = ((col16 & 1) << 15) | (col16 >> 1);
		}
	} else {
		u32 src_width = last_set_texture_image_width + 1;
		uint32_t somewidth = src_width;
		if (width <= ((src_width >> 1) + 4)) {
			somewidth = width;
		} else {
			if (width == 20 && last_set_texture_image_width == 30) { 
				somewidth = width - 4;
			}
		}

		uint16_t* start = (uint16_t*) &rdp.loaded_texture[tile]
					.addr[(((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)) << 1) +
						((((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)) * (src_width)) << 1)];

		uint16_t *tex16 = rgba16_buf;
		for (i = 0; i < height; i++) {
			for (uint32_t x = 0; x < somewidth; x++) {
				uint16_t col16 = __builtin_bswap16(start[x]);
				*tex16++ = ((col16 & 1) << 15) | (col16 >> 1);
			}
			start += src_width;
		}

		width = somewidth;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif

static void import_texture_rgba32(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = (rdp.loaded_texture[tile].size_bytes >> 1) / rdp.texture_tile.line_size_bytes;
	uint32_t wxh = width*height;

	// mk64 has one 32-bit texture and it is "Mario Kart 64" on the title screen
	// it only needs one bit of alpha, just make it argb1555
	for (uint32_t i = 0; i < wxh; i++) {
		uint8_t r = rdp.loaded_texture[tile].addr[(4 * i) + 0] >> 3;
		uint8_t g = rdp.loaded_texture[tile].addr[(4 * i) + 1] >> 3;
		uint8_t b = rdp.loaded_texture[tile].addr[(4 * i) + 2] >> 3;
		uint8_t a = !!rdp.loaded_texture[tile].addr[(4 * i) + 3];
		rgba16_buf[i] = (a << 15) | (r << 10) | (g << 5) | (b);
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#if 0
static void import_texture_ia4_block(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes << 1;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
    uint32_t i;
    for (i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
        uint8_t byte = rdp.loaded_texture[tile].addr[i / 2];
        uint8_t part = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint8_t intensity = (SCALE_3_8(part >> 1) >> 3) & 0x1f;
        uint8_t alpha = part & 1;
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        uint16_t col16 = (alpha << 15) | (r << 10) | (g << 5) | (b);
        rgba16_buf[i] = col16;
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void import_texture_ia4_tile(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes << 1;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
    memset(xform_buf, 0, 8192);
    uint8_t* start =
        (uint8_t*) &rdp.loaded_texture[tile]
            .addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) >> 1) * (last_set_texture_image_width + 1)) +
                  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];
    uint8_t* tex8 = xform_buf;
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t x = 0; x < (last_set_texture_image_width + 1) * 2; x += 2) {
            uint32_t sidx = x >> 1;
            if (i & 1) {
                tex8[(x)] = (start[(sidx)] & 0xf);
                tex8[(x) + 1] = (start[(sidx)] >> 4) & 0xf;

            } else {
                tex8[(x)] = (start[(sidx)] >> 4) & 0xf;
                tex8[(x) + 1] = (start[(sidx)] & 0xf);
            }
        }
        start += (last_set_texture_image_width + 1);
        tex8 += (last_set_texture_image_width + 1);
    }
    for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
        uint8_t byte = xform_buf[i];
        uint8_t intensity = (SCALE_3_8(byte >> 1) >> 3) & 0x1f;
        uint8_t alpha = byte & 1;
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        uint16_t col16 = (alpha << 15) | (r << 10) | (g << 5) | (b);
        rgba16_buf[i] = col16;
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif
#if 1
static void import_texture_ia4(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes << 1;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t i;

	if (last_set_texture_image_width == 0) {
		for (i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
			uint8_t byte = rdp.loaded_texture[tile].addr[i / 2];
			uint8_t part = (byte >> (4 - (i % 2) * 4)) & 0xf;
			uint8_t intensity = (SCALE_3_8(part >> 1) >> 3) & 0x1f;
			uint8_t alpha = part & 1;
			uint8_t r = intensity;
			uint8_t g = intensity;
			uint8_t b = intensity;
			uint16_t col16 = (alpha << 15) | (r << 10) | (g << 5) | (b);
			rgba16_buf[i] = col16;
		}
	} else {
		memset(xform_buf, 0, 8192);
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) >> 1) * (last_set_texture_image_width + 1)) +
					  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];
		uint8_t *tex8 = xform_buf;
		for (uint32_t i = 0; i < height; i++) {
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1) * 2; x += 2) {
				uint32_t sidx = x>>1;
				if (i & 1) {
					tex8[(x)] = (start[(sidx)] & 0xf);
					tex8[(x) + 1] = (start[(sidx)] >> 4) & 0xf;

				} else {
					tex8[(x)] = (start[(sidx)] >> 4) & 0xf;
					tex8[(x) + 1] = (start[(sidx)] & 0xf);
				}
			}
			start += (last_set_texture_image_width + 1);
			tex8 += (last_set_texture_image_width + 1);
		}
		for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
			uint8_t byte = xform_buf[i];
			uint8_t intensity = (SCALE_3_8(byte >> 1) >> 3) & 0x1f;
			uint8_t alpha = byte & 1;
			uint8_t r = intensity;
			uint8_t g = intensity;
			uint8_t b = intensity;
			uint16_t col16 = (alpha << 15) | (r << 10) | (g << 5) | (b);
			rgba16_buf[i] = col16;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif

static void import_texture_ia8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	memset(rgba16_buf, 0, width*height*2);//8192*2);

	uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
						 .addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * (last_set_texture_image_width + 1)) +
							   (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)];

	u32 src_width;
	if (last_set_texture_image_width) {
		src_width = last_set_texture_image_width + 1;
	} else {
		src_width = width;
	}

	uint16_t *tex16 = rgba16_buf;
	for (uint32_t i = 0; i < height; i++) {
		for (uint32_t x = 0; x < src_width; x++) {
			uint8_t val = start[x];
			uint8_t in = ((val >> 4) & 0xf);
			uint8_t al = (val & 0xf);
			tex16[x] = (al << 12) | (in << 8) | (in << 4) | in;
		}
		start += src_width;
		tex16 += src_width;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}

static void import_texture_ia16(int tile) {
#if 0	
	uint32_t i;

	for (i = 0; i < rdp.loaded_texture[tile].size_bytes / 2; i++) {
		uint8_t in = (rdp.loaded_texture[tile].addr[2 * i] >> 4) & 0xf;
		uint8_t al = (rdp.loaded_texture[tile].addr[2 * i + 1] >> 4) & 0xf;
		rgba16_buf[i] = (al << 12) | (in << 8) | (in << 4) | in;
	}

	uint32_t width = rdp.texture_tile.line_size_bytes / 2;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
#endif
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	u32 src_width;
	if (last_set_texture_image_width) {
		src_width = last_set_texture_image_width + 1;
	} else {
		src_width = width;
	}

//	memset(rgba16_buf,0,src_width*height);

	uint16_t* start = rdp.loaded_texture[tile].addr;
	if (last_set_texture_image_width) {
		start =
			(uint16_t*) &rdp.loaded_texture[tile]
				.addr[((((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC))) << 1) +
					  (((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)) * ((src_width) << 1))];
	}

	uint16_t *tex16 = rgba16_buf;
	for (i = 0; i < height; i++) {
		for (uint32_t x = 0; x < src_width; x++) {
			uint16_t np = start[x];//++;
			uint8_t al = (np >> 12)&0xf;
			uint8_t in = (np >>  4)&0xf;
			tex16[x] = (al << 12) | (in << 8) | (in << 4) | in;
		}
		start += src_width;
		tex16 += src_width;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, src_width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}
#if 0
static void import_texture_i4_block(int tile) {
    uint32_t i;

    uint32_t width = rdp.texture_tile.line_size_bytes * 2;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

    height = (height + 3) & ~3;
    for (i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
        uint16_t idx = (i << 1);
        uint8_t byte = rdp.loaded_texture[tile].addr[i];
        uint8_t part1, part2;
        part1 = (byte >> 4) & 0xf;
        part2 = byte & 0xf;
        rgba16_buf[idx] = (part1 << 12) | (part1 << 8) | (part1 << 4) | part1;
        rgba16_buf[idx + 1] = (part2 << 12) | (part2 << 8) | (part2 << 4) | part2;
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}

static void import_texture_i4_tile(int tile) {
    uint32_t i;

    uint32_t width = rdp.texture_tile.line_size_bytes * 2;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

    height = (height + 3) & ~3;
    memset(rgba16_buf, 0, 8192);
    uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
                         .addr[(((((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) - 1) / 2) * (width) / 2)) +
                               (((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) - 1) / 2)];
    for (i = 0; i < height; i++) {
        uint32_t iw = i * width;
        for (uint32_t x = 0; x < (last_set_texture_image_width + 1) * 2; x += 2) {
            uint8_t startin = start[(x >> 1)];
            uint8_t in = (startin >> 4) & 0xf;
            rgba16_buf[iw + x] = (in << 12) | (in << 8) | (in << 4) | in;
            in = startin & 0xf;
            rgba16_buf[iw + x + 1] = (in << 12) | (in << 8) | (in << 4) | in;
        }
        start += (last_set_texture_image_width + 1);
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}
#endif
#if 1
static void import_texture_i4(int tile) {
	uint32_t i;

	uint32_t width = rdp.texture_tile.line_size_bytes * 2;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

	height = (height + 3) & ~3;

	if (last_set_texture_image_width == 0) {
		for (i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
			uint16_t idx = (i<<1);
			uint8_t byte = rdp.loaded_texture[tile].addr[i];
			uint8_t part1,part2;
			part1 = (byte >> 4) & 0xf;
			part2 = byte & 0xf;
			rgba16_buf[idx  ] = (part1 << 12) | (part1 << 8) | (part1 << 4) | part1;
			rgba16_buf[idx+1] = (part2 << 12) | (part2 << 8) | (part2 << 4) | part2;
		}
	} else {
		memset(rgba16_buf, 0, 8192);
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)-1)/2) * (width)/2)) +
					  (((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)-1)/2)];
		for (uint32_t i = 0; i < height; i++) {
			uint32_t iw = i * width;
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1)*2; x += 2) {
				uint8_t startin = start[(x >> 1)];
				uint8_t in = (startin >> 4) & 0xf;
				rgba16_buf[iw + x] = (in << 12) | (in << 8) | (in << 4) | in;
				in = startin & 0xf;
				rgba16_buf[iw + x + 1] = (in << 12) | (in << 8) | (in << 4) | in;
			}
			start += (last_set_texture_image_width + 1);
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}
#endif

static void import_texture_i8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

    memset(xform_buf, 0, width*height*2);

	uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
						 .addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * (last_set_texture_image_width + 1)) +
							   (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)];

	u32 src_width;
	if (last_set_texture_image_width) {
		src_width = last_set_texture_image_width + 1;
	} else {
		src_width = width;
	}
	for (uint32_t i = 0; i < height; i++) {
		for (uint32_t x = 0; x < src_width; x++) {
			xform_buf[(i * width) + x] = start[x];
		}
		start += (src_width);
	}

	for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
		uint8_t in = (xform_buf[i] >> 4) & 0xf;
		rgba16_buf[i] = (in << 12) | (in << 8) | (in << 4) | in;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}
#if 0
static void import_texture_ci4_block(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes * 2;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
    uint32_t i;
    for (i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
        uint8_t byte = rdp.loaded_texture[tile].addr[i / 2];
        uint8_t part;
        if (!(i & 1)) {
            part = (byte >> 4) & 0xf;
        } else {
            part = byte & 0xf;
        }
        rgba16_buf[i] = tlut[part];
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void import_texture_ci4_tile(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes * 2;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
    memset(xform_buf, 0, width * height * 2);
    uint8_t* start =
        (uint8_t*) &rdp.loaded_texture[tile]
            .addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) / 2) * (last_set_texture_image_width + 1)) +
                  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) / 2)];
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t x = 0; x < (last_set_texture_image_width + 1) * 2; x += 2) {
            if (i & 1) {
                xform_buf[(i * (width / 2)) + (x)] = (start[(x / 2)] & 0xf);
                xform_buf[((i * (width / 2)) + (x)) + 1] = (start[(x / 2)] >> 4) & 0xf;

            } else {
                xform_buf[(i * (width / 2)) + (x)] = (start[(x / 2)] >> 4) & 0xf;
                xform_buf[((i * (width / 2)) + (x)) + 1] = (start[(x / 2)] & 0xf);
            }
        }
        start += (last_set_texture_image_width + 1);
    }
    for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i += 4) {
        rgba16_buf[i] = tlut[xform_buf[i + 1]];
        rgba16_buf[i + 1] = tlut[xform_buf[i + 2]];
        rgba16_buf[i + 2] = tlut[xform_buf[i + 3]];
        rgba16_buf[i + 3] = tlut[xform_buf[i + 4]];
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif
#if 1
static void import_texture_ci4(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes * 2;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t i;

	if (last_set_texture_image_width == 0) {
		for (i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i++) {
			uint8_t byte = rdp.loaded_texture[tile].addr[i / 2];
			uint8_t part;
			if (!(i & 1)) {
				part = (byte >> 4) & 0xf;
			} else {
				part = byte & 0xf;
			}
			rgba16_buf[i] = tlut[part];
		}
	} else {
//		uint8_t xform_buf[8192];
		memset(xform_buf, 0, width * height * 2);
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) / 2) * (last_set_texture_image_width + 1)) +
					  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) / 2)];
		for (uint32_t i = 0; i < height; i++) {
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1) * 2; x += 2) {
				if (i & 1) {
					xform_buf[(i * (width / 2)) + (x)] = (start[(x / 2)] & 0xf);
					xform_buf[((i * (width / 2)) + (x)) + 1] = (start[(x / 2)] >> 4) & 0xf;

				} else {
					xform_buf[(i * (width / 2)) + (x)] = (start[(x / 2)] >> 4) & 0xf;
					xform_buf[((i * (width / 2)) + (x)) + 1] = (start[(x / 2)] & 0xf);
				}
			}
			start += (last_set_texture_image_width + 1);
		}
		for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i+=4) {
			rgba16_buf[i] = tlut[xform_buf[i+1]];
			rgba16_buf[i+1] = tlut[xform_buf[i+2]];
			rgba16_buf[i+2] = tlut[xform_buf[i+3]];
			rgba16_buf[i+3] = tlut[xform_buf[i+4]];
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif
#if 0
static __attribute__((noinline)) void import_texture_ci8_block(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

    __builtin_prefetch(tlut);
    uint8_t* tex8 = rdp.loaded_texture[tile].addr;
    if (!((uintptr_t) tex8 & 3)) {
        uint32_t* intex32 = (uint32_t*) tex8;
        __builtin_prefetch(intex32);
        uint32_t count = rdp.loaded_texture[tile].size_bytes;
        uint32_t* tex32 = (uint32_t) rgba16_buf;
        for (uint32_t i = 0; i < count; i += 8) {
            __builtin_prefetch((void*) (((uintptr_t) intex32 & 0xffffffe0) + 32));

            uint32_t fourpix1 = *intex32++;
            uint32_t fourpix2 = *intex32++;

            asm volatile("" : : : "memory");

            uint16_t t1, t2, t3, t4, t5, t6, t7, t8;

            t4 = tlut[(fourpix1 >> 24) & 0xff];
            t3 = tlut[(fourpix1 >> 16) & 0xff];
            t2 = tlut[(fourpix1 >> 8) & 0xff];
            t1 = tlut[(fourpix1) & 0xff];
            t8 = tlut[(fourpix2 >> 24) & 0xff];
            t7 = tlut[(fourpix2 >> 16) & 0xff];
            t6 = tlut[(fourpix2 >> 8) & 0xff];
            t5 = tlut[(fourpix2) & 0xff];

            asm volatile("" : : : "memory");

            *tex32++ = (t2 << 16) | t1;
            *tex32++ = (t4 << 16) | t3;
            *tex32++ = (t6 << 16) | t5;
            *tex32++ = (t8 << 16) | t7;
        }
    } else {
        __builtin_prefetch(tex8);
        uint32_t count = rdp.loaded_texture[tile].size_bytes;
        uint32_t* tex32 = (uint32_t) rgba16_buf;
        for (uint32_t i = 0; i < count; i += 8) {
            uint16_t t1, t2, t3, t4, t5, t6, t7, t8;
            __builtin_prefetch(tex8 + 16);
            t1 = tlut[*tex8++];
            t2 = tlut[*tex8++];
            t3 = tlut[*tex8++];
            t4 = tlut[*tex8++];
            t5 = tlut[*tex8++];
            t6 = tlut[*tex8++];
            t7 = tlut[*tex8++];
            t8 = tlut[*tex8++];
#if 1
            asm volatile("" : : : "memory");
#endif

            *tex32++ = (t2 << 16) | t1;
            *tex32++ = (t4 << 16) | t3;
            *tex32++ = (t6 << 16) | t5;
            *tex32++ = (t8 << 16) | t7;
        }
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static __attribute__((noinline)) void import_texture_ci8_tile(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes;
    uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

    __builtin_prefetch(tlut);
    u32 src_width = last_set_texture_image_width + 1;

    uint8_t* start =
        (uint8_t*) (&rdp.loaded_texture[tile]
                         .addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * last_set_texture_image_width) +
                               (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)]);
    //		uint32_t *start32;
    uint16_t* tex16 = rgba16_buf;
    for (uint32_t h = 0; h < height; h++) {
        // uint16_t *tx16 = tex16;
        uint16_t* tx32 = tex16;
        for (uint32_t w = 0; w < src_width; w += 4) {
            uint16_t t1, t2, t3, t4;
            t1 = tlut[*start++];
            t2 = tlut[*start++];
            t3 = tlut[*start++];
            t4 = tlut[*start++];

            *tx32++ = (t2 << 16) | t1;
            *tx32++ = (t4 << 16) | t3;
            //				*tx16++ = t2;
            //				*tx16++ = t3;
            //				*tx16++ = t4;
        }
        tex16 += width;
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif
#if 1
static __attribute__((noinline)) void import_texture_ci8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

	__builtin_prefetch(tlut);
	if (__builtin_expect((last_set_texture_image_width == 0),1)) {
		uint8_t *tex8 = rdp.loaded_texture[tile].addr;
		if (__builtin_expect((!((uintptr_t)tex8 & 3)),1)) {
			uint32_t *intex32 = (uint32_t *)tex8;
			__builtin_prefetch(intex32);
			uint32_t count = rdp.loaded_texture[tile].size_bytes;
			uint32_t *tex32 = (uint32_t)rgba16_buf;
			for (uint32_t i = 0; i < count; i+=8) {
				__builtin_prefetch((void*)(((uintptr_t)intex32&0xffffffe0)+32));

				uint32_t fourpix1 = *intex32++;
				uint32_t fourpix2 = *intex32++;

				asm volatile("": : : "memory");

				uint16_t t1,t2,t3,t4,t5,t6,t7,t8;

				t4 = tlut[(fourpix1 >> 24) & 0xff];
				t3 = tlut[(fourpix1 >> 16) & 0xff];
				t2 = tlut[(fourpix1 >>  8) & 0xff];
				t1 = tlut[(fourpix1      ) & 0xff];
				t8 = tlut[(fourpix2 >> 24) & 0xff];
				t7 = tlut[(fourpix2 >> 16) & 0xff];
				t6 = tlut[(fourpix2 >>  8) & 0xff];
				t5 = tlut[(fourpix2      ) & 0xff];

				asm volatile("": : : "memory");

				*tex32++ = (t2 << 16) | t1;
				*tex32++ = (t4 << 16) | t3;
				*tex32++ = (t6 << 16) | t5;
				*tex32++ = (t8 << 16) | t7;

			}
		} else {
			__builtin_prefetch(tex8);
			uint32_t count = rdp.loaded_texture[tile].size_bytes;
			uint32_t *tex32 = (uint32_t)rgba16_buf;
			for (uint32_t i = 0; i < count; i+=8) {
				uint16_t t1,t2,t3,t4,t5,t6,t7,t8;
				__builtin_prefetch(tex8+16);
				t1 = tlut[*tex8++];
				t2 = tlut[*tex8++];
				t3 = tlut[*tex8++];
				t4 = tlut[*tex8++];
				t5 = tlut[*tex8++];
				t6 = tlut[*tex8++];
				t7 = tlut[*tex8++];
				t8 = tlut[*tex8++];
	#if 1
				asm volatile("": : : "memory");
	#endif

				*tex32++ = (t2 << 16) | t1;
				*tex32++ = (t4 << 16) | t3;
				*tex32++ = (t6 << 16) | t5;
				*tex32++ = (t8 << 16) | t7;
			}
		}
	} else {
		u32 src_width;
		if (last_set_texture_image_width) {
			src_width = last_set_texture_image_width + 1;
		} else {
			src_width = width;
		}

		uint8_t* start = (uint8_t *)(&rdp.loaded_texture[tile]
							.addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * last_set_texture_image_width) +
									(rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)]);
//		uint32_t *start32;
		uint16_t *tex16 = rgba16_buf;
		for (uint32_t h = 0; h < height; h ++) {
			//uint16_t *tx16 = tex16;
			uint16_t *tx32 = tex16;
			for (uint32_t w = 0; w < src_width; w += 4) {
				uint16_t t1,t2,t3,t4;
				t1 = tlut[*start++];
				t2 = tlut[*start++];
				t3 = tlut[*start++];
				t4 = tlut[*start++];

				*tx32++ = (t2 << 16) | t1;
				*tx32++ = (t4 << 16) | t3;
				//				*tx16++ = t2;
//				*tx16++ = t3;
//				*tx16++ = t4;
			}
			tex16 += width;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}
#endif

static void __attribute__((noinline)) import_texture(int tile) {
	uint8_t fmt = rdp.texture_tile.fmt;
	uint8_t siz = rdp.texture_tile.siz;

//	if ((uintptr_t) rdp.loaded_texture[tile].addr < (uintptr_t) 0x8c010000) {
//		printf("invalid texture addr for import\n");
//		return;
//	}

	uint32_t tmem = rdp.texture_to_load.tmem;

	int cl_rv = gfx_texture_cache_lookup(tile, &rendering_state.textures[tile], rdp.loaded_texture[tile].addr, tmem,
										 fmt, siz, rdp.texture_tile.uls, rdp.texture_tile.ult);
	if (cl_rv == 1) {
		return;
	}

	last_cl_rv = cl_rv;
//	if (last_set_texture_image_width == 0) {
	if (fmt == G_IM_FMT_RGBA) {
		if (siz == G_IM_SIZ_16b) {
			import_texture_rgba16(tile);
		} else if (siz == G_IM_SIZ_32b) {
			import_texture_rgba32(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_IA) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ia4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ia8(tile);
		} else if (siz == G_IM_SIZ_16b) {
			import_texture_ia16(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_CI) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ci4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ci8(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_I) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_i4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_i8(tile);
		} else {
//			abort();
		}
	} else {
//		abort();
	}
/* } else {
	if (fmt == G_IM_FMT_RGBA) {
		if (siz == G_IM_SIZ_16b) {
			import_texture_rgba16_tile(tile);
		} else if (siz == G_IM_SIZ_32b) {
			import_texture_rgba32(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_IA) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ia4_tile(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ia8(tile);
		} else if (siz == G_IM_SIZ_16b) {
			import_texture_ia16(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_CI) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ci4_tile(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ci8_tile(tile);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_I) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_i4_tile(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_i8(tile);
		} else {
//			abort();
		}
	} else {
//		abort();
	}
} */
}
#include <kos.h>

static void gfx_normalize_vector(float v[3]) {
#if 0
	float s = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= s;
	v[1] /= s;
	v[2] /= s;
#endif
	vec3f_normalize(v[0], v[1], v[2]);
}

static void gfx_transposed_matrix_mul(float res[3], const float a[3], const float b[4][4]) {
	res[0] = fipr(a[0],a[1],a[2],0,b[0][0],b[0][1],b[0][2],0);
	res[1] = fipr(a[0],a[1],a[2],0,b[1][0],b[1][1],b[1][2],0);
	res[2] = fipr(a[0],a[1],a[2],0,b[2][0],b[2][1],b[2][2],0);
#if 0
	res[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2];
	res[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2];
	res[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2];
#endif
}

#define recip255 0.00392157f
#define recip127 0.00787402f
#define recip2pi 0.159155f
static void calculate_normal_dir(const Light_t* light, float coeffs[3]) {
	float light_dir[3] = { light->dir[0] * recip127, light->dir[1] * recip127, light->dir[2] * recip127 };
	gfx_transposed_matrix_mul(coeffs, light_dir,
							  (const float (*)[4]) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
	gfx_normalize_vector(coeffs);
}

// thanks @FalcoGirgis
inline static void fast_mat_store(matrix_t* mtx) {
	asm volatile(
		R"(
			fschg
			add            #64-8,%[mtx]
			fmov.d    xd14,@%[mtx]
			add            #-32,%[mtx]
			pref    @%[mtx]
			add         #32,%[mtx]
			fmov.d    xd12,@-%[mtx]
			fmov.d    xd10,@-%[mtx]
			fmov.d    xd8,@-%[mtx]
			fmov.d    xd6,@-%[mtx]
			fmov.d    xd4,@-%[mtx]
			fmov.d    xd2,@-%[mtx]
			fmov.d    xd0,@-%[mtx]
			fschg
		)"
		: [mtx] "+&r"(mtx), "=m"(*mtx)
		:
		:);
}

// thanks @FalcoGirgis
inline static void fast_mat_load(const matrix_t* mtx) {
	asm volatile(
		R"(
			fschg
			fmov.d    @%[mtx],xd0
			add        #32,%[mtx]
			pref    @%[mtx]
			add        #-(32-8),%[mtx]
			fmov.d    @%[mtx]+,xd2
			fmov.d    @%[mtx]+,xd4
			fmov.d    @%[mtx]+,xd6
			fmov.d    @%[mtx]+,xd8
			fmov.d    @%[mtx]+,xd10
			fmov.d    @%[mtx]+,xd12
			fmov.d    @%[mtx]+,xd14
			fschg
		)"
		: [mtx] "+r"(mtx)
		:
		:);
}

// thanks @FalcoGirgis
inline static void mat_load_apply(const matrix_t* matrix1, const matrix_t* matrix2) {
	unsigned int prefetch_scratch;

	asm volatile("mov %[bmtrx], %[pref_scratch]\n\t"
				 "add #32, %[pref_scratch]\n\t"
				 "fschg\n\t"
				 "pref @%[pref_scratch]\n\t"
				 // back matrix
				 "fmov.d @%[bmtrx]+, XD0\n\t"
				 "fmov.d @%[bmtrx]+, XD2\n\t"
				 "fmov.d @%[bmtrx]+, XD4\n\t"
				 "fmov.d @%[bmtrx]+, XD6\n\t"
				 "pref @%[fmtrx]\n\t"
				 "fmov.d @%[bmtrx]+, XD8\n\t"
				 "fmov.d @%[bmtrx]+, XD10\n\t"
				 "fmov.d @%[bmtrx]+, XD12\n\t"
				 "mov %[fmtrx], %[pref_scratch]\n\t"
				 "add #32, %[pref_scratch]\n\t"
				 "fmov.d @%[bmtrx], XD14\n\t"
				 "pref @%[pref_scratch]\n\t"
				 // front matrix
				 // interleave loads and matrix multiply 4x4
				 "fmov.d @%[fmtrx]+, DR0\n\t"
				 "fmov.d @%[fmtrx]+, DR2\n\t"
				 "fmov.d @%[fmtrx]+, DR4\n\t"
				 "ftrv XMTRX, FV0\n\t"

				 "fmov.d @%[fmtrx]+, DR6\n\t"
				 "fmov.d @%[fmtrx]+, DR8\n\t"
				 "ftrv XMTRX, FV4\n\t"

				 "fmov.d @%[fmtrx]+, DR10\n\t"
				 "fmov.d @%[fmtrx]+, DR12\n\t"
				 "ftrv XMTRX, FV8\n\t"

				 "fmov.d @%[fmtrx], DR14\n\t"
				 "fschg\n\t"
				 "ftrv XMTRX, FV12\n\t"
				 "frchg\n"
				 : [bmtrx] "+&r"((unsigned int) matrix1), [fmtrx] "+r"((unsigned int) matrix2),
				   [pref_scratch] "=&r"(prefetch_scratch)
				 : // no inputs
				 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
				   "fr14", "fr15");
}

// typedef float[4][4] __attribute__((aligned(32))) matrix_t;
// float res[4][4], const float a[4][4], const float b[4][4]) {
static void gfx_matrix_mul(matrix_t res, const matrix_t a, const matrix_t b) {
	mat_load_apply((matrix_t *)b, (matrix_t *)a);
	fast_mat_store((matrix_t *)res);
	#if 0
	float tmp[4][4];
	int i,j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			tmp[i][j] = a[i][0] * b[0][j] +
						a[i][1] * b[1][j] +
						a[i][2] * b[2][j] +
						a[i][3] * b[3][j];
		}
	}
	memcpy(res, tmp, sizeof(tmp));
#endif
}

static int matrix_dirty = 0;

extern void *memcpy32(void *restrict dst, const void *restrict src, size_t bytes);
static __attribute__((noinline)) void gfx_sp_matrix(uint8_t parameters, const int32_t* addr) {
	int32_t* saddr = (int32_t*) segmented_to_virtual((void*) addr);
	float matrix[4][4] __attribute__((aligned(32)));
#ifndef GBI_FLOATS
	// Original GBI where fixed point matrices are used
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j += 2) {
			int32_t int_part = saddr[i * 2 + j / 2];
			uint32_t frac_part = saddr[8 + i * 2 + j / 2];
			matrix[i][j] = (int32_t) ((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
			matrix[i][j + 1] = (int32_t) ((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
		}
	}
#else
	// For a modified GBI where fixed point values are replaced with floats
	n64_memcpy(matrix, saddr, sizeof(matrix));
#endif

	matrix_dirty = 1;

	if (parameters & G_MTX_PROJECTION) {
		if (parameters & G_MTX_LOAD) {
			memcpy32(rsp.P_matrix, matrix, sizeof(matrix));
		} else {
			gfx_matrix_mul(rsp.P_matrix, (const float (*)[4]) matrix, (const float (*)[4]) rsp.P_matrix);
		}
	} else { // G_MTX_MODELVIEW
		if ((parameters & G_MTX_PUSH) && rsp.modelview_matrix_stack_size < 11) {
			++rsp.modelview_matrix_stack_size;
			memcpy32(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
				   rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 2], sizeof(matrix));
		}
		if (parameters & G_MTX_LOAD) {
			memcpy32(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix, sizeof(matrix));
		} else {
			gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
							rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
		}
		rsp.lights_changed = 1;
	}
	gfx_matrix_mul(rsp.MP_matrix, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
					rsp.P_matrix);
}

static void  __attribute__((noinline)) gfx_sp_pop_matrix(uint32_t count) {
	while (count--) {
		if (rsp.modelview_matrix_stack_size > 0) {
			--rsp.modelview_matrix_stack_size;
		}
	}
	if (rsp.modelview_matrix_stack_size > 0) {
		gfx_matrix_mul(rsp.MP_matrix,
					   /* (const float (*)[4]) */ rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
					   /* (const float (*)[4]) */ rsp.P_matrix);
		matrix_dirty = 1;
//		glMatrixMode(GL_MODELVIEW);
//		glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
	}
}
#include "sh4zam.h"
//static float gfx_adjust_x_for_aspect_ratio(float x) {
//	return x * (4.0f / 3.0f) / ((float) gfx_current_dimensions.width / (float) gfx_current_dimensions.height);
//}
//int max_lights = 0;

/*

typedef struct __attribute__((aligned(32))) u_pvr_vertex_s {
	union WFlag {
		float w;
		uint32_t flags;
	};
	float x;
	float y;
	float z;
	float u;
	float v;
	uint32_t argb;
	uint32_t oargb;
} u_pvr_vertex_t;

pvr_vertex_t loaded_vertices[64 + 4];
float loaded_w[64 + 4];
float x,y,z,w;
size_t i;
fast_mat_load(&rsp.MP_matrix);


for (i = 0; i < n_vertices; i++, dest_index++) {

		const Vtx_t* v = &vertices[i].v;
		const Vtx_tn* vn = &vertices[i].n;

		struct pvr_vertex_t* d = &loaded_vertices[dest_index];

		x = v->ob[0];
		y = v->ob[1];
		z = v->ob[2];

		mat_trans_single3_nodivw(x,y,z,w);

		d->x = x;
		d->y = y;
		d->z = z;
		d->WFlag.w = w;

		// trivial clip rejection
        d->clip_rej = 0;
        d->wlt0 = 0;
        if (x < -w)
            d->clip_rej |= 1;
        if (x > w)
            d->clip_rej |= 2;
	    if (y < -w)
            d->clip_rej |= 4;
        if (y > w)
            d->clip_rej |= 8;
        if (z < -w)
            d->clip_rej |= 16;
        if (z > w)
            d->clip_rej |= 32;
        if (w < 0)
            d->wlt0 = 1;

		d->argb = (v->cn[3] << 24);

        if (rsp.geometry_mode & G_LIGHTING) {
        	if (rsp.lights_changed) {
                if (rsp.current_num_lights > max_lights) {
                    max_lights = rsp.current_num_lights;
                    printf("max lights %d\n", max_lights);
                }
				if (rsp.current_num_lights == 2) {
                    calculate_normal_dir(&rsp.current_lights[0], rsp.current_lights_coeffs[0]);
                }
                static const Light_t lookat_x = { { 0, 0, 0 }, 0, { 0, 0, 0 }, 0, { 127, 0, 0 }, 0 };
                static const Light_t lookat_y = { { 0, 0, 0 }, 0, { 0, 0, 0 }, 0, { 0, 127, 0 }, 0 };
                calculate_normal_dir(&lookat_x, rsp.current_lookat_coeffs[0]);
                calculate_normal_dir(&lookat_y, rsp.current_lookat_coeffs[1]);
                rsp.lights_changed = 0;
            }

            int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];
            int g = rsp.current_lights[rsp.current_num_lights - 1].col[1];
            int b = rsp.current_lights[rsp.current_num_lights - 1].col[2];

            if (rsp.current_num_lights == 2) {
                float intensity;
                intensity = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lights_coeffs[0][0],
                                        rsp.current_lights_coeffs[0][1], rsp.current_lights_coeffs[0][2],0);

                if (intensity > 0.0f) {
                    r += intensity * rsp.current_lights[0].col[0];
                    g += intensity * rsp.current_lights[0].col[1];
                    b += intensity * rsp.current_lights[0].col[2];
                }
            }

			d->argb |= (r << 16) | (g << 8) | (b);

            if (rsp.geometry_mode & G_TEXTURE_GEN) {
                float dotx; // = 0,
                float doty; // = 0;
                dotx = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, 
					rsp.current_lookat_coeffs[0][0],
                    rsp.current_lookat_coeffs[0][1],
					rsp.current_lookat_coeffs[0][2], 0);

                doty = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0,
					rsp.current_lookat_coeffs[1][0],
                    rsp.current_lookat_coeffs[1][1],
					rsp.current_lookat_coeffs[1][2], 0);

                if (dotx < -1.0f)
                    dotx = -1.0f;
                else if (dotx > 1.0f)
                    dotx = 1.0f;

                if (doty < -1.0f)
                    doty = -1.0f;
                else if (doty > 1.0f)
                    doty = 1.0f;

                if (rsp.geometry_mode & G_TEXTURE_GEN_LINEAR) {
					dotx = acosf(-dotx) * recip2pi;
                    doty = acosf(-doty) * recip2pi;
                } else {
                    dotx = (dotx * 0.25f) + 0.25f; ////1.0f) / 4.0f;
                    doty = (doty * 0.25f) + 0.25f; // 1.0f) / 4.0f;
                }

                U = (int32_t) (dotx * rsp.texture_scaling_factor.s);
            	V = (int32_t) (doty * rsp.texture_scaling_factor.t);
            }
        } else {
			d->argb |= (v->cn[0] << 16) | (v->cn[1] << 8) | (v->cn[2]);
        }

        d->u = U;
        d->v = V;
}


 */

int nearz_clip_verts = 0;
int total_verts = 0;

static void __attribute__((noinline)) gfx_sp_vertex_light(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_t* v = &vertices[i].v;
        const Vtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];

        float x, y, z, w;
        x = v->ob[0];
        y = v->ob[1];
        z = v->ob[2];
        mat_trans_single3_nodivw(x, y, z, w);

        short U = v->tc[0] * rsp.texture_scaling_factor.s >> 16;
        short V = v->tc[1] * rsp.texture_scaling_factor.t >> 16;

        int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];
        int g = rsp.current_lights[rsp.current_num_lights - 1].col[1];
        int b = rsp.current_lights[rsp.current_num_lights - 1].col[2];

        if (rsp.current_num_lights == 2) {
            float intensity;
            intensity = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lights_coeffs[0][0],
                                             rsp.current_lights_coeffs[0][1], rsp.current_lights_coeffs[0][2], 0);

            if (intensity > 0.0f) {
                r += intensity * rsp.current_lights[0].col[0];
                g += intensity * rsp.current_lights[0].col[1];
                b += intensity * rsp.current_lights[0].col[2];
            }
        }

        d->color.r = r > 255 ? 255 : r;
        d->color.g = g > 255 ? 255 : g;
        d->color.b = b > 255 ? 255 : b;
        d->color.a = v->cn[3];

        if (rsp.geometry_mode & G_TEXTURE_GEN) {
            float dotx; // = 0,
            float doty; // = 0;
            dotx = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lookat_coeffs[0][0],
                                        rsp.current_lookat_coeffs[0][1], rsp.current_lookat_coeffs[0][2], 0);

            doty = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lookat_coeffs[1][0],
                                        rsp.current_lookat_coeffs[1][1], rsp.current_lookat_coeffs[1][2], 0);

            if (dotx < -1.0f)
                dotx = -1.0f;
            else if (dotx > 1.0f)
                dotx = 1.0f;

            if (doty < -1.0f)
                doty = -1.0f;
            else if (doty > 1.0f)
                doty = 1.0f;

            if (rsp.geometry_mode & G_TEXTURE_GEN_LINEAR) {
                dotx = acosf(-dotx) * recip2pi;
                doty = acosf(-doty) * recip2pi;
            } else {
                dotx = (dotx * 0.25f) + 0.25f; ////1.0f) / 4.0f;
                doty = (doty * 0.25f) + 0.25f; // 1.0f) / 4.0f;
            }

            U = (int32_t) (dotx * rsp.texture_scaling_factor.s);
            V = (int32_t) (doty * rsp.texture_scaling_factor.t);
        }

        d->u = U;
        d->v = V;

        // trivial clip rejection
        d->wlt0 = (w < 0);
        uint8_t cr = 0;
        cr = !(z > w);
        cr = (cr << 1) | !(z < -w);
        cr = (cr << 1) | !(y > w);
        cr = (cr << 1) | !(y < -w);
        cr = (cr << 1) | !(x > w);
        cr = (cr << 1) | !(x < -w);
        d->clip_rej = cr;
//        if (z < -w)
//            nearz_clip_verts++;

		d->x = v->ob[0];
        d->y = v->ob[1];
        d->z = v->ob[2];
        d->w = 1.0f;

        d->_x = x;
        d->_y = y;
        d->_z = z;
        d->_w = w;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_no(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_t* v = &vertices[i].v;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        float x, y, z, w;

		x = d->x = v->ob[0];
        y = d->y = v->ob[1];
        z = d->z = v->ob[2];
        d->w = 1.0f;

		mat_trans_single3_nodivw(x, y, z, w);

		d->_x = x;
        d->_y = y;
        d->_z = z;
        d->_w = w;

        d->color.r = v->cn[0];
        d->color.g = v->cn[1];
        d->color.b = v->cn[2];
	    d->color.a = v->cn[3];

        d->u = (float)(v->tc[0] * rsp.texture_scaling_factor.s >> 16);
        d->v = (float)(v->tc[1] * rsp.texture_scaling_factor.t >> 16);

        // trivial clip rejection
        d->wlt0 = (w < 0);
		uint8_t cr = 0;
        cr = !(z > w);
		cr = (cr << 1) | !(z < -w);
		cr = (cr << 1) | !(y >  w);
		cr = (cr << 1) | !(y < -w);
		cr = (cr << 1) | !(x >  w);
		cr = (cr << 1) | !(x < -w);
		d->clip_rej = cr;
//		if(z < -w) nearz_clip_verts++;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    if (matrix_dirty) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((const float*) rsp.P_matrix);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
        matrix_dirty = 0;
    }
    fast_mat_load(&rsp.MP_matrix);
    total_verts += n_vertices;
    if (rsp.geometry_mode & G_LIGHTING) {
        if (rsp.lights_changed) {
            if (rsp.current_num_lights == 2) {
                calculate_normal_dir(&rsp.current_lights[0], rsp.current_lights_coeffs[0]);
            }
            static const Light_t lookat_x = { { 0, 0, 0 }, 0, { 0, 0, 0 }, 0, { 127, 0, 0 }, 0 };
            static const Light_t lookat_y = { { 0, 0, 0 }, 0, { 0, 0, 0 }, 0, { 0, 127, 0 }, 0 };
            calculate_normal_dir(&lookat_x, rsp.current_lookat_coeffs[0]);
            calculate_normal_dir(&lookat_y, rsp.current_lookat_coeffs[1]);
            rsp.lights_changed = 0;
        }
        gfx_sp_vertex_light(n_vertices, dest_index, vertices);
    } else {
        gfx_sp_vertex_no(n_vertices, dest_index, vertices);
    }
}

extern s32 gIsMirrorMode;

/*

    struct u_pvr_vertex_t* v1 = &loaded_vertices[vtx1_idx];
    struct u_pvr_vertex_t* v2 = &loaded_vertices[vtx2_idx];
    struct u_pvr_vertex_t* v3 = &loaded_vertices[vtx3_idx];
    struct u_pvr_vertex_t* v_arr[3] = { v1, v2, v3 };

    if (v1->clip_rej & v2->clip_rej & v3->clip_rej) {
        // The whole triangle lies outside the visible area
        return;
    }

	
*/

static inline float approx_recip_sign(float v) {
	float _v = 1.0f / sqrtf(v * v);
	return copysignf(_v, v);
}

int total_tri=0;
int rej_tri=0;
int nearz_tri = 0;
static void  __attribute__((noinline)) gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
    struct LoadedVertex* v1 = &rsp.loaded_vertices[vtx1_idx];
    struct LoadedVertex* v2 = &rsp.loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &rsp.loaded_vertices[vtx3_idx];
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };
	//total_tri++;
    if ((v1->clip_rej | v2->clip_rej | v3->clip_rej) != 0x3f) {
        // The whole triangle lies outside the visible area
		//rej_tri++;
		return;
    }

	if ((rsp.geometry_mode & G_CULL_BOTH) != 0) {
		float rw1 = approx_recip_sign(v1->_w);
		float rw2 = approx_recip_sign(v2->_w);
		float rw3 = approx_recip_sign(v3->_w);

        float dx1 = (v1->_x * rw1) - (v2->_x * rw2);
        float dy1 = (v1->_y * rw1) - (v2->_y * rw2);
        float dx2 = (v3->_x * rw3) - (v2->_x * rw2);
        float dy2 = (v3->_y * rw3) - (v2->_y * rw2);
        float cross = dx1 * dy2 - dy1 * dx2;

        if ((v1->wlt0) ^ (v2->wlt0) ^ (v3->wlt0)) {
            // If one vertex lies behind the eye, negating cross will give the correct result.
            // If all vertices lie behind the eye, the triangle will be rejected anyway.
            cross = -cross;
        }

        switch (!!gIsMirrorMode) {
            case 0:
                switch (rsp.geometry_mode & G_CULL_BOTH) {
                    case G_CULL_FRONT:
                        if (cross <= 0) {
							//rej_tri++;
							return;
                        }
						break;
                    case G_CULL_BACK:
                        if (cross >= 0) {
							//rej_tri++;
							return;
                        }
						break;
                    case G_CULL_BOTH: {
                        // Why is this even an option?
						//rej_tri++;
                        return;
						}
						break;        
					}
                break;
            case 1:
                break;
        }
    }

	//frame_tris++;

    uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if (depth_test != rendering_state.depth_test) {
        gfx_flush();
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
    if (z_upd != rendering_state.depth_mask) {
        gfx_flush();
        gfx_rapi->set_depth_mask(z_upd);
        rendering_state.depth_mask = z_upd;
    }

    uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if (zmode_decal != rendering_state.decal_mode) {
        gfx_flush();
        gfx_rapi->set_zmode_decal(zmode_decal);
        rendering_state.decal_mode = zmode_decal;
    }

    if (rdp.viewport_or_scissor_changed) {
        if (memcmp(&rdp.viewport, &rendering_state.viewport, sizeof(rdp.viewport)) != 0) {
            gfx_flush();
            gfx_rapi->set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
            rendering_state.viewport = rdp.viewport;
        }
        if (memcmp(&rdp.scissor, &rendering_state.scissor, sizeof(rdp.scissor)) != 0) {
            gfx_flush();
            gfx_rapi->set_scissor(rdp.scissor.x, rdp.scissor.y, rdp.scissor.width, rdp.scissor.height);
            rendering_state.scissor = rdp.scissor;
        }
        rdp.viewport_or_scissor_changed = 0;
    }

    uint32_t cc_id = rdp.combine_mode;

    uint8_t use_alpha = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
    uint8_t use_fog = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
    uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
    uint8_t use_noise = (rdp.other_mode_l & G_AC_DITHER) == G_AC_DITHER;

    if (texture_edge) {
        use_alpha = 1;
    }

    if (use_alpha)
        cc_id |= SHADER_OPT_ALPHA;
    if (use_fog)
        cc_id |= SHADER_OPT_FOG;
    if (texture_edge)
        cc_id |= SHADER_OPT_TEXTURE_EDGE;
    if (use_noise)
        cc_id |= SHADER_OPT_NOISE;

    if (!use_alpha) {
        cc_id &= ~0xfff000;
    }

    struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
    struct ShaderProgram* prg = comb->prg;
    if (prg != rendering_state.shader_program) {
        gfx_flush();
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if (use_alpha != rendering_state.alpha_blend) {
        gfx_flush();
        /* if (in_intro) {
        gfx_rapi->set_use_alpha(1);
        } */
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    uint8_t num_inputs;
    uint8_t used_textures[2];
    gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
    int i;

    for (i = 0; i < 2; i++) {
        if (used_textures[i]) {
            if (rdp.textures_changed[i]) {
                gfx_flush();
                import_texture(i);
                rdp.textures_changed[i] = 0;
            }
            uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
            if (linear_filter != rendering_state.textures[i]->linear_filter ||
                rdp.texture_tile.cms != rendering_state.textures[i]->cms ||
                rdp.texture_tile.cmt != rendering_state.textures[i]->cmt) {
                gfx_flush();
                gfx_rapi->set_sampler_parameters(i, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
                rendering_state.textures[i]->linear_filter = linear_filter;
                rendering_state.textures[i]->cms = rdp.texture_tile.cms;
                rendering_state.textures[i]->cmt = rdp.texture_tile.cmt;
            }
        }
    }

    /* Will be enabled when pvr fog is working, something isn't quite right current */
#if 1
    if (use_fog) {
        float fog_color[4] = { rdp.fog_color.r * recip255, rdp.fog_color.g * recip255,
                               rdp.fog_color.b * recip255, (float) (rdp.fog_color.a * recip255) * 0.75f };
        glFogfv(GL_FOG_COLOR, fog_color);
    }
#endif

    uint8_t use_texture = used_textures[0] || used_textures[1];
    uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
    uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

    for (i = 0; i < 3; i++) {
        buf_vbo[buf_num_vert].vert.x = v_arr[i]->x;
        buf_vbo[buf_num_vert].vert.y = v_arr[i]->y;
        buf_vbo[buf_num_vert].vert.z = v_arr[i]->z;

        if (use_texture) {
            float u = (v_arr[i]->u - (float)(rdp.texture_tile.uls << 3)) * 0.03125f;
            // / 32.0f;
            float v = (v_arr[i]->v - (float)(rdp.texture_tile.ult << 3)) * 0.03125f;
            // / 32.0f;
            if ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
                // Linear filter adds 0.5f to the coordinates
                u += 0.5f;
                v += 0.5f;
            }
            buf_vbo[buf_num_vert].texture.u = u / (float) tex_width;
            buf_vbo[buf_num_vert].texture.v = v / (float) tex_height;
        }

        int j, k;
        buf_vbo[buf_num_vert].color.packed = 0xffffffff;
        uint32_t color_r = 0;
        uint32_t color_g = 0;
        uint32_t color_b = 0;
        uint32_t color_a = 0;

		if (num_inputs == 2) {
            int i0 = comb->shader_input_mapping[0][1] == CC_PRIM;
            int i2 = comb->shader_input_mapping[0][0] == CC_ENV;

            int i3 = comb->shader_input_mapping[0][0] == CC_PRIM;
            int i4 = comb->shader_input_mapping[0][1] == CC_ENV;

            if (i0 && i2) {
                color_r = 255 - rdp.env_color.r;
                color_g = 255 - rdp.env_color.g;
                color_b = 255 - rdp.env_color.b;
				color_a = rdp.prim_color.a;
                //color_a = 255; // 255 - rdp.env_color.a;
#if 0
                color_r *= ((/* 255 - */ rdp.prim_color.r + 255) /* /2 */);
                color_g *= ((/* 255 - */ rdp.prim_color.g + 255) /* /2 */);
                color_b *= ((/* 255 - */ rdp.prim_color.b + 255) /* /2 */);
                color_a = rdp.prim_color.a; //((rdp.prim_color.a + 255)/* /2 */);

                color_r >>= 8;// /= 255;
                color_g >>= 8;// /= 255;
                color_b >>= 8;// /= 255;
                // color_a /= 255;

                uint32_t max_c = 255;
                if (color_r > max_c)
                    max_c = color_r;
                if (color_g > max_c)
                    max_c = color_g;
                if (color_b > max_c)
                    max_c = color_b;
                // if (color_a > max_c) max_c = color_a;

                float rn, gn, bn, an;
                rn = (float) color_r;
                gn = (float) color_g;
                bn = (float) color_b;
                an = (float) color_a;
                float maxc = 255.0f / (float) max_c;
                rn *= maxc;
                gn *= maxc;
                bn *= maxc;
                // an *= maxc;

                color_r = (uint32_t) rn;
                color_g = (uint32_t) gn;
                color_b = (uint32_t) bn;
                color_a = (uint32_t) an;
#endif
                buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
            } else if (i3 && i4) {
                color_r = /* 255 - */ rdp.prim_color.r;
                color_g = /* 255 - */ rdp.prim_color.g;
                color_b = /* 255 - */ rdp.prim_color.b;
                color_a = // 255 -
                    rdp.prim_color.a;

                color_r *= ((rdp.env_color.r + 255));
                color_g *= ((rdp.env_color.g + 255));
                color_b *= ((rdp.env_color.b + 255));
                color_a *= (rdp.env_color.a /* + 255 */);

                color_r >>= 8;// /= 255;
                color_g >>= 8;// /= 255;
                color_b >>= 8;// /= 255;
                color_a >>= 8;// /= 255;

                uint32_t max_c = 255;
                if (color_r > max_c)
                    max_c = color_r;
                if (color_g > max_c)
                    max_c = color_g;
                if (color_b > max_c)
                    max_c = color_b;
                if (color_a > max_c)
                    max_c = color_a;

                float rn, gn, bn, an;
                rn = (float) color_r;
                gn = (float) color_g;
                bn = (float) color_b;
                an = (float) color_a;
                float maxc = 255.0f / (float) max_c;
                rn *= maxc;
                gn *= maxc;
                bn *= maxc;
                an *= maxc;

                color_r = (uint32_t) rn;
                color_g = (uint32_t) gn;
                color_b = (uint32_t) bn;
                color_a = (uint32_t) an;

                buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
            } else {
                goto thenextthing;
            }
        } else {
        thenextthing:
 		if (!in_intro) {
			if ((v_arr[i]->color.a == 255) && (v_arr[i]->color.b == 0) &&
			(v_arr[i]->color.g == 0) && (v_arr[i]->color.a == 255)) {
				buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(v_arr[i]->color.r, v_arr[i]->color.g,
                                                                      v_arr[i]->color.b, v_arr[i]->color.a);
				goto nextvert;
			}
		}
            for (j = 0; j < num_inputs; j++) {
                /*@Note: use_alpha ? 1 : 0 */
                for (k = 0; k < 1 + (use_alpha ? 0 : 0); k++) {
                    switch (comb->shader_input_mapping[k][j]) {
                        case CC_PRIM:
                            buf_vbo[buf_num_vert].color.packed =
                                PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, rdp.prim_color.a);
                            break;
                        case CC_SHADE:
                            buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(v_arr[i]->color.r, v_arr[i]->color.g,
                                                                               v_arr[i]->color.b, v_arr[i]->color.a);
                            break;
                        case CC_ENV:
                            buf_vbo[buf_num_vert].color.packed =
                                PACK_ARGB8888(rdp.env_color.r, rdp.env_color.g, rdp.env_color.b, rdp.env_color.a);
                            break;
                        case CC_LOD: {
                            float distance_frac = (v1->w - 3000.0f) / 3000.0f;
                            if (distance_frac < 0.0f)
                                distance_frac = 0.0f;
                            if (distance_frac > 1.0f)
                                distance_frac = 1.0f;
                            const uint8_t frac = (uint8_t) (distance_frac * 255.0f);
                            buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(frac, frac, frac, frac);
                            break;
                        }
                        default:
                            buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(0xff, 0xff, 0xff, 0xff);
                            // fix the alpha on the Nintendo logo model
                            if (in_intro) {
                                buf_vbo[buf_num_vert].color.packed =
                                    (rdp.env_color.a << 24) | (buf_vbo[buf_num_vert].color.packed & 0x00FFFFFF);
                            }
                            break;
                    }
                }
            }
        }
nextvert:
		buf_num_vert++;
        buf_vbo_len += sizeof(dc_fast_t);
    }
    buf_vbo_num_tris += 1;

	if (buf_vbo_num_tris == MAX_BUFFERED) {
        gfx_flush();
    }
}

#if 1
extern int first_2d;
extern void gfx_opengl_reset_frame(int r, int g, int b);
extern void gfx_opengl_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);
static void  __attribute__((noinline)) gfx_sp_quad_2d(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, uint8_t vtx1_idx2, uint8_t vtx2_idx2,
						   uint8_t vtx3_idx2) {
	gfx_flush();
	dc_fast_t* v1 = &rsp.loaded_vertices_2D[vtx1_idx];
	dc_fast_t* v2 = &rsp.loaded_vertices_2D[vtx2_idx];
	dc_fast_t* v3 = &rsp.loaded_vertices_2D[vtx3_idx];

	dc_fast_t* v12 = &rsp.loaded_vertices_2D[vtx1_idx2];
	dc_fast_t* v22 = &rsp.loaded_vertices_2D[vtx2_idx2];
	dc_fast_t* v32 = &rsp.loaded_vertices_2D[vtx3_idx2];

	dc_fast_t* v_arr[6] = { v1, v2, v3, v12, v22, v32 };

	uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
	if (depth_test != rendering_state.depth_test) {
		gfx_flush();
		gfx_rapi->set_depth_test(depth_test);
		rendering_state.depth_test = depth_test;
	}

	uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
	if (z_upd != rendering_state.depth_mask) {
		gfx_flush();
		gfx_rapi->set_depth_mask(z_upd);
		rendering_state.depth_mask = z_upd;
	}

	uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
	if (zmode_decal != rendering_state.decal_mode) {
		gfx_flush();
		gfx_rapi->set_zmode_decal(zmode_decal);
		rendering_state.decal_mode = zmode_decal;
	}

	if (rdp.viewport_or_scissor_changed) {
		if (memcmp(&rdp.viewport, &rendering_state.viewport, sizeof(rdp.viewport)) != 0) {
			gfx_flush();
			gfx_rapi->set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
			rendering_state.viewport = rdp.viewport;
		}
		if (memcmp(&rdp.scissor, &rendering_state.scissor, sizeof(rdp.scissor)) != 0) {
			gfx_flush();
			gfx_rapi->set_scissor(rdp.scissor.x, rdp.scissor.y, rdp.scissor.width, rdp.scissor.height);
			rendering_state.scissor = rdp.scissor;
		}
		rdp.viewport_or_scissor_changed = 0;
	}

	uint32_t cc_id = rdp.combine_mode;

	uint8_t use_alpha = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
	uint8_t use_fog = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
	uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
	uint8_t use_noise = (rdp.other_mode_l & G_AC_DITHER) == G_AC_DITHER;

	if (texture_edge) {
		use_alpha = 1;
	}

	if (use_alpha)
		cc_id |= SHADER_OPT_ALPHA;
	if (use_fog)
		cc_id |= SHADER_OPT_FOG;
	if (texture_edge)
		cc_id |= SHADER_OPT_TEXTURE_EDGE;
	if (use_noise)
		cc_id |= SHADER_OPT_NOISE;

	if (!use_alpha) {
		cc_id &= ~0xfff000;
	}

	struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
	struct ShaderProgram* prg = comb->prg;
	if (prg != rendering_state.shader_program) {
		gfx_flush();
		gfx_rapi->unload_shader(rendering_state.shader_program);
		gfx_rapi->load_shader(prg);
		rendering_state.shader_program = prg;
	}

	if (use_alpha != rendering_state.alpha_blend) {
		gfx_flush();
		gfx_rapi->set_use_alpha(use_alpha);
		rendering_state.alpha_blend = use_alpha;
	}
	uint8_t num_inputs;
	uint8_t used_textures[2];
	gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
	int i;

	for (i = 0; i < 2; i++) {
		if (used_textures[i]) {
			if (rdp.textures_changed[i]) {
				gfx_flush();
				import_texture(i);
				rdp.textures_changed[i] = 0;
			}
			uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
			if (linear_filter != rendering_state.textures[i]->linear_filter ||
				rdp.texture_tile.cms != rendering_state.textures[i]->cms ||
				rdp.texture_tile.cmt != rendering_state.textures[i]->cmt) {
				gfx_flush();
				gfx_rapi->set_sampler_parameters(i, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
				rendering_state.textures[i]->linear_filter = linear_filter;
				rendering_state.textures[i]->cms = rdp.texture_tile.cms;
				rendering_state.textures[i]->cmt = rdp.texture_tile.cmt;
			}
		}
	}

	uint8_t use_texture = used_textures[0] || used_textures[1];
	uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) / 4;
	uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) / 4;

	int tri_num_vert = 0;
	for (tri_num_vert = 0; tri_num_vert < 6; tri_num_vert++) {
		quad_vbo[tri_num_vert].vert.x = v_arr[tri_num_vert]->vert.x;
		quad_vbo[tri_num_vert].vert.y = v_arr[tri_num_vert]->vert.y;
		quad_vbo[tri_num_vert].vert.z = v_arr[tri_num_vert]->vert.z;

		if (use_texture) {
			float u = (v_arr[tri_num_vert]->texture.u - rdp.texture_tile.uls * 8) / 32.0f;
			float v = (v_arr[tri_num_vert]->texture.v - rdp.texture_tile.ult * 8) / 32.0f;
			if ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
				// Linear filter adds 0.5f to the coordinates
				u += 0.5f;
				v += 0.5f;
			}
			quad_vbo[tri_num_vert].texture.u = u / tex_width;
			quad_vbo[tri_num_vert].texture.v = v / tex_height;
		}

		struct RGBA white = (struct RGBA) { 0xff, 0xff, 0xff, 0xff };
		struct RGBA* color = &white;
		int j, k;

		uint32_t color_r = 0;
		uint32_t color_g = 0;
		uint32_t color_b = 0;
		uint32_t color_a = 0;

		if ((num_inputs == 2) && 
		((comb->shader_input_mapping[0][0] == CC_ENV) && 
		(comb->shader_input_mapping[0][1] == CC_PRIM))) {		
			color_r = 255 - rdp.env_color.r;
			color_r *= ((rdp.prim_color.r + 255));
			color_r >>= 8;

			color_g = 255 - rdp.env_color.g;
			color_g *= ((rdp.prim_color.g + 255));
			color_g >>= 8;

			color_b = 255 - rdp.env_color.b;
			color_b *= ((rdp.prim_color.b + 255));
			color_b >>= 8;

			color_a = rdp.prim_color.a;

			uint32_t max_c = 255;
			if (color_r > max_c)
				max_c = color_r;
			if (color_g > max_c)
				max_c = color_g;
			if (color_b > max_c)
				max_c = color_b;

			float rn, gn, bn;
			rn = (float) color_r;
			gn = (float) color_g;
			bn = (float) color_b;
			float maxc = 255.0f / (float) max_c;
			rn *= maxc;
			gn *= maxc;
			bn *= maxc;

			color_r = (uint32_t) rn;
			color_g = (uint32_t) gn;
			color_b = (uint32_t) bn;
			quad_vbo[tri_num_vert].color.array.r = color_r;
			quad_vbo[tri_num_vert].color.array.g = color_g;
			quad_vbo[tri_num_vert].color.array.b = color_b;
			quad_vbo[tri_num_vert].color.array.a = color_a;
		} else {
			for (j = 0; j < num_inputs; j++) {
				/*@Note: use_alpha ? 1 : 0 */
				for (k = 0; k < 1 + (use_alpha ? 0 : 0); k++) {
					switch (comb->shader_input_mapping[k][j]) {
						case CC_PRIM:
							color = &rdp.prim_color;
							break;
						case CC_SHADE:
							color = &v_arr[i]->color;
							break;
						case CC_ENV:
							color = &rdp.env_color;
							break;
						default:
							color = &white;
							break;
					}
				}
			}

			quad_vbo[tri_num_vert].color.array.r = color->r;
			quad_vbo[tri_num_vert].color.array.g = color->g;
			quad_vbo[tri_num_vert].color.array.b = color->b;
			quad_vbo[tri_num_vert].color.array.a = color->a;
		}
	}

	gfx_opengl_draw_triangles_2d((void*) quad_vbo, 0, use_texture);
}
#endif

static void gfx_sp_geometry_mode(uint32_t clear, uint32_t set) {
	rsp.geometry_mode &= ~clear;
	rsp.geometry_mode |= set;
}

static void gfx_calc_and_set_viewport(const Vp_t* viewport) {
	// 2 bits fraction
//	float width = 2.0f * viewport->vscale[0] / 4.0f;
//	float height = 2.0f * viewport->vscale[1] / 4.0f;
//	float x = (viewport->vtrans[0] / 4.0f) - width / 2.0f;
//	float y = SCREEN_HEIGHT - ((viewport->vtrans[1] / 4.0f) + height / 2.0f);
	float width = viewport->vscale[0] * 0.5f;
	float height = viewport->vscale[1] * 0.5f;
	float x = (viewport->vtrans[0] * 0.25f) - width * 0.5f;
	float y = SCREEN_HEIGHT - ((viewport->vtrans[1] * 0.25f) + height * 0.5f);

	width *= RATIO_X;
	height *= RATIO_Y;
	x *= RATIO_X;
	y *= RATIO_Y;

	rdp.viewport.x = x;
	rdp.viewport.y = y;
	rdp.viewport.width = width;
	rdp.viewport.height = height;

	rdp.viewport_or_scissor_changed = 1;
}

static void  gfx_sp_movemem(uint8_t index, UNUSED uint8_t offset, const void* data) {
	switch (index) {
		case G_MV_VIEWPORT:
			gfx_calc_and_set_viewport((const Vp_t*) data);
			break;
		case G_MV_L0:
		case G_MV_L1:
		case G_MV_L2:
			// NOTE: reads out of bounds if it is an ambient light
			n64_memcpy(rsp.current_lights + (index - G_MV_L0) / 2, data, sizeof(Light_t));
			break;
		default:
			break;
	}
}
int16_t fog_mul;
int16_t fog_ofs;
static void  gfx_sp_moveword(uint8_t index, UNUSED uint16_t offset, uint32_t data) {
	switch (index) {
		case G_MW_NUMLIGHT:
			// Ambient light is included
			// The 31th bit is a flag that lights should be recalculated
			rsp.current_num_lights = (data - 0x80000000U) / 32;
			rsp.lights_changed = 1;
			break;
		case G_MW_FOG:
			fog_mul = rsp.fog_mul = (int16_t) (data >> 16);
			fog_ofs = rsp.fog_offset = (int16_t) data;
			// would be a good place for a
			// fog_dirty = 1;
			break;
		default:
			break;
	}
}

static void gfx_sp_texture(uint16_t sc, uint16_t tc, UNUSED uint8_t level, UNUSED uint8_t tile, UNUSED uint8_t on) {
	rsp.texture_scaling_factor.s = sc;
	rsp.texture_scaling_factor.t = tc;
}

static void gfx_dp_set_scissor(UNUSED uint32_t mode, uint32_t ulx, uint32_t uly, uint32_t lrx,
							   uint32_t lry) {
//	float x = ulx / 4.0f * RATIO_X;
//	float y = (SCREEN_HEIGHT - lry / 4.0f) * RATIO_Y;
//	float width = (lrx - ulx) / 4.0f * RATIO_X;
//	float height = (lry - uly) / 4.0f * RATIO_Y;
	float x = ulx * 0.25f * RATIO_X;
	float y = (SCREEN_HEIGHT - lry * 0.25f) * RATIO_Y;
	float width = (lrx - ulx) * 0.25f * RATIO_X;
	float height = (lry - uly) * 0.25f * RATIO_Y;

	rdp.scissor.x = x;
	rdp.scissor.y = y;
	rdp.scissor.width = width;
	rdp.scissor.height = height;

	rdp.viewport_or_scissor_changed = 1;
}

static void gfx_dp_set_texture_image(UNUSED uint32_t format, uint32_t size, UNUSED uint32_t width,
									 UNUSED const void* addr) {
	rdp.texture_to_load.addr = segmented_to_virtual((void*)addr);
	rdp.texture_to_load.siz = size;
	last_set_texture_image_width = width;
}

static void  gfx_dp_set_tile(uint8_t fmt, uint32_t siz, uint32_t line, uint32_t tmem, uint8_t tile,
							UNUSED uint32_t palette, uint32_t cmt, UNUSED uint32_t maskt, UNUSED uint32_t shiftt,
							uint32_t cms, UNUSED uint32_t masks, UNUSED uint32_t shifts) {
	if (tile == G_TX_RENDERTILE) {
		//SUPPORT_CHECK(palette == 0); // palette should set upper 4 bits of color index in 4b mode
		rdp.texture_tile.fmt = fmt;
		rdp.texture_tile.siz = siz;

		if (cms == G_TX_WRAP && masks == G_TX_NOMASK) {
			cms = G_TX_CLAMP;
		}
		if (cmt == G_TX_WRAP && maskt == G_TX_NOMASK) {
			cmt = G_TX_CLAMP;
		}
		rdp.texture_tile.cms = cms;
		rdp.texture_tile.cmt = cmt;
		rdp.texture_tile.line_size_bytes = line * 8;
		rdp.textures_changed[0] = 1;
		rdp.textures_changed[1] = 1;
	}

	if (tile == G_TX_LOADTILE) {
		rdp.texture_to_load.tile_number = tmem >> 8;
	} else {
		rdp.texture_to_load.tile_number = tile;
	}
	rdp.texture_to_load.tmem = tmem;
}

static void  gfx_dp_set_tile_size(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
	if (tile == G_TX_RENDERTILE) {
		rdp.texture_tile.uls = uls;
		rdp.texture_tile.ult = ult;
		rdp.texture_tile.lrs = lrs;
		rdp.texture_tile.lrt = lrt;
		rdp.textures_changed[0] = 1;
		rdp.textures_changed[1] = 1;
	}
}
extern uint16_t common_tlut_hud_type_C_rank_font[];
extern uint16_t common_tlut_finish_line_banner[];
static void  __attribute__((noinline)) gfx_dp_load_tlut(UNUSED uint8_t tile, UNUSED uint32_t high_index) {
	rdp.palette = rdp.texture_to_load.addr;
#if 1
	uint32_t* srcp = (uint32_t *)segmented_to_virtual((void*)rdp.texture_to_load.addr);
	uint16_t clearcolor;
	int hud_thing = (srcp == segmented_to_virtual(common_tlut_hud_type_C_rank_font));
	if (segmented_to_virtual(common_tlut_finish_line_banner) == srcp) {
		clearcolor = 0x7fff;
	} else {
		clearcolor = 0;
	}

	uint32_t* tlp32 = (uint32_t *)tlut;
	for (int i = 0; i < 128; i++) {
		uint32_t twoc = *srcp++;
		uint16_t c1 = twoc & 0xffff;
		uint16_t c2 = twoc >> 16;
		uint8_t a = !!(c1 & 0x100);
		if (a || hud_thing) {
			c1 = __builtin_bswap16(c1);//(c1 << 8) | ((c1 >> 8) & 0xff);
			uint8_t r = c1 >> 11;
			uint8_t g = (c1 >> 6) & 0x1f;
			uint8_t b = (c1 >> 1) & 0x1f;
			a = (c1 & 1);
			c1 = (a << 15) | (r << 10) | (g << 5) | (b);
		} else {
			c1 = clearcolor;
		}
		a = !!(c2 & 0x100);
		if (a || hud_thing) {
			c2 = __builtin_bswap16(c2);//(c2 << 8) | ((c2 >> 8) & 0xff);
			uint8_t r = c2 >> 11;
			uint8_t g = (c2 >> 6) & 0x1f;
			uint8_t b = (c2 >> 1) & 0x1f;
			a = (c2 & 1);
			c2 = (a << 15) | (r << 10) | (g << 5) | (b);
		} else {
			c2 = clearcolor;
		}	
		*tlp32++ = (c2 << 16) | c1;
	}
#else
	uint16_t* srcp = segmented_to_virtual((void*)rdp.texture_to_load.addr);
	if (((uintptr_t)srcp & 3) == 0)
		printf("word tlut\n");

	uint16_t* tlp = tlut;
	for (int i = 0; i < 256; i++) {
		uint16_t c = *srcp++;
		uint8_t a = !!(c & 0x100);
		if (a) {
			c = (c << 8) | ((c >> 8) & 0xff);
			uint8_t r = c >> 11;
			uint8_t g = (c >> 6) & 0x1f;
			uint8_t b = (c >> 1) & 0x1f;
			*tlp++ = 0x8000 | (r << 10) | (g << 5) | (b);
		} else {
			*tlp++ = 0;
		}
	}
#endif
}

static void  gfx_dp_load_block(UNUSED uint8_t tile, UNUSED uint32_t uls, UNUSED uint32_t ult, uint32_t lrs,
							  UNUSED uint32_t dxt) {
	// The lrs field rather seems to be number of pixels to load
	uint32_t word_size_shift = 0;
	switch (rdp.texture_to_load.siz) {
		case G_IM_SIZ_4b:
			word_size_shift = 0; // Or -1? It's unused in SM64 anyway.
			break;
		case G_IM_SIZ_8b:
			word_size_shift = 0;
			break;
		case G_IM_SIZ_16b:
			word_size_shift = 1;
			break;
		case G_IM_SIZ_32b:
			word_size_shift = 2;
			break;
	}
	uint32_t size_bytes = (lrs + 1) << word_size_shift;
	rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;
	assert(size_bytes <= 4096 && "bug: too big texture");
	rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;

	rdp.textures_changed[rdp.texture_to_load.tile_number] = 1;
}

static void gfx_dp_load_tile(UNUSED uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
	uint32_t word_size_shift = 0;
	switch (rdp.texture_to_load.siz) {
		case G_IM_SIZ_4b:
			word_size_shift = 0;
			break;
		case G_IM_SIZ_8b:
			word_size_shift = 0;
			break;
		case G_IM_SIZ_16b:
			word_size_shift = 1;
			break;
		case G_IM_SIZ_32b:
			word_size_shift = 2;
			break;
	}

	uint32_t size_bytes = ((((lrs - uls) >> G_TEXTURE_IMAGE_FRAC) + 1) * (((lrt - ult) >> G_TEXTURE_IMAGE_FRAC) + 1))
						  << word_size_shift;
	rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;

//	assert(size_bytes <= 4096 && "bug: too big texture");
	rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;
	rdp.texture_tile.uls = uls;
	rdp.texture_tile.ult = ult;
	rdp.texture_tile.lrs = lrs;
	rdp.texture_tile.lrt = lrt;

	rdp.textures_changed[rdp.texture_to_load.tile_number] = 1;
}

static uint8_t color_comb_component(uint32_t v) {
	switch (v) {
		case G_CCMUX_TEXEL0:
			return CC_TEXEL0;
		case G_CCMUX_TEXEL1:
			return CC_TEXEL1;
		case G_CCMUX_PRIMITIVE:
			return CC_PRIM;
		case G_CCMUX_SHADE:
			return CC_SHADE;
		case G_CCMUX_ENVIRONMENT:
			return CC_ENV;
		case G_CCMUX_TEXEL0_ALPHA:
			return CC_TEXEL0A;
		case G_CCMUX_LOD_FRACTION:
			return CC_LOD;
		default:
			return CC_0;
	}
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
	return color_comb_component(a) | (color_comb_component(b) << 3) | (color_comb_component(c) << 6) |
		   (color_comb_component(d) << 9);
}
static void gfx_dp_set_combine_mode(uint32_t rgb, uint32_t alpha) {
	rdp.combine_mode = rgb | (alpha << 12);
}
uint8_t er,eg,eb,ea;

static void  gfx_dp_set_env_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	er = rdp.env_color.r = r;
	eg = rdp.env_color.g = g;
	eb = rdp.env_color.b = b;
	ea = rdp.env_color.a = a;
}

uint8_t pr,pg,pb,pa;

static void gfx_dp_set_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	pr = rdp.prim_color.r = r;
	pg = rdp.prim_color.g = g;
	pb = rdp.prim_color.b = b;
	pa = rdp.prim_color.a = a;
}

static void gfx_dp_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	rdp.fog_color.r = r;
	rdp.fog_color.g = g;
	rdp.fog_color.b = b;
	rdp.fog_color.a = a;
}

static void  gfx_dp_set_fill_color(uint32_t packed_color) {
	uint16_t col16 = (uint16_t) packed_color;
	uint32_t r = col16 >> 11;
	uint32_t g = (col16 >> 6) & 0x1f;
	uint32_t b = (col16 >> 1) & 0x1f;
	uint32_t a = col16 & 1;
	rdp.fill_color.r = SCALE_5_8(r);
	rdp.fill_color.g = SCALE_5_8(g);
	rdp.fill_color.b = SCALE_5_8(b);
	rdp.fill_color.a = a * 255;
}

#if 1
void  __attribute__((noinline)) gfx_opengl_2d_projection(void) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 640, 480, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void  __attribute__((noinline)) gfx_opengl_reset_projection(void) {
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((const float*) rsp.P_matrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
}
#endif

float screen_2d_z;

static void  __attribute__((noinline)) gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
	uint32_t saved_other_mode_h = rdp.other_mode_h;
	uint32_t cycle_type = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

	if (cycle_type == G_CYC_COPY) {
		rdp.other_mode_h = (rdp.other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT;
	}
	//frame_quads++;
	// U10.2 coordinates
	float ulxf = ulx;
	float ulyf = uly;
	float lrxf = lrx;
	float lryf = lry;

	ulxf = ulxf / (4.0f * HALF_SCREEN_WIDTH) - 1.0f;
	ulyf = (ulyf / (4.0f * HALF_SCREEN_HEIGHT)) - 1.0f;
	lrxf = lrxf / (4.0f * HALF_SCREEN_WIDTH) - 1.0f;
	lryf = (lryf / (4.0f * HALF_SCREEN_HEIGHT)) - 1.0f;

	// ulxf = gfx_adjust_x_for_aspect_ratio(ulxf);
	// lrxf = gfx_adjust_x_for_aspect_ratio(lrxf);

	ulxf = (ulxf * 320) + 320;
	lrxf = (lrxf * 320) + 320;

	ulyf = (ulyf * 240) + 240;
	lryf = (lryf * 240) + 240;

	dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
	dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
	dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
	dc_fast_t* ur = &rsp.loaded_vertices_2D[3];
	screen_2d_z += 0.0005f;
	ul->vert.x = ulxf;
	ul->vert.y = ulyf;
	ul->vert.z = screen_2d_z;

	ll->vert.x = ulxf;
	ll->vert.y = lryf;
	ll->vert.z = screen_2d_z;

	lr->vert.x = lrxf;
	lr->vert.y = lryf;
	lr->vert.z = screen_2d_z;

	ur->vert.x = lrxf;
	ur->vert.y = ulyf;
	ur->vert.z = screen_2d_z;

	// The coordinates for texture rectangle shall bypass the viewport setting
	struct XYWidthHeight default_viewport = { 0, 0, gfx_current_dimensions.width, gfx_current_dimensions.height };
	struct XYWidthHeight viewport_saved = rdp.viewport;
	uint32_t geometry_mode_saved = rsp.geometry_mode;

	rdp.viewport = default_viewport;
	rdp.viewport_or_scissor_changed = 1;
	rsp.geometry_mode = 0;

	gfx_sp_quad_2d(0, 1, 3, 1, 2, 3);

	rsp.geometry_mode = geometry_mode_saved;
	rdp.viewport = viewport_saved;
	rdp.viewport_or_scissor_changed = 1;

	if (cycle_type == G_CYC_COPY) {
		rdp.other_mode_h = saved_other_mode_h;
	}
}

static void  __attribute__((noinline)) gfx_dp_texture_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, UNUSED uint8_t tile,
									 int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, uint8_t flip) {
	uint32_t saved_combine_mode = rdp.combine_mode;
	if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_COPY) {
		// Per RDP Command Summary Set Tile's shift s and this dsdx should be set to 4 texels
		// Divide by 4 to get 1 instead
		dsdx >>= 2;

		// Color combiner is turned off in copy mode
		gfx_dp_set_combine_mode(color_comb(0, 0, 0, G_CCMUX_TEXEL0), color_comb(0, 0, 0, G_ACMUX_TEXEL0));

		// Per documentation one extra pixel is added in this modes to each edge
		lrx += 1 << 2;
		lry += 1 << 2;
	}

	// uls and ult are S10.5
	// dsdx and dtdy are S5.10
	// lrx, lry, ulx, uly are U10.2
	// lrs, lrt are S10.5
	if (flip) {
		dsdx = -dsdx;
		dtdy = -dtdy;
	}
	int16_t width = !flip ? lrx - ulx : lry - uly;
	int16_t height = !flip ? lry - uly : lrx - ulx;
	float lrs = ((uls << 7) + dsdx * width) >> 7;
	float lrt = ((ult << 7) + dtdy * height) >> 7;

	dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
	dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
	dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
	dc_fast_t* ur = &rsp.loaded_vertices_2D[3];
	ul->texture.u = uls;
	ul->texture.v = ult;
	lr->texture.u = lrs;
	lr->texture.v = lrt;

	if (!flip) {
		ll->texture.u = uls;
		ll->texture.v = lrt;
		ur->texture.u = lrs;
		ur->texture.v = ult;
	} else {
		ll->texture.u = lrs;
		ll->texture.v = ult;
		ur->texture.u = uls;
		ur->texture.v = lrt;
	}

	gfx_draw_rectangle(ulx, uly, lrx, lry);
	rdp.combine_mode = saved_combine_mode;
}

static void  __attribute__((noinline)) gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
	int i;
	do_fill_rect = 1;

	if (rdp.color_image_address == rdp.z_buf_address) {
		// Don't clear Z buffer here since we already did it with glClear
		do_fill_rect = 0;
		return;
	}

	uint32_t mode = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

	if (mode == G_CYC_COPY || mode == G_CYC_FILL) {
		// Per documentation one extra pixel is added in this modes to each edge
		lrx += 1 << 2;
		lry += 1 << 2;
	}

	for (i = 0; i < 4; i++) {
		dc_fast_t* v = &rsp.loaded_vertices_2D[i];
		v->color.array.a = rdp.fill_color.a;
		v->color.array.b = rdp.fill_color.b;
		v->color.array.g = rdp.fill_color.g;
		v->color.array.r = rdp.fill_color.r;
	}

//	uint32_t saved_combine_mode = rdp.combine_mode;
//	gfx_dp_set_combine_mode(color_comb(0, 0, 0, G_CCMUX_SHADE), color_comb(0, 0, 0, G_ACMUX_SHADE));
	gfx_draw_rectangle(ulx, uly, lrx, lry);
//	rdp.combine_mode = saved_combine_mode;

//	do_fill_rect = 0;
}

static void gfx_dp_set_z_image(void* z_buf_address) {
	rdp.z_buf_address = z_buf_address;
}

static void  gfx_dp_set_color_image(UNUSED uint32_t format, UNUSED uint32_t size, UNUSED uint32_t width, void* address) {
	rdp.color_image_address = address;
}

static void  gfx_sp_set_other_mode(uint32_t shift, uint32_t num_bits, uint64_t mode) {
	uint64_t mask = (((uint64_t) 1 << num_bits) - 1) << shift;
	uint64_t om = rdp.other_mode_l | ((uint64_t) rdp.other_mode_h << 32);
	om = (om & ~mask) | mode;
	rdp.other_mode_l = (uint32_t) om;
	rdp.other_mode_h = (uint32_t) (om >> 32);
}
static inline void* seg_addr(uintptr_t w1) {
	return (void*) segmented_to_virtual((void*) w1);
}

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))

int use_one_inv = 0;
int depth_off = 0;
static void  __attribute__((noinline)) gfx_run_dl(Gfx* cmd) {
	//printf("starting at %08x\n", cmd);

	cmd = seg_addr((uintptr_t) cmd);
	for (;;) {
		uint32_t opcode = cmd->words.w0 >> 24;

		// custom f3d opcodes, sorry
		if (cmd->words.w0 == 0x424C4E44) {
			if (cmd->words.w1 == 0x4655434B) {
				// gSPSignaling
				blend_fuck ^= 1;
			} else if(cmd->words.w1 == 0x4655434C) {
				// gSPBlendOneInv
				use_one_inv ^= 1;
			} else if(cmd->words.w1 == 0x4655434D) {
				depth_off ^= 1;
			}
			++cmd;
			continue;
		}

		switch (opcode) {
			// RSP commands:
			case G_MTX:
				gfx_flush();
#ifdef F3DEX_GBI_2
				gfx_sp_matrix(C0(0, 8) ^ G_MTX_PUSH, (const int32_t*) seg_addr(cmd->words.w1));
#else
				//printf("processing %08x\n", cmd);
				gfx_sp_matrix(C0(16, 8), (const int32_t*) seg_addr(cmd->words.w1));
#endif
				break;

			case (uint8_t) G_POPMTX:
				gfx_flush();
#ifdef F3DEX_GBI_2
				gfx_sp_pop_matrix(cmd->words.w1 / 64);
#else
				gfx_sp_pop_matrix(1);
#endif
				break;

			case G_MOVEMEM:
#ifdef F3DEX_GBI_2
				gfx_sp_movemem(C0(0, 8), C0(8, 8) * 8, seg_addr(cmd->words.w1));
#else
				gfx_sp_movemem(C0(16, 8), 0, seg_addr(cmd->words.w1));
#endif
				break;

			case (uint8_t) G_MOVEWORD:
#ifdef F3DEX_GBI_2
				gfx_sp_moveword(C0(16, 8), C0(0, 16), cmd->words.w1);
#else
				gfx_sp_moveword(C0(0, 8), C0(8, 16), cmd->words.w1);
#endif
				break;

			case (uint8_t) G_TEXTURE:
#ifdef F3DEX_GBI_2
				gfx_sp_texture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(1, 7));
#else
				gfx_sp_texture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(0, 8));
#endif
				break;

			case G_VTX:
#ifdef F3DEX_GBI_2
				gfx_sp_vertex(C0(12, 8), C0(1, 7) - C0(12, 8), seg_addr(cmd->words.w1));
#elif defined(F3DEX_GBI) || defined(F3DLP_GBI)
				gfx_sp_vertex(C0(10, 6), C0(16, 8) / 2, seg_addr(cmd->words.w1));
#else
				gfx_sp_vertex((C0(0, 16)) / sizeof(Vtx), C0(16, 4), seg_addr(cmd->words.w1));
#endif
				break;

			case G_DL:
				if (C0(16, 1) == 0) {
					// Push return address
						//printf("branch to %08x\n", cmd->words.w1);

					gfx_run_dl((Gfx*) seg_addr(cmd->words.w1));
				} else {
						//printf("branch to %08x\n", cmd->words.w1);
					cmd = (Gfx*) seg_addr(cmd->words.w1);
					--cmd; // increase after break
				}
				break;

			case (uint8_t) G_ENDDL:
				return;

#ifdef F3DEX_GBI_2
			case G_GEOMETRYMODE:
				gfx_sp_geometry_mode(~C0(0, 24), cmd->words.w1);
				break;
#else
			case (uint8_t) G_SETGEOMETRYMODE:
				gfx_sp_geometry_mode(0, cmd->words.w1);
				break;

			case (uint8_t) G_CLEARGEOMETRYMODE:
				gfx_sp_geometry_mode(cmd->words.w1, 0);
				break;
#endif

			case (uint8_t) G_QUAD:
				// C1(24, 8) / 2    3
				// C1(16, 8) / 2    0
				//  C1(8, 8) / 2    1
				// C1(0, 8) / 2     2
				gfx_sp_tri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(24, 8) / 2);
				gfx_sp_tri1(C1(8, 8) / 2, C1(0, 8) / 2, C1(24, 8) / 2);
				break;

			case (uint8_t) G_TRI1:
#ifdef F3DEX_GBI_2
				gfx_sp_tri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2);
#elif defined(F3DEX_GBI) || defined(F3DLP_GBI)
				gfx_sp_tri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2);
#else
				gfx_sp_tri1(C1(16, 8) / 10, C1(8, 8) / 10, C1(0, 8) / 10);
#endif
				break;

#if defined(F3DEX_GBI) || defined(F3DLP_GBI)
			case (uint8_t) G_TRI2:
				gfx_sp_tri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2);
				gfx_sp_tri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2);
				break;
#endif

			case (uint8_t) G_SETOTHERMODE_L:
#ifdef F3DEX_GBI_2
				gfx_sp_set_other_mode(31 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, cmd->words.w1);
#else
				gfx_sp_set_other_mode(C0(8, 8), C0(0, 8), cmd->words.w1);
#endif
				break;

			case (uint8_t) G_SETOTHERMODE_H:
#ifdef F3DEX_GBI_2
				gfx_sp_set_other_mode(63 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, (uint64_t) cmd->words.w1 << 32);
#else
				gfx_sp_set_other_mode(C0(8, 8) + 32, C0(0, 8), (uint64_t) cmd->words.w1 << 32);
#endif
				break;

			// RDP Commands:
			case G_SETTIMG:
				gfx_dp_set_texture_image(C0(21, 3), C0(19, 2), C0(0, 10), cmd->words.w1);
				break;

			case G_LOADBLOCK:
				gfx_dp_load_block(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_LOADTILE:
				gfx_dp_load_tile(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_SETTILE:
				gfx_dp_set_tile(C0(21, 3), C0(19, 2), C0(9, 9), C0(0, 9), C1(24, 3), C1(20, 4), C1(18, 2), C1(14, 4),
								C1(10, 4), C1(8, 2), C1(4, 4), C1(0, 4));
				break;

			case G_SETTILESIZE:
				gfx_dp_set_tile_size(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_LOADTLUT:
				gfx_dp_load_tlut(C1(24, 3), C1(14, 10));
				break;

			case G_SETENVCOLOR:
				gfx_dp_set_env_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;

			case G_SETPRIMCOLOR:
				gfx_dp_set_prim_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;
				
			case G_SETFOGCOLOR:
				gfx_dp_set_fog_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;

			case G_SETFILLCOLOR:
				gfx_dp_set_fill_color(cmd->words.w1);
				break;

			case G_SETCOMBINE:
				gfx_dp_set_combine_mode(color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)),
										color_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)));
				/*color_comb(C0(5, 4), C1(24, 4), C0(0, 5), C1(6, 3)),
				color_comb(C1(21, 3), C1(3, 3), C1(18, 3), C1(0, 3)));*/
				break;

			// G_SETPRIMCOLOR, G_CCMUX_PRIMITIVE, G_ACMUX_PRIMITIVE, is used by Goddard
			// G_CCMUX_TEXEL1, LOD_FRACTION is used in Bowser room 1

			case G_TEXRECT:
			case G_TEXRECTFLIP: {
				int32_t lrx, lry, tile, ulx, uly;
				uint32_t uls, ult, dsdx, dtdy;
#ifdef F3DEX_GBI_2E
				lrx = (int32_t) (C0(0, 24) << 8) >> 8;
				lry = (int32_t) (C1(0, 24) << 8) >> 8;
				++cmd;
				ulx = (int32_t) (C0(0, 24) << 8) >> 8;
				uly = (int32_t) (C1(0, 24) << 8) >> 8;
				++cmd;
				uls = C0(16, 16);
				ult = C0(0, 16);
				dsdx = C1(16, 16);
				dtdy = C1(0, 16);
#else
				lrx = C0(12, 12);
				lry = C0(0, 12);
				tile = C1(24, 3);
				ulx = C1(12, 12);
				uly = C1(0, 12);
				++cmd;
				uls = C1(16, 16);
				ult = C1(0, 16);
				++cmd;
				dsdx = C1(16, 16);
				dtdy = C1(0, 16);
#endif
				gfx_dp_texture_rectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == G_TEXRECTFLIP);
				break;
			}

			case G_FILLRECT:
#ifdef F3DEX_GBI_2E
			{
				int32_t lrx, lry, ulx, uly;
				lrx = (int32_t) (C0(0, 24) << 8) >> 8;
				lry = (int32_t) (C1(0, 24) << 8) >> 8;
				++cmd;
				ulx = (int32_t) (C0(0, 24) << 8) >> 8;
				uly = (int32_t) (C1(0, 24) << 8) >> 8;
				gfx_dp_fill_rectangle(ulx, uly, lrx, lry);
				break;
			}
#else
				gfx_dp_fill_rectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
				break;
#endif
			case G_SETSCISSOR:
				gfx_dp_set_scissor(C1(24, 2), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_SETZIMG:
				gfx_dp_set_z_image(seg_addr(cmd->words.w1));
				break;

			case G_SETCIMG:
				gfx_dp_set_color_image(C0(21, 3), C0(19, 2), C0(0, 11), seg_addr(cmd->words.w1));
				break;
		}
		++cmd;
	}
}

static void gfx_sp_reset() {
	rsp.modelview_matrix_stack_size = 1;
	rsp.current_num_lights = 2;
	rsp.lights_changed = 1;
}

void gfx_get_dimensions(uint32_t* width, uint32_t* height) {
	gfx_wapi->get_dimensions(width, height);
}

void gfx_init(struct GfxWindowManagerAPI* wapi, struct GfxRenderingAPI* rapi, const char* game_name,
			  uint8_t start_in_fullscreen) {
	size_t i;
	draw_rect = 0;
	gfx_wapi = wapi;
	gfx_rapi = rapi;
	gfx_wapi->init(game_name, start_in_fullscreen);
	gfx_rapi->init();

	// Used in the 120 star TAS
	static uint32_t precomp_shaders[] = { 0x01200200, 0x00000045, 0x00000200, 0x01200a00, 0x00000a00, 0x01a00045,
										  0x00000551, 0x01045045, 0x05a00a00, 0x01200045, 0x05045045, 0x01045a00,
										  0x01a00a00, 0x0000038d, 0x01081081, 0x0120038d, 0x03200045, 0x03200a00,
										  0x01a00a6f, 0x01141045, 0x07a00a00, 0x05200200, 0x03200200, 0x09200200,
										  0x0920038d, 0x09200045 };
	for (i = 0; i < sizeof(precomp_shaders) / sizeof(uint32_t); i++) {
		gfx_lookup_or_create_shader_program(precomp_shaders[i]);
	}
	gfx_wapi->get_dimensions(&gfx_current_dimensions.width, &gfx_current_dimensions.height);
	if (gfx_current_dimensions.height == 0) {
		// Avoid division by zero
		gfx_current_dimensions.height = 1;
	}
	gfx_current_dimensions.aspect_ratio = (float) gfx_current_dimensions.width / (float) gfx_current_dimensions.height;

	oops_texture_id = gfx_rapi->new_texture();
	gfx_rapi->select_texture(0, oops_texture_id);
	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, 64, 64, GL_UNSIGNED_SHORT_4_4_4_4_REV);

	memset(&oops_node, 0, sizeof(oops_node));
	oops_node.texture_id = oops_texture_id;
}

struct GfxRenderingAPI* gfx_get_current_rendering_api(void) {
	return gfx_rapi;
}

void gfx_start_frame(void) {
	//frame_quads = frame_tris = frame_verts = 0;
	gfx_wapi->handle_events();
}

void gfx_run(Gfx* commands) {
	gfx_sp_reset();

	if (!gfx_wapi->start_frame()) {
		dropped_frame = 1;
		return;
	}

	dropped_frame = 0;

	gfx_rapi->start_frame();
	gfx_run_dl(commands);
	gfx_flush();
	gfx_rapi->end_frame();
	gfx_wapi->swap_buffers_begin();
}

void gfx_end_frame(void) {
	//printf("total vert %d nearz %d\n", total_verts, nearz_clip_verts);
//	printf("total tri %d rej %d nearz %d\n", total_tri, rej_tri, nearz_tri);
	total_verts = nearz_clip_verts = total_tri = rej_tri = nearz_tri = 0;
	//printf("verts %d tris %d quads %d\n", frame_verts, frame_tris, frame_quads);
	if (!dropped_frame) {
		gfx_rapi->finish_render();
		gfx_wapi->swap_buffers_end();
	}
}
