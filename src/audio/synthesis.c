#include <ultra64.h>
#include <macros.h>
#include "audio/synthesis.h"
#include "audio/heap.h"
#include "audio/data.h"
#include "audio/load.h"
#include "audio/seqplayer.h"
#include "audio/internal.h"
// #include "audio/external.h"
//#include "PR/abi.h"
#undef VIRTUAL_TO_PHYSICAL2
#include "mixer.h"
#include <stdio.h>
#define aSetLoadBufferPair(pkt, c, off)                                               \
    aSetBuffer(pkt, 0, c + DMEM_ADDR_WET_LEFT_CH, 0, DEFAULT_LEN_1CH - c);            \
    aLoadBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.left + (off))); \
    aSetBuffer(pkt, 0, c + DMEM_ADDR_WET_RIGHT_CH, 0, DEFAULT_LEN_1CH - c);           \
    aLoadBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.right + (off)))

#define aSetSaveBufferPair(pkt, c, d, off)                                            \
    aSetBuffer(pkt, 0, 0, c + DMEM_ADDR_WET_LEFT_CH, d);                              \
    aSaveBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.left + (off))); \
    aSetBuffer(pkt, 0, 0, c + DMEM_ADDR_WET_RIGHT_CH, d);                             \
    aSaveBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.right + (off)));

struct VolumeChange {
    u16 sourceLeft;
    u16 sourceRight;
    u16 targetLeft;
    u16 targetRight;
};

struct SynthesisReverb gSynthesisReverbs[4] = {0};
u8 sAudioSynthesisPad[0x10] = {0};

// s16 gVolume;
// s8 gUseReverb;
// s8 gNumSynthesisReverbs;
// struct NoteSubEu *gNoteSubsEu;

// f32 gLeftVolRampings;
// f32 gRightVolRampings[3][1024];
// f32 *gCurrentLeftVolRamping; // Points to any of the three left buffers above
// f32 *gCurrentRightVolRamping; // Points to any of the three right buffers above

/**
 * Given that (almost) all of these are format strings, it is highly likely
 * that they are meant to be used in some sort of ////printf variant. But I don't
 * care to try and figure out which function gets which string(s)
 * So I've place them all here instead.
 **/
char synthesisAudioString0[] = "Terminate-Canceled Channel %d,Phase %d\n";
char synthesisAudioString1[] = "Copy %d\n";
char synthesisAudioString2[] = "%d->%d\n";
char synthesisAudioString3[] = "pitch %x: delaybytes %d : olddelay %d\n";
char synthesisAudioString4[] = "cont %x: delaybytes %d : olddelay %d\n";

// Equivalent functionality as the US/JP version,
// just that the reverb structure is chosen from an array with index
void prepare_reverb_ring_buffer(s32 chunkLen, u32 updateIndex, s32 reverbIndex) {
    struct ReverbRingBufferItem* item = NULL;
    struct SynthesisReverb* reverb = &gSynthesisReverbs[reverbIndex];
    s32 srcPos = 0;
    s32 dstPos = 0;
    s32 nSamples = 0;
    s32 excessiveSamples = 0;
//    s32 UNUSED pad[3] = {0};
    if (reverb->downsampleRate != 1) {
        if (reverb->framesLeftToIgnore == 0) {
            // Now that the RSP has finished, downsample the samples produced two frames ago by skipping
            // samples.
            item = &reverb->items[reverb->curFrame][updateIndex];

            // Touches both left and right since they are adjacent in memory
            osInvalDCache(item->toDownsampleLeft, 0x300);

            for (srcPos = 0, dstPos = 0; dstPos < item->lengthA / 2; srcPos += reverb->downsampleRate, dstPos++) {
                reverb->ringBuffer.left[item->startPos + dstPos] = item->toDownsampleLeft[srcPos];
                reverb->ringBuffer.right[item->startPos + dstPos] = item->toDownsampleRight[srcPos];
            }
            for (dstPos = 0; dstPos < item->lengthB / 2; srcPos += reverb->downsampleRate, dstPos++) {
                reverb->ringBuffer.left[dstPos] = item->toDownsampleLeft[srcPos];
                reverb->ringBuffer.right[dstPos] = item->toDownsampleRight[srcPos];
            }
        }
    }

    item = &reverb->items[reverb->curFrame][updateIndex];
    nSamples = chunkLen / reverb->downsampleRate;
    excessiveSamples = (nSamples + reverb->nextRingBufferPos) - reverb->bufSizePerChannel;
    if (excessiveSamples < 0) {
        // There is space in the ring buffer before it wraps around
        item->lengthA = nSamples * 2;
        item->lengthB = 0;
        item->startPos = (s32) reverb->nextRingBufferPos;
        reverb->nextRingBufferPos += nSamples;
    } else {
        // Ring buffer wrapped around
        item->lengthA = (nSamples - excessiveSamples) * 2;
        item->lengthB = excessiveSamples * 2;
        item->startPos = reverb->nextRingBufferPos;
        reverb->nextRingBufferPos = excessiveSamples;
    }
    // These fields are never read later
    item->numSamplesAfterDownsampling = nSamples;
    item->chunkLen = chunkLen;
}

