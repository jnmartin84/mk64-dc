#include <kos.h>
#undef CONT_C
#undef CONT_B
#undef CONT_A
#undef CONT_START
#undef CONT_DPAD_UP
#undef CONT_DPAD_DOWN
#undef CONT_DPAD_LEFT
#undef CONT_DPAD_RIGHT
#undef CONT_Z
#undef CONT_Y
#undef CONT_X
#undef CONT_D
#undef CONT_DPAD2_UP
#undef CONT_DPAD2_DOWN
#undef CONT_DPAD2_LEFT
#undef CONT_DPAD2_RIGHT
#undef bool
#include <ultra64.h>
#include <macros.h>

#include "audio/load.h"
#include "audio/data.h"
#include "audio/heap.h"
#include "audio/internal.h"
#include "audio/playback.h"
#include "audio/synthesis.h"
#include "audio/seqplayer.h"
#include "audio/port_eu.h"
#include "buffers/gfx_output_buffer.h"
#include <stdio.h>
#define ALIGN16(val) (((val) + 0xF) & ~0xF)

#include <stdio.h>
#include <string.h>

extern void *segmented_to_virtual(void *addr);

struct SequencePlayer gSequencePlayers[SEQUENCE_PLAYERS] = {0};
struct SequenceChannel gSequenceChannels[SEQUENCE_CHANNELS] = {0};
struct SequenceChannelLayer gSequenceLayers[SEQUENCE_LAYERS] = {0};
struct SequenceChannel gSequenceChannelNone = {0};
// D_803B5F5C
struct AudioListItem gLayerFreeList = {0};
struct NotePool gNoteFreeLists = {0};
OSMesgQueue gCurrAudioFrameDmaQueue = {0};
OSMesg gCurrAudioFrameDmaMesgBufs[AUDIO_FRAME_DMA_QUEUE_SIZE] = {0};
OSIoMesg gCurrAudioFrameDmaIoMesgBufs[AUDIO_FRAME_DMA_QUEUE_SIZE] = {0};
OSMesgQueue D_803B6720 = {0};
OSMesg D_803B6738 = {0};

OSIoMesg D_803B6740 = {0};
struct SharedDma sSampleDmas[0x70] = {0};
u32 gSampleDmaNumListItems = 0;
u32 sSampleDmaListSize1 = 0;
s32 D_803B6E60 = 0;
s32 load_bss_pad = 0;

u8 sSampleDmaReuseQueue1[256] = {0}; // sSampleDmaReuseQueue1
u8 sSampleDmaReuseQueue2[256] = {0}; // sSampleDmaReuseQueue2
u8 sSampleDmaReuseQueueTail1 = 0;  // sSampleDmaReuseQueueTail1
u8 sSampleDmaReuseQueueTail2 = 0;  // sSampleDmaReuseQueueTail2
u8 sSampleDmaReuseQueueHead1 = 0;  // sSampleDmaReuseQueueHead1
u8 sSampleDmaReuseQueueHead2 = 0;  // sSampleDmaReuseQueueHead2

ALSeqFile* gSeqFileHeader = NULL;
ALSeqFile* gAlCtlHeader = NULL;
ALSeqFile* gAlTbl = NULL;
u8* gAlBankSets = NULL;
u16 gSequenceCount = 0;
struct CtlEntry* gCtlEntries = NULL;
struct AudioBufferParametersEU gAudioBufferParameters = {0};
u32 D_803B70A8 = 0;
s32 gMaxAudioCmds = 0;
s32 gMaxSimultaneousNotes = 0;
s16 gTempoInternalToExternal = 0;
s8 gAudioLibSoundMode = 0;
// If we take SM64 as gospel, these should be in data.c, but that doesn't match
volatile s32 gAudioFrameCount = 0;
s32 gCurrAudioFrameDmaCount = 0;

/**
 * Given that (almost) all of these are format strings, it is highly likely
 * that they are meant to be used in some sort of //printf variant. But I don't
 * care to try and figure out which function gets which string(s)
 * So I've place them all here instead.
 **/
