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

#include "sh4zam.h"
#include <kos.h>

extern int in_intro;

int fix_flash = 0;

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

// Set when a G_MTX_PROJECTION load is an ORTHO matrix (guOrtho: [3][3]==1; guPerspective:
// [3][3]==0). Ortho geometry is "2D drawn as triangles" (constant _w), so on the PVR
// backend gfx_sp_tri1 routes its z into the 2D screen_2d_z scheme instead of 1/w — that's
// how the pause-menu glyph text sorts ABOVE the 2D dimming quad (same idea as SF64's
// render-on-top, but keyed off the ortho projection rather than a custom DL command).
int proj_is_ortho = 0;
// Whether a PERSPECTIVE projection loaded this frame (=> a 3D scene exists). Latched into
// prev_frame_had_persp at start_frame and read by gfx_sp_tri1: it separates a Z-off
// overlay-ortho BACKGROUND (the over-skybox clouds, drawn in a race) from a Z-off
// overlay-ortho OVERLAY (menu glyph text, drawn with no 3D scene) — otherwise identical.
int cur_frame_persp = 0;
int prev_frame_had_persp = 0;
// Set when a PERSPECTIVE (real 3D) triangle has been emitted this frame; reset in
// start_frame. Used by gfx_draw_rectangle: a 2D quad drawn BEFORE any 3D, in a frame that
// has a 3D scene, is a BACKGROUND (title bg strips) -> far-pin; once 3D has drawn, 2D is a
// foreground overlay (the logo) -> normal near screen_2d_z.
int has_done_3d = 0;
extern float screen_2d_z;   // defined later in this file; needed up here by gfx_sp_tri1
// Z-off far-pin base + per-primitive draw-order stagger (reset each frame in gfx_start_frame;
// see the comment at its use in gfx_sp_tri1). Base 1e-5 = the historical far-pin constant.
static float far_pin_z = 0.00001f;

// ---- GFXPROF: per-frame renderer profile to serial (ported from sf64-dc) --------------------
// One greppable line per second (every 60 frames) plus any frame whose interpreter walk exceeds
// GFX_PROF_SLOW_US. walk = DL interpretation incl. TA submission, flush = PT/TR bucket replay
// (rapi->end_frame), finish = pvr_scene_finish (blocks while the GPU finishes the previous
// frame -> GPU-bound signal). Timer = SH4 PRFC0 elapsed cycles (perf_cntr_timer_enable in
// gfx_init) — NEVER timer_us_gettime64 in hot loops (~10ms/frame at 5 reads/tri on sf64).
// PRFC1 rotates through pipeline-freeze (stall) modes, one delta per walk, all 5 in ~40 frames.
#define GFX_PROF 0   // 1 = measurement session (bake split, scissor split, cc histogram; costs 4-6ms/frame)
#define GFX_PROF_SLOW_US 22000

// Contiguous hot-code section (ported from sf64-dc): the per-frame hot path pinned into one
// block so it cannot alias ITSELF in the SH4's 8KB direct-mapped I-cache — with LTO +
// -fno-toplevel-reorder, unpinned layout shifts on any size change swung sf64's icache stall
// by +/-1.7ms with identical workloads. Section order = symbol CREATION (first declaration)
// order; the attribute BLOCKS inlining, so never put it on small static-inline helpers.
// optimize("O2") on the hot members trades -O3 loop unrolling for code size: the block had grown
// to 9388B, past the 8KB direct-mapped I-cache (tail wraps onto head, ~1.2KB self-aliasing); at O2
// it measures 5888B — fully fitting, 2.3KB headroom. NB: optimize("Os") is IMPOSSIBLE here — the
// SH backend disables fsrra/fsca under optimize_size, and the sh4zam always_inline helpers then
// hard-error with "target specific option mismatch" (no flag incantation fixes it; SH has no
// per-function target attribute). GCC >= 12 merges this attribute with command-line flags (older
// GCCs reset them — do not copy this pattern to an old-toolchain tree). Whole-file -O2 was tried
// and reverted in sf64 for perf; THIS is different (hot members only, rest of file stays -O3).
// Judge with GFXLITE on HW, not by eye.
#define GFX_HOT __attribute__((section(".text.hot.gfx"), optimize("O2")))
// GFXLITE: release-config frame meter. NO hot-path instrumentation — two counter reads per
// frame around the walk plus the frame-to-frame period, one line per second. `late` counts
// frames whose PERIOD blew the 30Hz deadline (40ms) — a locked-30 game prints late=0/60 with
// per avg~33333us. Safe to leave enabled in shipping builds.
#define GFX_PROF_LITE 1   // 1 = always-on GFXLITE frame meter (per avg/max, late=N/60, walk avg/max)
#if GFX_PROF || GFX_PROF_LITE
#include <dc/perfctr.h>
#define PROF_NOW() perf_cntr_count(PRFC0)
#define PROF_US(c) ((unsigned long) ((c) / 200u))
#endif
#if GFX_PROF_LITE && !GFX_PROF
static uint64_t lite_last = 0, lite_per_sum = 0, lite_per_max = 0;
static uint64_t lite_walk_sum = 0, lite_walk_max = 0;
static uint32_t lite_n = 0, lite_late = 0;
#endif
#if GFX_PROF
static uint32_t prof_tris = 0, prof_verts = 0, prof_mtx = 0, prof_texup = 0, prof_frame = 0;
static uint64_t prof_t_walk = 0, prof_t_flush = 0, prof_t_finish = 0;
static uint64_t prof_t_vtx = 0, prof_t_tri = 0, prof_t_tri_setup = 0;
static uint64_t prof_t_clip = 0, prof_t_bake = 0, prof_t_submit = 0;
// Software-scissor split (the 4P question: how much of the walk is pane clipping?):
// sc_oc = outcode refresh + classify, paid by EVERY tri in split-screen (sc_is_fullscreen never
// applies there); sc_fan = Sutherland-Hodgman + fan emit, paid only by pane-crossing tris.
// Counters: asis = emitted unclipped, rej = dropped off-pane, clip = fan calls, fanout = tris out.
static uint64_t prof_t_sc_oc = 0, prof_t_sc_fan = 0;
static uint32_t prof_sc_asis = 0, prof_sc_rej = 0, prof_sc_clip = 0, prof_sc_fanout = 0;
// Bake-bucket split (where do the 13-17ms go?): SAMPLED — every 8th source triangle gets
// per-vertex sub-timers (full per-vertex timing would swamp the measurement). Printed x8 as an
// estimate. res = pvr_reserve (PT/TR flag-stamp), sm = 1/w + screen map + z, uv = UV transform,
// cc = combiner cache/eval + fog + colour stores. bake minus the scaled sum = loop/store overhead.
static uint64_t prof_t_bk_res = 0, prof_t_bk_sm = 0, prof_t_bk_uv = 0, prof_t_bk_cc = 0;
static uint32_t prof_bk_samples = 0;
// Shade-pass combiner fast path: evals taking the shortcut, and sampled self-check mismatches
// (general term eval vs shortcut on live verts — MUST stay 0; nonzero = classifier bug).
static uint32_t prof_cc_pass = 0, prof_cc_mism = 0;
static uint32_t prof_evals = 0, prof_hits = 0, prof_stamps = 0, prof_setups = 0;
static uint64_t prof_t_quad = 0, prof_t_mtx = 0;
static uint32_t prof_quads = 0;
static uint32_t prof_op_verts = 0, prof_pt_verts = 0, prof_tr_verts = 0;
static uint64_t prof_su[4];   // setup sections: 0 depth/fog/viewport, 1 combiner/shader/blend, 2 texture, 3 texenv/key/prepare
static const struct { int mode; const char* name; } prof_stall_modes[] = {
    { PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE, "icache" }, { PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE, "dcache" },
    { PMCR_PIPELINE_FREEZE_BY_FPU_MODE, "fpu" },            { PMCR_PIPELINE_FREEZE_BY_BRANCH_MODE, "branch" },
    { PMCR_PIPELINE_FREEZE_BY_CPU_REGISTER_MODE, "reg" },
};
static int prof_stall_idx = 0;
static uint64_t prof_stall_walk = 0;
#define PROF_INC(x) ((x)++)
#define PROF_ADD(x, n) ((x) += (n))
#else
#define PROF_INC(x) ((void) 0)
#define PROF_ADD(x, n) ((void) 0)
#endif

// ---- Per-loaded-vertex combiner-eval cache (ported from sf64-dc) ----------------------------
// pvr_eval_combiner runs per vertex per triangle, and a mesh vertex is shared by ~4-6 tris, so
// evaluate ONCE per loaded vertex per combiner STATE: the result (argb/oargb, without the
// per-vertex fog byte, which is OR'd in after) is cached by vertex slot and stamped with the
// combiner-state stamp; gfx_sp_vertex invalidates the slots it reloads; a state-key change
// (combine words, prim, env, texenv+textured, cycle type) bumps the stamp. Near-clip/scissor
// fan temporaries (clip_bufA/B) fall outside the slot range and bypass the cache. Semantically
// INERT: every input pvr_eval_combiner reads is either in the key or per-vertex-invalidated.
static struct { uint32_t stamp, argb, oargb; } cc_cache[MAX_VERTICES + 4];
static uint32_t cc_state_stamp = 1;
// [0] starts at an impossible value (real combine w0 always carries the 0xFC opcode byte) so
// the very first triangle is guaranteed to take the key-change path and set up ccp_active.
static uint32_t cc_last_key[6] = { 0xFFFFFFFFu, 0, 0, 0, 0, 0 };
static inline void cc_cache_invalidate(int first, int n) {
    for (int i = 0; i < n && first + i < MAX_VERTICES + 4; i++) cc_cache[first + i].stamp = 0;
}
// Recent-state stamp table: MK64 alternates materials constantly (track/kart/track...), and a
// monotonically-new stamp per change meant a RETURNING state invalidated every cached vertex
// (28% hit rate measured). A state seen recently reuses its old stamp instead, so vertices
// evaluated under it (and not reloaded since) hit again. Stamps are never reassigned to a
// different key, so a stamp match still uniquely identifies (state, vertex-shade) — inert.
// On 32-bit stamp wrap (~days of play) everything is cleared. Slot stamp 0 = empty (never
// matches, and vertex-invalidate also writes stamp 0).
// Prepared combiner (built once per NEW state, ~25/frame): every state-constant part of
// pvr_eval_combiner — mux bit-decode, term selection, prim/env float conversion, branch
// classification — resolved ahead of time. Each of the 16 (a-b)*c+d terms becomes either a
// constant or a shade-channel/shade-alpha select; the per-vertex fast path evaluates the SAME
// formula in the SAME order with the SAME clamps, so results are bit-identical to the generic
// evaluator (which stays as-is for the 2D quad path).
struct ccp_term { uint8_t sel; float k; };   // sel: 0=const k, 1=shade[ch], 2=shade[3]
struct ccp {
    uint8_t replace;       // GFX_TEXENV_REPLACE: constant {argb=~0, oargb=0}
    uint8_t cct;           // color_const_textured: whole colour -> oargb, argb.rgb = 0
    uint8_t offset;        // additive PRIM/ENV 'd' routed to constant oargb
    uint8_t shade_pass;    // whole eval == shade passthrough: argb = pack(shade bytes), oargb = 0
                           // (modulate-class states, e.g. G_CC_MODULATEIA — see classifier)
    uint32_t oargb_const;  // the offset case's oargb (state-constant)
    uint8_t const_out;     // no term references shade: whole output is a state constant,
    uint8_t cpad[3];       // evaluated once at prepare into c_argb/c_oargb (bit-exact by identity)
    uint32_t c_argb, c_oargb;
    struct ccp_term c[3][4];   // per colour channel: a, b, c, d
    struct ccp_term a[4];      // alpha: aa, ab, ac, ad (only const / shade[3])
};
#define CC_STATE_SLOTS 16
// prof_ev: GFX_PROF-only per-state eval counter for the combiner-state histogram (top states by
// eval count printed once per second — the data that tells us which mux words deserve fast paths).
static struct { uint32_t key[6]; uint32_t stamp; uint32_t prof_ev; struct ccp prep; } cc_states[CC_STATE_SLOTS];
static uint32_t cc_next_stamp = 1;
static uint32_t cc_state_rr = 0;
static const struct ccp* ccp_active = NULL;   // current state's prepared combiner
static int cc_active_slot = -1;               // its slot: protected from ring eviction

// ---- Triangle state-setup hoist (ported from sf64-dc; its single biggest CPU win) -----------
// The full per-triangle state derivation (depth/fog/viewport flushes, combiner+shader lookup,
// OP/PT/TR routing, texture import/bind, texenv, cc-cache key) only changes when a non-TRI DL
// opcode runs, so it is derived ONCE per rdp_state_gen change (~140/frame) instead of per
// triangle (~800/frame). gfx_run_dl bumps the gen on every opcode EXCEPT the pure-geometry ones
// (TRI1/TRI2/QUAD/VTX/MTX/POPMTX/DL/ENDDL) and the 2D rect ops; the 2D quad path mutates backend
// state directly, so it bumps the gen itself when it finishes. gfx_run bumps once per frame.
// DC 4P far pull-in: per-frame threshold in view-depth (w) units; 0 = disabled.
// Per-course scale of gCourseFarPersp (1500-7000, mostly 4500). The section lists already cull to
// well inside ~0.6*far, so a scale must sit nearer than that to bite. 0 disables for courses where
// long sight lines ARE the gameplay (or that are too light to be worth the visual cost). Tune here.
#define DC_4P_FAR_SCALE 0.0f    // 0 = DISABLED everywhere (trying tick-speed compensation alone).
                                // Tuning notes if re-enabled: useful band is 0.35 (hard, ~800
                                // tris/frame, -5ms walk) to ~0.55; 0.6+ does nothing (section
                                // lists cull first). Rainbow Road/Choco/Double Deck should stay 0
                                // (sight-line gameplay / far plane already 1500), Turnpike gentle
                                // (oncoming traffic visibility).
static float dc4p_course_far_scale(int courseId) {
    (void) courseId;
    return DC_4P_FAR_SCALE;
}
static float dc4p_far_w = 0.0f;
#if GFX_PROF
static uint32_t prof_farcull = 0;
#endif

static uint32_t rdp_state_gen = 1, tri_setup_gen = 0;
static struct {
    uint8_t depth_test, zmode_decal, use_texture, linear_filter;
    uint16_t uls, ult;              // texture_tile snapshot for the bake loop's UV math
    uint32_t texenv;
    float recip_tex_width, recip_tex_height;
} ts;

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
	// 46 — Cohen-Sutherland-style scissor outcode (which scissor edges this vertex
	// is OUTSIDE of), plus the scissor generation it was computed for so we can
	// lazily recompute it if the scissor changed after the vertex was loaded.
	uint8_t scissor_oc;
	// 47
	uint8_t scissor_gen;
	// 48 — set when this vertex was loaded by the lighting path (gfx_sp_vertex_light).
	// gfx_sp_tri1 uses it to decide whether to modulate the material colour by the
	// per-vertex shade (Star Fox-style light×colour mix). OLD packed this as bit 0x80
	// of a side clip_rej[] byte because its LoadedVertex was crammed to exactly 32
	// bytes; CURRENT's cell has room, so it gets its own field (no clip-path masking).
	uint8_t lit;
	// 49 — per-vertex N64 fog coefficient (0..255), computed at vertex-load from the clip
	// z/w and the RSP fog mul/offset (G_FOG geometry mode). Emitted into the PVR vertex's
	// offset-colour (oargb) ALPHA, which PVR hardware VERTEX fog reads as the fog density.
	uint8_t fog;
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
		uint32_t tmem;   // render tile's tmem address; selects the loaded_texture slot (tmem>>8)
	} texture_tile;
	uint8_t textures_changed[2];

	uint32_t other_mode_l, other_mode_h;
	uint32_t combine_mode;
#ifdef GFX_BACKEND_PVR
	// Raw G_SETCOMBINE words, kept for the PVR combiner evaluator: the compressed
	// combine_mode (cc_id) drops the N64 G_CCMUX_1 constant + the true alpha mux, so
	// the faithful (a-b)*c+d eval reads these instead. (See pvr_eval_combiner.)
	uint32_t combine_w0, combine_w1;
#endif

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
	uint8_t fog_enabled;      // PVR vertex fog on (G_FOG geometry mode)
	uint8_t tex_env;          // PVR texel<->colour combine (enum gfx_tex_env), derived from combiner
	uint32_t fog_color;       // last fog colour pushed to the PVR fog register (0xAARRGGBB)
	// 12
	struct XYWidthHeight viewport;
	// 20
	struct XYWidthHeight scissor;
	// 28
	uint32_t pad;
};

// tex_env defaults to MODULATE to match the backend's sTexEnv default, so the first textured
// draw doesn't desync (a first REPLACE draw would otherwise skip set_tex_env). All else zero.
struct RenderingState rendering_state = { .tex_env = GFX_TEXENV_MODULATE };

struct GfxDimensions gfx_current_dimensions;

static uint8_t dropped_frame;

// ---- Software scissor clipping state ----
// GLdc still applies the per-player viewport (glViewport); we only replace the
// scissor. The scissor rectangle is expressed as NDC bounds in the player's own
// clip space, i.e. relative to the active viewport's window mapping:
//   win = v.xy + (ndc+1)/2 * v.wh   =>   ndc = 2*(scissor - v.xy)/v.wh - 1
static float sc_ndc_xmin = -1.0f, sc_ndc_xmax = 1.0f;
static float sc_ndc_ymin = -1.0f, sc_ndc_ymax = 1.0f;
static int sc_is_fullscreen = 1;
// Which scissor edges actually need the homogeneous CLIP pass (SC_LEFT/RIGHT/BOTTOM/TOP).
// An edge whose clip boundary lands on the framebuffer border is redundant: geometry past
// it maps OFF-SCREEN, where the PVR's guard band drops it (exactly the single-player
// sc_is_fullscreen case) — only edges that border a NEIGHBOURING split-screen pane can let
// overhang bleed in, so only those get clipped. Trivial-reject still uses all four edges.
static uint8_t sc_active_mask = 0x0F;   // == SC_EDGE_MASK (defined below)
// Eye-plane guard so the homogeneous scissor planes always divide by w > 0.
#define SCISSOR_W_EPS 0.00001f

