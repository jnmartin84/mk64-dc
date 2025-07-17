#include <ultra64.h>
#include <macros.h>
#include <common_structs.h>
#include <defines.h>
#include <decode.h>
#include <mk64.h>
#include <course.h>

#include "main.h"
#include "code_800029B0.h"
#include "buffers.h"
#include "save.h"
#include "staff_ghosts.h"
#include "code_8006E9C0.h"
#include "menu_items.h"
#include "code_80057C60.h"
#include "kart_dma.h"

extern void *segmented_to_virtual(void *addr);

extern s32 mio0encode(s32 input, s32, s32);
extern s32 func_80040174(void*, s32, s32);

u8* sReplayGhostBuffer;
s16 sReplayGhostBufferSize;
s16 D_80162D86;
u16 D_80162D88;

u32 sPlayerGhostFramesRemaining;
s16 sPlayerGhostReplayIdx;
u32* sPlayerGhostReplay;

u16 D_80162D98;
u32 D_80162D9C;
s16 D_80162DA0;
u32* D_80162DA4;

u16 sPostTTButtonsPrev;
s32 sPostTTFramesRemaining;
s16 sPostTTReplayIdx;
u32* sPostTTReplay;

s16 sPlayerInputIndex;
u32* sPlayerInputs;

u16 D_80162DC0;
u32 D_80162DC4;
s32 D_80162DC8;
s32 D_80162DCC;
s32 D_80162DD0;
u16 bPlayerGhostDisabled;
u16 bCourseGhostDisabled;
u16 D_80162DD8;
s32 D_80162DDC;
s32 D_80162DE0; // ghost kart id?
s32 D_80162DE4;
s32 D_80162DE8;
s32 sUnusedReplayCounter;
s32 gPauseTriggered;
s32 D_80162DF4;
s32 gPostTimeTrialReplayCannotSave;
s32 D_80162DFC;

s32 D_80162E00;

u32* sReplayGhostEncoded = (u32*) &D_802BFB80.arraySize8[0][2][3];
u32* gReplayGhostCompressed = (u32*) &D_802BFB80.arraySize8[1][1][3];

extern s32 gLapCountByPlayerId[];

extern StaffGhost* d_mario_raceway_staff_ghost;
extern StaffGhost* d_royal_raceway_staff_ghost;
extern StaffGhost* d_luigi_raceway_staff_ghost;

uint8_t stupidbuff[16384];
#define REPLAY_FRAME_COUNTER 0xff0000
void load_course_ghost(void) {
    // jnmartin84 - wtf
#if 0
    D_80162DA4 = (u32*) &D_802BFB80.arraySize8[0][2][3];
    osInvalDCache(&D_80162DA4[0], 0x4000);
    osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) &_kart_texturesSegmentRomStart[SEGMENT_OFFSET(D_80162DC4)], D_80162DA4,
                 0x4000, &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, OS_MESG_BLOCK);
#else
    D_80162DA4 = (u32*) &D_802BFB80.arraySize8[0][2][3];
    dma_copy(D_80162DA4, (uintptr_t)D_80162DC4, 0x4000);
    for (int i=0;i<0x1000;i++) {
        D_80162DA4[i] = __builtin_bswap32(D_80162DA4[i]);
    }
#endif
    D_80162D9C = (*D_80162DA4 & REPLAY_FRAME_COUNTER);
    D_80162DA0 = 0;
}

void load_post_time_trial_replay(void) {
    sPostTTReplay = (u32*) &D_802BFB80.arraySize8[0][D_80162DD0][3];
    sPostTTFramesRemaining = *sPostTTReplay & 0xFF0000;
    sPostTTReplayIdx = 0;
}

void load_player_ghost(void) {
    sPlayerGhostReplay = (u32*) &D_802BFB80.arraySize8[0][D_80162DC8][3];
    sPlayerGhostFramesRemaining = (s32) *sPlayerGhostReplay & 0xFF0000;
    sPlayerGhostReplayIdx = 0;
}
/**
 * Activates staff ghost if time trial lap time is lower enough
 *
 */