Acmd* synthesis_load_reverb_ring_buffer(Acmd* acmd, u16 addr, u16 srcOffset, s32 len, s32 reverbIndex) {
    //printf("load_reverb_ring_buffer(%04x, %04x, %d, %d)\n", addr, srcOffset, len, reverbIndex);
    aLoadBuffer(acmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.left[srcOffset]), addr, len);
    aLoadBuffer(acmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.right[srcOffset]), addr + 0x180,
                len);
    return acmd;
}

Acmd* synthesis_save_reverb_ring_buffer(Acmd* acmd, u16 addr, u16 destOffset, s32 len, s32 reverbIndex) {
    //printf("save_reverb_ring_buffer(%04x, %04x, %d, %d)\n", addr, destOffset, len, reverbIndex);
    aSaveBuffer(acmd++, addr, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.left[destOffset]), len);
    aSaveBuffer(acmd++, addr + 0x180,
                VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.right[destOffset]), len);
    return acmd;
}

void func_800B6FB4(s32 updateIndexStart, s32 noteIndex) {
    s32 i = 0;

    for (i = updateIndexStart + 1; i < gAudioBufferParameters.updatesPerFrame; i++) {
        if (!gNoteSubsEu[gMaxSimultaneousNotes * i + noteIndex].needsInit) {
            gNoteSubsEu[gMaxSimultaneousNotes * i + noteIndex].enabled = 0;
        } else {
            break;
        }
    }
}

void synthesis_load_note_subs_eu(s32 updateIndex) {
    struct NoteSubEu* src = NULL;
    struct NoteSubEu* dest = NULL;
    s32 i = 0;

    for (i = 0; i < gMaxSimultaneousNotes; i++) {
        src = &gNotes[i].noteSubEu;
        dest = &gNoteSubsEu[gMaxSimultaneousNotes * updateIndex + i];
        if (src->enabled) {
            *dest = *src;
            src->needsInit = 0;
        } else {
            dest->enabled = 0;
        }
    }
}

Acmd* synthesis_execute(Acmd* acmd, s32* writtenCmds, s16* aiBuf, s32 bufLen) {
    s32 i = 0, j = 0;
    u32* aiBufPtr = NULL;
    Acmd* cmd = acmd = NULL;
    s32 chunkLen = 0;

    for (i = gAudioBufferParameters.updatesPerFrame; i > 0; i--) {
        process_sequences(i - 1);
        synthesis_load_note_subs_eu(gAudioBufferParameters.updatesPerFrame - i);
    }
    aSegment(cmd++, 0, 0);
    aiBufPtr = (u32*) aiBuf;
    for (i = gAudioBufferParameters.updatesPerFrame; i > 0; i--) {
        if (i == 1) {
            chunkLen = bufLen;
        } else {
            if (bufLen / i >= gAudioBufferParameters.samplesPerUpdateMax) {
                chunkLen = gAudioBufferParameters.samplesPerUpdateMax;
            } else if (bufLen / i <= gAudioBufferParameters.samplesPerUpdateMin) {
                chunkLen = gAudioBufferParameters.samplesPerUpdateMin;
            } else {
                chunkLen = gAudioBufferParameters.samplesPerUpdate;
            }
        }
        for (j = 0; j < gNumSynthesisReverbs; j++) {
            if (gSynthesisReverbs[j].useReverb != 0) {
                prepare_reverb_ring_buffer(chunkLen, gAudioBufferParameters.updatesPerFrame - i, j);
            }
        }
        cmd = synthesis_do_one_audio_update((s16*) aiBufPtr, chunkLen, cmd, gAudioBufferParameters.updatesPerFrame - i);
        bufLen -= chunkLen;
        aiBufPtr += chunkLen;
    }

    for (j = 0; j < gNumSynthesisReverbs; j++) {
        if (gSynthesisReverbs[j].framesLeftToIgnore != 0) {
            gSynthesisReverbs[j].framesLeftToIgnore--;
        }
        gSynthesisReverbs[j].curFrame ^= 1;
    }
    *writtenCmds = cmd - acmd;
    return cmd;
}

