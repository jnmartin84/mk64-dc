#include <ultra64.h>
#include <macros.h>
#include <PR/ucode.h>

#include "audio/synthesis.h"
#include "audio/seqplayer.h"
#include "audio/port_eu.h"
#include "audio/load.h"
#include "audio/heap.h"
#include "audio/data.h"
static inline uint32_t Swap32(uint32_t val)
{
	return ((((val)&0xff000000) >> 24) | (((val)&0x00ff0000) >> 8) |
		(((val)&0x0000ff00) << 8) | (((val)&0x000000ff) << 24));
}

static inline short SwapShort(short dat)
{
        return ((((dat << 8) | (dat >> 8 & 0xff)) << 16) >> 16);
}

s32 AosSendMesg( OSMesgQueue *mq,  OSMesg msg,  s32 flag);
s32 AosRecvMesg( OSMesgQueue *mq,  OSMesg *msg,  s32 flag);

volatile s32 gThing = 0;
volatile s32 gThingSent = 0;


OSMesgQueue D_801937C0 = {0};
OSMesgQueue D_801937D8 = {0};
OSMesgQueue D_801937F0 = {0};
OSMesgQueue D_80193808 = {0};

struct EuAudioCmd sAudioCmd[0x100] = {0};

// Seems oversized by 1
OSMesg D_80194020[8] = {0};
OSMesg D_80194028[16] = {0};
OSMesg D_80194038[4] = {0};
OSMesg D_8019403C[4] = {0};

u8 D_800EA3A0[] = { 0, 0, 0, 0 };

u8 D_800EA3A4[] = { 0, 0, 0, 0 };

OSMesgQueue* D_800EA3A8 = &D_801937C0;
OSMesgQueue* D_800EA3AC = &D_801937D8;
OSMesgQueue* D_800EA3B0 = &D_801937F0;
OSMesgQueue* D_800EA3B4 = &D_80193808;

char port_eu_unused_string0[] = "DAC:Lost 1 Frame.\n";
char port_eu_unused_string1[] = "DMA: Request queue over.( %d )\n";
char port_eu_unused_string2[] = "DMA [ %d lines] TIMEOUT\n";
char port_eu_unused_string3[] = "Warning: WaveDmaQ contains %d msgs.\n";
char port_eu_unused_string4[] = "Audio:now-max tasklen is %d / %d\n";
char port_eu_unused_string5[] = "Audio:Warning:ABI Tasklist length over (%d)\n";

s32 D_800EA484 = 128;

char port_eu_unused_string6[] = "AudioSend: %d -> %d (%d)\n";

s32 D_800EA4A4 = 0;

char port_eu_unused_string7[] = "Undefined Port Command %d\n";
extern volatile s32 gPresetId;
extern volatile s32 gPresetSent;

void create_next_audio_buffer(s16* samples, u32 num_samples) {
//    static s32 gMaxAbiCmdCnt = 128;   
    s32 abiCmdCount = 0;
//    OSMesg specId;
    OSMesg msg = {0};

    gAudioFrameCount++;
    gCurrAiBufferIndex %= 3;

    gCurrAudioFrameDmaCount = 0;
    decrease_sample_dma_ttls();

    if (gPresetSent) {
        gPresetSent = 0;
        gAudioResetPresetIdToLoad = (u8)gPresetId;
        gAudioResetStatus = 5;
    }
    
//    if (AosRecvMesg(D_800EA3B0, &specId, 0) != -1) {
//        gAudioResetPresetIdToLoad = (u8) (u32)specId;
//        gAudioResetStatus = 5;
//    }

    if (gAudioResetStatus != 0) {
        if (audio_shut_down_and_reset_step() == 0) {
//            if (gAudioResetStatus == 0) {
//                AosSendMesg(D_800EA3B4, (OSMesg) (u32) gAudioResetPresetIdToLoad/* OS_MESG_8(gAudioResetPresetIdToLoad) */, OS_MESG_NOBLOCK);
//            }
            return;  
        }
    }

    if (gThingSent) {
        msg = (u32)gThing;
        gThingSent = 0;
        func_800CBCB0((u32) msg);
    }

//    if (AosRecvMesg(D_800EA3AC, &msg, 0) != -1) {
//        func_800CBCB0((u32)msg);
//    }

    gAudioCmd = gAudioCmdBuffers[gAudioTaskIndex]; 
    gAudioCmd = synthesis_execute((Acmd*) gAudioCmd, &abiCmdCount, samples, num_samples);
    gAudioRandom = osGetCount() * (gAudioRandom + gAudioFrameCount);
}