// Per-vertex scissor outcode bits: each set bit means the vertex is OUTSIDE that
// scissor edge. SC_FORCE marks a vertex at/behind the eye (w<=eps), whose edge
// bits are meaningless (homogeneous test sign is undefined), so any triangle that
// touches it must take the full clip path.
#define SC_LEFT   0x01
#define SC_RIGHT  0x02
#define SC_BOTTOM 0x04
#define SC_TOP    0x08
#define SC_FORCE  0x10
#define SC_EDGE_MASK 0x0F
// Bumped whenever the scissor planes change; vertices store the gen they were
// computed under so gfx_sp_tri1 can lazily refresh a stale outcode.
static uint8_t cur_scissor_gen = 1;

// Float copies of the viewport/scissor rects (lower-left window space), captured
// before they are truncated into the uint16 rdp.viewport/rdp.scissor fields. The
// uint16 store wraps negative coords (which happen during sliding/shrinking
// transitions) to ~65000 and drops the fraction, so the clip math must NOT read
// the rdp fields. Init to a full 640x480 screen.
static float vpf_x = 0.0f,   vpf_y = 0.0f,   vpf_w = 640.0f, vpf_h = 480.0f;
static float scf_x = 0.0f,   scf_y = 0.0f,   scf_w = 640.0f, scf_h = 480.0f;
#define GFX_BACKEND_PVR
#ifdef GFX_BACKEND_PVR
// NDC -> PVR window (pixel) mapping coefficients. The raw-PVR backend does NO
// transform, so gfx_retro_dc owns the full object->screen path: MVP at vertex-load
// (clip-space _x/_y/_z/_w), then perspective divide + viewport map at emit:
//   screen_x = sm_xscale*(_x/_w) + sm_xbias
//   screen_y = sm_yscale*(_y/_w) + sm_ybias   (yscale<0: N64 NDC y-up -> PVR y-down)
//   screen_z = 1/_w                            (PVR inverse-depth)
// Top-left origin pixels, matching the 2D path. (GLdc build: none of this exists.)
static float sm_xscale = 320.0f, sm_xbias = 320.0f;
static float sm_yscale = -240.0f, sm_ybias = 240.0f;

static void gfx_recompute_screen_map(void) {
	float vw = vpf_w <= 0.0f ? 1.0f : vpf_w;
	float vh = vpf_h <= 0.0f ? 1.0f : vpf_h;
	float fb_h = (float) gfx_current_dimensions.height;
	sm_xscale = vw * 0.5f;
	sm_xbias  = vpf_x + vw * 0.5f;
	sm_yscale = -(vh * 0.5f);
	sm_ybias  = fb_h - vpf_y - vh * 0.5f;
}
#endif

// Recompute the scissor NDC bounds from the current viewport + scissor rects.
// Called whenever either changes (both live in the same lower-left window space).
static void gfx_recompute_scissor_planes(void) {
	float vx = vpf_x;
	float vy = vpf_y;
	float vw = vpf_w;
	float vh = vpf_h;
	if (vw <= 0.0f) vw = 1.0f;
	if (vh <= 0.0f) vh = 1.0f;

	float sx = scf_x;
	float sy = scf_y;
	float sw = scf_w;
	float sh = scf_h;

	sc_ndc_xmin = 2.0f * (sx - vx) / vw - 1.0f;
	sc_ndc_xmax = 2.0f * ((sx + sw) - vx) / vw - 1.0f;
	sc_ndc_ymin = 2.0f * (sy - vy) / vh - 1.0f;
	sc_ndc_ymax = 2.0f * ((sy + sh) - vy) / vh - 1.0f;

	// Clip region is scissor INTERSECT viewport. On N64 the RSP confines geometry
	// to the viewport frustum (NDC +/-1), so geometry never legitimately exists
	// outside the viewport rect; a scissor looser than the viewport (e.g. race
	// start sets scissor = whole half but viewport = one quadrant) must NOT widen
	// the clip past the viewport edge, or GLdc's guard band lets the frustum
	// overhang bleed into the neighbouring pane. Clamp every bound to [-1, 1].
	// (Non-overlapping scissor/viewport collapse to an empty interval => nothing
	// draws, which is also the correct N64 result.)
	sc_ndc_xmin = sc_ndc_xmin < -1.0f ? -1.0f : (sc_ndc_xmin > 1.0f ? 1.0f : sc_ndc_xmin);
	sc_ndc_xmax = sc_ndc_xmax < -1.0f ? -1.0f : (sc_ndc_xmax > 1.0f ? 1.0f : sc_ndc_xmax);
	sc_ndc_ymin = sc_ndc_ymin < -1.0f ? -1.0f : (sc_ndc_ymin > 1.0f ? 1.0f : sc_ndc_ymin);
	sc_ndc_ymax = sc_ndc_ymax < -1.0f ? -1.0f : (sc_ndc_ymax > 1.0f ? 1.0f : sc_ndc_ymax);

	// Skip the software clip only when the viewport fills the whole framebuffer
	// (single player): then in-frustum geometry maps inside the screen and any
	// frustum overhang falls off-screen harmlessly. For a sub-viewport (split
	// screen) we must clip even if scissor == viewport, because GLdc's imprecise
	// frustum clipping lets overhang bleed into the neighbouring pane.
	float fbw = (float) gfx_current_dimensions.width;
	float fbh = (float) gfx_current_dimensions.height;
	int vp_is_full = (vx <= 0.5f && vy <= 0.5f &&
	                  (vx + vw) >= fbw - 0.5f && (vy + vh) >= fbh - 0.5f);
	int scissor_covers_vp = (sc_ndc_xmin <= -0.999f && sc_ndc_xmax >= 0.999f &&
	                         sc_ndc_ymin <= -0.999f && sc_ndc_ymax >= 0.999f);
	sc_is_fullscreen = (vp_is_full && scissor_covers_vp);

	// Per-edge clip skip: map each clip boundary back to framebuffer space (NDC -1 -> v.xy,
	// +1 -> v.xy+v.wh) and drop the edge from the CLIP pass when it sits on the framebuffer
	// border (nothing to bleed into there — the PVR guard band discards the off-screen
	// overhang). Edges that fall on an interior split line stay active. Half-pixel slop
	// matches vp_is_full. (Trivial-reject in gfx_sp_tri1 still consults all four edges.)
	{
		float bx_min = vx + (sc_ndc_xmin + 1.0f) * 0.5f * vw;
		float bx_max = vx + (sc_ndc_xmax + 1.0f) * 0.5f * vw;
		float by_min = vy + (sc_ndc_ymin + 1.0f) * 0.5f * vh;
		float by_max = vy + (sc_ndc_ymax + 1.0f) * 0.5f * vh;
		uint8_t m = 0;
		if (bx_min >  0.5f)       m |= SC_LEFT;
		if (bx_max <  fbw - 0.5f) m |= SC_RIGHT;
		if (by_min >  0.5f)       m |= SC_BOTTOM;
		if (by_max <  fbh - 0.5f) m |= SC_TOP;
		sc_active_mask = m;
	}

	// Invalidate cached per-vertex scissor outcodes (they were computed against the
	// old planes). 0 is reserved as "never computed", so skip it on wrap.
	if (++cur_scissor_gen == 0)
		cur_scissor_gen = 1;
}

// Compute a vertex's scissor outcode from its (homogeneous) clip-space coords.
// w>0 is guaranteed past the eye-plane check, so do one fast reciprocal + two
// muls to get NDC and compare directly against the bounds (cheaper than 4 muls,
// and fdiv is far slower on SH4). The reciprocal is approximate, but the outcode
// is only a classifier — gfx_build_clipped_fan still clips with exact math.
static inline uint8_t compute_scissor_outcode(float x, float y, float w) {
	if (w <= SCISSOR_W_EPS)
		return SC_FORCE;
	float rw = shz_inverse_posf(w); // w>0 here: single fsrra-based reciprocal
	float nx = x * rw;
	float ny = y * rw;
	uint8_t oc = 0;
	if (nx < sc_ndc_xmin) oc |= SC_LEFT;
	if (nx > sc_ndc_xmax) oc |= SC_RIGHT;
	if (ny < sc_ndc_ymin) oc |= SC_BOTTOM;
	if (ny > sc_ndc_ymax) oc |= SC_TOP;
	return oc;
}

static dc_fast_t __attribute__((aligned(32))) quad_vbo[2 * 3];           // 2 tris make a quad (2D path, both backends)

#ifdef GFX_BACKEND_PVR
// PVR streams ALL 3D geometry with NO buf_vbo: OP (kind 0) bakes into op_emit then DR-submits per
// source-triangle (pvr_submit_op); PT/TR (kind 1/2) bake DIRECTLY into their deferred bucket
// (pvr_reserve). op_emit holds ONE source triangle's worst-case clipped fan: 3 verts + 5 clip planes
// (near + 4 scissor) = 8-vert polygon = 6 tris = 18 verts, never more.
static dc_fast_t __attribute__((aligned(32))) op_emit[6 * 3];
extern void pvr_submit_op(const dc_fast_t *tris, size_t n);
extern dc_fast_t *pvr_reserve(int kind, size_t n);

// Inline OP submit for the per-triangle path (ported from sf64-dc; the 2D paths still use the
// backend's pvr_submit_op): the backend call + per-vertex DR loop were out-of-object hops per
// triangle whose addresses alias the hot front-end code at the linker's whim (8KB direct-mapped
// I-cache). Same semantics: header re-emit only when dirty, then one SQ bulk ship of n verts —
// which requires the bake loop to stamp the strip flags at vertex creation (VERTEX/VERTEX_EOL).
extern uint8_t gfx_pvr_op_dirty;
extern void pvr_emit_op_header_slow(void);
// KOS sq_fast_cpy's loop (pair-single fmov.d, one SQ line per iteration), inlined. dst = SQ area
// address, src 32B-aligned, n = number of 32-byte lines (> 0). fschg is balanced inside the asm.
static inline void pvr_sq_ship(void *dst, const void *src, size_t n) {
    void *t;
    __asm__ __volatile__(
        "fschg\n"
        "1:\n\t"
        "fmov.d @%[s]+, dr0\n\t"
        "mov    %[d], %[t]\n\t"
        "fmov.d @%[s]+, dr2\n\t"
        "add    #32, %[t]\n\t"
        "fmov.d @%[s]+, dr4\n\t"
        "fmov.d @%[s]+, dr6\n\t"
        "pref   @%[s]\n\t"
        "dt     %[n]\n\t"
        "fmov.d dr6, @-%[t]\n\t"
        "fmov.d dr4, @-%[t]\n\t"
        "fmov.d dr2, @-%[t]\n\t"
        "fmov.d dr0, @-%[t]\n\t"
        "add    #32, %[d]\n\t"
        "bf.s   1b\n\t"
        "pref   @%[t]\n\t"
        "fschg"
        : [d] "+r" (dst), [s] "+r" (src), [n] "+r" (n), [t] "=&r" (t)
        :
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "memory", "t");
}
static inline void pvr_submit_op_inline(const dc_fast_t *tris, size_t n) {
    if (gfx_pvr_op_dirty) pvr_emit_op_header_slow();
    pvr_sq_ship(SQ_MASK_DEST(PVR_TA_INPUT), tris, n);
}
#endif

static struct GfxWindowManagerAPI* gfx_wapi;
static struct GfxRenderingAPI* gfx_rapi;

static uint16_t __attribute__((aligned(32))) tlut[256];

int do_fill_rect = 0;

extern s16 gCurrentCourseId;

static  __attribute__((noinline)) void gfx_flush(void) {
	// PVR: all 3D geometry streams as it's built (OP via DR, PT/TR direct into their buckets) — there
	// is no buffered batch to flush. The state-change callers still invoke gfx_flush(); it's a no-op.
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
	// Full wipe (matches sf64-dc), not just pool_pos: nuke_everything / the VRAM watchdog reset
	// the BACKEND id allocator (gfx_pvr_clear_all_textures: sTexCount=0) right before this runs.
	// If the node keys survive, reused nodes keep their OLD texture ids while fresh nodes get
	// RE-ISSUED ids from 1 — two front-end entries then share one backend slot, and every upload
	// swaps that slot's contents/dims under the other owner: wrong textures everywhere, squashed
	// UV scales, and free/malloc size flip-flop churn that fragments the PVR heap into the menu
	// OOM. Clearing the keys makes every node re-request an id, keeping the id spaces aligned.
	gfx_texture_cache.pool_pos = 0;
	memset(&gfx_texture_cache, 0, sizeof(gfx_texture_cache));
}