Acmd* synthesis_resample_and_mix_reverb(Acmd* acmd, s32 bufLen, s16 reverbIndex, s16 updateIndex) {
    struct ReverbRingBufferItem* item = NULL;
    s16 startPad = 0;
    s16 paddedLengthA = 0;

    item = &gSynthesisReverbs[reverbIndex].items[gSynthesisReverbs[reverbIndex].curFrame][updateIndex];
    aClearBuffer(acmd++, 0x840, 0x300);
    if (gSynthesisReverbs[reverbIndex].downsampleRate == 1) {

        acmd = synthesis_load_reverb_ring_buffer(acmd, 0x840, item->startPos, item->lengthA, reverbIndex);
        if (item->lengthB != 0) {
            acmd = synthesis_load_reverb_ring_buffer(acmd, item->lengthA + 0x840, 0U, item->lengthB, reverbIndex);
        }

////        aSMix(acmd++, 0x7fff, 0x840, 0x540, 0x300);
//        aMix(acmd++, 0x8000 + gSynthesisReverbs[reverbIndex].reverbGain, 0x840, 0x840, 0x300);
    } else {
        startPad = (item->startPos % 8U) * 2;
        paddedLengthA = ALIGN(startPad + item->lengthA, 4);

        acmd =
            synthesis_load_reverb_ring_buffer(acmd, 0x0020, item->startPos - (startPad / 2), 0x00000180, reverbIndex);
        if (item->lengthB != 0) {
            acmd = synthesis_load_reverb_ring_buffer(acmd, paddedLengthA + 0x20, 0, 0x180 - paddedLengthA, reverbIndex);
        }

        aSetBuffer(acmd++, 0, 0x20 + startPad, 0x840, bufLen * 2);
        aResample(acmd++, gSynthesisReverbs[reverbIndex].resampleFlags, gSynthesisReverbs[reverbIndex].resampleRate,
                  VIRTUAL_TO_PHYSICAL2(gSynthesisReverbs[reverbIndex].resampleStateLeft));
        aSetBuffer(acmd++, 0, 0x1A0 + startPad, 0x9C0, bufLen * 2);
        aResample(acmd++, gSynthesisReverbs[reverbIndex].resampleFlags, gSynthesisReverbs[reverbIndex].resampleRate,
                  VIRTUAL_TO_PHYSICAL2(gSynthesisReverbs[reverbIndex].resampleStateRight));
 ////       aSMix(acmd++, 0x7fff, 0x840, 0x540, 0x300);
 //       aMix(acmd++, 0x8000 + gSynthesisReverbs[reverbIndex].reverbGain, 0x840, 0x840, 0x300);
    }
    return acmd;
}

Acmd* synthesis_save_reverb_samples(Acmd* acmd, s16 reverbIndex, s16 updateIndex) {
    struct ReverbRingBufferItem* item = NULL;

    item = &gSynthesisReverbs[reverbIndex].items[gSynthesisReverbs[reverbIndex].curFrame][updateIndex];
    if (gSynthesisReverbs[reverbIndex].useReverb != 0) {
        switch (gSynthesisReverbs[reverbIndex].downsampleRate) {
            case 1:
                acmd = synthesis_save_reverb_ring_buffer(acmd, 0x840, item->startPos, item->lengthA, reverbIndex);
                if (item->lengthB != 0) {
                    acmd =
                        synthesis_save_reverb_ring_buffer(acmd, 0x840 + item->lengthA, 0, item->lengthB, reverbIndex);
                }
                break;
            default:
                aSaveBuffer(acmd++, 0x840,
                            VIRTUAL_TO_PHYSICAL2(gSynthesisReverbs[reverbIndex]
                                                     .items[gSynthesisReverbs[reverbIndex].curFrame][updateIndex]
                                                     .toDownsampleLeft),
                            0x300);
                gSynthesisReverbs[reverbIndex].resampleFlags = 0;
                break;
        }
    }
    return acmd;
}