char loadAudioString00[] = "Romcopy %x -> %x ,size %x\n";
char loadAudioString01[] = "Romcopyend\n";
char loadAudioString02[] = "CAUTION:WAVE CACHE FULL %d";
char loadAudioString03[] = "LOAD  Rom :%x -> Ram :%x  Len:%x\n";
char loadAudioString04[] = "BASE %x %x\n";
char loadAudioString05[] = "LOAD %x %x %x\n";
char loadAudioString06[] = "INSTTOP    %x\n";
char loadAudioString07[] = "INSTMAP[0] %x\n";
char loadAudioString08[] = "already flags %d\n";
char loadAudioString09[] = "already flags %d\n";
char loadAudioString10[] = "ERR:SLOW BANK DMA BUSY\n";
char loadAudioString11[] = "ERR:SLOW DMA BUSY\n";
char loadAudioString12[] = "Check %d  bank %d\n";
char loadAudioString13[] = "Cache Check\n";
char loadAudioString14[] = "NO BANK ERROR\n";
char loadAudioString15[] = "BANK %d LOADING START\n";
char loadAudioString16[] = "BANK %d LOAD MISS (NO MEMORY)!\n";
char loadAudioString17[] = "BANK %d ALREADY CACHED\n";
char loadAudioString18[] = "BANK LOAD MISS! FOR %d\n";
char loadAudioString19[] = "Seq %d Loading Start\n";
char loadAudioString20[] = "Heap Overflow Error\n";
char loadAudioString21[] = "SEQ  %d ALREADY CACHED\n";
char loadAudioString22[] = "Ok,one bank slow load Start \n";
char loadAudioString23[] = "Sorry,too many %d bank is none.fast load Start \n";
char loadAudioString24[] = "Seq %d:Default Load Id is %d\n";
char loadAudioString25[] = "Seq Loading Start\n";
char loadAudioString26[] = "Error:Before Sequence-SlowDma remain.\n";
char loadAudioString27[] = "      Cancel Seq Start.\n";
char loadAudioString28[] = "SEQ  %d ALREADY CACHED\n";
char loadAudioString29[] = "Clear Workarea %x -%x size %x \n";
char loadAudioString30[] = "AudioHeap is %x\n";
char loadAudioString31[] = "Heap reset.Synth Change %x \n";
char loadAudioString32[] = "Heap %x %x %x\n";
char loadAudioString33[] = "Main Heap Initialize.\n";
char loadAudioString34[] = "---------- Init Completed. ------------\n";
char loadAudioString35[] = " Syndrv    :[%6d]\n";
char loadAudioString36[] = " Seqdrv    :[%6d]\n";
char loadAudioString37[] = " audiodata :[%6d]\n";
char loadAudioString38[] = "---------------------------------------\n";


static inline uint32_t Swap32(uint32_t val)
{
	return ((((val)&0xff000000) >> 24) | (((val)&0x00ff0000) >> 8) |
		(((val)&0x0000ff00) << 8) | (((val)&0x000000ff) << 24));
}

static inline short SwapShort(short dat)
{
    return (((dat) & 0xff) << 8 | (((dat) >> 8) & 0xff));
}

/**
 * Performs an immediate DMA copy
 */
void audio_dma_copy_immediate(u8* devAddr, void* vAddr, size_t nbytes) {
    //printf("Romcopy %x -> %x ,size %x\n", devAddr, vAddr, nbytes);
    osPiStartDma(&D_803B6740, OS_MESG_PRI_HIGH, OS_READ, (uintptr_t) devAddr, vAddr, nbytes, &D_803B6720);
}

void decrease_sample_dma_ttls() {
    u32 i = 0;
    //printf("%s()\n",__func__);
    for (i = 0; i < sSampleDmaListSize1; i++) {
        struct SharedDma* temp = &sSampleDmas[i];
        if (temp->ttl != 0) {
            temp->ttl--;
            if (temp->ttl == 0) {
                temp->reuseIndex = sSampleDmaReuseQueueHead1;
                sSampleDmaReuseQueue1[sSampleDmaReuseQueueHead1++] = (u8) i;
            }
        }
    }

    for (i = sSampleDmaListSize1; i < gSampleDmaNumListItems; i++) {
        struct SharedDma* temp = &sSampleDmas[i];
        if (temp->ttl != 0) {
            temp->ttl--;
            if (temp->ttl == 0) {
                temp->reuseIndex = sSampleDmaReuseQueueHead2;
                sSampleDmaReuseQueue2[sSampleDmaReuseQueueHead2++] = (u8) i;
            }
        }
    }

    D_803B6E60 = 0;
}
#include <stdio.h>
void* dma_sample_data(uintptr_t devAddr, u32 size, s32 arg2, u8* dmaIndexRef) {
    s32 hasDma = 0;
    struct SharedDma* dma = NULL;
    uintptr_t dmaDevAddr = 0;
    u32 transfer = 0;
    u32 i = 0;
    u32 dmaIndex = 0;
    ssize_t bufferPos = 0;
    UNUSED u32 pad = 0;
////printf("%s(%08x,%u,%d,%08x)\n",__func__,devAddr,size,arg2,dmaIndexRef);

////printf("dma_sample_data\n");
    if (arg2 != 0 || *dmaIndexRef >= sSampleDmaListSize1) {
        for (i = sSampleDmaListSize1; i < gSampleDmaNumListItems; i++) {
            dma = &sSampleDmas[i];
            bufferPos = devAddr - dma->source;
            if (0 <= bufferPos && (size_t) bufferPos <= dma->bufSize - size) {
                // We already have a DMA request for this memory range.
                if (dma->ttl == 0 && sSampleDmaReuseQueueTail2 != sSampleDmaReuseQueueHead2) {
                    // Move the DMA out of the reuse queue, by swapping it with the
                    // tail, and then incrementing the tail.
                    if (dma->reuseIndex != sSampleDmaReuseQueueTail2) {
                        sSampleDmaReuseQueue2[dma->reuseIndex] = sSampleDmaReuseQueue2[sSampleDmaReuseQueueTail2];
                        sSampleDmas[sSampleDmaReuseQueue2[sSampleDmaReuseQueueTail2]].reuseIndex = dma->reuseIndex;
                    }
                    sSampleDmaReuseQueueTail2++;
                }
                dma->ttl = 60;
                *dmaIndexRef = (u8) i;
                return &dma->buffer[(devAddr - dma->source)];
            }
        }

        if ((sSampleDmaReuseQueueTail2 != sSampleDmaReuseQueueHead2) && (arg2 != 0)) {
            // Allocate a DMA from reuse queue 2. This queue can be empty, since
            // TTL 60 is pretty large.
            dmaIndex = sSampleDmaReuseQueue2[sSampleDmaReuseQueueTail2++];
            dma = &sSampleDmas[dmaIndex];
            hasDma = 1;
        }
    } else {
        dma = &sSampleDmas[*dmaIndexRef];
        for (i = 0; i < sSampleDmaListSize1; dma = &sSampleDmas[i++]) {
            bufferPos = devAddr - dma->source;
            if (0 <= bufferPos && (size_t) bufferPos <= dma->bufSize - size) {
                // We already have DMA for this memory range.
                if (dma->ttl == 0) {
                    // Move the DMA out of the reuse queue, by swapping it with the
                    // tail, and then incrementing the tail.
                    if (dma->reuseIndex != sSampleDmaReuseQueueTail1) {
                        sSampleDmaReuseQueue1[dma->reuseIndex] = sSampleDmaReuseQueue1[sSampleDmaReuseQueueTail1];
                        sSampleDmas[sSampleDmaReuseQueue1[sSampleDmaReuseQueueTail1]].reuseIndex = dma->reuseIndex;
                    }
                    sSampleDmaReuseQueueTail1++;
                }
                dma->ttl = 2;
                return dma->buffer + (devAddr - dma->source);
            }
        }
    }

    if (!hasDma) {
        // Allocate a DMA from reuse queue 1. This queue will hopefully never
        // be empty, since TTL 2 is so small.
        dmaIndex = sSampleDmaReuseQueue1[sSampleDmaReuseQueueTail1++];
        dma = &sSampleDmas[dmaIndex];
        hasDma = 1;
    }

    transfer = dma->bufSize;
    dmaDevAddr = devAddr & ~0xF;
    dma->ttl = 2;
    dma->source = dmaDevAddr;
    dma->sizeUnused = transfer;
#if 1
//    osPiStartDma(&gCurrAudioFrameDmaIoMesgBufs[gCurrAudioFrameDmaCount++], OS_MESG_PRI_NORMAL, OS_READ, dmaDevAddr,
  //               dma->buffer, transfer, &gCurrAudioFrameDmaQueue);
    dma->buffer = dmaDevAddr;
  #endif
//    dma_copy(dma->buffer, devAddr, transfer);
    *dmaIndexRef = dmaIndex;
    return (devAddr - dmaDevAddr) + dma->buffer;
}

