/* ---- CPU AI rubber-band trace (comment the #define to disable) ---- */
#define MK64_CPU_AI_LOG 0
#if MK64_CPU_AI_LOG
static s16 sPrevDecision[12];   /* last D_801634C0[] decision code seen */
static s8  sPrevFlag[12];       /* last CPU_FAST_EFFECT state seen      */
static u16 sHeartbeat[12];      /* per-kart throttle for fallen-back heartbeat */
static u32 sCpuLogCount;        /* global cap so it can never flood the serial */
static f32 sPrevSpeed[12];      /* last frame's player->speed (scrub edge detect) */
static u8  sScrubbing[12];      /* was this kart below the scrub threshold last frame */
static u16 sScrubHb[12];        /* per-kart throttle for sustained-scrub heartbeat */
static u32 sScrubLogCount;      /* global cap for scrub log */
#endif

void func_80007D04(s32 playerId, Player* player) {
    s16 temp_t1;
    s16 temp_t2;
    s32 var_v0;

    temp_t1 = gNumPathPointsTraversed[gBestRankedHumanPlayer];
    temp_t2 = gNumPathPointsTraversed[playerId];

    if (gGPCurrentRaceRankByPlayerId[playerId] < 2) {
        s16 val1 = gGPCurrentRaceRankByPlayerId[gBestRankedHumanPlayer];
        s16 val2 = temp_t2 - temp_t1;

        if (val2 > 400 && val1 >= 6) {
            player->effects &= ~CPU_FAST_EFFECT;
            player_accelerate(player);
            D_801634C0[playerId] = 4;
            return;
        }
    } else {
        player->effects |= CPU_FAST_EFFECT;
        player_accelerate(player);
        D_801634C0[playerId] = 3;
        return;
    }

    switch (gCCSelection) { // WTF, FAKE ?
        case CC_EXTRA:
            break;
    }

    switch (gCCSelection) {
        case CC_50:
            var_v0 = 0;
            if (playerId == D_80163344[0]) {
                var_v0 = 0x14;
            }
            break;

        case CC_100:
            var_v0 = 8;
            if (playerId == D_80163344[0]) {
                var_v0 = 0x18;
            }
            break;

        case CC_150:
            var_v0 = 0x12;
            if (playerId == D_80163344[0]) {
                var_v0 = 0x24;
            }
            break;

        case CC_EXTRA:
            var_v0 = 8;
            if (playerId == D_80163344[0]) {
                var_v0 = 0x18;
            }
            break;

        default:
            var_v0 = 0;
            break;
    }

    if (temp_t2 < temp_t1) {
        player->effects |= CPU_FAST_EFFECT;
        player_accelerate(player);
        D_801634C0[playerId] = 1;
    } else if (temp_t2 < (temp_t1 + var_v0 + 0x32)) {
        player->effects &= ~CPU_FAST_EFFECT;
        player_accelerate(player);
        D_801634C0[playerId] = 3;
    } else if (D_801631E0[playerId] == 0) {
        player->effects &= ~CPU_FAST_EFFECT;
        player_accelerate(player);
        D_801634C0[playerId] = 2;
    } else {
        player->effects &= ~CPU_FAST_EFFECT;
        decelerate_player(player, 1.0f);
        D_801634C0[playerId] = -1;
    }
}

