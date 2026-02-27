#include <actors.h>
#include <code_800029B0.h>
#include <PR/gbi.h>
extern u16 common_texture_flat_banana[];

Vtx l_common_vtx_flat_banana[] = {
    {{{     6,     -3,      0}, 0, {  2048,   1024}, {255, 254, 254, 255}}},
    {{{     0,      4,      0}, 0, {  1023,   -409}, {193, 255,   0, 255}}},
    {{{    -6,     -3,      0}, 0, {     0,   1024}, {255, 254, 254, 255}}},
    {{{     0,     -3,      6}, 0, {  2048,   1024}, {211, 218, 173, 255}}},
    {{{     0,      4,      0}, 0, {  1024,   -409}, {193, 255,   0, 255}}},
    {{{     0,     -3,     -6}, 0, {     0,   1024}, {211, 218, 173, 255}}},
};

Gfx l_common_model_flat_banana[] = {
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_MODULATEIDECALA, G_CC_MODULATEIDECALA),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x00FC, 0x007C),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, common_texture_flat_banana),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 2047, 128),
    gsSPClearGeometryMode(G_CULL_BACK | G_LIGHTING),
    gsSPVertex(l_common_vtx_flat_banana, 6, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(3, 4, 5, 0),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

/**
 * @brief Render the banana actor
 *
 * @param camera
 * @param arg1
 * @param banana
 */
void render_actor_banana(Camera* camera, UNUSED Mat4 arg1, struct BananaActor* banana) {
    UNUSED s32 pad[2];
    s32 maxObjectsReached;
    Vec3s sp7C;
    Mat4 sp3C;

    f32 temp = is_within_render_distance(camera->pos, banana->pos, camera->rot[1], 0, gCameraZoom[camera - camera1],
                                         490000.0f);
    if (temp < 0.0f) {
        actor_not_rendered(camera, (struct Actor*) banana);
        return;
    }

    if ((banana->pos[1] > gCourseMaxY + 800.0f)) {
        actor_not_rendered(camera, (struct Actor*) banana);
        return;
    }
    if (banana->pos[1] < (gCourseMinY - 800.0f)) {
        actor_not_rendered(camera, (struct Actor*) banana);
        return;
    }

    actor_rendered(camera, (struct Actor*) banana);

    if (banana->state == 5) {
        mtxf_pos_rotation_xyz(sp3C, banana->pos, banana->rot);
    } else {
        sp7C[0] = 0;
        sp7C[1] = 0;
        sp7C[2] = 0;
        mtxf_pos_rotation_xyz(sp3C, banana->pos, sp7C);
    }

    maxObjectsReached = render_set_position(sp3C, 0) == 0;
    if (maxObjectsReached) {
        return;
    }


    if (banana->state != 5) {
        gSPDisplayList(gDisplayListHead++, &common_model_banana);
    } else {
        gSPDisplayList(gDisplayListHead++, &l_common_model_flat_banana);
    }
}