// init_sample_dma_buffers
void func_800BB030(UNUSED s32 arg0) {
    s32 i = 0;
//printf("%s(%d)\n",__func__,arg0);
#define j i

    D_803B70A8 = 0x5A0;

    for (i = 0; i < gMaxSimultaneousNotes * 3 * gAudioBufferParameters.presetUnk4; i++) {
        sSampleDmas[gSampleDmaNumListItems].buffer = NULL;//soundAlloc(&gNotesAndBuffersPool, D_803B70A8);
//        if (sSampleDmas[gSampleDmaNumListItems].buffer == NULL) {
//            break;
//        }
        sSampleDmas[gSampleDmaNumListItems].bufSize = D_803B70A8;
        sSampleDmas[gSampleDmaNumListItems].source = 0;
        sSampleDmas[gSampleDmaNumListItems].sizeUnused = 0;
        sSampleDmas[gSampleDmaNumListItems].unused2 = 0;
        sSampleDmas[gSampleDmaNumListItems].ttl = 0;
        gSampleDmaNumListItems++;
    }

    for (i = 0; (u32) i < gSampleDmaNumListItems; i++) {
        sSampleDmaReuseQueue1[i] = (u8) i;
        sSampleDmas[i].reuseIndex = (u8) i;
    }

    for (j = gSampleDmaNumListItems; j < 0x100; j++) {
        sSampleDmaReuseQueue1[j] = 0;
    }

    sSampleDmaReuseQueueTail1 = 0;
    sSampleDmaReuseQueueHead1 = (u8) gSampleDmaNumListItems;
    sSampleDmaListSize1 = gSampleDmaNumListItems;

    D_803B70A8 = 0x180;
    for (i = 0; i < gMaxSimultaneousNotes; i++) {
        sSampleDmas[gSampleDmaNumListItems].buffer = NULL;//soundAlloc(&gNotesAndBuffersPool, D_803B70A8);
//        if (sSampleDmas[gSampleDmaNumListItems].buffer == NULL) {
  //          break;
    //    }
        sSampleDmas[gSampleDmaNumListItems].bufSize = D_803B70A8;
        sSampleDmas[gSampleDmaNumListItems].source = 0;
        sSampleDmas[gSampleDmaNumListItems].sizeUnused = 0;
        sSampleDmas[gSampleDmaNumListItems].unused2 = 0;
        sSampleDmas[gSampleDmaNumListItems].ttl = 0;
        gSampleDmaNumListItems++;
    }

    for (i = sSampleDmaListSize1; (u32) i < gSampleDmaNumListItems; i++) {
        sSampleDmaReuseQueue2[i - sSampleDmaListSize1] = (u8) i;
        sSampleDmas[i].reuseIndex = (u8) (i - sSampleDmaListSize1);
    }

    // This probably meant to touch the range size1..size2 as well... but it
    // doesn't matter, since these values are never read anyway.
    for (j = gSampleDmaNumListItems; j < 0x100; j++) {
        sSampleDmaReuseQueue2[j] = sSampleDmaListSize1;
    }

    sSampleDmaReuseQueueTail2 = 0;
    sSampleDmaReuseQueueHead2 = gSampleDmaNumListItems - sSampleDmaListSize1;
#undef j
}