Acmd* synthesis_do_one_audio_update(s16* aiBuf, s32 bufLen, Acmd* acmd, s32 updateIndex) {
    struct NoteSubEu* noteSubEu = NULL;
    u8 noteIndices[56] = {0};
    s32 temp = 0;
    s32 i = 0;
    s16 j = 0;
    s16 notePos = 0;

    if (gNumSynthesisReverbs == 0) {
        for (i = 0; i < gMaxSimultaneousNotes; i++) {
            if (gNoteSubsEu[gMaxSimultaneousNotes * updateIndex + i].enabled) {
                noteIndices[notePos++] = i;
            }
        }
    } else {
        for (j = 0; j < gNumSynthesisReverbs; j++) {
            for (i = 0; i < gMaxSimultaneousNotes; i++) {
                noteSubEu = &gNoteSubsEu[gMaxSimultaneousNotes * updateIndex + i];
                if (noteSubEu->enabled && j == noteSubEu->reverbIndex) {
                    noteIndices[notePos++] = i;
                }
            }
        }

        for (i = 0; i < gMaxSimultaneousNotes; i++) {
            noteSubEu = &gNoteSubsEu[gMaxSimultaneousNotes * updateIndex + i];
            if (noteSubEu->enabled && noteSubEu->reverbIndex >= gNumSynthesisReverbs) {
                noteIndices[notePos++] = i;
            }
        }
    }
    aClearBuffer(acmd++, DMEM_ADDR_LEFT_CH, DEFAULT_LEN_2CH);
    i = 0;
    for (j = 0; j < gNumSynthesisReverbs; j++) {
        gUseReverb = 0;//gSynthesisReverbs[j].useReverb;
        if (gUseReverb != 0) {
            acmd = synthesis_resample_and_mix_reverb(acmd, bufLen, j, updateIndex);
        }
        for (; i < notePos; i++) {
            temp = updateIndex * gMaxSimultaneousNotes;
            if (j == gNoteSubsEu[temp + noteIndices[i]].reverbIndex) {
                acmd = synthesis_process_note(noteIndices[i], &gNoteSubsEu[temp + noteIndices[i]],
                                              &gNotes[noteIndices[i]].synthesisState, aiBuf, bufLen, acmd, updateIndex);
                continue;
            } else {
                break;
            }
        }
        if (gSynthesisReverbs[j].useReverb != 0) {
            acmd = synthesis_save_reverb_samples(acmd, j, updateIndex);
        }
    }
    for (; i < notePos; i++) {
        temp = updateIndex * gMaxSimultaneousNotes;
        if (IS_BANK_LOAD_COMPLETE(gNoteSubsEu[temp + noteIndices[i]].bankId)/*  == true */) {
            acmd = synthesis_process_note(noteIndices[i], &gNoteSubsEu[temp + noteIndices[i]],
                                          &gNotes[noteIndices[i]].synthesisState, aiBuf, bufLen, acmd, updateIndex);
        } else {
            gAudioErrorFlags = (gNoteSubsEu[temp + noteIndices[i]].bankId + (i << 8)) + 0x10000000;
        }
    }

    temp = bufLen * 2;
    aSetBuffer(acmd++, 0, 0, DMEM_ADDR_TEMP, temp);
    aInterleave(acmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH);
    aSaveBuffer(acmd++, DMEM_ADDR_TEMP, VIRTUAL_TO_PHYSICAL2(aiBuf), temp * 2);
    return acmd;
}
#include <stdlib.h>
//uint16_t sometmpstate[16];
Acmd* synthesis_process_note(s32 noteIndex, struct NoteSubEu* noteSubEu, struct NoteSynthesisState* synthesisState,
                             UNUSED s16* aiBuf, s32 inBuf, Acmd* cmd, s32 updateIndex) {
//    s32 pad[3] = {0};
    struct AudioBankSample *audioBookSample = NULL; // sp130
    struct AdpcmLoop *loopInfo = NULL; // sp12C
    s16 *curLoadedBook = NULL; // sp128
    s32 pad4 = 0;
    s32 nSamplesToLoad = 0;
    s32 noteFinished = 0; // sp11C
    s32 restart = 0; // sp118
    s32 flags = 0;
    u16 resamplingRateFixedPoint = 0; // sp112
//    s32 pad2[1] ={ 0};
    u16 headsetPanRight = 0;
    s32 loopInfo_2 = 0;
    s32 a1 = 0;
//    s32 pad51232132123 = 0;
    s32 spFC = 0; // spFC
    s32 pad3 = 0;
    s32 nAdpcmSamplesProcessed = 0;
    s32 s4 = 0;
    u8 *sampleAddr = NULL; // spEC
    s32 s3  = 0;
    s32 samplesLenAdjusted = 0; // spE4
    s32 leftRight = 0;
    s32 endPos = 0; // spDC
    s32 nSamplesToProcess = 0; // spD8
    u32 samplesLenFixedPoint = 0;
    s32 var_s6 = 0;
    s32 nSamplesInThisIteration = 0;
    u32 var_t2 = 0;
    u8 *var_a0_2 = NULL;
    s32 s5Aligned = 0;
    s32 temp_t6 = 0;
    s32 aligned = 0;
    struct AudioBankSample *bankSample=NULL;
    s32 nParts = 0; // spB0
    s32 curPart = 0; // spAC
//    s32 pad5 = 0;
    s16 addr = 0;
    s32 resampledTempLen = 0; // spA0
    u16 noteSamplesDmemAddrBeforeResampling = 0;
    s32 samplesRemaining = 0;
    s32 s1 = 0;
    u32 nEntries = 0;
    struct Note *note = NULL;

    curLoadedBook = NULL;
    note = &gNotes[noteIndex];
    flags = 0;
    if (noteSubEu->needsInit/*  == true */) {
        flags = A_INIT;
        synthesisState->restart = 0;
        synthesisState->samplePosInt = 0;
        synthesisState->samplePosFrac = 0;
        synthesisState->curVolLeft = 0;
        synthesisState->curVolRight = 0;
        synthesisState->prevHeadsetPanRight = 0;
        synthesisState->prevHeadsetPanLeft = 0;
    }
    resamplingRateFixedPoint = noteSubEu->resamplingRateFixedPoint;

    nParts = noteSubEu->hasTwoAdpcmParts + 1;
    samplesLenFixedPoint = ((resamplingRateFixedPoint * inBuf) * 2) + synthesisState->samplePosFrac;
    nSamplesToLoad = samplesLenFixedPoint >> 0x10;
    synthesisState->samplePosFrac = samplesLenFixedPoint & 0xFFFF;

    if (noteSubEu->isSyntheticWave) {
        cmd = load_wave_samples(cmd, noteSubEu, synthesisState, nSamplesToLoad);

        noteSamplesDmemAddrBeforeResampling = (synthesisState->samplePosInt * 2) + 0x1A0; // DMEM_ADDR_UNCOMPRESSED_NOTE
        synthesisState->samplePosInt += nSamplesToLoad;
    } else {
        audioBookSample = noteSubEu->sound.audioBankSound->sample;

        loopInfo = audioBookSample->loop;
        endPos = __builtin_bswap32(loopInfo->end);
        sampleAddr = audioBookSample->sampleAddr;
        resampledTempLen = 0;

        for (curPart = 0; curPart < nParts; curPart++) {
            bankSample = audioBookSample;
            nAdpcmSamplesProcessed = 0;
            s4 = 0;

            if (nParts == 1) {
                samplesLenAdjusted = nSamplesToLoad;
            } else if (nSamplesToLoad & 1) {
                samplesLenAdjusted = (nSamplesToLoad & (~1)) + (curPart * 2);
            } else {
                samplesLenAdjusted = nSamplesToLoad;
            }

            if (curLoadedBook != (*bankSample->book).book) {
                curLoadedBook = bankSample->book->book;
                nEntries = (16 * __builtin_bswap32(bankSample->book->order)) * __builtin_bswap32(bankSample->book->npredictors);
                aLoadADPCM(cmd++, nEntries, VIRTUAL_TO_PHYSICAL2(noteSubEu->bookOffset + curLoadedBook));
            }
            if (noteSubEu->bookOffset != 0) {
                curLoadedBook = &gUnknownData_800F6290[0];
            }
            while (nAdpcmSamplesProcessed != samplesLenAdjusted) {
                noteFinished = 0;
                restart = 0;

                s3 = synthesisState->samplePosInt & 0xF;
                samplesRemaining = endPos - synthesisState->samplePosInt;
                nSamplesToProcess = samplesLenAdjusted - nAdpcmSamplesProcessed;
                if ((s3 == 0) && (synthesisState->restart == 0)) {
                    s3 = 16;
                }

                a1 = 16 - s3;

                if (nSamplesToProcess < samplesRemaining) {
                    loopInfo_2 = ((nSamplesToProcess - a1) + 0xF) / 16;
                    s1 = loopInfo_2 * 16;
                    var_s6 = (a1 + s1) - nSamplesToProcess;
                } else {
                    s1 = samplesRemaining - a1;
                    var_s6 = 0;
                    if (s1 <= 0) {
                        s1 = 0;
                        a1 = samplesRemaining;
                    }
                    loopInfo_2 = (s1 + 0xF) / 16;
                    if (loopInfo->count != 0) {
                        restart = 1;
                    } else {
                        noteFinished = 1;
                    }
                }

                if (loopInfo_2 != 0) {
                    temp_t6 = ((synthesisState->samplePosInt - s3) + 16) / 16; // diff from sm64 sh
                    if (bankSample->loaded == 0x81) {
                        var_a0_2 = (temp_t6 * 9) + sampleAddr;
                    } else {
                        var_a0_2 =
                            dma_sample_data((uintptr_t) (temp_t6 * 9) + sampleAddr, ALIGN(((loopInfo_2 * 9) + 16), 4),
                                            flags, &synthesisState->sampleDmaIndex);
                    }

                    var_t2 = ((uintptr_t) var_a0_2) & 0xF;
                    aligned = ALIGN(((loopInfo_2 * 9) + 16), 4);
                    addr = (0x540 - aligned); // DMEM_ADDR_COMPRESSED_ADPCM_DATA
                    aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(var_a0_2 - var_t2), addr, aligned);
                } else {
                    s1 = 0; // ?
                    var_t2 = 0;
                }

                if (synthesisState->restart != 0) {
                    aSetLoop(cmd++,  (bankSample->loop->state));
                    flags = A_LOOP;
                    synthesisState->restart = 0;
                }
                nSamplesInThisIteration = (s1 + a1) - var_s6;
                s5Aligned = ALIGN(s4 + 16, 4);
                if (nAdpcmSamplesProcessed == 0) {
                    aligned = ALIGN(((loopInfo_2 * 9) + 16), 4);
                    addr = (0x540 - aligned);
                    aSetBuffer(cmd++, 0, addr + var_t2, 0x1A0, s1 * 2);
                    aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                    spFC = s3 * 2;
                } else {
                    aligned = ALIGN(((loopInfo_2 * 9) + 16), 4);
                    addr = (0x540 - aligned);
                    aSetBuffer(cmd++, 0, addr + var_t2, 0x1A0 + s5Aligned, s1 * 2);
                    aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                    aDMEMMove(cmd++, 0x1A0 + s5Aligned + (s3 * 2), 0x1A0 + s4, nSamplesInThisIteration * 2);
                }

                nAdpcmSamplesProcessed += nSamplesInThisIteration;
                switch (flags) {
                    case 1:
                        spFC = 0x20;
                        s4 = (s1 * 2) + 0x20;
                        break;
                    case 2:
                        s4 = (nSamplesInThisIteration * 2) + s4;
                        break;
                    default:
                        if (s4 != 0) {
                            s4 = (nSamplesInThisIteration * 2) + s4;
                        } else {
                            s4 = (s3 + nSamplesInThisIteration) * 2;
                        }
                        break;
                }

                flags = 0;
                if (noteFinished) {
                    aClearBuffer(cmd++, 0x1A0 + s4, (samplesLenAdjusted - nAdpcmSamplesProcessed) * 2);
                    noteSubEu->finished = 1;
                    note->noteSubEu.finished = 1;
                    note->noteSubEu.enabled = 0;
                    func_800B6FB4(updateIndex, noteIndex);
                    break;
                }

                if (restart) {
                    synthesisState->restart = 1;
                    ////printf("loopInfo->start %08x\n", loopInfo->start);
                    synthesisState->samplePosInt = __builtin_bswap32(loopInfo->start);
                } else {
                    synthesisState->samplePosInt += nSamplesToProcess;
                }
            }

            switch (nParts) {
                case 1:
                    noteSamplesDmemAddrBeforeResampling = 0x1A0 + spFC;
                    break;
                case 2:
                    switch (curPart) {
                        case 0:
                            aDownsampleHalf(cmd++, ALIGN(samplesLenAdjusted / 2, 3), 0x1A0 + spFC, DMEM_ADDR_RESAMPLED);
                            resampledTempLen = samplesLenAdjusted;
                            noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_RESAMPLED;
                            if (noteSubEu->finished != 0) {
                                aClearBuffer(cmd++, noteSamplesDmemAddrBeforeResampling + resampledTempLen,
                                             samplesLenAdjusted + 0x10);
                            }
                            break;
                        case 1:
                            aDownsampleHalf(cmd++, ALIGN(samplesLenAdjusted / 2, 3), DMEM_ADDR_RESAMPLED2 + spFC,
                                            resampledTempLen + DMEM_ADDR_RESAMPLED);
                            break;
                    }
            }
            if (noteSubEu->finished != 0) {
                break;
            }
        }
    }
    flags = 0;
    if (noteSubEu->needsInit/*  == true */) {
        flags = A_INIT;
        noteSubEu->needsInit = 0;
    }

    cmd = final_resample(cmd, synthesisState, inBuf * 2, resamplingRateFixedPoint, noteSamplesDmemAddrBeforeResampling, flags);
    headsetPanRight = noteSubEu->headsetPanRight;
    if ((headsetPanRight & 0xFFFF) || synthesisState->prevHeadsetPanRight) {
        leftRight = 1;
    } else if (noteSubEu->headsetPanLeft || synthesisState->prevHeadsetPanLeft) {
        leftRight = 2;
    } else {
        leftRight = 0;
    }
    cmd = func_800B86A0(cmd, noteSubEu, synthesisState, inBuf, 0, leftRight, flags);
    if (noteSubEu->usesHeadsetPanEffects) {
        cmd = note_apply_headset_pan_effects(cmd, noteSubEu, synthesisState, inBuf * 2, flags, leftRight);
    }
    return cmd;
}

