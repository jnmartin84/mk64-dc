#include <actors.h>
#include <PR/gbi.h>
#include <main.h>
#include <assets/common_data.h>
#include "courses/all_course_data.h"

/**
 * @brief Renders the tree actor in Mario rawceay.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
Gfx l_d_course_mario_raceway_dl_tree_setup[] = {
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x007C, 0x00FC),
    gsDPLoadTextureBlock(0x03009000, G_IM_FMT_CI, G_IM_SIZ_8b, 32, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD),
    gsSPEndDisplayList(),
};

#if 1
Gfx l_d_course_mario_raceway_dl_tree[] = {
    gsSPVertex(d_course_mario_raceway_tree_model, 4, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(0, 2, 3, 0),
    gsSPEndDisplayList(),
};
#endif

void setup_actor_tree_mario_raceway(void) {
    gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
    gSPDisplayList(gDisplayListHead++, l_d_course_mario_raceway_dl_tree_setup);
}

void finish_actor_tree_mario_raceway(void) {
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
}

void render_actor_tree_mario_raceway(Camera* camera, Mat4 arg1, struct Actor* arg2, int pass) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 = is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1],
                                        16000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (pass == 1) {
    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 3.0f);
    }
    return;
    } else if (pass == 0) {
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
#if 0
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, d_course_mario_raceway_dl_tree);
#else
        gSPVertex(gDisplayListHead++, d_course_mario_raceway_tree_model, 4, 0);
        gSP1Triangle(gDisplayListHead++, 0, 1, 2, 0);
        gSP1Triangle(gDisplayListHead++, 0, 2, 3, 0);
//        gSPDisplayList(gDisplayListHead++, l_d_course_mario_raceway_dl_tree);
#endif
    }
    }
}

/**
 * @brief Renders the tree actor in Yoshi Valley.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_yoshi_valley(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, d_course_yoshi_valley_dl_tree);
    }
}

/**
 * @brief Renders the tree actor in Royal Raceway.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_royal_raceway(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, d_course_royal_raceway_dl_tree);
    }
}

/**
 * @brief Renders the tree actor in Moo Moo Farm.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_moo_moo_farm(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 6250000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 600.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 5.0f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, d_course_moo_moo_farm_dl_tree);
    }
}

Vtx l_d_course_moo_moo_farm_tree_model[] = {
    { { { 0, 95*3/7, 0 }, 0, { 1024, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -50*3/7, 95*3/7, 0 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { -50*3/7, -5*3/7, 0 }, 0, { 0, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 0, -5*3/7, 0 }, 0, { 1024, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 50*3/7, 95*3/7, 0 }, 0, { 1023, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 0, 95*3/7, 0 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 0, -5*3/7, 0 }, 0, { 0, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 50*3/7, -5*3/7, 0 }, 0, { 1023, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
};

Gfx l_d_course_moo_moo_farm_dl_tree[] = {
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x007C, 0x00FC),
    gsDPLoadTextureBlock(0x03009000, G_IM_FMT_CI, G_IM_SIZ_8b, 32, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD),
    gsSPVertex(l_d_course_moo_moo_farm_tree_model, 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsDPLoadTextureBlock(0x03009800, G_IM_FMT_CI, G_IM_SIZ_8b, 32, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD),
    gsSP2Triangles(4, 5, 6, 0, 4, 6, 7, 0),
    gsSPEndDisplayList(),
};

// have all the properties of the tree
void render_actor_tree_luigi_raceway(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, l_d_course_moo_moo_farm_dl_tree);
        // Based on the TLUT being loaded above, this ought to be be another
        // tree related DL, presumably one found in a course other than Moo Moo farm
        //                                 0x0600FC70
    }
}

/**
 * @brief Renders the tree actor in Bowser's Castle.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */


void render_actor_tree_bowser_castle(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, l_d_course_moo_moo_farm_dl_tree);
    }
}

/**
 * @brief Renders the bush actor in Bowser's Castle.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_bush_bowser_castle(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 640000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_trees_import);
        gSPDisplayList(gDisplayListHead++, d_course_bowsers_castle_dl_bush);
    }
}

/**
 * @brief Renders the tree actor in Frappe Snowland.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_frappe_snowland(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 250000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 2.79999995f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gSPDisplayList(gDisplayListHead++, d_course_frappe_snowland_dl_tree);
    }
}

/**
 * @brief Renders the a first variant of cactus in Kalimari Desert.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_cactus1_kalimari_desert(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 40000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 1.0f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gSPDisplayList(gDisplayListHead++, d_course_kalimari_desert_dl_cactus1);
    }
}

/**
 * @brief Renders the a second variant of cactus in Kalimari Desert.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_cactus2_kalimari_desert(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 40000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 1.0f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gSPDisplayList(gDisplayListHead++, d_course_kalimari_desert_dl_cactus2);
    }
}

/**
 * @brief Renders the a third variant of cactus in Kalimari Desert.
 *
 * @param camera
 * @param arg1
 * @param arg2
 */
void render_actor_tree_cactus3_kalimari_desert(Camera* camera, Mat4 arg1, struct Actor* arg2) {
    f32 temp_f0;
    s16 temp_v0 = arg2->flags;

    if ((temp_v0 & 0x800) != 0) {
        return;
    }

    temp_f0 =
        is_within_render_distance(camera->pos, arg2->pos, camera->rot[1], 0, gCameraZoom[camera - camera1], 4000000.0f);

    if (temp_f0 < 0.0f) {
        return;
    }

    if (((temp_v0 & 0x400) == 0) && (temp_f0 < 40000.0f)) {
        render_shadow_for_tree(arg2->pos, arg2->rot, 0.80000001f);
    }
    arg1[3][0] = arg2->pos[0];
    arg1[3][1] = arg2->pos[1];
    arg1[3][2] = arg2->pos[2];

    if (render_set_position(arg1, 0) != 0) {
        gSPDisplayList(gDisplayListHead++, d_course_kalimari_desert_dl_cactus3);
    }
}