static inline uint32_t unpack_A(uint64_t key) {
    // Extract 19-bit field
    return (uint32_t)((key >> 42) & 0x7FFFF);
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

	__builtin_prefetch(*node);

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

/* Per-texture UV correction for the PoT padding done in gfx_opengl_upload_texture
   (real size / padded size). Multiplied into the texel->[0,1] normalization so the
   padded region is never sampled. */
extern float get_current_u_scale(void);
extern float get_current_v_scale(void);
#ifdef GFX_BACKEND_PVR
extern float gfx_pvr_get_u_scale(void);
extern float gfx_pvr_get_v_scale(void);
#define get_current_u_scale gfx_pvr_get_u_scale
#define get_current_v_scale gfx_pvr_get_v_scale
extern void gfx_pvr_set_blend(uint8_t kind);   // 0=OP, 1=PT, 2=TR
// Tracks the last OP/PT/TR classification sent to the backend so we flush on change.
// Persists across frames in lockstep with the backend's sListKind (neither resets).
static uint8_t pvr_cur_kind = 0;

extern void gfx_pvr_set_blend_factors(uint8_t src, uint8_t dst);   // gfx_blend_factor codes
// Last TR blend factors pushed to the backend (abstract gfx_blend_factor codes).
static uint8_t pvr_cur_bsrc = GFX_BLENDF_SRCALPHA, pvr_cur_bdst = GFX_BLENDF_INVSRCALPHA;


// Stable-allocation hint: size the backend's VRAM buffer to the FULL loaded texture so menu squish
// re-uploads don't realloc/fragment VRAM. Padded + applied in the backend (gfx_pvr.c).
extern void gfx_pvr_set_alloc_floor(uint32_t full_w, uint32_t full_h);
// Ask the backend to drop the whole texture cache at the next frame start (VRAM/pool watchdog).
extern void gfx_pvr_request_cache_flush(void);
// Is a cached texture's VRAM still resident? The backend evicts LRU textures under VRAM
// pressure; a cache hit whose block was evicted must re-upload rather than draw a freed pointer.
extern int gfx_pvr_texture_resident(uint32_t id);

// PVR hardware vertex fog (ported from the OoT raw-PVR renderer).
extern void gfx_pvr_set_fog(uint8_t enabled);
extern void gfx_pvr_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Derive PVR blend factors from the N64 blender (the final cycle in other_mode_l). The
// blender computes (P*A + M*B); for the standard "incoming over framebuffer" arrangement
// (P=CLR_IN, M=CLR_MEM) that is exactly src*src_factor + dst*dst_factor, with A (the
// incoming-color alpha mux) -> src factor and B (the framebuffer-color alpha mux) -> dst
// factor. Layout: P=[base+6], M=[base+4], A=[base+2], B=[base+0]; cycle-1 base 24, cycle-2
// base 16. Fully game-agnostic — additive/premultiplied/alpha all fall out of the muxes.
static void pvr_derive_blend(uint32_t oml, uint32_t omh, uint8_t *src, uint8_t *dst) {
    int base = ((omh & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE) ? 16 : 24;
    int p = (oml >> (base + 6)) & 3;     // P color select
    int m = (oml >> (base + 4)) & 3;     // M color select
    int a = (oml >> (base + 2)) & 3;     // A: incoming-color alpha (m1b)
    int b = (oml >> (base + 0)) & 3;     // B: framebuffer-color alpha (m2b)
    if (p != G_BL_CLR_IN || m != G_BL_CLR_MEM) {   // non-standard arrangement -> safe default
        *src = GFX_BLENDF_SRCALPHA; *dst = GFX_BLENDF_INVSRCALPHA;
        return;
    }
    // A (m1b): 0=A_IN, 1=A_FOG, 2=A_SHADE, 3=zero. PVR src alpha is the post-combine fragment
    // alpha, so A_IN/FOG/SHADE all collapse to SRC_ALPHA; only an explicit zero -> ZERO.
    *src = (a == G_BL_0) ? GFX_BLENDF_ZERO : GFX_BLENDF_SRCALPHA;
    // B (m2b): 0=1-A -> INV_SRC_ALPHA, 1=mem alpha -> DST_ALPHA, 2=one -> ONE, 3=zero -> ZERO.
    *dst = (b == G_BL_1MA)   ? GFX_BLENDF_INVSRCALPHA
         : (b == G_BL_A_MEM) ? GFX_BLENDF_DSTALPHA
         : (b == G_BL_1)     ? GFX_BLENDF_ONE
         :                     GFX_BLENDF_ZERO;
}
#endif

// `tile` = backend texture UNIT (0/1, drives select_texture + the cache node). `dslot` = the
// loaded_texture DATA slot = render tile's tmem block (0/1); decoupled so a render tile re-pointed
// to tmem 0x0100 samples the texture loaded there instead of always slot 0.
static  __attribute__((noinline)) uint8_t gfx_texture_cache_lookup(int tile, int dslot, struct TextureHashmapNode** n, const uint8_t* orig_addr,
										uint32_t tmem, uint32_t fmt, uint32_t siz, uint16_t uls, uint16_t ult) {
	void* segaddr = segmented_to_virtual((void *)orig_addr);
	size_t hash = (uintptr_t) segaddr;
	hash = (hash >> 5) & 0x3ff;

	uint64_t newkey = pack_key(segaddr,uls,ult,fmt,siz);

	/* The key omits the tile SIZE (lrs/lrt/line/size_bytes). Menu squish/scale
	   effects reshape the SAME source address every frame, so on a key hit they'd
	   otherwise get the stale upload. Fold the dimensions into a separate
	   discriminator; on a hit where it changed we re-upload into the SAME texture_id
	   (return 2) rather than spawning a new cache node / VRAM texture per scale. */
	uint32_t dimkey = ((uint32_t) rdp.texture_tile.lrs)
	                ^ ((uint32_t) rdp.texture_tile.lrt << 12)
	                ^ ((uint32_t) rdp.texture_tile.line_size_bytes << 20)
	                ^ ((uint32_t) rdp.loaded_texture[dslot].size_bytes * 2654435761u);

	struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
	if ((uintptr_t)rdp.loaded_texture[dslot].addr < 0x8c010000u) {
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

			int evicted = 0;
#ifdef GFX_BACKEND_PVR
			/* The backend may have freed this texture's VRAM under memory pressure (LRU
			   eviction). A hit on an evicted texture must re-upload, not draw a freed block. */
			evicted = !gfx_pvr_texture_resident((*node)->texture_id);
#endif
			/* dirty (explicit invalidate) OR the dims changed (squish/scale) OR the VRAM
			   was evicted -> re-upload into this same texture_id. */
			if ((*node)->dirty || (*node)->pad2 != dimkey || evicted) {
				(*node)->dirty = 0;
				(*node)->pad2 = dimkey;
				return 2;
			} else {
				*n = *node;
				return 1;
			}
		}
		node = &(*node)->next;
	}

	if (gfx_texture_cache.pool_pos == sizeof(gfx_texture_cache.pool) / sizeof(struct TextureHashmapNode)) {
		// Front-end node pool is full (1024 distinct textures since the last flush). Rather than
		// crash, draw this one frame with the placeholder and ask the backend to drop the whole
		// cache at the next frame start (VRAM/pool watchdog) — it rebuilds from scratch.
#ifdef GFX_BACKEND_PVR
		gfx_pvr_request_cache_flush();
#endif
		gfx_rapi->select_texture(tile, oops_texture_id);
		*node = &oops_node;
		return 1;
	}

	*node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];
	if ((*node)->key == 0) {
		(*node)->texture_id = gfx_rapi->new_texture();
	}
	gfx_rapi->select_texture(tile, (*node)->texture_id);
	gfx_opengl_set_tile_addr(tile, segaddr);

	gfx_rapi->set_sampler_parameters(tile, 0, 0, 0);
	(*node)->key = pack_key(segaddr,uls,ult,fmt,siz);
	(*node)->pad2 = dimkey;

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
		/* Decode is bounded by the STAGED data: `height` stays the original
		   size_bytes/line_size_bytes (down-rounded), and columns are capped to the
		   real source stride -- so this can never over-read the source (that read
		   bound is exactly what the shipped code relied on, and what my earlier
		   tile-rect-height/width version violated -> arch abort on the bottom bg
		   strip). Only the UPLOAD width takes the tri code's up-rounded tile-rect
		   value so the UV normalization matches (fixes OK/OPTION/DATA). Any extra
		   column is zero-padded. */
		uint32_t src_stride = last_set_texture_image_width + 1;
		uint32_t up_w = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		uint32_t up_h = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
		uint32_t cols;
		if (up_w <= src_stride + 1) {
			/* normal (sub)tile: up_w is within the +4 rounding of the source row.
			   Cap reads to the source row so we never over-read (that was the bottom-
			   bg-strip abort); zero-pad the rounding column. */
			cols = up_w < src_stride ? up_w : src_stride;
			/* A HORIZONTAL sub-tile (up_w < src_stride) loads part of the width but
			   the full height -- e.g. the pulsing "OK?" prompt, drawn as a left+right
			   pair by func_80097E58. Its two halves have different widths, so the
			   per-half size_bytes/line_size_bytes height rounds differently and the
			   left half ends up a row short of the right (the visible compression).
			   up_h is derived from the shared vertical extent, so it's identical for
			   both halves -> they align (this is what v1 did). The bottom bg strip is
			   full-width (up_w ~= src_stride), so it stays on the read-safe size/line
			   height and the over-read can't happen. */
			if (up_w < src_stride) {
				height = up_h;
			}
		} else if (up_w * up_h <= 8192) {
			/* legit wide reshape: the texture really IS up_w wide, built by reading
			   overlapping rows from a narrower source stride. Use the tile-rect dims.
			   The 8192 cap keeps padded(up_w*up_h) inside scaled (16384 u16). */
			cols = up_w;
			height = up_h;
		} else {
			/* pathological wrap coverage (tile rect >> source, e.g. 253-of-64): would
			   overrun the staging buffers and be wrong anyway. Fall back to the source
			   width; WRAP tiles it at draw time. */
			up_w = src_stride;
			cols = src_stride;
		}

		uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr
			+ ((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * src_stride)
			+ (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC);

		uint16_t* dst = rgba16_buf;
		for (i = 0; i < height; i++) {
			uint32_t x = 0;
			for (; x < cols; x++) {
				uint16_t col16 = __builtin_bswap16(start[x]);
				dst[x] = ((col16 & 1) << 15) | (col16 >> 1);
			}
			for (; x < up_w; x++) {
				dst[x] = 0;
			}
			start += src_stride;
			dst += up_w;
		}

		width = up_w;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

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
		/* LoadTile path: upload at the tri code's tex dims (see import_texture_i4).
		   NOTE: the only IA4 tile loader (func_80044AB8) is UNUSED, so this is
		   currently dead; kept consistent with the proven i4 path. Row-major
		   decode, no odd-row swap (source is plain row-major in DRAM). */
		width  = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

		uint32_t src_stride = last_set_texture_image_width + 1; /* source row, bytes */
		uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
			.addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * src_stride) +
				  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];

		memset(rgba16_buf, 0, width * height * 2);
		uint16_t* dst = rgba16_buf;
		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {
				uint32_t b = x >> 1;
				if (b >= src_stride) {
					continue;
				}
				uint8_t byte = start[b];
				uint8_t part = (x & 1) ? (byte & 0xf) : ((byte >> 4) & 0xf);
				uint8_t intensity = (SCALE_3_8(part >> 1) >> 3) & 0x1f;
				uint8_t alpha = part & 1;
				dst[x] = (alpha << 15) | (intensity << 10) | (intensity << 5) | intensity;
			}
			start += src_stride;
			dst += width;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

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
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;

	if (last_set_texture_image_width == 0) {
		uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr;
		uint32_t count = width * height;
		for (i = 0; i < count; i++) {
			uint16_t np = start[i];
			uint8_t al = (np >> 12) & 0xf;
			uint8_t in = (np >>  4) & 0xf;
			rgba16_buf[i] = (al << 12) | (in << 8) | (in << 4) | in;
		}
	} else {
		/* Bounded sub-rect decode; upload width = tri tile rect. See the detailed
		   rationale in import_texture_rgba16. Old code uploaded src_width (the full
		   source row width), wrong for sub-tile loads. */
		uint32_t src_stride = last_set_texture_image_width + 1;
		uint32_t up_w = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		uint32_t up_h = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
		uint32_t cols;
		if (up_w <= src_stride + 1) {
			cols = up_w < src_stride ? up_w : src_stride;
			/* horizontal sub-tile (e.g. OK's left/right halves): use the shared
			   tile-rect height so both halves align; see rgba16 for details. */
			if (up_w < src_stride) {
				height = up_h;
			}
		} else if (up_w * up_h <= 8192) {
			cols = up_w;                       /* legit wide reshape; see rgba16 */
			height = up_h;
		} else {
			up_w = src_stride;                 /* wrap coverage; fall back */
			cols = src_stride;
		}

		uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr
			+ ((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * src_stride)
			+ (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC);

		uint16_t* dst = rgba16_buf;
		for (i = 0; i < height; i++) {
			uint32_t x = 0;
			for (; x < cols; x++) {
				uint16_t np = start[x];
				uint8_t al = (np >> 12) & 0xf;
				uint8_t in = (np >>  4) & 0xf;
				dst[x] = (al << 12) | (in << 8) | (in << 4) | in;
			}
			for (; x < up_w; x++) {
				dst[x] = 0;
			}
			start += src_stride;
			dst += up_w;
		}

		width = up_w;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}

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
		/* LoadTile path (e.g. gDPLoadTextureTile_4b menu fonts). The uploaded
		   texture MUST match the dimensions the triangle code uses to normalize
		   UVs -- tex_width/tex_height = (lr - ul + 4) >> 2 -- or the glyph is
		   sampled at the wrong scale and renders squished ("tall and skinny").
		   The old code uploaded line_size_bytes*2 (the TMEM word-rounded stride,
		   e.g. 32 for a 26px glyph) which disagrees with that formula, so the
		   right columns sampled blank padding. Decode row-by-row into a buffer
		   packed at exactly `width`, reading the source at its real byte stride. */
		width  = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

		uint32_t src_stride = last_set_texture_image_width + 1; /* source row, bytes */
		uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
			.addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * src_stride) +
				  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];

		memset(rgba16_buf, 0, width * height * 2);
		uint16_t* dst = rgba16_buf;
		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {
				uint32_t b = x >> 1;
				if (b >= src_stride) {
					continue; /* trailing pad column from the +4 rect rounding */
				}
				uint8_t byte = start[b];
				uint8_t in = (x & 1) ? (byte & 0xf) : ((byte >> 4) & 0xf);
				dst[x] = (in << 12) | (in << 8) | (in << 4) | in;
			}
			start += src_stride;
			dst += width;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}

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
		/* LoadTile path: upload at the tri code's tex dims (see import_texture_i4).
		   NOTE: the only CI8 tile loader (func_80045614) is UNUSED, so this is
		   currently dead -- the live CI8 path is the block load above. Row-major
		   decode at the real source byte stride (CI8 = 1 byte/texel). */
		width  = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

		uint32_t src_stride = last_set_texture_image_width + 1; /* source row, bytes */
		uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
			.addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * src_stride) +
				  (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)];

		memset(rgba16_buf, 0, width * height * 2);
		uint16_t* dst = rgba16_buf;
		for (uint32_t h = 0; h < height; h++) {
			for (uint32_t w = 0; w < width; w++) {
				dst[w] = (w < src_stride) ? tlut[start[w]] : 0;
			}
			start += src_stride;
			dst += width;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void __attribute__((noinline)) import_texture(int tile) {
	PROF_INC(prof_texup);
	uint8_t fmt = rdp.texture_tile.fmt;
	uint8_t siz = rdp.texture_tile.siz;

	// DATA slot follows the RENDER tile's tmem block (tmem>>8 == 0/1, the two tmem halves); the
	// backend UNIT stays `tile`. This is what lets a render tile re-pointed to tmem 0x0100 (Moo Moo
	// Farm's grass<->dirt segments) sample the texture loaded there instead of always slot 0.
	int dslot = (tile == 0) ? (int)(rdp.texture_tile.tmem >> 8) : tile;
	uint32_t tmem = rdp.texture_tile.tmem;

	int cl_rv = gfx_texture_cache_lookup(tile, dslot, &rendering_state.textures[tile], rdp.loaded_texture[dslot].addr, tmem,
										 fmt, siz, rdp.texture_tile.uls, rdp.texture_tile.ult);
	if (cl_rv == 1) {
		return;
	}

	last_cl_rv = cl_rv;
#ifdef GFX_BACKEND_PVR
	// Size the VRAM buffer to the FULL loaded texture (from the LOAD: line_size_bytes/size_bytes),
	// not the possibly-squished tile, so menu squish re-uploads reshape into a fixed buffer instead
	// of free/bigger-malloc churn (which fragments VRAM -> OOM). The full dims are stable while the
	// squish only varies lrs/lrt. The backend still samples the current (squished) t->w x t->h.
	{
		uint32_t lsb = rdp.texture_tile.line_size_bytes;
		if (lsb) {
			uint32_t full_w = (siz == G_IM_SIZ_4b)  ? (lsb << 1)
			                : (siz == G_IM_SIZ_8b)  ? lsb
			                : (siz == G_IM_SIZ_16b) ? (lsb >> 1)
			                :                         (lsb >> 2);
			uint32_t full_h = rdp.loaded_texture[dslot].size_bytes / lsb;
			gfx_pvr_set_alloc_floor(full_w, full_h);
		}
	}
#endif
	// Importers read rdp.loaded_texture[<arg>] for the source data -> pass the DATA slot (dslot).
	if (fmt == G_IM_FMT_RGBA) {
		if (siz == G_IM_SIZ_16b) {
			import_texture_rgba16(dslot);
		} else if (siz == G_IM_SIZ_32b) {
			import_texture_rgba32(dslot);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_IA) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ia4(dslot);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ia8(dslot);
		} else if (siz == G_IM_SIZ_16b) {
			import_texture_ia16(dslot);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_CI) {
		if (siz == G_IM_SIZ_8b) {
			import_texture_ci8(dslot);
		} else {
//			abort();
		}
	} else if (fmt == G_IM_FMT_I) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_i4(dslot);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_i8(dslot);
		} else {
//			abort();
		}
	} else {
//		abort();
	}
}

static void gfx_normalize_vector(float v[3]) {
	shz_vec3_t norm = shz_vec3_normalize((shz_vec3_t) { .x = v[0], .y = v[1], .z = v[2] });
	v[0] = norm.x; v[1] = norm.y; v[2] = norm.z;
 }

static void gfx_transposed_matrix_mul(float res[3], const float a[3], const float b[4][4]) {
    *((shz_vec3_t *)res) = shz_matrix4x4_trans_vec3_transpose(b, *((shz_vec3_t *)a));
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

static void gfx_matrix_mul(shz_matrix_4x4_t *res, const shz_matrix_4x4_t *a, const shz_matrix_4x4_t *b) {
	shz_xmtrx_load_4x4_apply_store(res, b, a);
}

static int matrix_dirty = 0;

extern void *memcpy32(void *restrict dst, const void *restrict src, size_t bytes);
static __attribute__((noinline)) void gfx_sp_matrix(uint8_t parameters, const int32_t* addr) {
	PROF_INC(prof_mtx);
#if GFX_PROF
	uint64_t prof_t0 = PROF_NOW();
#endif
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
	shz_xmtrx_load_4x4_unaligned(saddr);
	shz_xmtrx_store_4x4(matrix);
	//n64_memcpy(matrix, saddr, sizeof(matrix));
#endif
	matrix_dirty = 1;

	if (parameters & G_MTX_PROJECTION) {
		if (parameters & G_MTX_LOAD) {
			// guOrtho leaves [3][3]==1 (w passthrough); guPerspective sets it to 0
			// (w <- -z). Only meaningful on a fresh LOAD; a lookat MUL just composes
			// onto the perspective without changing its ortho/perspective character.
			// FURTHER: distinguish a 2D OVERLAY ortho (menu/HUD) from the SKYBOX ortho.
			// Both are ortho, but the menu uses a symmetric z-range guOrtho(...,-100,100)
			// -> [3][2] = -(f+n)/(f-n) = 0, while the skybox uses guOrtho(...,0,5) ->
			// [3][2] = -1. Only overlay ortho gets lifted into the 2D screen_2d_z scheme;
			// the skybox must keep its far-plane pin (else it occludes the 3D scene).
			proj_is_ortho = (matrix[3][3] > 0.5f) && (matrix[3][2] > -0.5f);
			if (matrix[3][3] <= 0.5f) cur_frame_persp = 1;   // a perspective scene this frame
			shz_matrix_4x4_copy(rsp.P_matrix, matrix);
		} else {
			gfx_matrix_mul(rsp.P_matrix, (const float (*)[4]) matrix, (const float (*)[4]) rsp.P_matrix);
		}
	} else { // G_MTX_MODELVIEW
		if ((parameters & G_MTX_PUSH) && rsp.modelview_matrix_stack_size < 11) {
			++rsp.modelview_matrix_stack_size;
			shz_matrix_4x4_copy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
				                rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 2]);
		}
		if (parameters & G_MTX_LOAD) {
			shz_matrix_4x4_copy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix);
		} else {
			gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
							rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
		}
		rsp.lights_changed = 1;
	}
	gfx_matrix_mul(rsp.MP_matrix, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
					rsp.P_matrix);
#if GFX_PROF
	prof_t_mtx += PROF_NOW() - prof_t0;
#endif
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
	}
}

// N64 per-vertex fog coefficient from clip z/w (matches OoT's raw-PVR path and the RSP
// G_MWO_FOG math): fog = clamp(fog_mul*(z/w) + fog_offset, 0..255). Returns 0 when fog is
// off (G_FOG clear) or the vertex is behind the near plane, so non-fogged frames pay nothing.
static inline uint8_t gfx_calc_fog(float z, float w) {
    if (!(rsp.geometry_mode & G_FOG)) return 0;
    if (z < -w) return 0;
    float f = (float)rsp.fog_mul * (z * shz_fast_invf(w)) + (float)rsp.fog_offset;
    if (f > 255.0f) f = 255.0f;
    else if (f < 0.0f) f = 0.0f;
    return (uint8_t)f;
}

static void __attribute__((noinline)) gfx_sp_vertex_light(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        __builtin_prefetch(&vertices[i + 2]);   // 16B/Vtx: next line every other iteration
        const Vtx_t* v = &vertices[i].v;
        const Vtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];

        float x, y, z, w;
        // This copying in + out shit should get optimized away
        shz_vec4_t out = shz_xmtrx_trans_vec4((shz_vec4_t) { .x = v->ob[0], .y = v->ob[1], .z = v->ob[2], .w = 1.0f });
        x = out.x;
        y = out.y;
        z = out.z;
        w = out.w;

        short U = v->tc[0] * rsp.texture_scaling_factor.s >> 16;
        short V = v->tc[1] * rsp.texture_scaling_factor.t >> 16;

        int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];
        int g = rsp.current_lights[rsp.current_num_lights - 1].col[1];
        int b = rsp.current_lights[rsp.current_num_lights - 1].col[2];

        if (rsp.current_num_lights == 2) {
            float intensity;
            intensity = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0.0f, rsp.current_lights_coeffs[0][0],
                                             rsp.current_lights_coeffs[0][1], rsp.current_lights_coeffs[0][2], 0.0f);

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
            {
                register float fr8  asm ("fr8")  = vn->n[0];
                register float fr9  asm ("fr9")  = vn->n[1];
                register float fr10 asm ("fr10") = vn->n[2];
                register float fr11 asm ("fr11") = 0;
 
                dotx = recip127 * shz_dot8f(fr8, fr9, fr10, fr11, rsp.current_lookat_coeffs[0][0],
                                            rsp.current_lookat_coeffs[0][1], rsp.current_lookat_coeffs[0][2], 0);

                doty = recip127 * shz_dot8f(fr8, fr9, fr10, fr11, rsp.current_lookat_coeffs[1][0],
                                            rsp.current_lookat_coeffs[1][1], rsp.current_lookat_coeffs[1][2], 0);
            }
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

        // Invalidate the scissor outcode (gen 0 = "not computed"). gfx_sp_tri1
        // computes it lazily on first use, so single-player / unused verts pay
        // nothing and each used vertex is classified once per scissor generation.
        d->scissor_gen = 0;

        d->lit = 1; // lit path: eligible for the light×colour mix in gfx_sp_tri1

		d->x = v->ob[0];
        d->y = v->ob[1];
        d->z = v->ob[2];
        d->w = 1.0f;

        d->_x = x;
        d->_y = y;
        d->_z = z;
        d->_w = w;

        d->fog = gfx_calc_fog(z, w);
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_no(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        __builtin_prefetch(&vertices[i + 2]);   // 16B/Vtx: next line every other iteration
        const Vtx_t* v = &vertices[i].v;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];

		d->x = v->ob[0];
        d->y = v->ob[1];
        d->z = v->ob[2];
        d->w = 1.0f;

		*((shz_vec4_t *)&d->_x) = shz_xmtrx_trans_vec4(*((shz_vec4_t *)&d->x));

        d->color.r = v->cn[0];
        d->color.g = v->cn[1];
        d->color.b = v->cn[2];
	    d->color.a = v->cn[3];

        d->u = (float)(v->tc[0] * rsp.texture_scaling_factor.s >> 16);
        d->v = (float)(v->tc[1] * rsp.texture_scaling_factor.t >> 16);

        // trivial clip rejection
        d->wlt0 = (d->_w < 0);
		uint8_t cr = 0;
        cr = !(d->_z > d->_w);
		cr = (cr << 1) | !(d->_z < -d->_w);
		cr = (cr << 1) | !(d->_y >  d->_w);
		cr = (cr << 1) | !(d->_y < -d->_w);
		cr = (cr << 1) | !(d->_x >  d->_w);
		cr = (cr << 1) | !(d->_x < -d->_w);
		d->clip_rej = cr;

        // Invalidate the scissor outcode (gen 0 = "not computed"); gfx_sp_tri1
        // computes it lazily on first use (see the lit path for the rationale).
        d->scissor_gen = 0;

        d->lit = 0; // unlit path: shade is the raw vertex colour, no light mix

        d->fog = gfx_calc_fog(d->_z, d->_w);
    }
}