Acmd* load_wave_samples(Acmd* acmd, struct NoteSubEu* noteSubEu, struct NoteSynthesisState* synthesisState,
                        s32 nSamplesToLoad) {
    s32 a3 = 0;
    s32 repeats = 0;
    aLoadBuffer(acmd++, VIRTUAL_TO_PHYSICAL2(noteSubEu->sound.samples), 0x1A0, 128);

    synthesisState->samplePosInt &= 0x3f;
    a3 = 64 - synthesisState->samplePosInt;
    if (a3 < nSamplesToLoad) {
        repeats = (nSamplesToLoad - a3 + 63) / 64;
        if (repeats != 0) {
            aDMEMMove2(acmd++, repeats, 0x1A0, 0x1A0 + 128, 128);
        }
    }
    return acmd;
}

Acmd* final_resample(Acmd* acmd, struct NoteSynthesisState* synthesisState, s32 count, u16 pitch, u16 dmemIn,
                     u32 flags) {
    aSetBuffer(acmd++, /*flags*/ 0, dmemIn, /*dmemout*/ 0, count);
    aResample(acmd++, flags, pitch, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->finalResampleState));
    return acmd;
}

Acmd* func_800B86A0(Acmd* cmd, struct NoteSubEu* note, struct NoteSynthesisState* synthesisState, s32 nSamples,
                    u16 inBuf, s32 headsetPanSettings, UNUSED u32 flags) {
    UNUSED s32 pad[2] = {0};
    u16 sourceRight = 0;
    u16 sourceLeft = 0;
    u16 targetLeft = 0;
    u16 targetRight = 0;
    s16 rampLeft = 0;
    s16 rampRight = 0;

    sourceLeft = synthesisState->curVolLeft;
    sourceRight = synthesisState->curVolRight;
    
    targetLeft = (note->targetVolLeft) << 4;
    targetRight = (note->targetVolRight) << 4;

    rampLeft  = ((targetLeft  - sourceLeft)  / (nSamples >> 3));
    rampRight = ((targetRight - sourceRight) / (nSamples >> 3));
    targetLeft  = sourceLeft  + rampLeft  * (nSamples >> 3);
    targetRight = sourceRight + rampRight * (nSamples >> 3);

    synthesisState->curVolLeft = targetLeft;
    synthesisState->curVolRight = targetRight;

    if (note->usesHeadsetPanEffects) {
        aClearBuffer(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, DEFAULT_LEN_1CH);
//        aEnvSetup1/* Alt */(cmd++, note->reverbVol, sourceLeft, /* sourceRight,  */(u32)rampLeft, (u32)rampRight);
        aEnvSetup1(cmd++, note->reverbVol, (((sourceLeft & 0xFF) << 8) | (sourceRight & 0xFF)), rampLeft, rampRight);
        aEnvSetup2(cmd++, sourceLeft, sourceRight);

        switch (headsetPanSettings) {;
            case 1:
                aEnvMixer(cmd++,
                    inBuf, nSamples,
                    0,
                    note->stereoStrongRight, note->stereoStrongLeft,
                    DMEM_ADDR_NOTE_PAN_TEMP,
                    DMEM_ADDR_RIGHT_CH,
                    DMEM_ADDR_WET_LEFT_CH,
                    DMEM_ADDR_WET_RIGHT_CH);
                break;
            case 2:
                aEnvMixer(cmd++,
                    inBuf, nSamples,
                    0,
                    note->stereoStrongRight, note->stereoStrongLeft,
                    DMEM_ADDR_LEFT_CH,
                    DMEM_ADDR_NOTE_PAN_TEMP,
                    DMEM_ADDR_WET_LEFT_CH,
                    DMEM_ADDR_WET_RIGHT_CH);
                break;
            default:
                aEnvMixer(cmd++,
                    inBuf, nSamples,
                    0,
                    note->stereoStrongRight, note->stereoStrongLeft,
                    DMEM_ADDR_LEFT_CH,
                    DMEM_ADDR_RIGHT_CH,
                    DMEM_ADDR_WET_LEFT_CH,
                    DMEM_ADDR_WET_RIGHT_CH);
                break;
        }
    } else {
        //aEnvSetup1/* Alt */(cmd++, note->reverbVol, sourceLeft, /* sourceRight,  */(u32)rampLeft, (u32)rampRight);
        aEnvSetup1(cmd++, note->reverbVol, (((sourceLeft & 0xFF) << 8) | (sourceRight & 0xFF)), rampLeft, rampRight);
        aEnvSetup2(cmd++, sourceLeft, sourceRight);
        aEnvMixer(cmd++,
                inBuf, nSamples,
                0,
                note->stereoStrongRight, note->stereoStrongLeft,
                DMEM_ADDR_LEFT_CH,
                DMEM_ADDR_RIGHT_CH,
                DMEM_ADDR_WET_LEFT_CH,
                DMEM_ADDR_WET_RIGHT_CH);
    }
    return cmd;
}