#ifdef VERSION_EU
#define BLAH 10700
#define BLAH2 19300
#define BLAH3 13300
#else
#define BLAH 9000
#define BLAH2 16000
#define BLAH3 11200

#endif

void set_staff_ghost(void) {
    u32 temp_v0; // Appears to be player total lap time.

    switch (gCurrentCourseId) {
        case COURSE_MARIO_RACEWAY:
            temp_v0 = func_800B4E24(0) & 0xfffff;
            if (temp_v0 <= BLAH) {
                bCourseGhostDisabled = 0;
                D_80162DF4 = 0;
            } else {
                bCourseGhostDisabled = 1;
                D_80162DF4 = 1;
            }
            D_80162DC4 = (u32) segmented_to_virtual(&d_mario_raceway_staff_ghost);
            D_80162DE4 = 0;
            break;
        case COURSE_ROYAL_RACEWAY:
            temp_v0 = func_800B4E24(0) & 0xfffff;
            if (temp_v0 <= BLAH2) {
                bCourseGhostDisabled = 0;
                D_80162DF4 = 0;
            } else {
                bCourseGhostDisabled = 1;
                D_80162DF4 = 1;
            }
            D_80162DC4 = (u32) segmented_to_virtual(&d_royal_raceway_staff_ghost);
            D_80162DE4 = 6;
            break;
        case COURSE_LUIGI_RACEWAY:
            temp_v0 = func_800B4E24(0) & 0xfffff;
            if (temp_v0 <= BLAH3) {
                bCourseGhostDisabled = 0;
                D_80162DF4 = 0;
            } else {
                bCourseGhostDisabled = 1;
                D_80162DF4 = 1;
            }
            D_80162DC4 = (u32) segmented_to_virtual(&d_luigi_raceway_staff_ghost);
            D_80162DE4 = 1;
            break;
        default:
            bCourseGhostDisabled = 1;
            D_80162DF4 = 1;
    }
}

int mio0_encode(const unsigned char *in, unsigned int length, unsigned char *out);

s32 compress_replay_ghost(void) {
    s32 phi_v0;

    if (sReplayGhostBufferSize != 0) {
        // func_80040174 in mio0_decode.s
//        func_80040174((void*) sReplayGhostBuffer, (sReplayGhostBufferSize * 4) + 0x20, (s32) sReplayGhostEncoded);
//        phi_v0 = mio0encode((s32) sReplayGhostEncoded, (sReplayGhostBufferSize * 4) + 0x20, (s32) gReplayGhostCompressed);
        phi_v0 = mio0_encode(sReplayGhostBuffer, (sReplayGhostBufferSize * 4) + 0x20, gReplayGhostCompressed);
        return phi_v0 + 0x1e;
    }
}
void mio0decode_noinval(const unsigned char *in, unsigned char *out);

void func_8000522C(void) {
    sPlayerGhostReplay = (u32*) &D_802BFB80.arraySize8[0][D_80162DC8][3];
    mio0decode_noinval((u8*) gReplayGhostCompressed, (u8*) sPlayerGhostReplay);
    sPlayerGhostFramesRemaining = (s32) (*sPlayerGhostReplay & REPLAY_FRAME_COUNTER);
    sPlayerGhostReplayIdx = 0;
    D_80162E00 = 1;
}

void func_800052A4(void) {
    s16 temp_v0;

    if (D_80162DC8 == 1) {
        D_80162DC8 = 0;
        D_80162DCC = 1;
    } else {
        D_80162DC8 = 1;
        D_80162DCC = 0;
    }
    temp_v0 = sPlayerInputIndex;
    sReplayGhostBuffer = (void*) &D_802BFB80.arraySize8[0][D_80162DC8][3];
    sReplayGhostBufferSize = temp_v0;
    D_80162D86 = temp_v0;
}