// Similar to patch_sound, but not really
void func_800BB304(struct AudioBankSample* sample) {
    u8* mem = NULL;
    //printf("%s(%08x)\n",__func__,sample);
    if (sample == (void*) NULL) {
        //printf("sample null\n");
        return;// -1;
    }

    if (sample->loaded == 1) {
        uint32_t sampleCopySize = sample->sampleSize;
        // this is why DK and Toad were broken once sound is active
        if (sampleCopySize > 0xffff) {
            sampleCopySize = Swap32(sampleCopySize);
        }
        // temp_a1 = sound->sampleAddr // unk10;
//        mem = soundAlloc(&gNotesAndBuffersPool, sampleCopySize);
        // temp_a1_2 = temp_v0;
  //      if (mem == (void*) NULL) {
            //printf("mem null\n");
    //        return;// -1;
      //  }

        //printf("about to copy to %08x\n", mem);
        //printf("sample addr is %08x\n", sample->sampleAddr);
        //printf("sample size is %08x\n", sample->sampleSize);
      //  uint32_t sampleCopySize = sample->sampleSize;
        // this is why DK and Toad were broken once sound is active
//        if (sampleCopySize > 0xffff) {
  //          sampleCopySize = Swap32(sampleCopySize);
    //    }
//////////        audio_dma_copy_immediate(sample->sampleAddr, mem, sampleCopySize);//sample->sampleSize);
        sample->loaded = 0x81;
        sample->sampleSize = sampleCopySize;
        sample->sampleAddr = sample->sampleAddr;//mem; // sound->unk4
    }
    //printf("returning from the func\n");
}

s32 func_800BB388(s32 bankId, s32 instId, s32 arg2) {
    struct Instrument* instr = NULL;
    struct Drum* drum = NULL;
    //printf("%s(%d,%d,%d)\n",__func__,bankId,instId,arg2);
    if (instId < 0x7F) {
        instr = get_instrument_inner(bankId, instId);
        if (instr == NULL) {
            //printf("instr null return -1\n");
            return -1;
        }
        //printf("instr is %08x\n", instr);
        if (instr->normalRangeLo != 0) {
            //printf("normalRangeLo != 0\n");
            func_800BB304(instr->lowNotesSound.sample);
        }
        func_800BB304(instr->normalNotesSound.sample);
        if (instr->normalRangeHi != 0x7F) {
            //printf("normalRangeHi != 0x7f\n");
            func_800BB304(instr->highNotesSound.sample);
        }
        //! @bug missing return
    } else if (instId == 0x7F) {
        drum = get_drum(bankId, arg2);
        if (drum == NULL) {
            //printf("drum null return -1\n");
            return -1;
        }
        func_800BB304(drum->sound.sample);
        return 0;
    }
#ifdef AVOID_UB
    return 0;
#endif
}

// This appears to be a modified version of alSeqFileNew
// from src/os/alBankNew.c
// Or maybe its patch_seq_file from SM64's load_sh.c?
void func_800BB43C(ALSeqFile* f, u8* base, u8 swap) {
    //printf("%s(%08x, %08x, %u)\n",__func__,f,base,swap);
#define PATCH(SRC, BASE, TYPE) SRC = (TYPE) ((u32) SRC + (u32) BASE)
    int i = 0;
    u8* wut = base;
    for (i = 0; i < f->seqCount; i++) {
        if (swap)
            f->seqArray[i].len = Swap32(f->seqArray[i].len);
        if (swap)
            f->seqArray[i].offset = Swap32(f->seqArray[i].offset);

        if (f->seqArray[i].len != 0) {
            PATCH(f->seqArray[i].offset, wut, u8*);
        }
    }
#undef PATCH
}

