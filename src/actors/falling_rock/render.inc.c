#include <actors.h>
#include <main.h>
#include "courses/choco_mountain/course_data.h"

Gfx l_d_course_choco_mountain_dl_falling_rock[] = {
    gsSPSetGeometryMode(G_LIGHTING),
    gsDPSetCycleType(G_CYC_2CYCLE),
    gsDPPipeSync(),
    gsSPSetGeometryMode(G_SHADING_SMOOTH),
    //gsSPSetGeometryMode(G_FOG),
    gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, 5, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP, 5, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x007C, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, d_course_choco_mountain_wall_texture),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 1023, 256),
    gsSPVertex(d_course_choco_mountain_falling_rock_model, 15, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(3, 4, 5, 0),
    gsSP1Triangle(6, 7, 8, 0),
    gsSP1Triangle(9, 10, 11, 0),
    gsSP1Triangle(12, 13, 14, 0),
    gsSPVertex(d_course_choco_mountain_6006C28, 5, 0),
    gsSPVertex(d_course_choco_mountain_6006D08, 10, 5),
    gsSP1Triangle(0, 5, 6, 0),
    gsSP1Triangle(1, 7, 8, 0),
    gsSP1Triangle(2, 9, 10, 0),
    gsSP1Triangle(3, 11, 12, 0),
    gsSP1Triangle(4, 13, 14, 0),
    gsSPVertex(d_course_choco_mountain_6006C78, 5, 0),
    gsSPVertex(d_course_choco_mountain_6006DA8, 10, 5),
    gsSP1Triangle(0, 5, 6, 0),
    gsSP1Triangle(1, 7, 8, 0),
    gsSP1Triangle(2, 9, 10, 0),
    gsSP1Triangle(3, 11, 12, 0),
    gsSP1Triangle(4, 13, 14, 0),
    gsSPVertex(d_course_choco_mountain_6006CC8, 4, 0),
    gsSPVertex(d_course_choco_mountain_6006E48, 8, 4),
    gsSP1Triangle(0, 4, 5, 0),
    gsSP1Triangle(1, 6, 7, 0),
    gsSP1Triangle(2, 8, 9, 0),
    gsSP1Triangle(3, 10, 11, 0),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0x0000, G_TX_RENDERTILE, 0, G_TX_MIRROR | G_TX_WRAP, 5, G_TX_NOLOD,
                G_TX_MIRROR | G_TX_WRAP, 5, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x007C, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, d_course_choco_mountain_rock_texture),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 1023, 256),
    gsSPVertex(d_course_choco_mountain_6006EC8, 3, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSPClearGeometryMode(G_FOG | G_LIGHTING),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPTexture(0xFFFF, 0xFFFF, 1, 1, G_OFF),
    gsDPPipeSync(),
    gsSPEndDisplayList(),
};

/**
 * @brief Renders the falling rock actor.
 * Actor used in Choco Mountain.
 *
 * @param camera
 * @param rock
 */
void render_actor_falling_rock(Camera* camera, struct FallingRock* rock) {
    Vec3s sp98;
    Vec3f sp8C;
    Mat4 sp4C;
    f32 height;
    UNUSED s32 pad[4];

    if (rock->respawnTimer != 0) {
        return;
    }

    height = is_within_render_distance(camera->pos, rock->pos, camera->rot[1], 400.0f, gCameraZoom[camera - camera1],
                                       4000000.0f);

    if (height < 0.0f) {
        return;
    }

    if (height < 250000.0f) {

        if (rock->unk30.unk34 == 1) {
            sp8C[0] = rock->pos[0];
            sp8C[2] = rock->pos[2];
            height = calculate_surface_height(sp8C[0], rock->pos[1], sp8C[2], rock->unk30.meshIndexZX);
            sp98[0] = 0;
            sp98[1] = 0;
            sp98[2] = 0;
            sp8C[1] = height + 2.0f;
            mtxf_pos_rotation_xyz(sp4C, sp8C, sp98);
            if (render_set_position(sp4C, 0) == 0) {
                return;
            }
            gSPDisplayList(gDisplayListHead++, d_course_choco_mountain_dl_6F88);
        }
    }
    mtxf_pos_rotation_xyz(sp4C, rock->pos, rock->rot);
    if (render_set_position(sp4C, 0) == 0) {
        return;
    }
    //gSPDisplayList(gDisplayListHead++, d_course_choco_mountain_dl_falling_rock);
    gSPDisplayList(gDisplayListHead++, l_d_course_choco_mountain_dl_falling_rock);
}