struct SPTask* create_next_audio_frame_task(void) {
    u32 samplesRemainingInAI = 0;
    s32 writtenCmds = 0;
    s32 index = 0;
    OSTask_t* task = NULL;
    s32 var_s0 = 0;
    s16* currAiBuffer = NULL;
    s32 oldDmaCount = 0;
    OSMesg sp58 = {0};
    OSMesg sp54 = {0};
    s32 writtenCmdsCopy = 0;

    gAudioFrameCount++;
    if ((gAudioFrameCount % gAudioBufferParameters.presetUnk4) != 0) {
        return NULL;
    }
    osSendMesg(D_800EA3A8, (OSMesg) gAudioFrameCount, OS_MESG_NOBLOCK);

    gAudioTaskIndex ^= 1;
    gCurrAiBufferIndex++;
    gCurrAiBufferIndex %= NUMAIBUFFERS;
    index = (gCurrAiBufferIndex + 1) % NUMAIBUFFERS;
    samplesRemainingInAI = osAiGetLength() / 4;

    if (gAiBufferLengths[index] != 0) {
        osAiSetNextBuffer(gAiBuffers[index], gAiBufferLengths[index] * 4);
    }
    oldDmaCount = gCurrAudioFrameDmaCount;
    for (var_s0 = 0; var_s0 < gCurrAudioFrameDmaCount; var_s0++) {
        if (osRecvMesg(&gCurrAudioFrameDmaQueue, NULL, 0) == 0) {
            oldDmaCount -= 1;
        }
    }
    if (oldDmaCount != 0) {
        for (var_s0 = 0; var_s0 < oldDmaCount; var_s0++) {
            osRecvMesg(&gCurrAudioFrameDmaQueue, NULL, 1);
        }
    }
    oldDmaCount = gCurrAudioFrameDmaQueue.validCount;
    if (oldDmaCount != 0) {
        for (var_s0 = 0; var_s0 < oldDmaCount; var_s0++) {
            osRecvMesg(&gCurrAudioFrameDmaQueue, NULL, 0);
        }
    }
    gCurrAudioFrameDmaCount = 0;
    decrease_sample_dma_ttls();
    if (osRecvMesg(D_800EA3B0, &sp58, 0) != -1) {
        gAudioResetPresetIdToLoad = (u8) (u32) sp58;
        gAudioResetStatus = 5;
    }
    if (gAudioResetStatus != 0) {
        if (audio_shut_down_and_reset_step() == 0) {
            if (gAudioResetStatus == 0) {
                osSendMesg(D_800EA3B4, (OSMesg) (u32) gAudioResetPresetIdToLoad, OS_MESG_NOBLOCK);
            }
            return NULL;
        }
    }

    gAudioTask = &gAudioTasks[gAudioTaskIndex];
    gAudioCmd = gAudioCmdBuffers[gAudioTaskIndex];
    index = gCurrAiBufferIndex;
    currAiBuffer = gAiBuffers[index];
    gAiBufferLengths[index] =
        ((gAudioBufferParameters.samplesPerFrameTarget - samplesRemainingInAI + EXTRA_BUFFERED_AI_SAMPLES_TARGET) &
         ~0xF) +
        SAMPLES_TO_OVERPRODUCE;
    if (gAiBufferLengths[index] < gAudioBufferParameters.minAiBufferLength) {
        gAiBufferLengths[index] = gAudioBufferParameters.minAiBufferLength;
    }
    if (gAiBufferLengths[index] > gAudioBufferParameters.maxAiBufferLength) {
        gAiBufferLengths[index] = gAudioBufferParameters.maxAiBufferLength;
    }
//    if (osRecvMesg(D_800EA3AC, &sp54, 0) != -1) {
    if (gThingSent) {
        sp54 = (u32)gThing;
        gThingSent = 0;
        func_800CBCB0((u32) sp54);
    }
    gAudioCmd = synthesis_execute((Acmd*) gAudioCmd, &writtenCmds, currAiBuffer, gAiBufferLengths[index]);
    gAudioRandom = osGetCount() * (gAudioRandom + gAudioFrameCount);
    gAudioRandom = gAudioRandom + gAiBuffers[index][gAudioFrameCount & 0xFF];

    index = gAudioTaskIndex;
    gAudioTask->msgqueue = NULL;
    // wtf?
    writtenCmdsCopy += 0;
    gAudioTask->msg = NULL;

