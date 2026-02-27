#include <kos.h>
#include "kos_undef.h"

#include <ultra64.h>
#include <macros.h>
#include <defines.h>
#include <segments.h>
#include <mk64.h>
#include <course.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code_80281780.h"
#include "memory.h"
#include "camera.h"
#include "camera_junk.h"
#include "spawn_players.h"
#include "skybox_and_splitscreen.h"
#include "code_8006E9C0.h"
#include "podium_ceremony_actors.h"
#include "cpu_vehicles_camera_path.h"
#include "collision.h"
#include "code_80281C40.h"
#include "code_800029B0.h"
#include "menu_items.h"
#include "main.h"
#include "menus.h"
#include "render_courses.h"

u8 defaultCharacterIds[] = { 1, 2, 3, 4, 5, 6, 7, 0 };

void debug_switch_character_ceremony_cutscene(void) {
    if (gEnableDebugMode) {
        if (gControllerOne->button & HOLD_ALL_DPAD_AND_C_BUTTONS) {
            // Allows to switch character in debug mode?
            if (gControllerOne->button & U_CBUTTONS) {
                gCharacterSelections[0] = LUIGI;
            } else if (gControllerOne->button & L_CBUTTONS) {
                gCharacterSelections[0] = YOSHI;
            } else if (gControllerOne->button & R_CBUTTONS) {
                gCharacterSelections[0] = TOAD;
            } else if (gControllerOne->button & D_CBUTTONS) {
                gCharacterSelections[0] = DK;
            } else if (gControllerOne->button & U_JPAD) {
                gCharacterSelections[0] = WARIO;
            } else if (gControllerOne->button & L_JPAD) {
                gCharacterSelections[0] = PEACH;
            } else if (gControllerOne->button & R_JPAD) {
                gCharacterSelections[0] = BOWSER;
            } else {
                gCharacterSelections[0] = MARIO;
            }
            //! @todo confirm this.
            // Resets gCharacterIdByGPOverallRank to default?
            bcopy(&defaultCharacterIds, &gCharacterIdByGPOverallRank, 8);
        }
    }
}

s32 func_80281880(s32 arg0) {
    s32 i;
    for (i = 0; i < NUM_PLAYERS; i++) {
        if (gCharacterIdByGPOverallRank[i] == gCharacterSelections[arg0]) {
            break;
        }
    }
    return i;
}

void func_802818BC(void) {
    s32 temp_v0;
    UNUSED s32 pad;
    s32 sp1C;
    s32 temp_v0_2;

    if (gPlayerCount != TWO_PLAYERS_SELECTED) {
        D_802874D8.unk1D = func_80281880(0);
        D_802874D8.unk1E = gCharacterSelections[0];
        return;
    }
    // weird pattern but if it matches it matches
    temp_v0 = sp1C = func_80281880(0);
    temp_v0_2 = func_80281880(1);
    if (sp1C < temp_v0_2) {
        D_802874D8.unk1E = gCharacterSelections[0];
        D_802874D8.unk1D = temp_v0;
    } else {
        D_802874D8.unk1E = gCharacterSelections[1];
        D_802874D8.unk1D = temp_v0_2;
    }
}

extern Gfx d_course_royal_raceway_packed_dl_67E8[];
extern Gfx d_course_royal_raceway_packed_dl_AEF8[];
extern Gfx d_course_royal_raceway_packed_dl_A970[];
extern Gfx d_course_royal_raceway_packed_dl_AC30[];
extern Gfx d_course_royal_raceway_packed_dl_CE0[];
extern Gfx d_course_royal_raceway_packed_dl_E88[];
extern Gfx d_course_royal_raceway_packed_dl_A618[];
extern Gfx d_course_royal_raceway_packed_dl_A618[];
extern Gfx d_course_royal_raceway_packed_dl_23F8[];
extern Gfx d_course_royal_raceway_packed_dl_2478[];
#include "buffer_sizes.h"
extern uint8_t __attribute__((aligned(32))) CEREMONY_BUF[CEREMONY_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) COURSE_BUF[COURSE_BUF_SIZE];
extern u16 reflection_map_silver[1024];
extern u16 reflection_map_gold[1024];
extern u16 reflection_map_brass[1024];
extern CollisionTriangle __attribute__((aligned(32))) allColTris[allColTris_SIZE];

extern char *fnpre;

static char texfn[256];

extern u16 gTexturePodium1[];
extern u16 gTexturePodium2[];
extern u16 gTexturePodium3[];

