#include <actors.h>
#include <main.h>
#include <macros.h>

Gfx l_itemBoxQuestionMarkModel[] = {
    gsSPClearGeometryMode(G_CULL_BACK),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_MODULATEIDECALA, G_CC_MODULATEIDECALA),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0x0000, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, 0x007C, 0x00FC),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, common_texture_item_box_question_mark),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 2047, 256),
    gsSPVertex(&common_vtx_itembox[4], 4, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};


extern Vtx common_vtx_itembox[];
Gfx l_D_0D002EE8[] = {
    gsDPPipeSync(),
    gsSPTexture(0xFFFF, 0xFFFF, 1, 1, G_OFF),
    gsSPClearGeometryMode(G_LIGHTING),
   gsDPSetRenderMode(G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2),
    gsDPSetPrimColor(0, 0, 0x08, 0x08, 0x08, 0x7F),
    gsDPSetCombineLERP(0, 0, SHADE, PRIMITIVE, 1, 0, PRIMITIVE, 0, 0, 0, SHADE, PRIMITIVE, 1, 0, PRIMITIVE, 0),
    gsSPVertex(common_vtx_itembox, 4, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSP1Triangle(0, 2, 3, 0),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsSPEndDisplayList(),
};


/**
 * @brief Renders the item box actor.
 *
 * @param camera
 * @param item_box
 */
void render_actor_item_box(Camera* camera, struct ItemBox* item_box) {
    UNUSED s32 pad[2];
    Vec3f someVec1;
    Vec3f someVec2;
    Vec3s someRot;
    f32 thing;
    UNUSED s32 pad2;
    Mat4 someMatrix1;
    Mat4 someMatrix2;
    UNUSED s32 pad3[4];
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;
    f32 temp_f12;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 someMultiplier;

    temp_f0 = is_within_render_distance(camera->pos, item_box->pos, camera->rot[1], 0.0f, gCameraZoom[camera - camera1],
                                        4000000.0f);
    if (!(temp_f0 < 0.0f) && !(600000.0f < temp_f0)) {
        if ((item_box->state == 2) && (temp_f0 < 100000.0f)) {
            someRot[0] = 0;
            someRot[1] = item_box->rot[1];
            someRot[2] = 0;
            someVec2[0] = item_box->pos[0];
            someVec2[1] = item_box->resetDistance + 2.0f;
            someVec2[2] = item_box->pos[2];
            mtxf_pos_rotation_xyz(someMatrix1, someVec2, someRot);

            if (!render_set_position(someMatrix1, 0)) {
                return;
            }

            //gSPDisplayList(gDisplayListHead++, D_0D002EE8);

            // item box shadows
            gSPDisplayList(gDisplayListHead++, l_D_0D002EE8);
            someRot[1] = item_box->rot[1] * 2;
            someVec2[1] = item_box->pos[1];
            mtxf_pos_rotation_xyz(someMatrix1, someVec2, someRot);

            if (!render_set_position(someMatrix1, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, l_itemBoxQuestionMarkModel);
        }
        if (item_box->state == 5) {
            mtxf_pos_rotation_xyz(someMatrix1, item_box->pos, item_box->rot);

            if (!render_set_position(someMatrix1, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, l_itemBoxQuestionMarkModel);
        }
        if (item_box->state != 3) {
            mtxf_pos_rotation_xyz(someMatrix1, item_box->pos, item_box->rot);

            if (!render_set_position(someMatrix1, 0)) {
                return;
            }

            gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
            gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA, G_CC_MODULATEIA);
            /* if ((item_box->rot[1] < 0xAA1) && (item_box->rot[1] > 0)) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else if ((item_box->rot[1] >= 0x6AA5) && (item_box->rot[1] < 0x754E)) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else if ((item_box->rot[1] >= 0x38E1) && (item_box->rot[1] < 0x438A)) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else if ((item_box->rot[1] >= 0xC711) && (item_box->rot[1] < 0xD1BA)) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else */ {
                /* gDPSetBlendMask(gDisplayListHead++, 0xFF);
                gDPSetRenderMode(gDisplayListHead++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2); */
                gDPSetRenderMode(gDisplayListHead++,
                                 AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
                                     GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA),
                                 AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
                                     GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA));
            }
            gSPSetGeometryMode(gDisplayListHead++, G_SHADING_SMOOTH);
            gSPDisplayList(gDisplayListHead++, D_0D003090);
        } else {
            gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
            gSPClearGeometryMode(gDisplayListHead++, G_CULL_BACK);
            gDPSetBlendMask(gDisplayListHead++, 0xFF);
            thing = item_box->someTimer;
            mtxf_pos_rotation_xyz(someMatrix1, item_box->pos, item_box->rot);
            if (thing < 10.0f) {
                someMultiplier = 1.0f;
            } else {
                someMultiplier = 1.0f - ((thing - 10.0f) * 0.1f);
            }
            mtxf_scale(someMatrix1, someMultiplier);
            if (item_box->someTimer & 1) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_XLU_INTER, G_RM_NOOP2);
            }
            temp_f2 = 2.0f * thing;
            someVec1[0] = 0.0f;
            someVec1[1] = temp_f2;
            someVec1[2] = thing;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D003158);
            temp_f2_2 = 0.8f * thing;
            temp_f12 = 0.5f * thing;
            someVec1[0] = temp_f2_2;
            someVec1[1] = 2.3f * thing;
            someVec1[2] = temp_f12;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D0031B8);
            temp_f0_2 = -0.5f * thing;
            someVec1[0] = temp_f2_2;
            someVec1[1] = 1.2f * thing;
            someVec1[2] = temp_f0_2;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D003128);
            if (!(item_box->someTimer & 1)) {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            } else {
                gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_XLU_INTER, G_RM_NOOP2);
            }
            someVec1[0] = 0.0f;
            someVec1[1] = 1.8f * thing;
            someVec1[2] = -1.0f * thing;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D0031E8);
            temp_f0_3 = -0.8f * thing;
            someVec1[0] = temp_f0_3;
            someVec1[1] = 0.6f * thing;
            someVec1[2] = temp_f0_2;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D003188);
            someVec1[0] = temp_f0_3;
            someVec1[1] = temp_f2;
            someVec1[2] = temp_f12;
            add_translate_mat4_vec3f(someMatrix1, someMatrix2, someVec1);

            if (!render_set_position(someMatrix2, 0)) {
                return;
            }

            gSPDisplayList(gDisplayListHead++, D_0D0030F8);
            gSPSetGeometryMode(gDisplayListHead++, G_CULL_BACK);
        }
        gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    }
}