    task = &gAudioTask->task.t;
    task->type = M_AUDTASK;
    task->flags = 0;
    task->ucode_boot = rspF3DBootStart;
    task->ucode_boot_size = (u8*) rspF3DBootEnd - (u8*) rspF3DBootStart;
    task->ucode = rspAspMainStart;
    task->ucode_data = rspAspMainDataStart;
    task->ucode_size = 0x1000; // (This size is ignored (according to sm64))
    task->ucode_data_size = (rspAspMainDataEnd - rspAspMainDataStart) * sizeof(u64);
    task->dram_stack = NULL;
    task->dram_stack_size = 0;
    task->output_buff = NULL;
    task->output_buff_size = NULL;
    task->data_ptr = (u64*) gAudioCmdBuffers[index];
    task->data_size = writtenCmds * sizeof(u64);
    task->yield_data_ptr = NULL;
    task->yield_data_size = 0;
    writtenCmdsCopy = writtenCmds;
    if (D_800EA484 < writtenCmds) {
        D_800EA484 = writtenCmdsCopy;
    }
    return gAudioTask;
}

void eu_process_audio_cmd(struct EuAudioCmd* cmd) {
    s32 i = 0;

    switch (cmd->u.s.op) {
        case 0x81:
            preload_sequence(cmd->u.s.arg2, 3);
            break;

        case 0x82:
        case 0x88:
            load_sequence(cmd->u.s.bankId, cmd->u.s.arg2, cmd->u.s.arg3);
            func_800CBA64(cmd->u.s.bankId, Swap32(cmd->u2.as_s32));
            break;

        case 0x83:
            if (gSequencePlayers[cmd->u.s.bankId].enabled != false) {
                if (cmd->u2.as_s32 == 0) {
                    sequence_player_disable(&gSequencePlayers[cmd->u.s.bankId]);
                } else {
                    seq_player_fade_to_zero_volume(cmd->u.s.bankId, Swap32(cmd->u2.as_s32));
                }
            }
            break;

        case 0xf0:
            gAudioLibSoundMode = Swap32(cmd->u2.as_s32);
            break;

        case 0xf1:
            for (i = 0; i < 4; i++) {
                gSequencePlayers[i].muted = true;
                gSequencePlayers[i].recalculateVolume = true;
            }
            break;

        case 0xf2:
            for (i = 0; i < 4; i++) {
                gSequencePlayers[i].muted = false;
                gSequencePlayers[i].recalculateVolume = true;
            }
            break;
        case 0xF3:
            func_800BB388(cmd->u.s.bankId, cmd->u.s.arg2, cmd->u.s.arg3);
            break;
    }
}

void seq_player_fade_to_zero_volume(s32 arg0, s32 fadeOutTime) {
    struct SequencePlayer* player = NULL;

    if (fadeOutTime == 0) {
        fadeOutTime = 1;
    }
    player = &gSequencePlayers[arg0];
    player->state = 2;
    player->fadeRemainingFrames = fadeOutTime;
    player->fadeVelocity = -(player->fadeVolume / fadeOutTime);
}

void func_800CBA64(s32 playerIndex, s32 fadeInTime) {
    struct SequencePlayer* player = NULL;

    if (fadeInTime != 0) {
        player = &gSequencePlayers[playerIndex];
        player->state = 1;
        player->fadeTimerUnkEu = fadeInTime;
        player->fadeRemainingFrames = fadeInTime;
        player->fadeVolume = 0.0f;
        player->fadeVelocity = 0.0f;
    }
}

void port_eu_init_queues(void) {
    D_800EA3A0[0] = 0;
    D_800EA3A4[0] = 0;
    osCreateMesgQueue(D_800EA3A8, D_80194020, 1);
    osCreateMesgQueue(D_800EA3AC, D_80194028, 4);
    osCreateMesgQueue(D_800EA3B0, D_80194038, 1);
    osCreateMesgQueue(D_800EA3B4, D_8019403C, 1);
}

void func_800CBB48(s32 arg0, s32* arg1) {
    struct EuAudioCmd* cmd = &sAudioCmd[D_800EA3A0[0] & 0xff];
    cmd->u.first = arg0;
    cmd->u2.as_u32 = *arg1;
    D_800EA3A0[0]++;
}

void func_800CBB88(u32 arg0, f32 arg1) {
    func_800CBB48(arg0, (s32*) &arg1);
   // struct EuAudioCmd* cmd = &sAudioCmd[D_800EA3A0[0] & 0xff];
    //cmd->u.first = arg0;
    //cmd->u2.as_f32 = arg1;
    //D_800EA3A0[0]++;
}

