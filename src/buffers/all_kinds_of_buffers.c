#include <stdint.h>
#include "common_structs.h"
#include "main.h"
#include "buffer_sizes.h"
// uncompressed common data segment size
uint8_t __attribute__((aligned(32))) COMMON_BUF[COMMON_BUF_SIZE];
// wiggle room for ghost textures in Banshee Boardwalk
uint8_t __attribute__((aligned(32))) SEG3_BUF[SEG3_BUF_SIZE];
// largest observed value -- Bowser's Castle
uint8_t __attribute__((aligned(32))) SEG4_BUF[SEG4_BUF_SIZE];
// largest *tex.bin -- Luigi's Raceway
uint8_t __attribute__((aligned(32))) SEG5_BUF[SEG5_BUF_SIZE];

// DO NOT MAKE THIS SMALLER
// THERE IS A BUFFER OVERWRITE ISSUE AND IT CORRUPTS SEG5_BUF
uint8_t __attribute__((aligned(32))) CEREMONY_ACTOR_BUF[CEREMONY_ACTOR_BUF_SIZE]; 

// SAVE SPACE BY REUSING THIS FOR STARTUP
uint8_t __attribute__((aligned(32))) OTHER_BUF[OTHER_BUF_SIZE];

uint8_t __attribute__((aligned(32))) COURSE_BUF[COURSE_BUF_SIZE];
uint8_t __attribute__((aligned(32))) UNPACK_BUF[UNPACK_BUF_SIZE];
uint8_t __attribute__((aligned(32))) CEREMONY_BUF[CEREMONY_BUF_SIZE];////36232];
uint8_t __attribute__((aligned(32))) COMP_VERT_BUF[COMP_VERT_BUF_SIZE];
uint8_t __attribute__((aligned(32))) DECOMP_VERT_BUF[DECOMP_VERT_BUF_SIZE];

uint16_t __attribute__((aligned(32))) colls[colls_SIZE];
CollisionTriangle __attribute__((aligned(32))) allColTris[allColTris_SIZE];

struct __attribute__((aligned(32))) GfxPool gGfxPools[2];

uint8_t __attribute__((aligned(32))) backing_gCourseOutline[OUTLINE_BUF_COUNT][OUTLIZE_BUF_SIZE];
uint8_t __attribute__((aligned(32))) backing_gMenuTextureBuffer[MENUTEX_BUF_SIZE];
uint8_t __attribute__((aligned(32))) backing_gMenuCompressedBuffer[MENUCOMP_BUF_SIZE];
uint8_t __attribute__((aligned(32))) backing_sTKMK00_LowResBuffer[LOWRES_BUF_SIZE];
uint8_t __attribute__((aligned(32))) backing_gSomeDLBuffer[SOMEDL_BUF_SIZE];

int16_t audio_buffer[AUDIOBUF_SIZE] __attribute__((aligned(64)));

int must_inval_bg;
int stupid_fucking_faces_hack;


extern u8 d_course_koopa_troopa_beach_palm_frond[];
extern Vtx d_course_koopa_troopa_beach_unknown_model4[];
extern Vtx d_course_koopa_troopa_beach_unknown_model5[];
extern Vtx d_course_koopa_troopa_beach_unknown_model6[];
extern Lights1 d_course_koopa_troopa_beach_light2;
extern Lights1 d_course_koopa_troopa_beach_light3;
extern Lights1 d_course_koopa_troopa_beach_light4;
// 0x18870
Gfx l_d_course_koopa_troopa_beach_dl_18870[] = {
    gsSPSetLights1(d_course_koopa_troopa_beach_light3),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
	    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x00FC, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, d_course_koopa_troopa_beach_palm_frond),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 2047, 128),
    gsSPVertex(d_course_koopa_troopa_beach_unknown_model5, 16, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(0, 2, 3, 0),
    gsSP1Triangle(4, 5, 6, 0),
    gsSP1Triangle(4, 6, 7, 0),
    gsSP1Triangle(8, 9, 10, 0),
    gsSP1Triangle(8, 10, 11, 0),
    gsSP1Triangle(12, 13, 14, 0),
    gsSP1Triangle(12, 14, 15, 0),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_18938[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_18870),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_tree_top2[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_18938),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_18520[] = {
    gsSPSetLights1(d_course_koopa_troopa_beach_light2),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x00FC, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, d_course_koopa_troopa_beach_palm_frond),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 2047, 128),
    gsSPVertex(d_course_koopa_troopa_beach_unknown_model4, 16, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(0, 2, 3, 0),
    gsSP1Triangle(4, 5, 6, 0),
    gsSP1Triangle(4, 6, 7, 0),
    gsSP1Triangle(8, 9, 10, 0),
    gsSP1Triangle(8, 10, 11, 0),
    gsSP1Triangle(12, 13, 14, 0),
    gsSP1Triangle(12, 14, 15, 0),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_185E8[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_18520),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_tree_top1[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_185E8),
    gsSPEndDisplayList(),
};

// 0x18BC0
Gfx l_d_course_koopa_troopa_beach_dl_18BC0[] = {
    gsSPSetLights1(d_course_koopa_troopa_beach_light4),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
	gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x00FC, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,d_course_koopa_troopa_beach_palm_frond),// common_texture_item_box_question_mark),//
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 2047, 128),
    gsSPVertex(d_course_koopa_troopa_beach_unknown_model6, 16, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(0, 2, 3, 0),
    gsSP1Triangle(4, 5, 6, 0),
    gsSP1Triangle(4, 6, 7, 0),
    gsSP1Triangle(8, 9, 10, 0),
    gsSP1Triangle(8, 10, 11, 0),
    gsSP1Triangle(12, 13, 14, 0),
    gsSP1Triangle(12, 14, 15, 0),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_18C88[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_18BC0),
    gsSPEndDisplayList(),
};

Gfx l_d_course_koopa_troopa_beach_dl_tree_top3[] = {
    gsSPDisplayList(l_d_course_koopa_troopa_beach_dl_18C88),
    gsSPEndDisplayList(),
};

u16 l_d_course_rainbow_road_static_tluts[][256] = {
{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonPeach.rgba16.inc.c"
},
{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonLuigi.rgba16.inc.c"
},
{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonDonkeyKong.rgba16.inc.c"
},
{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonYoshi.rgba16.inc.c"
},

{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonBowser.rgba16.inc.c"
},

{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonWario.rgba16.inc.c"
},

{
    #include "assets/code/rainbow_road_tluts/gTLUTRainbowRoadNeonToad.rgba16.inc.c"
}
};

u16 l_common_texture_minimap_kart_mario[][64] = {
{
    	#include "assets/code/common_data/common_texture_minimap_kart_mario.rgba16.inc.c"
},
{
    	#include "assets/code/common_data/common_texture_minimap_kart_luigi.rgba16.inc.c"
},
{
    	#include "assets/code/common_data/common_texture_minimap_kart_yoshi.rgba16.inc.c"
},
{
    	#include "assets/code/common_data/common_texture_minimap_kart_toad.rgba16.inc.c"
},
{
    	#include "assets/code/common_data/common_texture_minimap_kart_donkey_kong.rgba16.inc.c"
},
{ 
       #include "assets/code/common_data/common_texture_minimap_kart_wario.rgba16.inc.c"
},
{ 
       #include "assets/code/common_data/common_texture_minimap_kart_peach.rgba16.inc.c"
},
{ 
       #include "assets/code/common_data/common_texture_minimap_kart_bowser.rgba16.inc.c"
},
{ 
       #include "assets/code/common_data/common_texture_minimap_progress_dot.rgba16.inc.c"
}
};