void patch_sound(struct AudioBankSound* sound, u8* memBase, u8* offsetBase) {
    struct AudioBankSample* sample = NULL;
    void* patched = NULL;
    u8* mem = NULL;
    //printf("%s(%08x,%08x,%08x)\n", __func__, sound, memBase, offsetBase);
#define PATCH(x, base) (patched = (void*) ((uintptr_t) (x) + (uintptr_t) base))
    // printf("sound->sample %08x memBase %08x offsetBase %08x\n", sound->sample, memBase, offsetBase);
    sound->sample = Swap32(sound->sample);
    if (sound->sample != NULL) {
        sample = sound->sample = (struct AudioBankSample*) PATCH(sound->sample, memBase);
        // printf("\tsample is %08x\n", sample);
        if (sample->loaded == 0) {
            sample->sampleAddr = Swap32(sample->sampleAddr);
            sample->sampleAddr = (u8*) PATCH(sample->sampleAddr, offsetBase);
            // printf("sample->sampleAddr %08x\n", sample->sampleAddr);
            sample->loop = Swap32(sample->loop);
            sample->loop = (struct AdpcmLoop*) PATCH(sample->loop, memBase);
            // printf("sample->loop %08x\n", sample->loop);
            sample->book = Swap32(sample->book);
            sample->book = (struct AdpcmBook*) PATCH(sample->book, memBase);
            // printf("sample->book %08x\n", sample->book);
            sample->loaded = 1;
        } else if (sample->loaded == 0x80) {
            // printf("sample->loaded IS 0x80\n");
            sample->sampleAddr = Swap32(sample->sampleAddr);
            PATCH(sample->sampleAddr, offsetBase);
            // printf("else sample->sampleAddr %08x\n", sample->sampleAddr);
            sample->sampleSize = Swap32(sample->sampleSize);
            // printf("else sample->sampleSize %08x\n", sample->sampleSize);
     //       mem = soundAlloc(&gNotesAndBuffersPool, sample->sampleSize);
       //     if (mem == NULL) {
         //       sample->sampleAddr = (u8*) patched;
                // printf("\tMEMNULL else sample->sampleAddr %08x\n", sample->sampleAddr);
           //     sample->loaded = 1;
            //} else {
                // printf("\telse about to copy to %08x\n", mem);
//////////                audio_dma_copy_immediate((u8*) patched, mem, sample->sampleSize);
                sample->loaded = 0x81;
                sample->sampleAddr = (u8*) patched;//mem;
            //}
            sample->loop = Swap32(sample->loop);
            sample->loop = (struct AdpcmLoop*) PATCH(sample->loop, memBase);
            // printf("else sample->loop %08x\n", sample->loop);
            sample->book = Swap32(sample->book);
            sample->book = (struct AdpcmBook*) PATCH(sample->book, memBase);
            // printf("else sample->book %08x\n", sample->book);
        }
    }

#undef PATCH
}

// There does not appear to an SM64 counterpart to this function
void func_800BB584(s32 bankId) {
    u8* var_a1 = NULL;
    //printf("%s(%d)\n",__func__,bankId);
    if (gAlTbl->seqArray[bankId].len == 0) {
        var_a1 = gAlTbl->seqArray[(s32) gAlTbl->seqArray[bankId].offset].offset;
    } else {
        var_a1 = gAlTbl->seqArray[bankId].offset;
    }

    //printf("var_a1 is %08x\n", var_a1);

    // wtf is up with the `gCtlEntries[bankId].instruments - 1` stuff?
    patch_audio_bank((struct AudioBank*) (gCtlEntries[bankId].instruments - 1), var_a1,
                     gCtlEntries[bankId].numInstruments, gCtlEntries[bankId].numDrums);
    gCtlEntries[bankId].drums = (struct Drum**) *(gCtlEntries[bankId].instruments - 1);
}

void patch_audio_bank(struct AudioBank* mem, u8* offset, u32 numInstruments, u32 numDrums) {
    struct Instrument* instrument = NULL;
    struct Instrument** itInstrs = NULL;
    struct Instrument** end = NULL;
    struct AudioBank* temp = NULL;
    u32 i = 0;
    void* patched = NULL;
    struct Drum* drum = NULL;
    struct Drum** drums = NULL;
    u32 numDrums2 = 0;

    //printf("%s(%08x,%08x,%u,%u)\n", __func__, mem, offset, numInstruments, numDrums);


//printf("PATCH AUDIO BANK\n");
#define BASE_OFFSET_REAL(x, base) (void*) ((u32) (x) + (u32) Swap32(base))
#define PATCH(x, base) (patched = BASE_OFFSET_REAL(x, base))
#define PATCH_MEM(x) x = PATCH(x, mem)

#define BASE_OFFSET(x, base) BASE_OFFSET_REAL(base, x)
//printf("mem %08x\n", mem);
    drums = Swap32(mem->drums);
    //printf("drums = %08x\n", drums);
    numDrums2 = numDrums;
    //printf("numdrums = %08x\n", numDrums);

    if (drums != NULL && numDrums2 > 0) {
        mem->drums = PATCH(drums, Swap32(mem));
        //printf("mem->drums is %08x\n", mem->drums);
        for (i = 0; i < numDrums2; i++) {
            patched = Swap32(mem->drums[i]);
            if (patched != NULL) {
                //printf("\tpatched is %08x\n", patched);
                drum = PATCH(patched, Swap32(mem));
                mem->drums[i] = drum;
                //printf("\tmem->drums[%d] == %08x\n", i, mem->drums[i]);
                if (drum->loaded == 0) {
                    patch_sound(&drum->sound, (u8*) mem, offset);
                    patched = Swap32(drum->envelope);
                    drum->envelope = BASE_OFFSET(Swap32(mem), patched);
                    //printf("\tdrum->envelope == %08x\n", drum->envelope);
                    drum->loaded = 1;
                   //printf("drum %08x ->tuning %08x\n", drum, *(uint32_t *)&drum->sound.tuning);
                }
            }
        }
    }

    //! Doesn't affect EU, but required for US/JP
    temp = &*mem;
    //printf("numInstruments %d\n", numInstruments);
    if (numInstruments > 0) {
        //! Doesn't affect EU, but required for US/JP
        struct Instrument** tempInst;
        itInstrs = temp->instruments;
        tempInst = temp->instruments;
        end = numInstruments + tempInst;

        do {
            if (*itInstrs != NULL) {
                *itInstrs = BASE_OFFSET(*itInstrs, mem);
                instrument = *itInstrs;
               // //printf("instrument is %08x\n", instrument);
                if (instrument->loaded == 0) {
                    patch_sound(&instrument->lowNotesSound, (u8*) mem, offset);
                    patch_sound(&instrument->normalNotesSound, (u8*) mem, offset);
                    patch_sound(&instrument->highNotesSound, (u8*) mem, offset);
                    patched = Swap32(instrument->envelope);
                    instrument->envelope = BASE_OFFSET(Swap32(mem), patched);
                    instrument->loaded = 1;
                }
            }
            itInstrs++;
        } while (end != itInstrs);
    }
#undef PATCH_MEM
#undef PATCH
#undef BASE_OFFSET_REAL
#undef BASE_OFFSET
}