static void __attribute__((noinline)) gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
	// Reloaded slots carry new shade colours -> their cached combiner results are stale.
	cc_cache_invalidate((int) dest_index, (int) n_vertices);
	PROF_ADD(prof_verts, n_vertices);
#if GFX_PROF
	uint64_t prof_t0 = PROF_NOW();
#endif
	shz_xmtrx_load_4x4(&rsp.MP_matrix);

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
#if GFX_PROF
	prof_t_vtx += PROF_NOW() - prof_t0;
#endif
}

extern s32 gIsMirrorMode;

static inline float approx_recip_sign(float v) {
	return shz_fast_invf(v);
}

// ---- Software scissor: homogeneous Sutherland-Hodgman clip -------------------
// A triangle is clipped in (baked) clip space against the eye plane (w>eps) and
// the 4 scissor planes. The plane-crossing parameter t is computed in clip space
// but applied to the *object-space* attributes too: because the projection is a
// linear map of homogeneous object coords, lerp(objA,objB,t) maps exactly onto
// lerp(clipA,clipB,t). The clipped object-space verts are then fed to GL as
// usual, keeping perspective-correct texturing/shading with no re-transform error.
#define CLIP_MAX 12

static struct LoadedVertex clip_bufA[CLIP_MAX] __attribute__((aligned(32)));
static struct LoadedVertex clip_bufB[CLIP_MAX] __attribute__((aligned(32)));

static inline void clip_lerp_vertex(struct LoadedVertex* o, const struct LoadedVertex* a,
                                    const struct LoadedVertex* b, float t) {
	o->_x = a->_x + t * (b->_x - a->_x);
	o->_y = a->_y + t * (b->_y - a->_y);
	o->_z = a->_z + t * (b->_z - a->_z);
	o->_w = a->_w + t * (b->_w - a->_w);
	o->x  = a->x  + t * (b->x  - a->x);
	o->y  = a->y  + t * (b->y  - a->y);
	o->z  = a->z  + t * (b->z  - a->z);
	o->w  = 1.0f;
	o->u  = a->u  + t * (b->u  - a->u);
	o->v  = a->v  + t * (b->v  - a->v);
	o->color.r = (uint8_t) (a->color.r + t * ((float) b->color.r - (float) a->color.r));
	o->color.g = (uint8_t) (a->color.g + t * ((float) b->color.g - (float) a->color.g));
	o->color.b = (uint8_t) (a->color.b + t * ((float) b->color.b - (float) a->color.b));
	o->color.a = (uint8_t) (a->color.a + t * ((float) b->color.a - (float) a->color.a));
	o->clip_rej = 0x3f;
	o->wlt0 = 0;
	o->lit = a->lit; // inherit lit state from the source edge vertex
	o->fog = (uint8_t) (a->fog + t * ((float) b->fog - (float) a->fog));
}

// Fill the active 5 clip planes (eye + 4 scissor) as {A,B,C,D,E}:
// signed distance = A*_x + B*_y + C*_z + D*_w + E ; inside when >= 0.
static inline void clip_fill_planes(float planes[5][5]) {
	// eye guard: _w - eps >= 0
	planes[0][0] = 0.0f; planes[0][1] = 0.0f; planes[0][2] = 0.0f; planes[0][3] = 1.0f; planes[0][4] = -SCISSOR_W_EPS;
	// _x/_w >= xmin  ->  _x - xmin*_w >= 0
	planes[1][0] = 1.0f; planes[1][1] = 0.0f; planes[1][2] = 0.0f; planes[1][3] = -sc_ndc_xmin; planes[1][4] = 0.0f;
	// _x/_w <= xmax  ->  xmax*_w - _x >= 0
	planes[2][0] = -1.0f; planes[2][1] = 0.0f; planes[2][2] = 0.0f; planes[2][3] = sc_ndc_xmax; planes[2][4] = 0.0f;
	// _y/_w >= ymin
	planes[3][0] = 0.0f; planes[3][1] = 1.0f; planes[3][2] = 0.0f; planes[3][3] = -sc_ndc_ymin; planes[3][4] = 0.0f;
	// _y/_w <= ymax
	planes[4][0] = 0.0f; planes[4][1] = -1.0f; planes[4][2] = 0.0f; planes[4][3] = sc_ndc_ymax; planes[4][4] = 0.0f;
}

static inline float clip_dist(const struct LoadedVertex* v, const float p[5]) {
	return p[0] * v->_x + p[1] * v->_y + p[2] * v->_z + p[3] * v->_w + p[4];
}

static int GFX_HOT clip_against_plane(const struct LoadedVertex* in, int n, struct LoadedVertex* out, const float p[5]) {
	int m = 0;
	for (int i = 0; i < n; i++) {
		const struct LoadedVertex* cur = &in[i];
		const struct LoadedVertex* nxt = &in[(i + 1 == n) ? 0 : (i + 1)];
		float dc = clip_dist(cur, p);
		float dn = clip_dist(nxt, p);
		int inc = (dc >= 0.0f);
		int inn = (dn >= 0.0f);
		if (inc && m < CLIP_MAX) {
			out[m++] = *cur;
		}
		if ((inc != inn) && m < CLIP_MAX) {
			float t = dc / (dc - dn);
			clip_lerp_vertex(&out[m++], cur, nxt, t);
		}
	}
	return m;
}

// Clip the triangle (a,b,c) and emit the resulting polygon as a triangle fan of
// pointers into a static buffer. Returns the number of output triangles (0..6).
// clip_mask says which planes the triangle actually crosses (from the OR of the
// three vertex outcodes): we only run a Sutherland-Hodgman pass for those, so a
// typical single-edge straddle costs one pass instead of five. Convexity
// guarantees clipping a crossed plane can't push verts outside an un-crossed one.
// The eye (w>eps) plane is mapped to SC_FORCE and clipped FIRST so the scissor
// passes see w>0.
static int GFX_HOT gfx_build_clipped_fan(const struct LoadedVertex* a, const struct LoadedVertex* b,
                                 const struct LoadedVertex* c, struct LoadedVertex* out_tris[][3],
                                 uint8_t clip_mask) {
	static const uint8_t plane_bit[5] = { SC_FORCE, SC_LEFT, SC_RIGHT, SC_BOTTOM, SC_TOP };

	clip_bufA[0] = *a;
	clip_bufA[1] = *b;
	clip_bufA[2] = *c;
	int n = 3;

	float planes[5][5];
	clip_fill_planes(planes);

	struct LoadedVertex* src = clip_bufA;
	struct LoadedVertex* dst = clip_bufB;
	for (int pi = 0; pi < 5; pi++) {
		if (!(clip_mask & plane_bit[pi]))
			continue;
		n = clip_against_plane(src, n, dst, planes[pi]);
		if (n < 3)
			return 0;
		struct LoadedVertex* tmp = src;
		src = dst;
		dst = tmp;
	}

	int nt = 0;
	for (int k = 1; k + 1 < n && nt < 6; k++) {
		out_tris[nt][0] = &src[0];
		out_tris[nt][1] = &src[k];
		out_tris[nt][2] = &src[k + 1];
		nt++;
	}
	return nt;
}

#ifdef GFX_BACKEND_PVR
// ---- Raw-PVR colour combiner evaluator (OoT-style, raw N64 mux) -------------
// Evaluate cycle-0 (a-b)*c+d for colour AND alpha from the RAW G_SETCOMBINE words
// (rdp.combine_w0/w1) — NOT the compressed cc_id, which drops G_CCMUX_1 and the true
// alpha mux (that's why the kart combiner (1-ENV)*TEXEL0+PRIM blacked out). TEXEL*
// substitute to 1.0 (the PVR does the real texture modulate), so argb is the modulate
// colour. A constant additive 'd' (PRIM/ENV) is routed to the vertex oargb (added
// post-modulate via the always-on specular bit) instead of argb (which multiplies) ->
// e.g. texel*(1-ENV) + PRIM, no blowout. Replaces mat_packed on the PVR path.
// Per-slot N64 mux encodings differ (a/b/d 4-bit, c 5-bit); idx 6 == G_CCMUX_1 == 1.0.
// Derive the PVR texenv mode from the N64 1-cycle combiner (compact cc_id = rdp.combine_mode, the
// CC_ enum). Maps the combiner's texel<->colour relationship onto REPLACE/MODULATE/DECAL/
// MODULATEALPHA — replaces the per-shader-id texenv switch in the backend. 2-cycle stays on
// MODULATE for now. Ported from sm64-dc; see [[reference_pvr_texenv_derivation]].
static uint32_t derive_pvr_texenv(uint32_t cc_id) {
	int ca = (cc_id >> 0) & 7, cb = (cc_id >> 3) & 7, cc = (cc_id >> 6) & 7, cd = (cc_id >> 9) & 7;
	int aa = (cc_id >> 12) & 7, ab = (cc_id >> 15) & 7, ac = (cc_id >> 18) & 7, ad = (cc_id >> 21) & 7;
	#define CC_ISTEX(v)  ((v) == CC_TEXEL0 || (v) == CC_TEXEL1)
	#define CC_ISTEXA(v) ((v) == CC_TEXEL0A)
	int color_has_tex = CC_ISTEX(ca) || CC_ISTEX(cb) || CC_ISTEX(cc) || CC_ISTEX(cd) ||
	                    CC_ISTEXA(ca) || CC_ISTEXA(cb) || CC_ISTEXA(cc) || CC_ISTEXA(cd);
	if (!color_has_tex) return GFX_TEXENV_MODULATE;     // texel-free colour: MODULATE + oargb routing

	int color_single = (cc == CC_0) || (ca == cb);      // colour = cd
	int color_mul    = (cb == CC_0) && (cd == CC_0);    // colour = ca * cc
	int color_mix    = !color_single && (cb == cd);     // colour = lerp(cd, ca, cc)
	int alpha_single_tex = ((ac == CC_0) || (aa == ab)) && CC_ISTEX(ad);   // alpha = ad, a texel
	int alpha_uses_texel = CC_ISTEX(aa) || CC_ISTEX(ab) || CC_ISTEX(ac) || CC_ISTEX(ad);

	// Default for a texel-modulated surface. When the ALPHA mux rides the texel WITH a non-texel
	// factor (e.g. smoke: alpha = TEXEL0*PRIM), plain MODULATE (a = tex.a) drops that factor and
	// the particle stops fading. MODULATEALPHA (a = tex.a * col.a) keeps it — and collapses to
	// MODULATE when col.a == 1 (pure texel alpha), so it's a safe superset. This also matches the
	// OLD behaviour: the prior code left non-special textures at KOS pvr_poly_cxt_txr's default,
	// which is MODULATEALPHA for the TR (alpha) list.
	uint32_t mode = alpha_uses_texel ? GFX_TEXENV_MODULATEALPHA : GFX_TEXENV_MODULATE;
	if (color_mix && CC_ISTEX(ca) && CC_ISTEXA(cc)) {
		// lerp(base, TEXEL0, TEXEL0A) — the penguins, BLEND*. BUT PVR DECAL forces the FINAL
		// alpha to the face colour (col.a) and structurally can't carry a texel-based alpha, so
		// a soft particle whose alpha rides the texel (I8 smoke: intensity == alpha) would go
		// opaque. When the alpha mux is texel-driven, fall to MODULATEALPHA (a = col.a*tex.a) so
		// the soft alpha survives; texel-independent alpha (the penguins/Boo) stays DECAL.
		mode = alpha_uses_texel ? GFX_TEXENV_MODULATEALPHA : GFX_TEXENV_DECAL;
	} else if (color_single && CC_ISTEX(cd)) {
		// colour = TEXEL0; the ALPHA mux picks the mode:
		//   alpha = TEXEL0          -> REPLACE        (DECALRGBA: rgb=tex, a=tex.a)
		//   alpha = TEXEL0 * const  -> MODULATEALPHA  (DECALFADEA: a = tex.a*env.a; col.rgb==white)
		//   alpha = const, no texel -> DECAL          (DECALRGB a=shade, DECALFADE a=env)
		mode = alpha_single_tex ? GFX_TEXENV_REPLACE
		     : alpha_uses_texel  ? GFX_TEXENV_MODULATEALPHA
		                         : GFX_TEXENV_DECAL;
	} else if (color_mul && (CC_ISTEX(ca) || CC_ISTEX(cc))) {
		mode = alpha_single_tex ? GFX_TEXENV_MODULATE        // colour=TEXEL0*col, alpha=TEXEL0
		                        : GFX_TEXENV_MODULATEALPHA;  // colour=TEXEL0*col, alpha=col(*tex)
	}
	#undef CC_ISTEX
	#undef CC_ISTEXA
	return mode;
}

static inline float pvr_cc4(int mux, int ch, float tex, const float prim[4], const float env[4], const float shade[4]) {
	switch (mux) {
		case 1: case 2: return tex;               // TEXEL0/TEXEL1 (substituted per texenv mode)
		case 3: return prim[ch];
		case 4: return shade[ch];
		case 5: return env[ch];
		case 6: return 1.0f;                      // G_CCMUX_1
		default: return 0.0f;                     // 0 / COMBINED(none in cycle0) / unsupported
	}
}
static inline float pvr_cc5(int mux, int ch, float tex, const float prim[4], const float env[4], const float shade[4]) {
	switch (mux) {
		case 1: case 2: case 8: case 9: return tex;  // TEXEL0/1 colour or alpha placeholder
		case 3: return prim[ch];
		case 4: return shade[ch];
		case 5: return env[ch];
		case 6: return 1.0f;
		case 10: return prim[3];
		case 11: return shade[3];
		case 12: return env[3];
		default: return 0.0f;
	}
}
static inline float pvr_ca(int mux, const float prim[4], const float env[4], const float shade[4]) {
	switch (mux) {
		case 1: case 2: return 1.0f;
		case 3: return prim[3];
		case 4: return shade[3];
		case 5: return env[3];
		case 6: return 1.0f;
		default: return 0.0f;
	}
}

static inline void pvr_eval_combiner(uint32_t w0, uint32_t w1, const struct RGBA* sh,
                                     int textured, uint32_t mode, uint32_t* out_argb, uint32_t* out_oargb) {
	// REPLACE ignores the vertex colour (px = texel); oargb (added afterward) must be 0.
	if (mode == GFX_TEXENV_REPLACE) { *out_argb = 0xFFFFFFFFu; *out_oargb = 0; return; }

	const float prim[4]  = { rdp.prim_color.r*recip255, rdp.prim_color.g*recip255, rdp.prim_color.b*recip255, rdp.prim_color.a*recip255 };
	const float env[4]   = { rdp.env_color.r*recip255,  rdp.env_color.g*recip255,  rdp.env_color.b*recip255,  rdp.env_color.a*recip255 };
	const float shade[4] = { sh->r*recip255, sh->g*recip255, sh->b*recip255, sh->a*recip255 };
	// DECAL: PVR lerps vtx<->texel by texel.a, so the FACE colour is the combiner with the texel
	// terms ABSENT (texel->0). MODULATE/MODULATEALPHA factor the texel OUT (texel->1) to get the
	// multiplier colour. This is the half that pairs with the backend's cxt.txr.env (sTexEnv).
	const float texv = (mode == GFX_TEXENV_DECAL) ? 0.0f : 1.0f;

	// cycle-0 colour mux (a 4b, b 4b, c 5b, d 3b) and alpha mux (all 3b) — bit layout
	// matches OoT parse_combiner / MK64's color_comb extraction.
	int a = (w0 >> 20) & 0xF, b = (w1 >> 28) & 0xF, c = (w0 >> 15) & 0x1F, d = (w1 >> 15) & 0x7;
	int aa = (w0 >> 12) & 0x7, ab = (w1 >> 12) & 0x7, ac = (w0 >> 9) & 0x7, ad = (w1 >> 9) & 0x7;

	// "Colour samples the texture" — check ALL four colour slots. The d slot matters:
	// G_CC_DECALRGB is (0,0,0,TEXEL0), i.e. colour == TEXEL0 carried in d. Missing it here
	// mis-classified every decal-textured surface as colour-constant and blew it out white.
	int has_texel = (a == 1 || a == 2 || b == 1 || b == 2 ||
	                 c == 1 || c == 2 || c == 8 || c == 9 ||
	                 d == 1 || d == 2);

	// The PVR MODULATE texenv ALWAYS multiplies tex.rgb into argb when the poly is
	// textured. So argb.rgb is a MULTIPLIER on the texture, and any colour the N64
	// combiner produces that is NOT meant to be modulated by tex.rgb has to be routed to
	// the additive oargb (added post-modulate) with argb.rgb forced so MODULATE yields 0.
	// Two such cases:
	//  (1) colour mux has NO texel but the poly is textured (texture bound for ALPHA only):
	//      e.g. the over-skybox cloud (colour = PRIM, alpha = TEXEL0). MODULATE would
	//      darken the constant colour by the texture's intensity ramp -> grey halo. Route
	//      the WHOLE colour to oargb, argb.rgb = 0.
	//  (2) additive PRIM/ENV 'd' sitting on top of a TEXEL base -> route just 'd' to oargb,
	//      exclude it from the modulated argb.
	// Otherwise the colour stays in argb (e.g. the untextured white intro fade
	// G_CC_PRIMITIVE ((0-0)*0 + PRIM): no texture to modulate, so PRIM must stay in argb or
	// the fade renders BLACK).
	//
	// CRITICAL: this evaluator only decodes CYCLE 0. In a 2-cycle combiner the texel
	// modulate frequently lives in cycle 1, so cycle 0 reads "no texel" even though the
	// surface is genuinely texture-modulated — and the mandatory MODULATE is what applies
	// it. So case (1) is only trustworthy in 1-CYCLE mode; gating on it keeps every 2-cycle
	// textured surface on the old (keep-in-argb, let-MODULATE-apply-texture) path.
	int two_cycle = ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE);
	int color_const_textured = textured && !has_texel && !two_cycle;
	// DECAL keeps its additive base colour in argb (via texv=0), so don't ALSO route it to oargb.
	int offset = (mode != GFX_TEXENV_DECAL) && !color_const_textured && (d == 3 || d == 5) && has_texel; // G_CCMUX d: 3=PRIM, 5=ENV

	uint32_t col[4];
	uint32_t oarr[3] = { 0, 0, 0 };
	for (int ch = 0; ch < 3; ch++) {
		float va = pvr_cc4(a, ch, texv, prim, env, shade);
		float vb = pvr_cc4(b, ch, texv, prim, env, shade);
		float vc = pvr_cc5(c, ch, texv, prim, env, shade);
		if (color_const_textured) {
			// whole texture-independent colour -> additive offset; modulate multiplier = 0
			float v = (va - vb) * vc + pvr_cc4(d, ch, texv, prim, env, shade);
			v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
			oarr[ch] = (uint32_t)(v * 255.0f);
			col[ch]  = 0;
		} else {
			float vd = offset ? 0.0f : pvr_cc4(d, ch, texv, prim, env, shade);
			float v = (va - vb) * vc + vd;
			v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
			col[ch] = (uint32_t)(v * 255.0f);
		}
	}

	if (color_const_textured) {
		*out_oargb = PACK_ARGB8888(oarr[0], oarr[1], oarr[2], 0);
	} else if (offset) {
		const float* o = (d == 3) ? prim : env;
		*out_oargb = PACK_ARGB8888((uint32_t)(o[0]*255.0f), (uint32_t)(o[1]*255.0f), (uint32_t)(o[2]*255.0f), 0);
	} else {
		*out_oargb = 0;
	}

	float av = (pvr_ca(aa, prim, env, shade) - pvr_ca(ab, prim, env, shade)) * pvr_ca(ac, prim, env, shade)
	           + pvr_ca(ad, prim, env, shade);
	av = av < 0.0f ? 0.0f : (av > 1.0f ? 1.0f : av);
	col[3] = (uint32_t)(av * 255.0f);

	*out_argb = PACK_ARGB8888(col[0], col[1], col[2], col[3]);
}