Acmd* note_apply_headset_pan_effects(Acmd* acmd, struct NoteSubEu* noteSubEu, struct NoteSynthesisState* note,
                                     s32 bufLen, s32 flags, s32 leftRight) {
    u16 dest = 0;
    u16 pitch = 0;
    u8 prevPanShift = 0;
    u8 panShift = 0;
    UNUSED u8 unkDebug = 0;

    switch (leftRight) {
        case 1:
            dest = 0x540;
            panShift = noteSubEu->headsetPanRight;
            note->prevHeadsetPanLeft = 0;
            prevPanShift = note->prevHeadsetPanRight;
            note->prevHeadsetPanRight = panShift;
            break;
        case 2:
            dest = 0x6C0;
            panShift = noteSubEu->headsetPanLeft;
            note->prevHeadsetPanRight = 0;

            prevPanShift = note->prevHeadsetPanLeft;
            note->prevHeadsetPanLeft = panShift;
            break;
        default:
            return acmd;
    }

    if (flags != 1) { // A_INIT?
        // Slightly adjust the sample rate in order to fit a change in pan shift
        if (prevPanShift == 0) {
            // Kind of a hack that moves the first samples into the resample state
            aDMEMMove(acmd++, 0x0200, 0x0000, 8);
            aClearBuffer(acmd++, 8, 8); // Set pitch accumulator to 0 in the resample state
            aDMEMMove(acmd++, 0x0200, 0x0000 + 0x10,
                      0x10); // No idea, result seems to be overwritten later

            aSaveBuffer(acmd++, 0x0000, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState),
                        sizeof(note->synthesisBuffers->panResampleState));

            pitch = (bufLen << 0xf) / (bufLen + panShift - prevPanShift + 8);
//            if (pitch) {}
            aSetBuffer(acmd++, 0, 0x0200 + 8, 0x0000, panShift + bufLen - prevPanShift);
            aResample(acmd++, 0, pitch, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState));
        } else {
            if (panShift == 0) {
                pitch = (bufLen << 0xf) / (bufLen - prevPanShift - 4);
            } else {
                pitch = (bufLen << 0xf) / (bufLen + panShift - prevPanShift);
            }

//            if (1) {}

            aSetBuffer(acmd++, 0, 0x0200, 0x0000, bufLen + panShift - prevPanShift);
            aResample(acmd++, 0, pitch, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState));
        }

        if (prevPanShift != 0) {
            aLoadBuffer(acmd++, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panSamplesBuffer), 0x0200, prevPanShift);
            aDMEMMove(acmd++, 0x0000, 0x0200 + prevPanShift, panShift + bufLen - prevPanShift);
        } else {
            aDMEMMove(acmd++, 0x0000, 0x0200, panShift + bufLen - prevPanShift);
        }
    } else {
        // Just shift right
        aDMEMMove(acmd++, 0x0200, 0x0000, bufLen);
        aDMEMMove(acmd++, 0x0000, 0x0200 + panShift, bufLen);
        aClearBuffer(acmd++, 0x0200, panShift);
    }

    if (panShift) {
        // Save excessive samples for next iteration
        aSaveBuffer(acmd++, 0x0200 + bufLen, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panSamplesBuffer), panShift);
    }

 ////   aSMix(acmd++, /*gain*/ 0x7FFF, /*in*/ 0x0200, /*out*/ dest, ALIGN(bufLen, 5));

    return acmd;
}