struct AudioBank* bank_load_immediate(s32 bankId, s32 arg1) {
    s32 alloc = 0;
    UNUSED s32 stackPadding0[9] = {0};
    struct AudioBank* ret = NULL;
    u8* ctlData = NULL;

    //printf("%s(%d,%d)\n", __func__, bankId, arg1);

    //printf("bank load immedite\n");
    alloc = gAlCtlHeader->seqArray[bankId].len + 0xf;
    alloc = ALIGN16(alloc);
    alloc -= 0x10;
    ctlData = gAlCtlHeader->seqArray[bankId].offset;
    ret = alloc_bank_or_seq(&gBankLoadedPool, 1, alloc, arg1, bankId);
    if (ret == NULL) {
        return NULL;
    }
        //printf("about to copy to alloc_bank_or_seq(&gBankLoadedPool %08x\n", ret);
    audio_dma_copy_immediate(ctlData + 0x10, ret, (u32) alloc);
    gCtlEntries[bankId].instruments = ret->instruments;
    func_800BB584(bankId);
    if (gBankLoadStatus[bankId] != 5) {
        gBankLoadStatus[bankId] = 2;
    }
    return ret;
}

void* sequence_dma_immediate(s32 seqId, s32 arg1) {
    s32 seqLength = 0;
    void* ptr = NULL;
    u8* seqData = 0;
    //printf("%s(%d,%d)\n", __func__, seqId, arg1);

    seqLength = gSeqFileHeader->seqArray[seqId].len;
    seqLength = ALIGN16(seqLength);
    //printf("seqLength = %08x\n", seqLength);

    seqData = gSeqFileHeader->seqArray[seqId].offset;

    ptr = alloc_bank_or_seq(&gSeqLoadedPool, 1, seqLength, arg1, seqId);
    if (ptr == NULL) {
        return NULL;
    }

    audio_dma_copy_immediate(seqData, ptr, seqLength);

    if (gSeqLoadStatus[seqId] != 5) {
        gSeqLoadStatus[seqId] = 2;
    }

    return ptr;
}

u8 get_missing_bank(u32 seqId, s32* nonNullCount, s32* nullCount) {
    void* temp = NULL;
    u32 bankId = 0;
    u16 offset = 0;
    u8 i = 0;
    u8 ret = 0;

    //printf("%s(%d,%08x,%08x)\n", __func__, seqId, nonNullCount, nullCount);

    *nullCount = 0;
    *nonNullCount = 0;
    //printf("seqId %d\n", seqId);
    offset = ((u16*) gAlBankSets)[seqId];
    //printf("offset %d\n", offset);
    for (i = gAlBankSets[offset++], ret = 0; i != 0; i--) {
        bankId = gAlBankSets[offset++];
        //printf("\tbankId %d\n", bankId);

        if (IS_BANK_LOAD_COMPLETE(bankId) == true) {
            temp = get_bank_or_seq(1, 2, bankId);
        } else {
            temp = NULL;
        }

        if (temp == NULL) {
            (*nullCount)++;
            ret = bankId;
        } else {
            (*nonNullCount)++;
        }
    }

    return ret;
}

struct AudioBank* load_banks_immediate(s32 seqId, u8* outDefaultBank) {
    void* ret = NULL;
    u32 bankId = 0;
    u16 offset = 0;
    u8 i = 0;

