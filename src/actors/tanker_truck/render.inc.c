#include <actors.h>
#include <main.h>
#include <defines.h>

/**
 * @brief Renders the tanker truck actor.
 * Actor used in Toad's Turnpike.
 * His update are made in vehicle.
 *
 * @param camera
 * @param arg1
 */
void render_actor_tanker_truck(Camera* camera, struct Actor* arg1) {
    Mat4 spC8;
    f32 temp_f0 = is_within_render_distance(camera->pos, arg1->pos, camera->rot[1], 2500.0f,
                                            gCameraZoom[camera - camera1], 9000000.0f);
    if (temp_f0 < 0.0f) {
        return;
    }

    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);

    mtxf_pos_rotation_xyz(spC8, arg1->pos, arg1->rot);
    if (render_set_position(spC8, 0) != 0) {
        if (gActiveScreenMode == SCREEN_MODE_1P) {
            /* if (temp_f0 < 160000.0f) {
                gSPDisplayList(gDisplayListHead++, &toads_turnpike_dl_6);
            } else */ if (temp_f0 < 640000.0f) {
                gSPDisplayList(gDisplayListHead++, &toads_turnpike_dl_7);
            } else {
                gSPDisplayList(gDisplayListHead++, &toads_turnpike_dl_8);
            }
        } else if (temp_f0 < 160000.0f) {
            gSPDisplayList(gDisplayListHead++, &toads_turnpike_dl_7);
        } else {
            gSPDisplayList(gDisplayListHead++, &toads_turnpike_dl_8);
        }
    }
}