void load_ceremony_data(void) {
    sprintf(texfn, "%s/dc_data/ceremony_data.bin", fnpre);
    FILE* file = fopen(texfn, "rb");
    if (!file) {
        perror("fopen");
        printf("\n");
        // while(1) {}
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    long toread = filesize;
    long didread = 0;

    while (didread < filesize) {
        long rv = fread(&CEREMONY_BUF[didread], 1, toread - didread, file);
        if (rv == -1) {
            perror("fread");
            printf("couldnt read into ceremony buf\n");
            // while(1) {}
            exit(-1);
        }
        toread -= rv;
        didread += rv;
    }
    fclose(file);
    file = NULL;
    set_segment_base_addr(0xB, (void*) CEREMONY_BUF);
    u16 *sPod1 = (u16 *)segmented_to_virtual(gTexturePodium1);
    u16 *sPod2 = (u16 *)segmented_to_virtual(gTexturePodium2);
    u16 *sPod3 = (u16 *)segmented_to_virtual(gTexturePodium3);
    for (int i=0;i<32*32;i++) {
        sPod1[i] = __builtin_bswap16(sPod1[i]);
        sPod2[i] = __builtin_bswap16(sPod2[i]);
        sPod3[i] = __builtin_bswap16(sPod3[i]);
    }
}

void load_ceremony_cutscene(void) {
    Camera* camera = &cameras[0];
    memset(&D_802874D8, 0, sizeof(D_802874D8));
    memset(sPodiumActorList, 0, (sizeof(CeremonyActor) * 200));
    sPodiumActorList = NULL;

    gCurrentCourseId = COURSE_ROYAL_RACEWAY;
    D_800DC5B4 = (u16) 1;
    gIsMirrorMode = 0;
    gGotoMenu = 0xFFFF;
    D_80287554 = 0;
    set_perspective_and_aspect_ratio();
    func_802A74BC();
    camera->unk_B4 = 60.0f;
    gCameraZoom[0] = 60.0f;
    D_800DC5EC->screenWidth = SCREEN_WIDTH;
    D_800DC5EC->screenHeight = SCREEN_HEIGHT;
    D_800DC5EC->screenStartX = 160;
    D_800DC5EC->screenStartY = 120;
    gScreenModeSelection = SCREEN_MODE_1P;
    gActiveScreenMode = SCREEN_MODE_1P;
    gModeSelection = GRAND_PRIX;
    load_course(gCurrentCourseId);
    load_ceremony_data();
    sprintf(texfn, "%s/dc_data/banshee_boardwalk_data.bin", fnpre);

    FILE* file = fopen(texfn, "rb");
    if (!file) {
        perror("fopen");
        printf("\n");
        // while(1) {}
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    long toread = filesize;
    long didread = 0;

    while (didread < filesize) {
        long rv = fread(&COURSE_BUF[didread], 1, toread - didread, file);
        if (rv == -1) {
            perror("fread");
            printf("couldnt read into course buf\n");
            // while(1) {}
            exit(-1);
        }
        toread -= rv;
        didread += rv;
    }
    fclose(file);
    file = NULL;

    set_segment_base_addr(6, (void*) COURSE_BUF);

    D_8015F8E4 = -2000.0f;

    gCourseMinX = -0x15A1;
    gCourseMinY = -0x15A1;
    gCourseMinZ = -0x15A1;

    gCourseMaxX = 0x15A1;
    gCourseMaxY = 0x15A1;
    gCourseMaxZ = 0x15A1;

    D_8015F59C = 0;
    D_8015F5A0 = 0;
    D_8015F58C = 0;
    gCollisionMeshCount = (u16) 0;
    D_800DC5BC = (u16) 0;
    D_800DC5C8 = (u16) 0;
    gCollisionMesh = (CollisionTriangle*) allColTris;
    //! @bug these segmented addresses need to be symbols for mobility
    // d_course_royal_raceway_packed_dl_67E8
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_67E8, -1);
    // d_course_royal_raceway_packed_dl_AEF8
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_AEF8, -1);
    // d_course_royal_raceway_packed_dl_A970
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_A970, 8);
    // d_course_royal_raceway_packed_dl_AC30
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_AC30, 8);
    // d_course_royal_raceway_packed_dl_CE0
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_CE0, 0x10);
    // d_course_royal_raceway_packed_dl_E88
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_E88, 0x10);
    // d_course_royal_raceway_packed_dl_A618
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_A618, -1);
    // d_course_royal_raceway_packed_dl_A618
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_A618, -1);
    // d_course_royal_raceway_packed_dl_23F8
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_23F8, 1);
    // d_course_royal_raceway_packed_dl_2478
    generate_collision_mesh_with_default_section_id((Gfx*) d_course_royal_raceway_packed_dl_2478, 1);
    func_80295C6C();
    debug_switch_character_ceremony_cutscene();
    func_802818BC();
    func_8003D080();
    init_hud();
    func_8001C05C();
    balloons_and_fireworks_init();
    init_camera_podium_ceremony();
    func_80093E60();
    // gold
    uint16_t* reflp = (uint16_t*) segmented_to_virtual(0x0B002F18);
    for (int i = 0; i < 32 * 32; i++) {
        uint16_t nextrp = reflp[i];
        nextrp = (nextrp << 8) | ((nextrp >> 8) & 0xff);
        reflp[i] = nextrp;
    } // brass
    reflp = (uint16_t*) segmented_to_virtual(0x0B003F18);
    for (int i = 0; i < 32 * 32; i++) {
        uint16_t nextrp = reflp[i];
        nextrp = (nextrp << 8) | ((nextrp >> 8) & 0xff);
        reflp[i] = nextrp;
    } // silver
    reflp = (uint16_t*) segmented_to_virtual(0x0B003718);
    for (int i = 0; i < 32 * 32; i++) {
        uint16_t nextrp = reflp[i];
        nextrp = (nextrp << 8) | ((nextrp >> 8) & 0xff);
        reflp[i] = nextrp;
    }
}