    //printf("%s(%d,%08x)\n", __func__, seqId, outDefaultBank);

//printf("LBI\n");
//printf("seqId %d\n", seqId);
    offset = ((u16*) gAlBankSets)[seqId];
  //  printf("offset %d\n", offset);
    //printf("offet %04x\n", offset);
//    if ((u32)offset > (u32)0x400000) {
//        offset = SwapShort(offset);
//    }
    for (i = gAlBankSets[offset++]; i != 0; i--) {
        bankId = gAlBankSets[offset++];
        //printf("bankId %d\n", bankId);
        if (IS_BANK_LOAD_COMPLETE(bankId) == true) {
            //printf("load complete\n");
            ret = get_bank_or_seq(1, 2, bankId);
        } else {
            //printf("load incomplete\n");
            ret = NULL;
        }

        if (ret == NULL) {
            ret = bank_load_immediate(bankId, 2);
        }
    }
    //printf("returning from lbi\n");
    *outDefaultBank = bankId;
    return ret;
}

void preload_sequence(u32 seqId, u8 preloadMask) {
    void* sequenceData = NULL;
    u8 temp = 0;
    //printf("%s(%u,%u)\n", __func__, seqId, preloadMask);

    if (seqId >= gSequenceCount) {
        return;
    }

    if (gSeqFileHeader->seqArray[seqId].len == 0) {
        seqId = (u32) gSeqFileHeader->seqArray[seqId].offset;
    }

//    //printf("seqId is ??? %08x\n", seqId);

    gAudioLoadLock = AUDIO_LOCK_LOADING;
    if (preloadMask & PRELOAD_BANKS) {
        load_banks_immediate(seqId, &temp);
    }

    if (preloadMask & PRELOAD_SEQUENCE) {
        //! @bug should be IS_SEQ_LOAD_COMPLETE
        if (//IS_SEQ_LOAD_COMPLETE(seqId) == 1) {
            IS_BANK_LOAD_COMPLETE(seqId) == 1) {
            sequenceData = //sequence_dma_immediate(seqId, 2);//
            get_bank_or_seq(0, 2, seqId);
        } else {
            sequenceData = NULL;
        }
        if (sequenceData == NULL && sequence_dma_immediate(seqId, 2) == NULL) {
            gAudioLoadLock = AUDIO_LOCK_NOT_LOADING;
            return;
        }
    }

    gAudioLoadLock = AUDIO_LOCK_NOT_LOADING;
}

void load_sequence(u32 player, u32 seqId) {
    //printf("%s(%u,%u)\n", __func__, player, seqId);
    gAudioLoadLock = AUDIO_LOCK_LOADING;
    load_sequence_internal(player, seqId);
    gAudioLoadLock = AUDIO_LOCK_NOT_LOADING;
}

void load_sequence_internal(u32 player, u32 seqId) {
    void* sequenceData = NULL;
    struct SequencePlayer* seqPlayer = &gSequencePlayers[player];
    UNUSED u32 padding[2] = {0};
    //printf("%s(%u,%u)\n", __func__, player, seqId);

    if (seqId >= gSequenceCount) {
        return;
    }

    if (gSeqFileHeader->seqArray[seqId].len == 0) {
        seqId = (u32) gSeqFileHeader->seqArray[seqId].offset;
    }

    sequence_player_disable(seqPlayer);

    if (load_banks_immediate(seqId, &seqPlayer->defaultBank[0]) == NULL) {
        return;
    }

    seqPlayer->seqId = seqId;
    sequenceData = get_bank_or_seq(0, 2, seqId);
    if (sequenceData == NULL) {
        sequenceData = sequence_dma_immediate(seqId, 2);

        if (sequenceData == NULL) {
            return;
        }
    }

    init_sequence_player(player);
    seqPlayer->scriptState.depth = 0;
    seqPlayer->delay = 0;
    seqPlayer->enabled = 1;
    seqPlayer->seqData = sequenceData;
    seqPlayer->scriptState.pc = sequenceData;
}

extern u8 *_audio_banksSegmentRomStart;
extern u8 *_audio_tablesSegmentRomStart;
extern u8 *_instrument_setsSegmentRomStart;
extern u8 *_sequencesSegmentRomStart;
/**
 * There is some wild bullshit going on in this function
 *  It is an unholy cross between SM64's EU and Shindou
 *  verions of this function, with the for loop towards
 *  the bottom resembling stuff from bank_load_async
 */