void func_80005310(void) {

    if (gModeSelection == TIME_TRIALS) {

        set_staff_ghost();

        if (D_80162DC0 != gCurrentCourseId) {
            bPlayerGhostDisabled = 1;
        }

        D_80162DC0 = (u16) gCurrentCourseId;
        gPauseTriggered = 0;
        sUnusedReplayCounter = 0;
        gPostTimeTrialReplayCannotSave = 0;

        if (gModeSelection == TIME_TRIALS && gActiveScreenMode == SCREEN_MODE_1P) {

            if (D_8015F890 == 1) {
                load_post_time_trial_replay();
                if (D_80162DD8 == 0) {
                    load_player_ghost();
                }
                if (bCourseGhostDisabled == 0) {
                    load_course_ghost();
                }
            } else {

                D_80162DD8 = 1U;
                sPlayerInputs = (u32*) &D_802BFB80.arraySize8[0][D_80162DCC][3];
                sPlayerInputs[0] = -1;
                sPlayerInputIndex = 0;
                D_80162DDC = 0;
                func_80091EE4();
                if (bPlayerGhostDisabled == 0) {
                    load_player_ghost();
                }
                if (bCourseGhostDisabled == 0) {
                    load_course_ghost();
                }
            }
        }
    }
}

/* Special handling for buttons saved in replays. The listing of L_TRIG here is odd
 * because it is not saved in the replay data structure. Possibly, L was initially deleted
 * here to make way for the frame counter, but then the format changed when the stick
 * coordinates were added */
#define ALL_BUTTONS                                                                                                   \
    (A_BUTTON | B_BUTTON | L_TRIG | R_TRIG | Z_TRIG | START_BUTTON | U_JPAD | L_JPAD | R_JPAD | D_JPAD | U_CBUTTONS | \
     L_CBUTTONS | R_CBUTTONS | D_CBUTTONS)
#define REPLAY_MASK (ALL_BUTTONS ^ (A_BUTTON | B_BUTTON | Z_TRIG | R_TRIG | L_TRIG))

/* Inputs for replays (including player and course ghosts) are saved in a s32[] where
   each entry is a combination of the inputs and  how long those inputs were held for.
   In essence it's "These buttons were pressed and the joystick was in this position.
   This was the case for X frames".

   bits 1-8: Stick X
   bits 9-16: Stick Y
   bits 17-24: Frame counter
   bits 25-28: Unused
   bit 29: R
   bit 30: Z
   bit 31: B
   bit 32: A
*/
#define REPLAY_FRAME_INCREMENT 0xFFFF0000
void process_post_time_trial_replay(void) {
    u32 inputs;
    u32 stickBytes;
    UNUSED u16 unk;
    u16 buttons_temp;
    s16 stickVal;
    s16 buttons = 0;

    if (sPostTTReplayIdx >= 0x1000) {
        gPlayerOne->type = PLAYER_CINEMATIC_MODE | PLAYER_START_SEQUENCE | PLAYER_CPU;
        return;
    }

    inputs = sPostTTReplay[sPostTTReplayIdx];
    stickBytes = inputs & 0xFF;

    if (stickBytes < 0x80U) {
        stickVal = (s16) (stickBytes & 0xFF);
    } else {
        stickVal = (s16) (stickBytes | (~0xFF));
    }

    stickBytes = (u32) (inputs & 0xFF00) >> 8;
    gControllerEight->rawStickX = stickVal;

    if (stickBytes < 0x80U) {
        stickVal = (s16) (stickBytes & 0xFF);
    } else {
        stickVal = (s16) (stickBytes | (~0xFF));
    }
    gControllerEight->rawStickY = stickVal;
    if (inputs & 0x80000000) {
        buttons |= A_BUTTON;
    }
    if (inputs & 0x40000000) {
        buttons |= B_BUTTON;
    }
    if (inputs & 0x20000000) {
        buttons |= Z_TRIG;
    }
    if (inputs & 0x10000000) {
        buttons |= R_TRIG;
    }
    buttons_temp = gControllerEight->buttonPressed & REPLAY_MASK;
    gControllerEight->buttonPressed = (buttons & (buttons ^ sPostTTButtonsPrev)) | buttons_temp;
    buttons_temp = gControllerEight->buttonDepressed & REPLAY_MASK;
    gControllerEight->buttonDepressed = (sPostTTButtonsPrev & (buttons ^ sPostTTButtonsPrev)) | buttons_temp;
    sPostTTButtonsPrev = buttons;
    gControllerEight->button = buttons;

    if (sPostTTFramesRemaining == 0) {
        sPostTTReplayIdx++;
        sPostTTFramesRemaining = (s32) (sPostTTReplay[sPostTTReplayIdx] & REPLAY_FRAME_COUNTER);
    } else {
        sPostTTFramesRemaining += REPLAY_FRAME_INCREMENT;
    }
}