// ---- Prepared-combiner build + fast per-vertex eval (see struct ccp) ------------------------
// The three resolvers mirror pvr_cc4 / pvr_cc5 / pvr_ca EXACTLY, mapping each mux to a
// state-constant or a shade select. Any divergence here breaks bit-identity with the generic
// evaluator — change them only in lockstep.
static struct ccp_term ccp_res4(int mux, int ch, float texv, const float prim[4], const float env[4]) {
	struct ccp_term t = { 0, 0.0f };
	switch (mux) {
		case 1: case 2: t.k = texv; break;
		case 3: t.k = prim[ch]; break;
		case 4: t.sel = 1; break;
		case 5: t.k = env[ch]; break;
		case 6: t.k = 1.0f; break;
		default: break;
	}
	return t;
}
static struct ccp_term ccp_res5(int mux, int ch, float texv, const float prim[4], const float env[4]) {
	struct ccp_term t = { 0, 0.0f };
	switch (mux) {
		case 1: case 2: case 8: case 9: t.k = texv; break;
		case 3: t.k = prim[ch]; break;
		case 4: t.sel = 1; break;
		case 5: t.k = env[ch]; break;
		case 6: t.k = 1.0f; break;
		case 10: t.k = prim[3]; break;
		case 11: t.sel = 2; break;
		case 12: t.k = env[3]; break;
		default: break;
	}
	return t;
}
static struct ccp_term ccp_resa(int mux, const float prim[4], const float env[4]) {
	struct ccp_term t = { 0, 0.0f };
	switch (mux) {
		case 1: case 2: case 6: t.k = 1.0f; break;
		case 3: t.k = prim[3]; break;
		case 4: t.sel = 2; break;
		case 5: t.k = env[3]; break;
		default: break;
	}
	return t;
}

// Build the prepared combiner for the CURRENT rdp state (same inputs as the cc key: combine
// words, prim, env, texenv+textured, cycle type). Mirrors pvr_eval_combiner's classification
// (has_texel, two_cycle, color_const_textured, offset) verbatim.
// Does (t[0]-t[1])*t[2]+t[3] reduce EXACTLY to the shade selector `ssel`? True for the two
// modulate arrangements (shade,0,1,0) and (1,0,shade,0). Float equality on the consts is
// deliberate: texv/zero terms are exact 0.0f/1.0f; a prim/env of 255 resolves to 1.00000035
// (255*recip255) and correctly fails the match — conservative misses stay on the general path.
static int ccp_terms_passthrough(const struct ccp_term t[4], uint8_t ssel) {
	// d-slot passthrough: (0-0)*0 + s == s exactly (0-0 = +0, +0*0 = +0, +0+s = s). This is
	// the STANDARD N64 idiom for shade alpha (G_ACMUX 0,0,0,SHADE — e.g. G_CC_MODULATERGB's
	// alpha) and for untextured shade colour (G_CC_SHADE). Histogram 2026-08-18: Turnpike's
	// dominant course state is modulate colour + exactly this alpha shape — the first
	// classifier version missed it (only knew the a/c-slot arrangements) and covered <7%.
	if (t[0].sel == 0 && t[0].k == 0.0f &&
	    t[1].sel == 0 && t[1].k == 0.0f &&
	    t[2].sel == 0 && t[2].k == 0.0f &&
	    t[3].sel == ssel) return 1;
	if (t[1].sel != 0 || t[1].k != 0.0f) return 0;
	if (t[3].sel != 0 || t[3].k != 0.0f) return 0;
	int a_is_s = (t[0].sel == ssel);
	int a_is_1 = (t[0].sel == 0 && t[0].k == 1.0f);
	int c_is_s = (t[2].sel == ssel);
	int c_is_1 = (t[2].sel == 0 && t[2].k == 1.0f);
	return (a_is_s && c_is_1) || (a_is_1 && c_is_s);
}

static inline void pvr_eval_combiner_terms(const struct ccp* p, const struct RGBA* sh,
                                           uint32_t* out_argb, uint32_t* out_oargb);

static void __attribute__((noinline)) pvr_cc_prepare(struct ccp* p, uint32_t w0, uint32_t w1,
                                                     int textured, uint32_t mode) {
	memset(p, 0, sizeof(*p));
	if (mode == GFX_TEXENV_REPLACE) { p->replace = 1; return; }

	const float prim[4]  = { rdp.prim_color.r*recip255, rdp.prim_color.g*recip255, rdp.prim_color.b*recip255, rdp.prim_color.a*recip255 };
	const float env[4]   = { rdp.env_color.r*recip255,  rdp.env_color.g*recip255,  rdp.env_color.b*recip255,  rdp.env_color.a*recip255 };
	const float texv = (mode == GFX_TEXENV_DECAL) ? 0.0f : 1.0f;

	int a = (w0 >> 20) & 0xF, b = (w1 >> 28) & 0xF, c = (w0 >> 15) & 0x1F, d = (w1 >> 15) & 0x7;
	int aa = (w0 >> 12) & 0x7, ab = (w1 >> 12) & 0x7, ac = (w0 >> 9) & 0x7, ad = (w1 >> 9) & 0x7;

	int has_texel = (a == 1 || a == 2 || b == 1 || b == 2 ||
	                 c == 1 || c == 2 || c == 8 || c == 9 ||
	                 d == 1 || d == 2);
	int two_cycle = ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE);
	int cct = textured && !has_texel && !two_cycle;
	int offset = (mode != GFX_TEXENV_DECAL) && !cct && (d == 3 || d == 5) && has_texel;
	p->cct = (uint8_t) cct;
	p->offset = (uint8_t) offset;

	for (int ch = 0; ch < 3; ch++) {
		p->c[ch][0] = ccp_res4(a, ch, texv, prim, env);
		p->c[ch][1] = ccp_res4(b, ch, texv, prim, env);
		p->c[ch][2] = ccp_res5(c, ch, texv, prim, env);
		// cct evaluates the FULL d; the non-cct path zeroes d when routed to oargb (offset).
		if (!cct && offset) {
			p->c[ch][3].sel = 0; p->c[ch][3].k = 0.0f;
		} else {
			p->c[ch][3] = ccp_res4(d, ch, texv, prim, env);
		}
	}
	if (offset) {
		const float* o = (d == 3) ? prim : env;
		p->oargb_const = PACK_ARGB8888((uint32_t)(o[0]*255.0f), (uint32_t)(o[1]*255.0f), (uint32_t)(o[2]*255.0f), 0);
	}
	p->a[0] = ccp_resa(aa, prim, env);
	p->a[1] = ccp_resa(ab, prim, env);
	p->a[2] = ccp_resa(ac, prim, env);
	p->a[3] = ccp_resa(ad, prim, env);

	// Shade-passthrough classification (measured 2026-08-18: cc = ~60% of bake = ~20% of the 4P
	// walk, and most course states are plain modulate). The eval collapses to argb=pack(shade),
	// oargb=0 when every channel's (va-vb)*vc+vd is EXACTLY the shade channel: term tuples
	// (s,0,1,0) or (1,0,s,0). Both are IEEE-exact identities ((s-0)*1+0 == s; (1-0)*s+0 == s;
	// s in [0,1]), and byte->float->*255->trunc roundtrips for 0..255 (recip255*255 = 1+4e-7,
	// clamp catches 255) — so the fast path is bit-identical to the term eval. Verified on HW by
	// the GFX_PROF self-check (ccpass mism counter). Conservative: any other shape stays general.
	{
		int sp = !p->replace && !p->cct && !p->offset;
		for (int ch = 0; sp && ch < 3; ch++)
			sp = ccp_terms_passthrough(p->c[ch], 1);   // sel 1 = shade[ch]
		if (sp)
			sp = ccp_terms_passthrough(p->a, 2);       // sel 2 = shade[3] (alpha)
		p->shade_pass = (uint8_t) sp;
	}

	// Constant-output classification (histogram 2026-08-18: the next ~8% after shade-pass —
	// texel-passthrough states whose texenv didn't map to REPLACE, const PRIM/ENV fills): if NO
	// term references shade, the whole result is a state constant. Evaluate the terms ONCE here
	// (the shade input is unread, so any value yields identical bits) and serve two stores per
	// vertex. Bit-exact by identity — it IS the term eval, hoisted.
	{
		int cst = !p->replace && !p->shade_pass;
		for (int ch = 0; cst && ch < 3; ch++)
			for (int j = 0; j < 4; j++)
				if (p->c[ch][j].sel != 0) { cst = 0; break; }
		if (cst)
			for (int j = 0; j < 4; j++)
				if (p->a[j].sel != 0) { cst = 0; break; }
		p->const_out = (uint8_t) cst;
		if (cst) {
			const struct RGBA dummy = { 0, 0, 0, 0 };
			pvr_eval_combiner_terms(p, &dummy, &p->c_argb, &p->c_oargb);
		}
	}
}

static inline float ccp_term_val(const struct ccp_term* t, float sch, float sa) {
	return t->sel == 0 ? t->k : (t->sel == 1 ? sch : sa);
}

// Full term evaluation (the general prepared path). Split out so the GFX_PROF self-check can
// run it against the shade-pass fast path on live vertices.
static inline void pvr_eval_combiner_terms(const struct ccp* p, const struct RGBA* sh,
                                           uint32_t* out_argb, uint32_t* out_oargb) {
	const float shade[4] = { sh->r*recip255, sh->g*recip255, sh->b*recip255, sh->a*recip255 };
	uint32_t col[4];
	uint32_t oarr[3] = { 0, 0, 0 };
	for (int ch = 0; ch < 3; ch++) {
		float sch = shade[ch];
		float va = ccp_term_val(&p->c[ch][0], sch, shade[3]);
		float vb = ccp_term_val(&p->c[ch][1], sch, shade[3]);
		float vc = ccp_term_val(&p->c[ch][2], sch, shade[3]);
		float vd = ccp_term_val(&p->c[ch][3], sch, shade[3]);
		float v = (va - vb) * vc + vd;
		v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
		if (p->cct) {
			oarr[ch] = (uint32_t)(v * 255.0f);
			col[ch] = 0;
		} else {
			col[ch] = (uint32_t)(v * 255.0f);
		}
	}

	if (p->cct)          *out_oargb = PACK_ARGB8888(oarr[0], oarr[1], oarr[2], 0);
	else if (p->offset)  *out_oargb = p->oargb_const;
	else                 *out_oargb = 0;

	float av = (ccp_term_val(&p->a[0], 0.0f, shade[3]) - ccp_term_val(&p->a[1], 0.0f, shade[3]))
	           * ccp_term_val(&p->a[2], 0.0f, shade[3]) + ccp_term_val(&p->a[3], 0.0f, shade[3]);
	av = av < 0.0f ? 0.0f : (av > 1.0f ? 1.0f : av);
	col[3] = (uint32_t)(av * 255.0f);

	*out_argb = PACK_ARGB8888(col[0], col[1], col[2], col[3]);
}

// Per-vertex fast path: identical formula/order/clamps to pvr_eval_combiner, minus all the
// state-constant work. Fog is NOT included (OR'd in by the caller, as with the generic eval).
// shade_pass states (modulate class — the bulk of course geometry) skip the term eval entirely:
// argb is just the shade bytes packed (bit-exact, see the classifier note in pvr_cc_prepare).
static inline void pvr_eval_combiner_fast(const struct ccp* p, const struct RGBA* sh,
                                          uint32_t* out_argb, uint32_t* out_oargb) {
	if (p->replace) { *out_argb = 0xFFFFFFFFu; *out_oargb = 0; return; }
	if (p->shade_pass) {
		*out_argb = PACK_ARGB8888((uint32_t) sh->r, (uint32_t) sh->g, (uint32_t) sh->b, (uint32_t) sh->a);
		*out_oargb = 0;
		return;
	}
	if (p->const_out) { *out_argb = p->c_argb; *out_oargb = p->c_oargb; return; }
	pvr_eval_combiner_terms(p, sh, out_argb, out_oargb);
}
#endif

// Coplanar-decal depth bias (N64 ZMODE_DEC). PVR z = 1/w (GEQUAL, larger == nearer); a decal
// shares its base surface's z and would z-fight on the tie. Scale 1/w slightly NEARER so the
// decal wins. Multiplicative (proportional to depth) — a flat epsilon would over/under-bias near
// vs far. Handled here in the front-end bake, so the backend's set_zmode_decal is a no-op. Tunable.
#define PVR_DECAL_ZBIAS 1.003f