void func_800CBBB8(u32 arg0, u32 arg1) {
    func_800CBB48(arg0, (s32*) &arg1);
}

void func_800CBBE8(u32 arg0, s8 arg1) {
    s32 sp34 = arg1 << 24;
    func_800CBB48(arg0, &sp34);
}


//! @todo clenanup, something's weird with the variables. D_800EA4A4 is probably EuAudioCmd bc of the + 0x100
void func_800CBC24(void) {
    s32 temp_t6 = 0;
    s32 test = 0;
    OSMesg thing = {0};
    temp_t6 = D_800EA3A0[0] - D_800EA3A4[0];
    test = (u8) temp_t6;
    test = (test + 0x100) & 0xFF;
    do {
    } while (0);
    if (D_800EA4A4 < test) {
        D_800EA4A4 = test;
    }
    thing = (OSMesg) ((D_800EA3A0[0] & 0xFF) | ((D_800EA3A4[0] & 0xFF) << 8));
//    osSendMesg(D_800EA3AC, thing, 0);
    gThing = thing;
    gThingSent = 1;
    D_800EA3A4[0] = D_800EA3A0[0];
}


void func_800CBCB0(u32 arg0) {
    struct EuAudioCmd* cmd = NULL;
    struct SequencePlayer* seqPlayer = NULL;
    struct SequenceChannel* chan = NULL;
    u8 end = arg0 & 0xff;
    u8 i = (arg0 >> 8) & 0xff;

    for (;;) {
        if (i == end) {
            break;
        }
        cmd = &sAudioCmd[i++ & 0xff];

        if ((cmd->u.s.op & 0xf0) == 0xf0) {
            eu_process_audio_cmd(cmd);
            goto why;
        }

        if (cmd->u.s.bankId < SEQUENCE_PLAYERS) {
            seqPlayer = &gSequencePlayers[cmd->u.s.bankId];
            if ((cmd->u.s.op & 0x80) != 0) {
                eu_process_audio_cmd(cmd);
            } else if ((cmd->u.s.op & 0x40) != 0) {
                switch (cmd->u.s.op) {
                    case 0x41:
                        seqPlayer->fadeVolumeScale = Swap32(cmd->u2.as_f32);
                        seqPlayer->recalculateVolume = true;
                        break;

                    case 0x47:
                        seqPlayer->tempo = Swap32(cmd->u2.as_s32) * TATUMS_PER_BEAT;
                        printf("!!! TEMPO IS %08x\n", seqPlayer->tempo);
                        break;

                    case 0x48:
                        seqPlayer->transposition = cmd->u2.as_s8;
                        break;

                    case 0x46:
                        seqPlayer->seqVariationEu[cmd->u.s.arg3] = cmd->u2.as_s8;
                        break;
                }
            } else if (seqPlayer->enabled != false && cmd->u.s.arg2 < 0x10) {
                chan = seqPlayer->channels[cmd->u.s.arg2];
                if (IS_SEQUENCE_CHANNEL_VALID(chan)) {
                    switch (cmd->u.s.op) {
                        case 1:
                            chan->volumeScale = Swap32(cmd->u2.as_f32);
                            printf("chan->volScale %f\n", chan->volumeScale);
                            chan->changes.as_bitfields.volume = true;
                            break;
                        case 2:
                            chan->volume = Swap32(cmd->u2.as_f32);
                            printf("chan->volume %f\n", chan->volume);
                            chan->changes.as_bitfields.volume = true;
                            break;
                        case 3:
                            chan->newPan = cmd->u2.as_s8;
                            chan->changes.as_bitfields.pan = true;
                            break;
                        case 4:
                            chan->freqScale = Swap32(cmd->u2.as_f32);
                            printf("chan->freqScale %f\n", chan->freqScale);
                            chan->changes.as_bitfields.freqScale = true;
                            break;
                        case 5:
                            chan->reverbVol = cmd->u2.as_s8;
                            break;
                        case 6:
                            if (cmd->u.s.arg3 < 8) {
                                chan->soundScriptIO[cmd->u.s.arg3] = cmd->u2.as_s8;
                            }
                            break;
                        case 8:
                            chan->stopSomething2 = cmd->u2.as_s8;
                    }
                }
            }
        }

    why:
        cmd->u.s.op = 0;
    }
}

void port_eu_init() {
    port_eu_init_queues();
}