void process_course_ghost_replay(void) {
    u32 temp_a0;
    u32 temp_v0;
    UNUSED u16 unk;
    u16 temp_v1;
    s16 phi_v1;
    s16 phi_a2 = 0;

    if (D_80162DA0 >= 0x1000) {
        func_80005AE8(gPlayerThree);
        return;
    }
    temp_a0 = D_80162DA4[D_80162DA0];
    temp_v0 = temp_a0 & 0xFF;
    if (temp_v0 < 0x80U) {
        phi_v1 = (s16) (temp_v0 & 0xFF);
    } else {
        phi_v1 = (s16) (temp_v0 | (~0xFF));
    }

    temp_v0 = (u32) (temp_a0 & 0xFF00) >> 8;
    gControllerSeven->rawStickX = phi_v1;

    if (temp_v0 < 0x80U) {
        phi_v1 = (s16) (temp_v0 & 0xFF);
    } else {
        phi_v1 = (s16) (temp_v0 | (~0xFF));
    }
    gControllerSeven->rawStickY = phi_v1;

    if (temp_a0 & 0x80000000) {
        phi_a2 = A_BUTTON;
    }
    if (temp_a0 & 0x40000000) {
        phi_a2 |= B_BUTTON;
    }
    if (temp_a0 & 0x20000000) {
        phi_a2 |= Z_TRIG;
    }
    if (temp_a0 & 0x10000000) {
        phi_a2 |= R_TRIG;
    }

    temp_v1 = gControllerSeven->buttonPressed & 0x1F0F;
    gControllerSeven->buttonPressed = (phi_a2 & (phi_a2 ^ D_80162D98)) | temp_v1;
    temp_v1 = gControllerSeven->buttonDepressed & 0x1F0F;
    gControllerSeven->buttonDepressed = (D_80162D98 & (phi_a2 ^ D_80162D98)) | temp_v1;
    D_80162D98 = phi_a2;
    gControllerSeven->button = phi_a2;
    if (D_80162D9C == 0) {
        D_80162DA0++;
        D_80162D9C = (s32) (D_80162DA4[D_80162DA0] & 0xFF0000);
    } else {
        D_80162D9C += (s32) 0xFFFF0000;
    }
}