// Full triangle-state derivation (see the call site in gfx_sp_tri1_impl). Cold relative to
// the per-triangle path, so NOT in the hot section.
static void __attribute__((noinline)) gfx_tri_state_setup(void) {
    tri_setup_gen = rdp_state_gen;
    PROF_INC(prof_setups);
#if GFX_PROF
    uint64_t su_t = PROF_NOW();
#endif
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

#ifdef GFX_BACKEND_PVR
    // PVR hardware vertex fog: on when the RSP G_FOG geometry mode is set (the same signal
    // that makes gfx_calc_fog produce per-vertex coefficients). The fog-colour register is
    // global/per-frame (like OoT), so per-object fog colours can't differ within a frame —
    // fine for MK64's per-course fog. Toggling fog_type dirties the header, so flush; the
    // colour register is read at render time, so it needs no flush.
    uint8_t fog_on = (rsp.geometry_mode & G_FOG) != 0;
    if (fog_on != rendering_state.fog_enabled) {
        gfx_flush();
        gfx_pvr_set_fog(fog_on);
        rendering_state.fog_enabled = fog_on;
    }
    if (fog_on) {
        uint32_t fc = PACK_ARGB8888(rdp.fog_color.r, rdp.fog_color.g, rdp.fog_color.b, rdp.fog_color.a);
        if (fc != rendering_state.fog_color) {
            gfx_pvr_set_fog_color(rdp.fog_color.r, rdp.fog_color.g, rdp.fog_color.b, rdp.fog_color.a);
            rendering_state.fog_color = fc;
        }
    }
#endif

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

#if GFX_PROF
    { uint64_t n = PROF_NOW(); prof_su[0] += n - su_t; su_t = n; }
#endif
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
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if (use_alpha != rendering_state.alpha_blend) {
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    // 3-way list routing for the raw-PVR backend (0=OP live, 1=PT bucket, 2=TR bucket):
    //   * FORCE_BL set (real translucent blend) -> TR (autosorted, composited last)
    //   * else alpha-test CUTOUT                -> PT (hard-edged, opaque pixels, depth-written)
    //   * else fully opaque                     -> OP (live)
    // A cutout is a coverage edge (texture_edge / CVG_X_ALPHA) OR a combiner whose ALPHA reads
    // a texel (the I4 menu glyphs: use_alpha==false but shape comes from the TEXTURE alpha — on
    // OP that intensity-0 background would be OPAQUE BLACK; PT alpha-tests it away with a hard
    // edge + correct depth). Alpha mux (3 bits each, as pvr_eval_combiner): 1=TEXEL0, 2=TEXEL1.
    {
        int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
        int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
        uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
                                    ac == 1 || ac == 2 || ad == 1 || ad == 2);
        // The combiner reading a texel for alpha only matters as a CUTOUT signal when the
        // hardware actually alpha-TESTS — i.e. alpha compare is enabled (G_AC_THRESHOLD/DITHER,
        // other_mode_l[1:0]). An OPAQUE surface (G_RM_*_OPA_SURF, G_AC_NONE) ignores alpha as
        // full coverage even if MODULATEIA computes a texel alpha; sending it to PT would
        // alpha-test it away when the texture's alpha bit is 0 (the Koopa Beach palm-trunk
        // RGBA16 model -> whole trunk invisible). Real PT cutouts (HUD G_AC_THRESHOLD glyphs,
        // TEX_EDGE foliage) keep their signal.
        uint8_t alpha_compare_on = (rdp.other_mode_l & 3) != 0;
        // FORCE_BL = the N64 "blend this surface against the framebuffer" flag: set on real
        // translucency (item-box glass, clouds, XLU), clear on coverage cutouts (TEX_EDGE).
        // It separates the two even when BOTH carry CVG_X_ALPHA, which the old
        // (G_BL_A_MEM<<18) heuristic could not — that misread sent cutouts to TR and, with the
        // texture_edge-first hack, alpha-tested translucent CVG_X_ALPHA glass to nothing on PT.
        uint8_t real_blend = (rdp.other_mode_l & FORCE_BL) != 0;
        uint8_t cutout     = texture_edge || (alpha_uses_texel && alpha_compare_on);
        uint8_t kind = real_blend ? 2 : (cutout ? 1 : 0);   // TR : PT : OP
        if (kind != pvr_cur_kind) {
            gfx_flush();
            gfx_pvr_set_blend(kind);
            pvr_cur_kind = kind;
        }
        if (kind == 2) {   // TR: derive + push blend factors from the N64 blender (flush on change)
            uint8_t bs, bd;
            pvr_derive_blend(rdp.other_mode_l, rdp.other_mode_h, &bs, &bd);
            if (bs != pvr_cur_bsrc || bd != pvr_cur_bdst) {
                gfx_flush();
                gfx_pvr_set_blend_factors(bs, bd);
                pvr_cur_bsrc = bs; pvr_cur_bdst = bd;
            }
        }
    }

#if GFX_PROF
    { uint64_t n = PROF_NOW(); prof_su[1] += n - su_t; su_t = n; }
#endif
    uint8_t num_inputs;
    uint8_t used_textures[2];
    gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
    int i;

    for (i = 0; i < 2; i++) {
        if (used_textures[i]) {
            if (rdp.textures_changed[i]) {
                import_texture(i);
                rdp.textures_changed[i] = 0;
            }
            uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
            if (linear_filter != rendering_state.textures[i]->linear_filter ||
                rdp.texture_tile.cms != rendering_state.textures[i]->cms ||
                rdp.texture_tile.cmt != rendering_state.textures[i]->cmt) {
                gfx_rapi->set_sampler_parameters(i, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
                rendering_state.textures[i]->linear_filter = linear_filter;
                rendering_state.textures[i]->cms = rdp.texture_tile.cms;
                rendering_state.textures[i]->cmt = rdp.texture_tile.cmt;
            }
        }
    }

#if GFX_PROF
    { uint64_t n = PROF_NOW(); prof_su[2] += n - su_t; su_t = n; }
#endif
    uint8_t use_texture = used_textures[0] || used_textures[1];
    uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
    uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
    // Fold the PoT-padding correction into the texel->[0,1] reciprocal (once per draw).
    float recip_tex_width = shz_inverse_posf((float) tex_width) * get_current_u_scale();
    float recip_tex_height = shz_inverse_posf((float) tex_height) * get_current_v_scale();

    // Texel<->colour combine DERIVED from the N64 combiner (REPLACE/MODULATE/DECAL/MODULATEALPHA),
    // replacing the backend's old per-shader-id switch. A change ends the current PVR batch (the
    // poly header carries cxt.txr.env), so flush first; the per-vertex argb below is then evaluated
    // by pvr_eval_combiner in the MATCHING mode (texv=0 for DECAL).
    uint32_t texenv = use_texture ? derive_pvr_texenv(rdp.combine_mode) : GFX_TEXENV_MODULATE;
    if (texenv != rendering_state.tex_env) {
        gfx_flush();
        gfx_rapi->set_tex_env(texenv);
        rendering_state.tex_env = texenv;
    }

    // Combiner-eval cache key (see cc_cache): covers every non-per-vertex input of
    // pvr_eval_combiner — combine words, prim, env, texenv (encodes DECAL's texv=0),
    // textured, cycle type (the two_cycle read of other_mode_h). Any change -> new stamp
    // -> each vertex re-evaluates once; unchanged state -> per-vertex cache hits below.
    {
        uint32_t k0 = rdp.combine_w0, k1 = rdp.combine_w1;
        uint32_t k2 = PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, rdp.prim_color.a);
        uint32_t k3 = PACK_ARGB8888(rdp.env_color.r, rdp.env_color.g, rdp.env_color.b, rdp.env_color.a);
        uint32_t k4 = texenv | ((uint32_t) use_texture << 8);
        uint32_t k5 = rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE);
        if (k0 != cc_last_key[0] || k1 != cc_last_key[1] || k2 != cc_last_key[2] ||
            k3 != cc_last_key[3] || k4 != cc_last_key[4] || k5 != cc_last_key[5]) {
            cc_last_key[0] = k0; cc_last_key[1] = k1; cc_last_key[2] = k2;
            cc_last_key[3] = k3; cc_last_key[4] = k4; cc_last_key[5] = k5;
            // A recently-seen state reuses its stamp (vertices cached under it hit again);
            // only a genuinely new state mints a fresh stamp.
            int found = -1;
            for (int s = 0; s < CC_STATE_SLOTS; s++) {
                if (cc_states[s].stamp != 0 &&
                    cc_states[s].key[0] == k0 && cc_states[s].key[1] == k1 &&
                    cc_states[s].key[2] == k2 && cc_states[s].key[3] == k3 &&
                    cc_states[s].key[4] == k4 && cc_states[s].key[5] == k5) {
                    found = s;
                    break;
                }
            }
            if (found >= 0) {
                cc_state_stamp = cc_states[found].stamp;
                ccp_active = &cc_states[found].prep;
                cc_active_slot = found;
            } else {
                if (++cc_next_stamp == 0) {
                    // Stamp wrap: a stale vertex entry could otherwise alias a reused stamp
                    // value under a different key. Nuke all cached results + the state table.
                    memset(cc_cache, 0, sizeof(cc_cache));
                    memset(cc_states, 0, sizeof(cc_states));
                    cc_next_stamp = 1;
                }
                cc_state_stamp = cc_next_stamp;
                int slot = (int) (cc_state_rr++ & (CC_STATE_SLOTS - 1));
                if (slot == cc_active_slot)   // never evict the slot ccp_active points into
                    slot = (int) (cc_state_rr++ & (CC_STATE_SLOTS - 1));
                cc_states[slot].key[0] = k0; cc_states[slot].key[1] = k1;
                cc_states[slot].key[2] = k2; cc_states[slot].key[3] = k3;
                cc_states[slot].key[4] = k4; cc_states[slot].key[5] = k5;
                cc_states[slot].stamp = cc_state_stamp;
                cc_states[slot].prof_ev = 0;   // slot reused by a new state: histogram count restarts
                pvr_cc_prepare(&cc_states[slot].prep, k0, k1, use_texture, texenv);
                ccp_active = &cc_states[slot].prep;
                cc_active_slot = slot;
                PROF_INC(prof_stamps);   // stamps now counts genuinely NEW states only
            }
        }
    }
    // Snapshot everything the per-triangle path consumes (valid until the next gen bump).
    ts.depth_test = depth_test; ts.zmode_decal = zmode_decal; ts.use_texture = use_texture;
    ts.texenv = texenv; ts.recip_tex_width = recip_tex_width; ts.recip_tex_height = recip_tex_height;
    ts.uls = rdp.texture_tile.uls; ts.ult = rdp.texture_tile.ult;
    ts.linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
#if GFX_PROF
    prof_su[3] += PROF_NOW() - su_t;
#endif
}

static void __attribute__((noinline)) gfx_sp_tri1_impl(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx);
static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
#if GFX_PROF
    uint64_t t0 = PROF_NOW();
    gfx_sp_tri1_impl(vtx1_idx, vtx2_idx, vtx3_idx);
    prof_t_tri += PROF_NOW() - t0;
#else
    gfx_sp_tri1_impl(vtx1_idx, vtx2_idx, vtx3_idx);
#endif
}
static void  __attribute__((noinline)) GFX_HOT gfx_sp_tri1_impl(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
    struct LoadedVertex* v1 = &rsp.loaded_vertices[vtx1_idx];
    struct LoadedVertex* v2 = &rsp.loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &rsp.loaded_vertices[vtx3_idx];
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };

	if ((v1->clip_rej | v2->clip_rej | v3->clip_rej) != 0x3f) {
        // The whole triangle lies outside the visible area
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
							return;
                        }
						break;
                    case G_CULL_BACK:
                        if (cross >= 0) {
							return;
                        }
						break;
                    case G_CULL_BOTH: {
                        // Why is this even an option?
                        return;
						}
						break;        
					}
                break;
            case 1:
                break;
        }
    }

    // Past cull/reject: this triangle is real work.
    PROF_INC(prof_tris);
#if GFX_PROF
    uint64_t pt0 = PROF_NOW();
#endif
    // Hoisted state derivation: runs only when a non-geometry opcode bumped rdp_state_gen
    // since the last triangle. Deliberately OUTSIDE the GFX_HOT section: it runs ~240x/frame
    // vs ~2200 triangle calls, and inside tri1_impl it bloated the hot block past the 8KB
    // I-cache (12.7KB measured) -> self-aliasing again.
    if (tri_setup_gen != rdp_state_gen)
        gfx_tri_state_setup();
    const uint8_t depth_test = ts.depth_test;
    // DC 4P far pull-in (dc4p_far_w == 0 in all other modes): whole tri beyond the reduced
    // far plane -> drop before scissor/clip/bake. _w is view depth, same units as gCourseFarPersp.
    if (dc4p_far_w > 0.0f && depth_test &&
        v1->_w > dc4p_far_w && v2->_w > dc4p_far_w && v3->_w > dc4p_far_w) {
        PROF_INC(prof_farcull);
        return;
    }
    const uint8_t zmode_decal = ts.zmode_decal;
    const uint8_t use_texture = ts.use_texture;
    const uint32_t texenv = ts.texenv;
    const float recip_tex_width = ts.recip_tex_width;
    const float recip_tex_height = ts.recip_tex_height;
    int i;
#if GFX_PROF
    uint64_t pt1 = PROF_NOW(); prof_t_tri_setup += pt1 - pt0;
#endif

    // Software scissor, classified from the three per-vertex scissor outcodes
    // (cheap byte ops, à la clip_rej). Refresh any outcode that went stale since
    // its vertex was loaded under an older scissor. Then: none-outside -> emit as
    // is; all-three-outside-one-edge -> drop; else clip only the crossed planes.
    // Single full-screen scissor skips all of this (single-player keeps its cost).
    static struct LoadedVertex* fan_tris[6][3];
    int n_tris;
    if (sc_is_fullscreen) {
        // PVR backend does no near-clipping, so the full-screen fast path must
        // reject/clip eye-plane straddles itself or 1/_w at emit explodes.
        if (v1->_w > SCISSOR_W_EPS && v2->_w > SCISSOR_W_EPS && v3->_w > SCISSOR_W_EPS) {
            fan_tris[0][0] = v1; fan_tris[0][1] = v2; fan_tris[0][2] = v3;
            n_tris = 1;
        } else {
            n_tris = gfx_build_clipped_fan(v1, v2, v3, fan_tris, SC_FORCE);
        }
    } else {
#if GFX_PROF
        uint64_t psc0 = PROF_NOW();
#endif
        if (v1->scissor_gen != cur_scissor_gen) { v1->scissor_oc = compute_scissor_outcode(v1->_x, v1->_y, v1->_w); v1->scissor_gen = cur_scissor_gen; }
        if (v2->scissor_gen != cur_scissor_gen) { v2->scissor_oc = compute_scissor_outcode(v2->_x, v2->_y, v2->_w); v2->scissor_gen = cur_scissor_gen; }
        if (v3->scissor_gen != cur_scissor_gen) { v3->scissor_oc = compute_scissor_outcode(v3->_x, v3->_y, v3->_w); v3->scissor_gen = cur_scissor_gen; }

        uint8_t oc_or  = v1->scissor_oc | v2->scissor_oc | v3->scissor_oc;
        uint8_t oc_and = v1->scissor_oc & v2->scissor_oc & v3->scissor_oc;
#if GFX_PROF
        prof_t_sc_oc += PROF_NOW() - psc0;   // outcode refresh + classify (every split-screen tri)
#endif

        if (oc_or == 0) {
            // fully inside every edge, none behind the eye -> emit unclipped
            fan_tris[0][0] = v1; fan_tris[0][1] = v2; fan_tris[0][2] = v3;
            n_tris = 1;
            PROF_INC(prof_sc_asis);
        } else if (oc_and & SC_EDGE_MASK) {
            // all three share an outside edge -> whole triangle is off-pane (incl.
            // off-screen past a redundant border edge: still a correct trivial reject)
            n_tris = 0;
            PROF_INC(prof_sc_rej);
        } else {
            // A behind-eye vertex's edge bits are meaningless, so when SC_FORCE is
            // present clip every plane; otherwise clip only the CROSSED, NON-REDUNDANT
            // edges (sc_active_mask drops screen-border edges — their overhang goes
            // off-screen and the PVR clips it, so the SH pass would be wasted work).
            uint8_t clip_mask = (oc_or & SC_FORCE) ? (SC_FORCE | SC_EDGE_MASK)
                                                   : (oc_or & sc_active_mask);
            if (clip_mask == 0) {
                // only redundant border edges were crossed -> emit as-is, PVR guard-bands it
                fan_tris[0][0] = v1; fan_tris[0][1] = v2; fan_tris[0][2] = v3;
                n_tris = 1;
                PROF_INC(prof_sc_asis);
            } else {
#if GFX_PROF
                uint64_t psc1 = PROF_NOW();
                n_tris = gfx_build_clipped_fan(v1, v2, v3, fan_tris, clip_mask);
                prof_t_sc_fan += PROF_NOW() - psc1;   // SH plane passes + fan build
                prof_sc_clip += 1;
                prof_sc_fanout += (uint32_t) n_tris;
#else
                n_tris = gfx_build_clipped_fan(v1, v2, v3, fan_tris, clip_mask);
#endif
            }
        }
    }

#if GFX_PROF
    uint64_t pt2 = PROF_NOW(); prof_t_clip += pt2 - pt1;
#endif
    // Ortho (2D-as-triangles) + Z-on geometry — e.g. the pause-menu glyph text — has
    // constant _w (1/w==1.0), so it would land in the 2D depth slot and get buried by the
    // dimming quad. Lift it into the 2D screen_2d_z scheme instead, advancing the same
    // counter the 2D quad path uses so text and fill quads interleave by submission order.
    // (Z-off ortho, i.e. the skybox, never reaches here — it takes the far-pin branch.)
    // Overlay-ortho is lifted to the 2D z scheme ONLY when it's a foreground overlay:
    // either Z-on (HUD/pause/menu text inheriting G_ZBUFFER), OR there's no 3D scene this
    // frame (pure menu). A Z-OFF overlay-ortho IN A 3D-scene frame is a BACKGROUND (the
    // over-skybox clouds) -> it must keep the far-pin, not be lifted near.
    int ortho_overlay = proj_is_ortho && (depth_test || !prev_frame_had_persp);
    float z2d = 0.0f;
    if (ortho_overlay && n_tris > 0) {
        screen_2d_z += 0.0005f;
        z2d = screen_2d_z;
    }
    // Far-pin DRAW-ORDER STAGGER (ported from SF64's backdrop_far_z; fixes the skybox triangle
    // slivers, e.g. KTB's blue sky band cutting through clouds): all Z-off geometry shares the far
    // slab, but an EXACT z tie between two backdrop layers (sky bands vs cloud billboards, both
    // autosort-TR) is resolved arbitrarily per tile -> per-triangle slivers. Give each successive
    // far-pinned primitive a tiny increment so the later-drawn layer (N64 painter order) wins
    // deterministically. Reset per frame; capped well below the course far plane (~1/30000).
    float rz_far = 0.0f;
    if (!ortho_overlay && !depth_test && n_tris > 0) {
        rz_far = far_pin_z;
        if (far_pin_z < 0.000025f) far_pin_z += 0.00000005f;
    }
    // No buf_vbo on PVR. OP (kind 0) bakes this source-triangle's fan into the op_emit scratch and
    // DR-submits it after the fan; PT/TR (kind 1/2) bake DIRECTLY into their deferred bucket, reserved
    // here (n_tris*3 verts). `emit` is this fan's destination; op_n is the per-fan vertex counter.
    const int op_stream = (pvr_cur_kind == 0);
    size_t op_n = 0;
#if GFX_PROF
    const int bk_sample = ((prof_tris & 7) == 0);   // 1-in-8 source tris get bake sub-timing
    uint64_t bkt = 0;
    if (bk_sample) bkt = PROF_NOW();
#endif
    dc_fast_t *emit = op_stream ? op_emit : pvr_reserve(pvr_cur_kind, (size_t) n_tris * 3);
#if GFX_PROF
    if (bk_sample) prof_t_bk_res += PROF_NOW() - bkt;