void func_80007FA4(s32 playerId, Player* player, f32 arg2) {
    f32 temp_f0;
    f32 dist;
    f32 temp_f2;
    s32 test;

    temp_f0 = D_80163418[playerId] - player->pos[0];
    temp_f2 = D_80163438[playerId] - player->pos[2];
    dist = (temp_f0 * temp_f0) + (temp_f2 * temp_f2);
    if (playerId == 3) {
        if ((dist < 25.0f) && (D_80163410[playerId] < 5)) {
            D_80163410[playerId] = 4;
            (arg2 < ((2.0 * 18.0) / 216.0)) ? func_80038BE4(player, 1) : decelerate_player(player, 1.0f);
        } else if ((dist < 3600.0f) && (D_80163410[playerId] < 4)) {
            D_80163410[playerId] = 3;
            (arg2 < ((5.0 * 18.0) / 216.0)) ? func_80038BE4(player, 1) : decelerate_player(player, 5.0f);
        } else {
            (arg2 < ((20.0 * 18.0) / 216.0)) ? func_80038BE4(player, 10) : decelerate_player(player, 1.0f);
        }
    } else {
        if ((dist < 25.0f) && (D_80163410[playerId] < 5)) {
            D_80163410[playerId] = 4;
            test = 2;
            (arg2 < ((test * 18.0) / 216.0)) ? func_80038BE4(player, 1) : decelerate_player(player, 1.0f);
        } else if ((dist < 4900.0f) && (D_80163410[playerId] < 4)) {
            D_80163410[playerId] = 3;
            test = 5;
            (arg2 < ((test * 18.0) / 216.0)) ? func_80038BE4(player, 1) : decelerate_player(player, 15.0f);
        } else if ((dist < 22500.0f) && (D_80163410[playerId] < 3)) {
            D_80163410[playerId] = 2;
            test = 20;
            (arg2 < ((test * 18.0) / 216.0)) ? func_80038BE4(player, 5) : decelerate_player(player, 1.0f);
        } else if ((dist < 90000.0f) && (D_80163410[playerId] < 2)) {
            D_80163410[playerId] = 1;
            test = 30;
            (arg2 < ((test * 18.0) / 216.0)) ? func_80038BE4(player, 6) : decelerate_player(player, 1.0f);
        } else if (D_80163410[playerId] == 0) {
            test = 35;
            (arg2 < (((test ^ 0) * 18.0) / 216.0)) ? func_80038BE4(player, 2) : decelerate_player(player, 1.0f);
        } else {
            decelerate_player(player, 1.0f);
        }
    }
}
#include <stdio.h>
void regulate_cpu_speed(s32 playerId, f32 targetSpeed, Player* player) {
    f32 speed;
    f32 var_f0;
    UNUSED s32 thing;
    s32 var_a1;

    speed = player->speed;
    if (!(player->effects & 0x80) && !(player->effects & 0x40) && !(player->effects & 0x20000) &&
        !(player->soundEffects & 0x400000) && !(player->soundEffects & 0x01000000) && !(player->soundEffects & 2) &&
        !(player->soundEffects & 4)) {
        if (gCurrentCourseId == COURSE_AWARD_CEREMONY) {
            func_80007FA4(playerId, player, speed);
        } else if ((bStopAICrossing[playerId] == 1) && !(player->effects & (STAR_EFFECT | BOO_EFFECT))) {
            decelerate_player(player, 10.0f);
            if (player->currentSpeed == 0.0) {
                player->velocity[0] = 0.0f;
                player->velocity[2] = 0.0f;
            }
        } else {
            var_f0 = 3.3333333f;
            switch (gCCSelection) { /* irregular */
                case CC_100:
                case CC_EXTRA:
                    break;
                case CC_50:
                    var_f0 = 2.5f;
                    break;
                case CC_150:
                    var_f0 = 3.75f;
                    break;
            }
            if (speed < var_f0) {
                player->effects &= ~CPU_FAST_EFFECT;
                player_accelerate(player);
            } else if (player->type & PLAYER_CINEMATIC_MODE) {
                if (speed < targetSpeed) {
                    player->effects &= ~CPU_FAST_EFFECT;
                    player_accelerate(player);
                } else {
                    player->effects &= ~CPU_FAST_EFFECT;
                    decelerate_player(player, 1.0f);
                }
            } else if ((D_801631E0[playerId] == 1) && (D_80163330[playerId] != 1)) {
                if (func_800088D8(playerId, gLapCountByPlayerId[playerId], gGPCurrentRaceRankByPlayerIdDup[playerId]) ==
                    1) {
                    player->effects |= CPU_FAST_EFFECT;
                    player_accelerate(player);
                } else {
                    player->effects &= ~CPU_FAST_EFFECT;
                    decelerate_player(player, 1.0f);
                }
            } else {
                var_a1 = 1;
                switch (gSpeedCPUBehaviour[playerId]) { /* switch 1; irregular */
                    case SPEED_cpu_BEHAVIOUR_FAST:      /* switch 1 */
                        player->effects &= ~CPU_FAST_EFFECT;
                        player_accelerate(player);
                        break;
                    case SPEED_cpu_BEHAVIOUR_MAX: /* switch 1 */
                        player->effects |= CPU_FAST_EFFECT;
                        player_accelerate(player);
                        break;
                    case SPEED_cpu_BEHAVIOUR_SLOW: /* switch 1 */
                        if (((speed / 18.0f) * 216.0f) > 20.0f) {
                            targetSpeed = 1.6666666f;
                        }
                        var_a1 = 0;
                        break;
                    case SPEED_cpu_BEHAVIOUR_NORMAL: /* switch 1 */
                    default:                         /* switch 1 */
                        var_a1 = 0;
                        break;
                }
                if (var_a1 != 1) {
                    if (speed < targetSpeed) {
                        if ((gDemoMode == 1) && (gCurrentCourseId != COURSE_AWARD_CEREMONY)) {
                            player_accelerate(player);
                        } else if (D_80163330[playerId] == 1) {
                            func_80007D04(playerId, player);
                        } else if (func_800088D8(playerId, gLapCountByPlayerId[playerId],
                                                 gGPCurrentRaceRankByPlayerIdDup[playerId]) == 1) {
                            player->effects |= CPU_FAST_EFFECT;
                            player_accelerate(player);
                        } else {
                            player->effects &= ~CPU_FAST_EFFECT;
                            decelerate_player(player, 1.0f);
                        }
                    } else {
                        player->effects &= ~CPU_FAST_EFFECT;
                        if (targetSpeed > 1.0f) {
                            decelerate_player(player, 2.0f);
                        } else {
                            decelerate_player(player, 5.0f);
                        }
                    }
                }
            }
        }
    }
#if MK64_CPU_AI_LOG
    if ((player->type & PLAYER_HUMAN) != PLAYER_HUMAN) {
        /* deficit = my path progress - leader's.  NEGATIVE = I'm behind. */
        s32 deficit = (s32)gNumPathPointsTraversed[playerId]
                    - (s32)gNumPathPointsTraversed[gBestRankedHumanPlayer];
        s32 flag    = (player->effects & CPU_FAST_EFFECT) ? 1 : 0;
        s32 code    = D_801634C0[playerId];
        s32 transition  = (code != sPrevDecision[playerId]) || (flag != sPrevFlag[playerId]);
        s32 fallingBack = (deficit < -300);   /* tune: trailing leader by >300 path pts */
        if (++sHeartbeat[playerId] >= 30) sHeartbeat[playerId] = 0;  /* ~twice/sec @30fps */
        if ((transition || (fallingBack && sHeartbeat[playerId] == 0)) && sCpuLogCount < 4000) {
            u16 ppid2 = gNearestPathPointByPlayerId[playerId];
            TrackPathPoint* pp2 = &gTrackPaths[gPathIndexByPlayerId[playerId]][ppid2];
            f32 dx2 = player->pos[0] - (f32)pp2->posX;   /* line-holding error */
            f32 dz2 = player->pos[2] - (f32)pp2->posZ;
            f32 lineErr2 = sqrtf((dx2 * dx2) + (dz2 * dz2));
            sCpuLogCount++;
            /* lineErr = how far off its racing line; turn = unk_104 turn-speed-reduction
             * (final speed *= 1-turn), so high turn in a corner = bleeding speed steering hard. */
            printf("CPUAI p%d def=%d rank=%d FAST=%d code=%d boost=%.1f spd=%.2f beh=%d lineErr=%.0f sect=%d turn=%.2f\n",
                   playerId, deficit, (s32)gGPCurrentRaceRankByPlayerId[playerId],
                   flag, code, player->unk_0E8, player->speed,
                   (s32)gSpeedCPUBehaviour[playerId],
                   lineErr2, (s32)pp2->trackSectionId, player->unk_104);
        }
        sPrevDecision[playerId] = code;
        sPrevFlag[playerId]     = flag;
    }
    /* ---- WHY does the kart scrub speed? cause of each speed collapse ---- */
    if ((player->type & PLAYER_HUMAN) != PLAYER_HUMAN) {
        f32 sp   = player->speed;
        s32 low  = (sp < 3.3f);                       /* scrubbing threshold (matches analysis) */
        s32 edge = low && !sScrubbing[playerId];      /* the frame it dropped into a scrub */
        if (++sScrubHb[playerId] >= 30) sScrubHb[playerId] = 0;
        if ((edge || (low && sScrubHb[playerId] == 0)) && sScrubLogCount < 4000) {
            u16 ppid = gNearestPathPointByPlayerId[playerId];
            TrackPathPoint* pp = &gTrackPaths[gPathIndexByPlayerId[playerId]][ppid];
            f32 dx = player->pos[0] - (f32)pp->posX;  /* offset from nearest racing-line point */
            f32 dz = player->pos[2] - (f32)pp->posZ;
            f32 lineErr = sqrtf((dx * dx) + (dz * dz));
            sScrubLogCount++;
            /* surf: 1=ASPHALT 2=DIRT(on-course Luigi); 8=GRASS(off-course); 0=AIRBORNE.
             * lineErr high => off the racing line (heading for a wall); dSpd very negative => hard bonk. */
            printf("CPUSCRUB p%d %s surf=%d lineErr=%.0f ppid=%d sect=%d spd=%.2f dSpd=%.2f FAST=%d\n",
                   playerId, edge ? "EDGE" : "hold", (s32)player->surfaceType,
                   lineErr, (s32)ppid, (s32)pp->trackSectionId,
                   sp, sp - sPrevSpeed[playerId],
                   (player->effects & CPU_FAST_EFFECT) ? 1 : 0);
        }
        sScrubbing[playerId] = (u8)low;
        sPrevSpeed[playerId] = sp;
    }
#endif
}