void process_player_ghost_replay(void) {
    u32 temp_a0;
    u32 temp_v0;
    UNUSED u16 unk;
    u16 temp_v1;
    s16 phi_v1;
    s16 phi_a2 = 0;

    if (sPlayerGhostReplayIdx >= 0x1000) {
        func_80005AE8(gPlayerTwo);
        return;
    }
    temp_a0 = sPlayerGhostReplay[sPlayerGhostReplayIdx];
    temp_v0 = temp_a0 & 0xFF;
    if (temp_v0 < 0x80U) {
        phi_v1 = (s16) (temp_v0 & 0xFF);
    } else {
        phi_v1 = (s16) (temp_v0 | ~0xFF);
    }

    temp_v0 = (u32) (temp_a0 & 0xFF00) >> 8;

    gControllerSix->rawStickX = phi_v1;

    if (temp_v0 < 0x80U) {
        phi_v1 = (s16) (temp_v0 & 0xFF);
    } else {
        phi_v1 = (s16) (temp_v0 | (~0xFF));
    }

    gControllerSix->rawStickY = phi_v1;

    if (temp_a0 & 0x80000000) {
        phi_a2 |= A_BUTTON;
    }
    if (temp_a0 & 0x40000000) {
        phi_a2 |= B_BUTTON;
    }
    if (temp_a0 & 0x20000000) {
        phi_a2 |= Z_TRIG;
    }
    if (temp_a0 & 0x10000000) {
        phi_a2 |= R_TRIG;
    }
    temp_v1 = gControllerSix->buttonPressed & 0x1F0F;
    gControllerSix->buttonPressed = (phi_a2 & (phi_a2 ^ D_80162D88)) | temp_v1;

    temp_v1 = gControllerSix->buttonDepressed & 0x1F0F;
    gControllerSix->buttonDepressed = (D_80162D88 & (phi_a2 ^ D_80162D88)) | temp_v1;
    D_80162D88 = phi_a2;
    gControllerSix->button = phi_a2;

    if (sPlayerGhostFramesRemaining == 0) {
        sPlayerGhostReplayIdx++;
        sPlayerGhostFramesRemaining = (s32) (sPlayerGhostReplay[sPlayerGhostReplayIdx] & 0xFF0000);
    } else {
        sPlayerGhostFramesRemaining += (s32) 0xFFFF0000;
    }
}

void save_player_replay(void) {
    s16 temp_a2;
    u32 phi_a3;
    u32 temp_v1;
    u32 temp_v2;
    u32 temp_v0;
    u32 temp_t0;
    u32 temp_a0_2;

    if (((sPlayerInputIndex >= 0x1000) || ((gPlayerOne->unk_0CA & 2) != 0)) || ((gPlayerOne->unk_0CA & 8) != 0)) {
        gPostTimeTrialReplayCannotSave = 1;
        return;
    }

    temp_v1 = gControllerOne->rawStickX;
    temp_v1 &= 0xFF;
    temp_v2 = gControllerOne->rawStickY;
    temp_v2 = (temp_v2 & 0xFF) << 8;
    temp_a2 = gControllerOne->button;
    phi_a3 = 0;
    if (temp_a2 & 0x8000) {
        phi_a3 |= 0x80000000;
    }
    if (temp_a2 & 0x4000) {
        phi_a3 |= 0x40000000;
    }
    if (temp_a2 & 0x2000) {
        phi_a3 |= 0x20000000;
    }
    if (temp_a2 & 0x0010) {
        phi_a3 |= 0x10000000;
    }
    phi_a3 |= temp_v1;
    phi_a3 |= temp_v2;
    temp_t0 = sPlayerInputs[sPlayerInputIndex];
    temp_a0_2 = temp_t0 & 0xFF00FFFF;

    if ((*sPlayerInputs) == 0xFFFFFFFF) {

        sPlayerInputs[sPlayerInputIndex] = phi_a3;

    } else if (temp_a0_2 == phi_a3) {

        temp_v0 = temp_t0 & 0xFF0000;

        if (temp_v0 == 0xFF0000) {

            sPlayerInputIndex++;
            sPlayerInputs[sPlayerInputIndex] = phi_a3;

        } else {

            temp_t0 += 0x10000;
            sPlayerInputs[sPlayerInputIndex] = temp_t0;
        }
    } else {
        sPlayerInputIndex++;
        sPlayerInputs[sPlayerInputIndex] = phi_a3;
    }
}

// sets player to AI? (unconfirmed)
void func_80005AE8(Player* ply) {
    if (((ply->type & PLAYER_INVISIBLE_OR_BOMB) != 0) && (ply != gPlayerOne)) {
        ply->type = PLAYER_CINEMATIC_MODE | PLAYER_START_SEQUENCE | PLAYER_CPU;
    }
}