#endif
    if (!emit) n_tris = 0;   // bucket overflow -> drop this source triangle (pvr_reserve already logged)
    for (int ti = 0; ti < n_tris; ti++) {
        v1 = fan_tris[ti][0];
        v_arr[0] = fan_tris[ti][0];
        v_arr[1] = fan_tris[ti][1];
        v_arr[2] = fan_tris[ti][2];


		for (i = 0; i < 3; i++) {
			dc_fast_t * const bv = &emit[op_n];
#if GFX_PROF
			if (bk_sample) bkt = PROF_NOW();
#endif
			// Raw-PVR backend: bake final screen coords + inverse-w here (post-clip;
			// the divide can't be carried across a clip edge). _w > eps guaranteed.
			float invw = shz_inverse_posf(v_arr[i]->_w);
			bv->vert.x = sm_xscale * (v_arr[i]->_x * invw) + sm_xbias;
			bv->vert.y = sm_yscale * (v_arr[i]->_y * invw) + sm_ybias;
			// Z-buffer-disabled geometry (e.g. the ortho screen-space skybox, w=1 -> 1/w=1.0
			// which on PVR is NEAR) must not occlude the perspective scene. Pin it to the
			// far plane (tiny 1/w) so the depth-tested course always wins. (1e-5 sits behind
			// the course far-plane ~1/30000; tunable if any course geometry is farther.)
			// Overlay-ortho -> 2D z scheme (near). Otherwise perspective 1/w, or far-pin when
			// Z is off (skybox AND the over-skybox clouds, which are Z-off ortho in a 3D frame).
			bv->vert.z = ortho_overlay ? z2d
										: depth_test    ? (zmode_decal ? invw * PVR_DECAL_ZBIAS : invw)
										: rz_far;   // far slab, draw-order staggered (see above)
#if GFX_PROF
			if (bk_sample) { uint64_t t = PROF_NOW(); prof_t_bk_sm += t - bkt; bkt = t; }
#endif

			if (use_texture) {
				float u = (v_arr[i]->u - (float)(ts.uls << 3)) * 0.03125f;
				// / 32.0f;
				float v = (v_arr[i]->v - (float)(ts.ult << 3)) * 0.03125f;
				// / 32.0f;
				if (ts.linear_filter) {
					// Linear filter adds 0.5f to the coordinates
					u += 0.5f;
					v += 0.5f;
				}
				bv->texture.u = u * recip_tex_width;
				bv->texture.v = v * recip_tex_height;
			}
#if GFX_PROF
			if (bk_sample) { uint64_t t = PROF_NOW(); prof_t_bk_uv += t - bkt; bkt = t; }
#endif

			// ---- Per-vertex colour -------------------------------------------------
			// Raw-PVR: the N64 colour+alpha combiner result, cached per loaded vertex per
			// combiner state (cc_cache; key checked once per tri above). Clip-fan temporaries
			// (clip_bufA/B) fall outside the slot range -> direct eval.
			{
				uint32_t _argb, _oargb;
				int vi = (int) (v_arr[i] - rsp.loaded_vertices);
				int cacheable = vi >= 0 && vi < MAX_VERTICES + 4;
				if (cacheable && cc_cache[vi].stamp == cc_state_stamp) {
					_argb = cc_cache[vi].argb; _oargb = cc_cache[vi].oargb;
					PROF_INC(prof_hits);
				} else {
					PROF_INC(prof_evals);
					pvr_eval_combiner_fast(ccp_active, &v_arr[i]->color, &_argb, &_oargb);
#if GFX_PROF
					if (cc_active_slot >= 0) cc_states[cc_active_slot].prof_ev++;
					if (ccp_active->shade_pass || ccp_active->const_out) {
						prof_cc_pass++;
						// Self-check (sampled): the general term eval must agree BIT-EXACTLY
						// with the shade-pass shortcut on live vertex data. Any mismatch is
						// counted and screamed about in the print block.
						if (bk_sample) {
							uint32_t chk_a, chk_o;
							pvr_eval_combiner_terms(ccp_active, &v_arr[i]->color, &chk_a, &chk_o);
							if (chk_a != _argb || chk_o != _oargb) prof_cc_mism++;
						}
					}
#endif
					if (cacheable) { cc_cache[vi].stamp = cc_state_stamp; cc_cache[vi].argb = _argb; cc_cache[vi].oargb = _oargb; }
				}
				// PVR vertex fog reads the fog density from the offset-colour (oargb) ALPHA. The
				// combiner evaluator leaves that alpha 0 (only RGB carries the additive offset), so
				// OR in this vertex's fog coefficient (per-vertex, so NOT part of the cached value).
				_oargb |= (uint32_t)v_arr[i]->fog << 24;
				bv->color.packed = _argb;
				bv->pad0.vertindex = _oargb;
			}
#if GFX_PROF
			if (bk_sample) { prof_t_bk_cc += PROF_NOW() - bkt; prof_bk_samples++; }
#endif
			// Strip flags stamped at creation (TRIANGLES: EOL every 3rd) so the inline OP
			// submit can bulk-SQ the whole fan. PT/TR bucket verts get flagged at flush time.
			if (op_stream) bv->flags = (i == 2) ? VERTEX_EOL : VERTEX;
			op_n += 1;
		}
    } // for (ti) clipped-fan triangles
#if GFX_PROF
    uint64_t pt3 = PROF_NOW(); prof_t_bake += pt3 - pt2;
#endif
    // OP baked its whole fan into the scratch — stream it to the live OP list now (one inline SQ
    // ship per source triangle; the header re-emits only on state change, so same-state runs are
    // headerless).
    if (op_stream && op_n) pvr_submit_op_inline(op_emit, op_n);
#if GFX_PROF
    prof_t_submit += PROF_NOW() - pt3;
    if (op_stream) PROF_ADD(prof_op_verts, op_n);
    else if (pvr_cur_kind == 1) PROF_ADD(prof_pt_verts, op_n);
    else PROF_ADD(prof_tr_verts, op_n);
#endif
    // A real 3D (perspective) triangle has now drawn this frame -> subsequent 2D quads are
    // foreground overlays (near), not background. (Ortho 2D-as-tris like glyphs don't count.)
    if (n_tris > 0 && !proj_is_ortho)
        has_done_3d = 1;
}

#if 1
extern int first_2d;
extern void gfx_opengl_reset_frame(int r, int g, int b);
extern void gfx_opengl_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);
#ifdef GFX_BACKEND_PVR
extern void gfx_pvr_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);
#endif
static void  __attribute__((noinline)) gfx_sp_quad_2d(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, uint8_t vtx1_idx2, uint8_t vtx2_idx2,
						   uint8_t vtx3_idx2) {
	PROF_INC(prof_quads);
#if GFX_PROF
	uint64_t prof_t0 = PROF_NOW();
#endif
	gfx_flush();
	dc_fast_t* v1 = &rsp.loaded_vertices_2D[vtx1_idx];
	dc_fast_t* v2 = &rsp.loaded_vertices_2D[vtx2_idx];
	dc_fast_t* v3 = &rsp.loaded_vertices_2D[vtx3_idx];

	dc_fast_t* v12 = &rsp.loaded_vertices_2D[vtx1_idx2];
	dc_fast_t* v22 = &rsp.loaded_vertices_2D[vtx2_idx2];
	dc_fast_t* v32 = &rsp.loaded_vertices_2D[vtx3_idx2];

	dc_fast_t* v_arr[6] = { v1, v2, v3, v12, v22, v32 };

#ifdef GFX_BACKEND_PVR
	// Guaranteed-opaque 2D (same test as the q_tr classifier below) lands on the live OP
	// list, which renders BEFORE TR. For an opaque 2D overlay to cover TR geometry it must
	// WRITE DEPTH at its near screen z — otherwise TR paints right over it. Concretely: the
	// intro white fade is translucent (TR, autosorts over the logo) until its alpha hits 255,
	// at which point it's opaque -> OP -> and, being fill-mode Z-off, wrote no depth, so the
	// perspective Nintendo logo (TR) drew over it and the fade "disappeared". So force depth
	// test (GEQUAL) + write for opaque 2D; translucent 2D stays on TR and needs no force.
	uint8_t opaque_2d;
	{
		uint8_t ua = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
		if ((rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA) ua = 1;
		int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7,
		    ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
		uint8_t aut = (aa==1||aa==2||ab==1||ab==2||ac==1||ac==2||ad==1||ad==2);
		opaque_2d = !(ua || aut);
	}
#endif
	uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
	// NOTE: do NOT force depth_test for opaque_2d here. Forcing GEQUAL made opaque 2D *test*
	// depth and get rejected by nearer world geometry (split-screen HUD obscured; intro fade
	// failed at some pixels). The depth WRITE below is what the fade needs; the test stays
	// ALWAYS (for Z-off 2D) so the overlay always draws. [UNVERIFIED — revisit next session.]
	if (depth_test != rendering_state.depth_test) {
		gfx_flush();
		gfx_rapi->set_depth_test(depth_test);
		rendering_state.depth_test = depth_test;
	}

	uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
#ifdef GFX_BACKEND_PVR
	if (opaque_2d) z_upd = 1;        // write depth at the near 2D z so TR can't paint over it
#endif
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
	// Fold the PoT-padding correction into the texel->[0,1] reciprocal (once per draw).
	float recip_tex_width = shz_inverse_posf((float) tex_width) * get_current_u_scale();
	float recip_tex_height = shz_inverse_posf((float) tex_height) * get_current_v_scale();

#ifdef GFX_BACKEND_PVR
	// Same combiner-derived texel<->colour combine as the 3D path, for 2D texrects/quads.
	uint32_t texenv = use_texture ? derive_pvr_texenv(rdp.combine_mode) : GFX_TEXENV_MODULATE;
	if (texenv != rendering_state.tex_env) {
		gfx_flush();
		gfx_rapi->set_tex_env(texenv);
		rendering_state.tex_env = texenv;
	}
#endif

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
			quad_vbo[tri_num_vert].texture.u = u * recip_tex_width;
			quad_vbo[tri_num_vert].texture.v = v * recip_tex_height;
		}

#ifdef GFX_BACKEND_PVR
		// 2D uses the SAME raw-mux combiner evaluator as 3D — replacing the old
		// white-defaulting material hack. The portrait DECAL background now resolves to
		// the combiner's real colour (e.g. black) instead of white, so no vertex-colour
		// force is needed. TEXEL substituted=1; the PVR does the real texture combine.
		{
			uint32_t _argb, _oargb;
			pvr_eval_combiner(rdp.combine_w0, rdp.combine_w1, &v_arr[tri_num_vert]->color, use_texture, texenv, &_argb, &_oargb);
			quad_vbo[tri_num_vert].color.packed = _argb;
			quad_vbo[tri_num_vert].pad0.vertindex = _oargb;
		}
#else
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
			float maxc = 255.0f * shz_inverse_posf((float) max_c);
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
#endif
	}

#ifdef GFX_BACKEND_PVR
	// Classify the 2D quad with the same 3-way split as the 3D path (0=OP,1=PT,2=TR):
	// FORCE_BL (real translucent) -> TR; else alpha-test CUTOUT (coverage edge or texture-
	// driven alpha, e.g. glyphs) -> PT; else fully opaque -> OP. Alpha mux (3b): 1=TEXEL0,2=TEXEL1.
	{
		int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
		int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
		uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
		                            ac == 1 || ac == 2 || ad == 1 || ad == 2);
		// alpha-from-texel is only a CUTOUT signal when alpha compare is actually on
		// (G_AC_THRESHOLD/DITHER) — see gfx_sp_tri1. HUD cutouts set it; opaque textured
		// surfaces (G_AC_NONE) stay OP instead of being alpha-tested to nothing.
		uint8_t alpha_compare_on = (rdp.other_mode_l & 3) != 0;
		// FORCE_BL = authoritative translucent flag (see gfx_sp_tri1): real blend -> TR,
		// coverage/texture cutout -> PT, else opaque -> OP.
		uint8_t real_blend = (rdp.other_mode_l & FORCE_BL) != 0;
		uint8_t cutout     = texture_edge || (alpha_uses_texel && alpha_compare_on);
		uint8_t q_kind = real_blend ? 2 : (cutout ? 1 : 0);   // TR : PT : OP
		gfx_pvr_set_blend(q_kind);
		if (q_kind == 2) {   // TR: derive + push blend factors from the N64 blender
			uint8_t bs, bd;
			pvr_derive_blend(rdp.other_mode_l, rdp.other_mode_h, &bs, &bd);
			gfx_pvr_set_blend_factors(bs, bd);
			pvr_cur_bsrc = bs; pvr_cur_bdst = bd;
		}
		// CRITICAL: keep the 3D classifier's tracker in sync. The 2D and 3D paths share the
		// backend's sListKind, but only the 3D path (gfx_sp_tri1) gates its set_blend on
		// pvr_cur_kind. A 2D quad here sets sListKind directly yet would leave pvr_cur_kind
		// stale, so the next 3D primitive could see kind==pvr_cur_kind, skip set_blend, and
		// emit with the wrong list. (This is why the records-box glyphs once rendered as
		// opaque overlapping cells.)
		pvr_cur_kind = q_kind;
	}
	gfx_pvr_draw_triangles_2d((void*) quad_vbo, 0, use_texture);
#else
	gfx_opengl_draw_triangles_2d((void*) quad_vbo, 0, use_texture);
#endif
	// The 2D path mutates backend state (shader/blend/samplers/texenv/viewport) outside the
	// hoisted setup's view -> force the next 3D triangle to re-derive.
	rdp_state_gen++;
#if GFX_PROF
	prof_t_quad += PROF_NOW() - prof_t0;
#endif
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

	// Keep the un-truncated float rect for the clip math (rdp.viewport is uint16).
	vpf_x = x;
	vpf_y = y;
	vpf_w = width;
	vpf_h = height;

	// GLdc applies this viewport via glViewport; recompute the software-scissor NDC
	// bounds, which are expressed relative to the viewport mapping. The PVR backend
	// additionally maps NDC->pixels itself, so refresh those coefficients too.
#ifdef GFX_BACKEND_PVR
	gfx_recompute_screen_map();
#endif
	gfx_recompute_scissor_planes();

	rdp.viewport_or_scissor_changed = 1;
}

static void  gfx_sp_movemem(uint8_t index, const void* data) {
	switch (index) {
		case G_MV_VIEWPORT:
			gfx_calc_and_set_viewport((const Vp_t*) data);
			break;
		case G_MV_L0:
		case G_MV_L1:
		case G_MV_L2:
			// NOTE: reads out of bounds if it is an ambient light
			n64_memcpy(rsp.current_lights + (index - G_MV_L0) / 2, data, sizeof(Light_t));
#if LIGHT_DEBUG
			// Dump the RAW source bytes for the egg's directional light: settles
			// source-has-no-dir vs copy-drops-dir. col@0-2, colc@4-6, dir@8-10.
			// Gate on the egg's exact diffuse (255,254,254) -- NOT just col[0]==255,
			// which the title screen's white lights also hit -- and dedup distinct
			// 12-byte patterns so a per-frame-repeating light can't exhaust the cap
			// before the egg loads.
			{
				const uint8_t* lb = (const uint8_t*) data;
				if (index == G_MV_L0 && lb[0] == 255 && lb[1] == 254 && lb[2] == 254) {
					static uint8_t seen[16][12];
					static int seen_n = 0;
					int found = 0;
					for (int q = 0; q < seen_n; q++) {
						if (memcmp(seen[q], lb, 12) == 0) { found = 1; break; }
					}
					if (!found && seen_n < 16) {
						memcpy(seen[seen_n++], lb, 12);
						printf("LIGHTSRC idx=%d src=%p raw16=[%02x %02x %02x %02x %02x %02x %02x %02x "
						       "%02x %02x %02x %02x %02x %02x %02x %02x] sizeofLight=%d\n",
						       index, (void*) data, lb[0], lb[1], lb[2], lb[3], lb[4], lb[5], lb[6], lb[7],
						       lb[8], lb[9], lb[10], lb[11], lb[12], lb[13], lb[14], lb[15], (int) sizeof(Light));
					}
				}
			}
#endif
			break;
		default:
			break;
	}
}
int16_t fog_mul;
int16_t fog_ofs;
static void  gfx_sp_moveword(uint8_t index, uint32_t data) {
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

// SH4 ABI: only r4-r7 carry integer args; the 5th+ go through the stack. Handlers below are
// trimmed to the arguments they actually read (sf64-dc shape) so no gfx_run_dl dispatch call
// spills to the stack, and the C0/C1 field extraction happens in the (non-hot) handlers.
static void gfx_sp_texture(uint16_t sc, uint16_t tc) {
	rsp.texture_scaling_factor.s = sc;
	rsp.texture_scaling_factor.t = tc;
}

static void gfx_dp_set_scissor(uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry) {
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

	// Keep the un-truncated float rect for the clip math (rdp.scissor is uint16).
	scf_x = x;
	scf_y = y;
	scf_w = width;
	scf_h = height;

	// Scissoring is done in software (homogeneous clip in gfx_sp_tri1) instead of
	// glScissor / PVR userclip; recompute the NDC bounds relative to the viewport.
	gfx_recompute_scissor_planes();

	rdp.viewport_or_scissor_changed = 1;
}

static void gfx_dp_set_texture_image(uint32_t size, uint32_t width, const void* addr) {
	rdp.texture_to_load.addr = segmented_to_virtual((void*)addr);
	rdp.texture_to_load.siz = size;
	last_set_texture_image_width = width;
}

// G_SETTILE handler takes the two raw command words (12 extracted args would put 8 on the
// stack per call — texture-load triplets are among the most frequent DL opcodes). Field
// extraction happens here, out of the GFX_HOT dispatch loop. (sf64-dc's gfx_dp_set_tile2 shape;
// body semantics unchanged — MK64 keys loaded_texture by tmem BLOCK, see comment below.)
#define C0alt(pos, width) ((w0 >> (pos)) & ((1U << width) - 1))
#define C1alt(pos, width) ((w1 >> (pos)) & ((1U << width) - 1))
static void  gfx_dp_set_tile(uint32_t w0, uint32_t w1) {
	uint8_t  fmt   = C0alt(21, 3);
	uint32_t siz   = C0alt(19, 2);
	uint32_t line  = C0alt(9, 9);
	uint32_t tmem  = C0alt(0, 9);
	uint8_t  tile  = C1alt(24, 3);
	uint32_t cmt   = C1alt(18, 2);
	uint32_t maskt = C1alt(14, 4);
	uint32_t cms   = C1alt(8, 2);
	uint32_t masks = C1alt(4, 4);
	// palette (C1(20,4)), shiftt (C1(10,4)), shifts (C1(0,4)) are not consumed by this port.
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
		rdp.texture_tile.tmem = tmem;   // render tile's tmem -> selects loaded_texture[tmem>>8]
		rdp.textures_changed[0] = 1;
		rdp.textures_changed[1] = 1;
	}

	// Index the loaded_texture table by TMEM BLOCK (tmem>>8) for ALL loads, not the tile index: a
	// texture loaded via a non-loadtile (e.g. Moo Moo Farm's grass<->dirt 2nd texture at tmem 0x0100,
	// loaded through tile 6) must land in the slot the render tile will read. tmem maxes at 0x1FF, so
	// this is just 0 or 1 (the two tmem halves) — matching the two TEXEL units.
	rdp.texture_to_load.tile_number = tmem >> 8;
	rdp.texture_to_load.tmem = tmem;
}

// Render-tile check lives at the call site (sf64-dc shape): 4 args, all in registers.
static void  gfx_dp_set_tile_size(uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
	rdp.texture_tile.uls = uls;
	rdp.texture_tile.ult = ult;
	rdp.texture_tile.lrs = lrs;
	rdp.texture_tile.lrt = lrt;
	rdp.textures_changed[0] = 1;
	rdp.textures_changed[1] = 1;
}


extern uint16_t common_tlut_hud_type_C_rank_font[];
extern uint16_t common_tlut_finish_line_banner[];
static void  __attribute__((noinline)) gfx_dp_load_tlut(UNUSED uint32_t high_index) {
	rdp.palette = rdp.texture_to_load.addr;
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
			c1 = __builtin_bswap16(c1);
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
			c2 = __builtin_bswap16(c2);
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
}