void audio_init(void) {
    s32 i = 0;
//    UNUSED s32 pad[6] = {0};
    s32 j = 0, k = 0;
    s32 ctlSeqCount = 0;
//    UNUSED s32 lim4 = 0, lim5 = 0;
    u32 buf[16] = {0};
//    UNUSED s32 lim2 = 0, lim3 = 0;
    s32 size = 0;
//    u64* ptr64;
//    UNUSED s32 pad2 = 0;
//    UNUSED s32 one = 1;
    void* data = NULL;

//    gdb_init();
    //printf("%s()\n");

    gAudioLoadLock = 0;

    for (i = 0; i < gAudioHeapSize / 8; i++) {
        ((u64*) gAudioHeap)[i] = 0;
    }

#ifdef TARGET_N64
    // It seems boot.s doesn't clear the .bss area for audio, so do it here.
    ptr64 = (u64*) ((u8*) gGfxSPTaskOutputBuffer + sizeof(gGfxSPTaskOutputBuffer));
    for (k = ((uintptr_t) &gAudioGlobalsEndMarker - (uintptr_t) ((u64 *)((u8 *) gGfxSPTaskOutputBuffer + sizeof(gGfxSPTaskOutputBuffer))) ) / 8; k >= 0; k--) {
        *ptr64++ = 0;
    }
#endif

#if 0
#ifdef VERSION_EU
    D_803B7178 = 20.03042f;
    gRefreshRate = 50;
#else // US
    switch (osTvType) {
        case TV_TYPE_PAL:
            D_803B7178 = 20.03042f;
            gRefreshRate = 50;
            break;
        case TV_TYPE_MPAL:
            D_803B7178 = 16.546f;
            gRefreshRate = 60;
            break;
        case TV_TYPE_NTSC:
        default:
            D_803B7178 = 16.713f;
            gRefreshRate = 60;
            break;
    }
#endif
#endif

    D_803B7178 = 16.713f;
    gRefreshRate = 60;

    port_eu_init();

//    if (k) {} // fake

    for (i = 0; i < NUMAIBUFFERS; i++) {
        gAiBufferLengths[i] = 0xa0;
    }

    gAudioTaskIndex = gAudioFrameCount = 0;
//    gCurrAiBufferIndex = 0;
    gAudioLibSoundMode = 0;
    gAudioTask = NULL;
    gAudioTasks[0].task.t.data_size = 0;
    gAudioTasks[1].task.t.data_size = 0;
    osCreateMesgQueue(&D_803B6720, &D_803B6738, 1);
    osCreateMesgQueue(&gCurrAudioFrameDmaQueue, gCurrAudioFrameDmaMesgBufs, ARRAY_COUNT(gCurrAudioFrameDmaMesgBufs));
    gCurrAudioFrameDmaCount = 0;
    gSampleDmaNumListItems = 0;

    sound_init_main_pools(gAudioInitPoolSize);

    for (i = 0; i < NUMAIBUFFERS; i++) {
        gAiBuffers[i] = soundAlloc(&gAudioInitPool, AIBUFFER_LEN);

        for (j = 0; j < (s32) (AIBUFFER_LEN / sizeof(s16)); j++) {
            gAiBuffers[i][j] = 0;
        }
    }

    gAudioResetPresetIdToLoad = 0;
    gAudioResetStatus = 1;//one;
    audio_shut_down_and_reset_step();

    // Load headers for sounds and sequences
    gSeqFileHeader = (ALSeqFile*) buf;
    data = _sequencesSegmentRomStart;
    audio_dma_copy_immediate(data, gSeqFileHeader, 0x10);
    gSequenceCount = gSeqFileHeader->seqCount;
    size = gSequenceCount * sizeof(ALSeqData) + 4;
    size = ALIGN16(size);
    gSeqFileHeader = soundAlloc(&gAudioInitPool, size);
    audio_dma_copy_immediate(data, gSeqFileHeader, size);
    func_800BB43C(gSeqFileHeader, data, 0);

    // Load header for CTL (instrument metadata)
    gAlCtlHeader = (ALSeqFile*) buf;
    data = _audio_banksSegmentRomStart;
    audio_dma_copy_immediate(data, gAlCtlHeader, 0x10);
    ctlSeqCount = SwapShort(gAlCtlHeader->seqCount);
    size = ALIGN16(ctlSeqCount * sizeof(ALSeqData) + 4);
    gAlCtlHeader = soundAlloc(&gAudioInitPool, size);
    audio_dma_copy_immediate(data, gAlCtlHeader, size);
    gAlCtlHeader->revision = SwapShort(gAlCtlHeader->revision);
    gAlCtlHeader->seqCount = SwapShort(gAlCtlHeader->seqCount);
    func_800BB43C(gAlCtlHeader, data, 1);
    gCtlEntries = soundAlloc(&gAudioInitPool, ctlSeqCount * sizeof(struct CtlEntry));
    for (i = 0; i < ctlSeqCount; i++) {
        audio_dma_copy_immediate(gAlCtlHeader->seqArray[i].offset, buf, 0x10);
        gCtlEntries[i].numInstruments = Swap32(buf[0]);
        gCtlEntries[i].numDrums = Swap32(buf[1]);
    }

    // Load header for TBL (raw sound data)
    gAlTbl = (ALSeqFile*) buf;
    data = _audio_tablesSegmentRomStart;
    audio_dma_copy_immediate(data, gAlTbl, 0x10);
    size = SwapShort(gAlTbl->seqCount) * sizeof(ALSeqData) + 4;
    size = ALIGN16(size);
    gAlTbl = soundAlloc(&gAudioInitPool, size);
    audio_dma_copy_immediate(data, gAlTbl, size);
    gAlTbl->seqCount = SwapShort(gAlTbl->seqCount);
    func_800BB43C(gAlTbl, data, 1);

    // Load bank sets for each sequence
    gAlBankSets = soundAlloc(&gAudioInitPool, 0x100);
    audio_dma_copy_immediate(_instrument_setsSegmentRomStart, gAlBankSets, 0x100);
//    for (int i=0;i<128;i++) {

  //  }

    sound_alloc_pool_init(&gUnkPool1.pool, soundAlloc(&gAudioInitPool, (u32) D_800EA5D8), (u32) D_800EA5D8);
    init_sequence_players();
    gAudioLoadLock = 0x76557364;
    //printf("out of audio init\n");
}