void func_80005B18(void) {
    if (gModeSelection == TIME_TRIALS) {
        if ((gLapCountByPlayerId[0] == 3) && (D_80162DDC == 0) && (gPostTimeTrialReplayCannotSave != 1)) {
            if (bPlayerGhostDisabled == 1) {
                D_80162DD0 = D_80162DCC;
                func_800052A4();
                bPlayerGhostDisabled = 0;
                D_80162DDC = 1;
                D_80162DE0 = gPlayerOne->characterId;
                D_80162DE8 = gPlayerOne->characterId;
                D_80162E00 = 0;
                D_80162DFC = playerHUD[PLAYER_ONE].someTimer;
                func_80005AE8(gPlayerTwo);
                func_80005AE8(gPlayerThree);
            } else if (gLapCountByPlayerId[1] != 3) {
                D_80162DD0 = D_80162DCC;
                func_800052A4();
                D_80162DDC = 1;
                D_80162DE0 = gPlayerOne->characterId;
                D_80162DFC = playerHUD[PLAYER_ONE].someTimer;
                D_80162E00 = 0;
                D_80162DE8 = gPlayerOne->characterId;
                func_80005AE8(gPlayerTwo);
                func_80005AE8(gPlayerThree);
            } else {
                sReplayGhostBuffer = D_802BFB80.arraySize8[0][D_80162DC8][3].pixel_index_array;
                sReplayGhostBufferSize = D_80162D86;
                D_80162DD0 = D_80162DCC;
                D_80162DE8 = gPlayerOne->characterId;
                D_80162DD8 = 0;
                bPlayerGhostDisabled = 0;
                D_80162DDC = 1;
                func_80005AE8(gPlayerTwo);
                func_80005AE8(gPlayerThree);
            }
        } else {
            if ((gLapCountByPlayerId[0] == 3) && (D_80162DDC == 0) && (gPostTimeTrialReplayCannotSave == 1)) {
                sReplayGhostBuffer = D_802BFB80.arraySize8[0][D_80162DC8][3].pixel_index_array;
                sReplayGhostBufferSize = D_80162D86;
                D_80162DDC = 1;
            }
            if ((gPlayerOne->type & 0x800) == 0x800) {
                func_80005AE8(gPlayerTwo);
                func_80005AE8(gPlayerThree);
            } else {
                sUnusedReplayCounter += 1;
                if (sUnusedReplayCounter >= 0x65) {
                    sUnusedReplayCounter = 0x00000064;
                }
                if ((gModeSelection == TIME_TRIALS) && (gActiveScreenMode == SCREEN_MODE_1P)) {
                    if ((bPlayerGhostDisabled == 0) && (gLapCountByPlayerId[1] != 3)) {
                        process_player_ghost_replay();
                    }
                    if ((bCourseGhostDisabled == 0) && (gLapCountByPlayerId[2] != 3)) {
                        process_course_ghost_replay();
                    }
                    if (!(gPlayerOne->type & 0x800)) {
                        save_player_replay();
                    }
                }
            }
        }
    }
}

void func_80005E6C(void) {
    if ((gModeSelection == TIME_TRIALS) && (gModeSelection == TIME_TRIALS) && (gActiveScreenMode == SCREEN_MODE_1P)) {
        if ((D_80162DD8 == 0) && (gLapCountByPlayerId[1] != 3)) {
            process_player_ghost_replay(); // 3
        }
        if ((bCourseGhostDisabled == 0) && (gLapCountByPlayerId[2] != 3)) {
            process_course_ghost_replay(); // 2
        }
        if ((gPlayerOne->type & PLAYER_CINEMATIC_MODE) != PLAYER_CINEMATIC_MODE) {
            process_post_time_trial_replay(); // 1
            return;
        }
        func_80005AE8(gPlayerTwo);
        func_80005AE8(gPlayerThree);
    }
}

void staff_ghosts_loop(void) {
    if (D_8015F890 == 1) {
        func_80005E6C();
        return;
    }
    if (!gPauseTriggered) {
        func_80005B18();
        return;
    }
    gPostTimeTrialReplayCannotSave = 1;
}