static void  gfx_dp_load_block(uint32_t lrs) {
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

static void gfx_dp_load_tile(uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
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
#ifndef GFX_BACKEND_PVR
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 640, 480, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
#endif
}

void  __attribute__((noinline)) gfx_opengl_reset_projection(void) {
#ifndef GFX_BACKEND_PVR
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((const float*) rsp.P_matrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
#endif
}
#endif

float screen_2d_z;

static void  __attribute__((noinline)) gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
	uint32_t saved_other_mode_h = rdp.other_mode_h;
	uint32_t cycle_type = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

	if (cycle_type == G_CYC_COPY) {
		rdp.other_mode_h = (rdp.other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT;
	}

	// U10.2 coordinates
	float ulxf = ulx;
	float ulyf = uly;
	float lrxf = lrx;
	float lryf = lry;

	ulxf = ulxf / (4.0f * HALF_SCREEN_WIDTH) - 1.0f;
	ulyf = (ulyf / (4.0f * HALF_SCREEN_HEIGHT)) - 1.0f;
	lrxf = lrxf / (4.0f * HALF_SCREEN_WIDTH) - 1.0f;
	lryf = (lryf / (4.0f * HALF_SCREEN_HEIGHT)) - 1.0f;

	ulxf = (ulxf * 320) + 320;
	lrxf = (lrxf * 320) + 320;

	ulyf = (ulyf * 240) + 240;
	lryf = (lryf * 240) + 240;

	dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
	dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
	dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
	dc_fast_t* ur = &rsp.loaded_vertices_2D[3];
	screen_2d_z += 0.0005f;
	float rz = screen_2d_z;
#ifdef GFX_BACKEND_PVR
	// A TEXTURED 2D quad drawn BEFORE any 3D, in a frame that HAS a 3D scene, is a backdrop
	// (the title bg strips behind the perspective checkered flag) -> far-pin it. Excludes
	// FILLRECTS (do_fill_rect): a solid-colour fill drawn before 3D is an OVERLAY/effect (the
	// intro white fade over the spinning logo), which must stay near. Once 3D has drawn, or
	// there's no 3D scene (pure menu, prev_frame_had_persp false), 2D stays near (the logo).
	if (prev_frame_had_persp && !has_done_3d && !do_fill_rect)
		rz = 0.00001f;
	do_fill_rect = 0;   // consume per-rect (it isn't reliably reset elsewhere)
#endif
	ul->vert.x = ulxf;
	ul->vert.y = ulyf;
	ul->vert.z = rz;

	ll->vert.x = ulxf;
	ll->vert.y = lryf;
	ll->vert.z = rz;

	lr->vert.x = lrxf;
	lr->vert.y = lryf;
	lr->vert.z = rz;

	ur->vert.x = lrxf;
	ur->vert.y = ulyf;
	ur->vert.z = rz;

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

static void  __attribute__((noinline)) gfx_dp_texture_rectangle2(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry,
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

	gfx_draw_rectangle(ulx, uly, lrx, lry);

	do_fill_rect = 0;
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

// G_TEXRECT front half (sf64-dc shape): takes the raw 3-word command and parses it here, out of
// the GFX_HOT dispatch loop, then calls the 9-arg body — the only remaining stack-spilling call,
// once per rect instead of extraction code bloating gfx_run_dl. `cmd` param shadows the macro use.
static void __attribute__((noinline)) gfx_dp_texture_rectangle(Gfx* cmd, uint8_t flip) {
	int32_t lrx, lry, ulx, uly;
	int16_t uls, ult, dsdx, dtdy;
	lrx = C0(12, 12);
	lry = C0(0, 12);
	ulx = C1(12, 12);
	uly = C1(0, 12);
	++cmd;
	uls = C1(16, 16);
	ult = C1(0, 16);
	++cmd;
	dsdx = C1(16, 16);
	dtdy = C1(0, 16);
	gfx_dp_texture_rectangle2(ulx, uly, lrx, lry, uls, ult, dsdx, dtdy, flip);
}

int title_backdrop = 0;
int use_one_inv = 0;
int depth_off = 0;
static void  __attribute__((noinline)) GFX_HOT gfx_run_dl(Gfx* cmd) {
	//printf("starting at %08x\n", cmd);

	cmd = seg_addr((uintptr_t) cmd);
	for (;;) {
		// DL stream prefetch: commands are consumed sequentially; pull the line 4 cmds
		// (32B) ahead so the dispatch never waits on a compulsory miss.
		__builtin_prefetch(cmd + 4);
		uint32_t opcode = cmd->words.w0 >> 24;

		// Any opcode except pure geometry (TRI/QUAD/VTX/MTX/POPMTX/DL/ENDDL) and the 2D rect
		// ops can change triangle state -> bump the setup generation so the next triangle
		// re-derives (gfx_tri_state_setup hoist). The 2D quad path bumps for itself (it
		// mutates backend state directly), so the rect opcodes stay off this list.
		{
			uint8_t op8 = (uint8_t) opcode;
			if (op8 != (uint8_t) G_TRI1 && op8 != (uint8_t) G_TRI2 && op8 != (uint8_t) G_QUAD &&
			    op8 != (uint8_t) G_VTX && op8 != (uint8_t) G_MTX && op8 != (uint8_t) G_POPMTX &&
			    op8 != (uint8_t) G_DL && op8 != (uint8_t) G_ENDDL &&
			    op8 != (uint8_t) G_TEXRECT && op8 != (uint8_t) G_TEXRECTFLIP &&
			    op8 != (uint8_t) G_FILLRECT && op8 != (uint8_t) G_SETFILLCOLOR)
				rdp_state_gen++;
		}

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
			} else if(cmd->words.w1 == 0x46554360) {
				title_backdrop ^= 1;
			} else if (cmd->words.w1 == 0xA6A7A8A9) {
				fix_flash ^= 1;
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
				gfx_sp_movemem(C0(0, 8), seg_addr(cmd->words.w1));
#else
				gfx_sp_movemem(C0(16, 8), seg_addr(cmd->words.w1));
#endif
				break;

			case (uint8_t) G_MOVEWORD:
#ifdef F3DEX_GBI_2
				gfx_sp_moveword(C0(16, 8), cmd->words.w1);
#else
				gfx_sp_moveword(C0(0, 8), cmd->words.w1);
#endif
				break;

			case (uint8_t) G_TEXTURE:
#ifdef F3DEX_GBI_2
				gfx_sp_texture(C1(16, 16), C1(0, 16));
#else
				gfx_sp_texture(C1(16, 16), C1(0, 16));
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
				gfx_dp_set_texture_image(C0(19, 2), C0(0, 10), cmd->words.w1);
				break;

			case G_LOADBLOCK:
				gfx_dp_load_block(C1(12, 12));
				break;

			case G_LOADTILE:
				gfx_dp_load_tile(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_SETTILE:
				gfx_dp_set_tile(cmd->words.w0, cmd->words.w1);
				break;

			case G_SETTILESIZE:
				if (C1(24, 3) == G_TX_RENDERTILE)
					gfx_dp_set_tile_size(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_LOADTLUT:
				gfx_dp_load_tlut(C1(14, 10));
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
#ifdef GFX_BACKEND_PVR
				rdp.combine_w0 = cmd->words.w0;   // raw mux for the faithful PVR eval
				rdp.combine_w1 = cmd->words.w1;
#endif
				gfx_dp_set_combine_mode(color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)),
										color_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)));
				/*color_comb(C0(5, 4), C1(24, 4), C0(0, 5), C1(6, 3)),
				color_comb(C1(21, 3), C1(3, 3), C1(18, 3), C1(0, 3)));*/
				break;

			// G_SETPRIMCOLOR, G_CCMUX_PRIMITIVE, G_ACMUX_PRIMITIVE, is used by Goddard
			// G_CCMUX_TEXEL1, LOD_FRACTION is used in Bowser room 1

			case G_TEXRECT:
			case G_TEXRECTFLIP: {
				// Raw 3-word command parsed inside the noinline wrapper (out of the hot block).
				// (Old F3DEX_GBI_2E parsing branch dropped — never defined in this build.)
				Gfx* texrectCmd = cmd;
				++cmd;
				++cmd;
				gfx_dp_texture_rectangle(texrectCmd, opcode == G_TEXRECTFLIP);
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
				gfx_dp_set_scissor(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
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

#if GFX_PROF || GFX_PROF_LITE
	perf_cntr_timer_enable();   // PRFC0 elapsed CPU cycles for the GFXPROF/GFXLITE timers
#endif
#if GFX_PROF
	perf_cntr_start(PRFC1, prof_stall_modes[0].mode, PMCR_COUNT_CPU_CYCLES);
#endif
}

struct GfxRenderingAPI* gfx_get_current_rendering_api(void) {
	return gfx_rapi;
}

void gfx_start_frame(void) {
    far_pin_z = 0.00001f;   // far-pin stagger is per-frame (see gfx_sp_tri1)
	gfx_wapi->handle_events();
}

void gfx_run(Gfx* commands) {
	gfx_sp_reset();

	// DC 4P perf: pulled-in far plane. Depth-tested tris with ALL THREE verts beyond
	// gCourseFarPersp * scale are dropped before clip/bake (see gfx_sp_tri1_impl).
	// 0 disables (all other modes). Backdrops/Z-off decor are exempt via depth_test.
	{
		extern f32 gCourseFarPersp;
		extern s32 gPlayerCountSelection1;
		extern s32 gActiveScreenMode;
		extern s16 gCurrentCourseId;
		dc4p_far_w = ((gPlayerCountSelection1 == 4) && (gActiveScreenMode == 3 /* 3P/4P split */))
		                 ? gCourseFarPersp * dc4p_course_far_scale(gCurrentCourseId) : 0.0f;
	}

	if (!gfx_wapi->start_frame()) {
		dropped_frame = 1;
		return;
	}

	dropped_frame = 0;

#if GFX_PROF_LITE && !GFX_PROF
	{
		uint64_t now = PROF_NOW();
		if (lite_last) {
			uint64_t per = now - lite_last;
			lite_per_sum += per;
			if (per > lite_per_max) lite_per_max = per;
			if (per > 40000u * 200u) lite_late++;   // blew the 30Hz deadline
		}
		lite_last = now;
	}
#endif
	gfx_rapi->start_frame();
	rdp_state_gen++;   // per-frame: backend headers reset in start_frame -> re-derive on first tri
#if GFX_PROF
	uint64_t t0 = PROF_NOW();
	uint64_t st0 = perf_cntr_count(PRFC1);
#endif
#if GFX_PROF_LITE && !GFX_PROF
	uint64_t lt0 = PROF_NOW();
#endif
	gfx_run_dl(commands);
#if GFX_PROF_LITE && !GFX_PROF
	{
		uint64_t lw = PROF_NOW() - lt0;
		lite_walk_sum += lw;
		if (lw > lite_walk_max) lite_walk_max = lw;
		if (++lite_n >= 60) {
			printf("GFXLITE per avg=%luus max=%luus late=%lu/60 | walk avg=%luus max=%luus\n",
			       PROF_US(lite_per_sum / lite_n), PROF_US(lite_per_max), (unsigned long) lite_late,
			       PROF_US(lite_walk_sum / lite_n), PROF_US(lite_walk_max));
			lite_per_sum = lite_per_max = lite_walk_sum = lite_walk_max = 0;
			lite_n = lite_late = 0;
		}
	}
#endif
#if GFX_PROF
	uint64_t t1 = PROF_NOW();
	prof_stall_walk = perf_cntr_count(PRFC1) - st0;
#endif
	gfx_flush();
	gfx_rapi->end_frame();
#if GFX_PROF
	prof_t_walk = t1 - t0; prof_t_flush = PROF_NOW() - t1;
#endif
	gfx_wapi->swap_buffers_begin();
}

void gfx_end_frame(void) {
	if (!dropped_frame) {
#if GFX_PROF
		uint64_t t3 = PROF_NOW();
#endif
		gfx_rapi->finish_render();
#if GFX_PROF
		prof_t_finish = PROF_NOW() - t3;
#endif
		gfx_wapi->swap_buffers_end();
	}
#if GFX_PROF
	prof_frame++;
	int prof_slow = prof_t_walk > (uint64_t) GFX_PROF_SLOW_US * 200u;
	if ((prof_frame % 60) == 0 || prof_slow) {
		printf("GFXPROF f=%lu walk=%luus flush=%luus finish=%luus tris=%lu verts=%lu mtx=%lu texup=%lu "
		       "op=%lu pt=%lu tr=%lu | vtx=%luus tri=%luus (setup=%luus clip=%luus bake=%luus submit=%luus) "
		       "evals=%lu hits=%lu stamps=%lu setups=%lu su=%lu/%lu/%lu/%lu quads=%lu q=%luus mtx=%luus fcull=%lu\n",
		       (unsigned long) prof_frame, PROF_US(prof_t_walk), PROF_US(prof_t_flush), PROF_US(prof_t_finish),
		       (unsigned long) prof_tris, (unsigned long) prof_verts, (unsigned long) prof_mtx,
		       (unsigned long) prof_texup, (unsigned long) prof_op_verts, (unsigned long) prof_pt_verts,
		       (unsigned long) prof_tr_verts, PROF_US(prof_t_vtx), PROF_US(prof_t_tri),
		       PROF_US(prof_t_tri_setup), PROF_US(prof_t_clip), PROF_US(prof_t_bake), PROF_US(prof_t_submit),
		       (unsigned long) prof_evals, (unsigned long) prof_hits, (unsigned long) prof_stamps,
		       (unsigned long) prof_setups, PROF_US(prof_su[0]), PROF_US(prof_su[1]), PROF_US(prof_su[2]), PROF_US(prof_su[3]),
		       (unsigned long) prof_quads, PROF_US(prof_t_quad), PROF_US(prof_t_mtx),
		       (unsigned long) prof_farcull);
		printf("GFXPROF   stall %s = %luus of walk\n",
		       prof_stall_modes[prof_stall_idx].name, PROF_US(prof_stall_walk));
		// Software-scissor split: scoc (outcode+classify, every split tri) + scfan (SH clip+fan)
		// should ~= the clip= bucket in split-screen; n = asis/rej/clip/fanout tri outcomes.
		printf("GFXPROF   scissor oc=%luus fan=%luus n=%lu/%lu/%lu/%lu (asis/rej/clip/fanout)\n",
		       PROF_US(prof_t_sc_oc), PROF_US(prof_t_sc_fan),
		       (unsigned long) prof_sc_asis, (unsigned long) prof_sc_rej,
		       (unsigned long) prof_sc_clip, (unsigned long) prof_sc_fanout);
		// Bake split, 1-in-8 sampled, printed x8 (estimate of the full bucket's composition).
		// bake= minus (res+sm+uv+cc) = loop/flag/store overhead + sampling error.
		printf("GFXPROF   bake~ res=%luus sm=%luus uv=%luus cc=%luus (x8 est, n=%lu sampled verts) "
		       "ccpass=%lu/%lu\n",
		       PROF_US(prof_t_bk_res * 8), PROF_US(prof_t_bk_sm * 8),
		       PROF_US(prof_t_bk_uv * 8), PROF_US(prof_t_bk_cc * 8),
		       (unsigned long) prof_bk_samples,
		       (unsigned long) prof_cc_pass, (unsigned long) prof_evals);
		if (prof_cc_mism)
			printf("GFXPROF   !!! ccpass SELF-CHECK MISMATCH n=%lu — shade-pass classifier is WRONG\n",
			       (unsigned long) prof_cc_mism);
		// Combiner-state histogram: the top states by eval count this period, with the raw
		// G_SETCOMBINE words and packed prim/env — the ground truth for designing eval fast
		// paths (the shade-pass guess measured <7% coverage; these words say what dominates).
		for (int t = 0; t < 4; t++) {
			int best = -1; uint32_t bn = 0;
			for (int s = 0; s < CC_STATE_SLOTS; s++)
				if (cc_states[s].prof_ev > bn) { bn = cc_states[s].prof_ev; best = s; }
			if (best < 0) break;
			printf("GFXPROF   ccstate w0=%08lx w1=%08lx prim=%08lx env=%08lx evals=%lu pass=%d k4=%03lx\n",
			       (unsigned long) cc_states[best].key[0], (unsigned long) cc_states[best].key[1],
			       (unsigned long) cc_states[best].key[2], (unsigned long) cc_states[best].key[3],
			       (unsigned long) bn,
			       (int) (cc_states[best].prep.shade_pass | (cc_states[best].prep.const_out << 1)),
			       (unsigned long) cc_states[best].key[4]);
			cc_states[best].prof_ev = 0;   // exclude from the next pick
		}
		for (int s = 0; s < CC_STATE_SLOTS; s++) cc_states[s].prof_ev = 0;
	}
	prof_tris = prof_verts = prof_mtx = prof_texup = 0;
	prof_t_vtx = prof_t_tri = prof_t_tri_setup = prof_t_clip = prof_t_bake = prof_t_submit = 0;
	prof_evals = prof_hits = prof_stamps = prof_setups = 0;
	prof_t_quad = prof_t_mtx = 0; prof_quads = 0;
	prof_op_verts = prof_pt_verts = prof_tr_verts = 0;
	prof_farcull = 0;
	prof_t_sc_oc = prof_t_sc_fan = 0;
	prof_sc_asis = prof_sc_rej = prof_sc_clip = prof_sc_fanout = 0;
	prof_t_bk_res = prof_t_bk_sm = prof_t_bk_uv = prof_t_bk_cc = 0;
	prof_bk_samples = 0;
	prof_cc_pass = prof_cc_mism = 0;
	prof_su[0] = prof_su[1] = prof_su[2] = prof_su[3] = 0;
	// Rotate the stall mode after every PRINT (1/sec or slow-frame), not on a fixed frame
	// stride: 60-frame prints x 8-frame rotation sampled only 2 of the 5 modes once walks
	// stopped crossing the slow threshold. Now each successive print shows the next mode —
	// full sweep every 5 seconds.
	if ((prof_frame % 60) == 0 || prof_slow) {
		prof_stall_idx = (prof_stall_idx + 1) % (int) (sizeof(prof_stall_modes) / sizeof(prof_stall_modes[0]));
		perf_cntr_stop(PRFC1); perf_cntr_clear(PRFC1);
		perf_cntr_start(PRFC1, prof_stall_modes[prof_stall_idx].mode, PMCR_COUNT_CPU_CYCLES);
	}
#endif
}

