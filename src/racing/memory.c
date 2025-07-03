#include <ultra64.h>
#include <PR/ultratypes.h>
#include <macros.h>
#include <common_structs.h>
#include <segments.h>
#include <decode.h>

#include "memory.h"
#include "main.h"
#include "code_800029B0.h"
#include "math_util.h"
#include "courses/courseTable.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern s16 gCurrentCourseId;
extern uint8_t __attribute__((aligned(32))) DECOMP_VERT_BUF[228656];
extern u8 __attribute__((aligned(32))) SEG4_BUF[228656];
extern u8 __attribute__((aligned(32))) SEG5_BUF[133120];
extern uint8_t __attribute__((aligned(32))) COMP_VERT_BUF[65536];
static char __attribute__((aligned(32))) texfn[256];

s32 sGfxSeekPosition;
s32 sPackedSeekPosition;

uintptr_t sPoolFreeSpace;
struct MainPoolBlock* sPoolListHeadL;
struct MainPoolBlock* sPoolListHeadR;

struct MainPoolState* gMainPoolState = NULL;

struct UnkStruct_802B8CD4 D_802B8CD4[] = { 0 };
s32 D_802B8CE4 = 0; // pad
s32 memoryPadding[2];

/**
 * @brief Returns the address of the next available memory location and updates the memory pointer
 * to reference the next location of available memory based provided size to allocate.
 * @param size of memory to allocate.
 * @return Address of free memory
 */
void* get_next_available_memory_addr(uintptr_t size) {
    uintptr_t freeSpace = (uintptr_t) gNextFreeMemoryAddress;
    size = ALIGN16(size);
    gNextFreeMemoryAddress += size;
    return (void*) freeSpace;
}

/**
 * @brief Stores the physical memory addr for segmented memory in `gSegmentTable` using the segment number as an index.
 *
 * This function takes a segment number and a pointer to a memory address, and stores the address in the `gSegmentTable`
 * array at the specified segment index. The stored address is truncated to a 29-bit value to ensure that it fits within
 * the memory address. This allows converting between segmented memory and physical memory.
 *
 * @param segment A segment number from 0x0 to 0xF to set the base address.
 * @param addr A pointer containing the physical memory address of the data.
 * @return The stored base address, truncated to a 29-bit value.
 */
uintptr_t set_segment_base_addr(s32 segment, void* addr) {
    gSegmentTable[segment] = (uintptr_t) addr & 0x1FFFFFFF;
    return gSegmentTable[segment];
}

/**
 * @brief Returns the physical memory location of a segment.
 * @param permits segment numbers from 0x0 to 0xF.
 */
void* get_segment_base_addr(s32 segment) {
    return (void*) (gSegmentTable[segment] | 0x80000000);
}

/**
 * @brief converts an RSP segment + offset address to a normal memory address
 */
void* segmented_to_virtual(const void* addr) {
    size_t segment = (uintptr_t) addr >> 24;
    size_t offset = (uintptr_t) addr & 0x00FFFFFF;

    return (void*) ((gSegmentTable[segment] + offset) | 0x80000000);
}

void move_segment_table_to_dmem(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        gSPSegment(gDisplayListHead++, i, gSegmentTable[i]);
    }
}

/**
 * @brief Sets the starting location for allocating memory and calculates pool size.
 *
 * Default memory size, 701.984 Kilobytes.
 */
void initialize_memory_pool(uintptr_t poolStart, uintptr_t poolEnd) {

    poolStart = ALIGN16(poolStart);
    // Truncate to a 16-byte boundary.
    poolEnd &= ~0xF;

    gFreeMemorySize = (poolEnd - poolStart) - 0x10;
    gNextFreeMemoryAddress = poolStart;
}

/**
 * @brief Allocates memory and adjusts gFreeMemorySize.
 */
void* allocate_memory(size_t size) {
    uintptr_t freeSpace;

    size = ALIGN16(size);
    gFreeMemorySize -= size;
    freeSpace = gNextFreeMemoryAddress;
    gNextFreeMemoryAddress += size;

    return (void*) freeSpace;
}

UNUSED void func_802A7D54(s32 arg0, s32 arg1) {
    gD_80150158[arg0].unk0 = arg0;
    gD_80150158[arg0].unk8 = arg1;
}

/**
 * @brief Allocate and DMA.
 */
void* load_data(uintptr_t startAddr, uintptr_t endAddr) {
    void* allocated;
    uintptr_t size = endAddr - startAddr;

    allocated = allocate_memory(size);
    if (allocated != 0) {
        dma_copy((u8*) allocated, (u8*) startAddr, size);
    }
    return (void*) allocated;
}

UNUSED void main_pool_init(uintptr_t start, uintptr_t end) {
    start = ALIGN16(start);
    end = ALIGN16(end - 15);

    sPoolFreeSpace = (end - start) - 16;

    sPoolListHeadL = (struct MainPoolBlock*) start;
    sPoolListHeadR = (struct MainPoolBlock*) end;
    sPoolListHeadL->prev = NULL;
    sPoolListHeadL->next = NULL;
    sPoolListHeadR->prev = NULL;
    sPoolListHeadR->next = NULL;
}

/**
 * Allocate a block of memory from the pool of given size, and from the
 * specified side of the pool (MEMORY_POOL_LEFT or MEMORY_POOL_RIGHT).
 * If there is not enough space, return NULL.
 */
UNUSED void* main_pool_alloc(uintptr_t size, uintptr_t side) {
    struct MainPoolBlock* newListHead;
    void* addr = NULL;

    size = ALIGN16(size) + 8;
    if (sPoolFreeSpace >= size) {
        sPoolFreeSpace -= size;
        if (side == MEMORY_POOL_LEFT) {
            newListHead = (struct MainPoolBlock*) ((u8*) sPoolListHeadL + size);
            sPoolListHeadL->next = newListHead;
            newListHead->prev = sPoolListHeadL;
            addr = (u8*) sPoolListHeadL + 8;
            sPoolListHeadL = newListHead;
        } else {
            newListHead = (struct MainPoolBlock*) ((u8*) sPoolListHeadR - size);
            sPoolListHeadR->prev = newListHead;
            newListHead->next = sPoolListHeadR;
            sPoolListHeadR = newListHead;
            addr = (u8*) sPoolListHeadR + 8;
        }
    }
    return addr;
}
/**
 * Free a block of memory that was allocated from the pool. The block must be
 * the most recently allocated block from its end of the pool, otherwise all
 * newer blocks are freed as well.
 * Return the amount of free space left in the pool.
 */
UNUSED uintptr_t main_pool_free(void* addr) {
    struct MainPoolBlock* block = (struct MainPoolBlock*) ((u8*) addr - 8);
    struct MainPoolBlock* oldListHead = (struct MainPoolBlock*) ((u8*) addr - 8);

    if (oldListHead < sPoolListHeadL) {
        while (oldListHead->next != NULL) {
            oldListHead = oldListHead->next;
        }
        sPoolListHeadL = block;
        sPoolListHeadL->next = NULL;
        sPoolFreeSpace += (uintptr_t) oldListHead - (uintptr_t) sPoolListHeadL;
    } else {
        while (oldListHead->prev != NULL) {
            oldListHead = oldListHead->prev;
        }
        sPoolListHeadR = block->next;
        sPoolListHeadR->prev = NULL;
        sPoolFreeSpace += (uintptr_t) sPoolListHeadR - (uintptr_t) oldListHead;
    }
    return sPoolFreeSpace;
}
// main_pool_realloc
UNUSED void* main_pool_realloc(void* addr, uintptr_t size) {
    void* newAddr = NULL;
    struct MainPoolBlock* block = (struct MainPoolBlock*) ((u8*) addr - 8);

    if (block->next == sPoolListHeadL) {
        main_pool_free(addr);
        newAddr = main_pool_alloc(size, MEMORY_POOL_LEFT);
    }
    return newAddr;
}

UNUSED uintptr_t main_pool_available(void) {
    return sPoolFreeSpace - 8;
}

UNUSED uintptr_t main_pool_push_state(void) {
    struct MainPoolState* prevState = gMainPoolState;
    uintptr_t freeSpace = sPoolFreeSpace;
    struct MainPoolBlock* lhead = sPoolListHeadL;
    struct MainPoolBlock* rhead = sPoolListHeadR;

    gMainPoolState = main_pool_alloc(sizeof(*gMainPoolState), MEMORY_POOL_LEFT);
    gMainPoolState->freeSpace = freeSpace;
    gMainPoolState->listHeadL = lhead;
    gMainPoolState->listHeadR = rhead;
    gMainPoolState->prev = prevState;
    return sPoolFreeSpace;
}

/**
 * Restore pool state from a previous call to main_pool_push_state. Return the
 * amount of free space left in the pool.
 */
UNUSED uintptr_t main_pool_pop_state(void) {
    sPoolFreeSpace = gMainPoolState->freeSpace;
    sPoolListHeadL = gMainPoolState->listHeadL;
    sPoolListHeadR = gMainPoolState->listHeadR;
    gMainPoolState = gMainPoolState->prev;
    return sPoolFreeSpace;
}
// similar to sm64 dma_read
UNUSED void* func_802A80B0(u8* dest, u8* srcStart, u8* srcEnd) {
    void* addr;
    uintptr_t size = srcStart - dest;
    addr = main_pool_alloc(size, (uintptr_t) srcEnd);

    if (addr != 0) {

        osInvalDCache(addr, size);
        osPiStartDma(&gDmaIoMesg, OS_MESG_PRI_NORMAL, OS_READ, (uintptr_t) dest, addr, size, &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, OS_MESG_BLOCK);
    }
    return addr;
}

// replaces call to dynamic_dma_read with dma_read.
UNUSED void* load_segment(s32 segment, u8* srcStart, u8* srcEnd, u8* side) {
    void* addr = func_802A80B0(srcStart, srcEnd, side);

    if (addr != NULL) {
        set_segment_base_addr(segment, addr);
    }
    return addr;
}

// Similar to sm64 load_to_fixed_pool_addr?
UNUSED void* func_802A8190(s32 arg0, u8* arg1) {
    // uintptr_t srcSize = ALIGN16(srcEnd - srcStart);
    // uintptr_t destSize = ALIGN16((u8 *) sPoolListHeadR - destAddr);
    void* addr;
    uintptr_t temp_v0 = D_802B8CD4[arg0].unk4;
    uintptr_t temp_v1 = D_802B8CD4[arg0].unk8;
    uintptr_t temp_v2 = D_802B8CD4[arg0].unk2;
    addr = func_802A80B0((u8*) temp_v0, (u8*) temp_v1, arg1);

    // dest = main_pool_alloc(destSize, MEMORY_POOL_RIGHT);
    if (addr != 0) {
        set_segment_base_addr(temp_v2, addr);
    }
    return (void*) addr;
}

UNUSED void func_802A81EC(void) {
    s32 temp_s0;
    s16* phi_s1;
    s32 phi_s0;

    phi_s1 = (s16*) &D_802B8CD4;
    phi_s0 = 0;
    do {
        if ((*phi_s1 & 1) != 0) {
            func_802A8190(phi_s0, 0);
        }
        temp_s0 = phi_s0 + 1;
        phi_s1 += 8;
        phi_s0 = temp_s0;
    } while (phi_s0 != 3);
}

UNUSED struct AllocOnlyPool* alloc_only_pool_init(uintptr_t size, uintptr_t side) {
    void* addr;
    struct AllocOnlyPool* subPool = NULL;

    size = ALIGN4(size);
    addr = main_pool_alloc(size + sizeof(struct AllocOnlyPool), side);
    if (addr != NULL) {
        subPool = (struct AllocOnlyPool*) addr;
        subPool->totalSpace = size;
        subPool->usedSpace = (s32) addr + sizeof(struct AllocOnlyPool);
        subPool->startPtr = 0;
        subPool->freePtr = (u8*) addr + sizeof(struct AllocOnlyPool);
    }
    return subPool;
}

UNUSED uintptr_t func_802A82AC(s32 arg0) {
    uintptr_t temp_v0;
    uintptr_t phi_v1;

    temp_v0 = D_801502A0 - arg0;
    phi_v1 = 0;
    if (temp_v0 >= (uintptr_t) gDisplayListHead) {
        D_801502A0 = temp_v0;
        phi_v1 = temp_v0;
    }
    return phi_v1;
}

/**
 * @brief Returns pointer to mio0 compressed Vtx.
 */
u8* dma_compressed_vtx(u8* start, u8* end) {
    u8* freeSpace;
    uintptr_t size;

    size = ALIGN16(end - start);
    freeSpace = (u8*) gNextFreeMemoryAddress;
    dma_copy(freeSpace, start, size);
    gNextFreeMemoryAddress += size;
    return freeSpace;
}

// unused mio0 decode func.
UNUSED uintptr_t func_802A8348(s32 arg0, s32 arg1, s32 arg2) {
    uintptr_t offset;
    UNUSED void* pad;
    uintptr_t oldAddr;
    void* newAddr;

    offset = ALIGN16(arg1 * arg2);
    oldAddr = gNextFreeMemoryAddress;
    newAddr = (void*) (oldAddr + offset);
    pad = &newAddr;
    osInvalDCache(newAddr, offset);
    osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) &_other_texturesSegmentRomStart[SEGMENT_OFFSET(arg0)], newAddr, offset,
                 &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, 1);

    func_80040030((u8*) newAddr, (u8*) oldAddr);
    gNextFreeMemoryAddress += offset;
    return oldAddr;
}

UNUSED u8* func_802A841C(u8* arg0, s32 arg1, s32 arg2) {
    u8* temp_v0;
    void* temp_a0;
    temp_v0 = (u8*) gNextFreeMemoryAddress;
    temp_a0 = temp_v0 + arg2;
    arg1 = ALIGN16(arg1);
    arg2 = ALIGN16(arg2);

    osInvalDCache(temp_a0, arg1);
    osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) &_other_texturesSegmentRomStart[SEGMENT_OFFSET(arg0)], temp_a0, arg1,
                 &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, 1);
    func_80040030((u8*) temp_a0, temp_v0);
    gNextFreeMemoryAddress += arg2;
    return temp_v0;
}

u8* dma_textures(u8 texture[], size_t arg1, size_t arg2) {
    u8* temp_v0;
    void* temp_a0;

    temp_v0 = (u8*) gNextFreeMemoryAddress;
    temp_a0 = temp_v0 + arg2;
    arg1 = ALIGN16(arg1);
    arg2 = ALIGN16(arg2);
    osInvalDCache((void*) temp_a0, arg1);
    osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) &_other_texturesSegmentRomStart[SEGMENT_OFFSET(texture)],
                 (void*) temp_a0, arg1, &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, (int) 1);
    mio0decode((u8*) temp_a0, temp_v0);
    gNextFreeMemoryAddress += arg2;
    return temp_v0;
}

uintptr_t MIO0_0F(u8* arg0, uintptr_t arg1, uintptr_t arg2) {
    uintptr_t oldHeapEndPtr;
    void* temp_v0;

    arg1 = ALIGN16(arg1);
    arg2 = ALIGN16(arg2);
    oldHeapEndPtr = gHeapEndPtr;
    temp_v0 = (void*) gNextFreeMemoryAddress;

    osInvalDCache(temp_v0, arg1);
    osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) &_other_texturesSegmentRomStart[SEGMENT_OFFSET(arg0)], temp_v0, arg1,
                 &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, 1);
    mio0decode((u8*) temp_v0, (u8*) oldHeapEndPtr);
    gHeapEndPtr += arg2;
    return oldHeapEndPtr;
}

void func_802A86A8(CourseVtx* data, u32 arg1) {
    CourseVtx* courseVtx = data;
    Vtx* vtx;
    s32 tmp = ALIGN16(arg1 * 0x10);
#ifdef AVOID_UB
    u32 i;
#else
    s32 i;
#endif
    s8 temp_a0;
    s8 temp_a3;
    s8 flags;

    gHeapEndPtr -= tmp;
    vtx = (Vtx*) gHeapEndPtr;

    // s32 to u32 comparison required for matching.
    for (i = 0; i < arg1; i++) {
        if (gIsMirrorMode) {
            vtx->v.ob[0] = -courseVtx->ob[0];
        } else {
            vtx->v.ob[0] = courseVtx->ob[0];
        }

        vtx->v.ob[1] = (courseVtx->ob[1] * vtxStretchY);
        temp_a0 = courseVtx->ca[0];
        temp_a3 = courseVtx->ca[1];

        flags = temp_a0 & 3;
        flags |= (temp_a3 << 2) & 0xC;

        vtx->v.ob[2] = courseVtx->ob[2];
        vtx->v.tc[0] = courseVtx->tc[0];
        vtx->v.tc[1] = courseVtx->tc[1];
        vtx->v.cn[0] = (temp_a0 & 0xFC);
        vtx->v.cn[1] = (temp_a3 & 0xFC);
        vtx->v.cn[2] = courseVtx->ca[2];
        vtx->v.flag = flags;
        vtx->v.cn[3] = 0xFF;
        vtx++;
        courseVtx++;
    }
}

void decompress_vtx(CourseVtx* arg0, u32 vertexCount) {
    s32 size = ALIGN16(vertexCount * 0x18);
    u32 segment = SEGMENT_NUMBER2(arg0);
    u32 offset = SEGMENT_OFFSET(arg0);
    void* freeSpace;
    u8* vtxCompressed = VIRTUAL_TO_PHYSICAL2(gSegmentTable[segment] + offset);
    UNUSED s32 pad;

    freeSpace = (void*) gNextFreeMemoryAddress;
    gNextFreeMemoryAddress += size;

    mio0decode(vtxCompressed, (u8*) freeSpace);
    func_802A86A8((CourseVtx*) freeSpace, vertexCount);
    set_segment_base_addr(4, (void*) gHeapEndPtr);
}

UNUSED void func_802A8844(void) {
}

void unpack_lights(Gfx* arg0, UNUSED u8* arg1, s8 arg2) {
    UNUSED s32 pad;
    s32 a = (arg2 * 0x18) + 0x9000008;
    s32 b = (arg2 * 0x18) + 0x9000000;
    Gfx macro[] = { gsSPNumLights(NUMLIGHTS_1) };

    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;

    sGfxSeekPosition++;
    arg0[sGfxSeekPosition].words.w0 = 0x3860010;

    arg0[sGfxSeekPosition].words.w1 = a;

    sGfxSeekPosition++;
    arg0[sGfxSeekPosition].words.w0 = 0x3880010;
    arg0[sGfxSeekPosition].words.w1 = b;
    sGfxSeekPosition++;
}

void unpack_displaylist(Gfx* arg0, u8* args, UNUSED s8 opcode) {
    uintptr_t temp_v0 = args[sPackedSeekPosition++];
    uintptr_t temp_t7 = ((args[sPackedSeekPosition++]) << 8 | temp_v0) * 8;
    arg0[sGfxSeekPosition].words.w0 = 0x06000000;
    // Segment seven addr
    arg0[sGfxSeekPosition].words.w1 = 0x07000000 + temp_t7;
    sGfxSeekPosition++;
}

// end displaylist
void unpack_end_displaylist(Gfx* arg0, UNUSED u8* arg1, UNUSED s8 arg2) {
    arg0[sGfxSeekPosition].words.w0 = (uintptr_t) (uint8_t) G_ENDDL << 24;
    arg0[sGfxSeekPosition].words.w1 = 0;
    sGfxSeekPosition++;
}

void unpack_set_geometry_mode(Gfx* arg0, UNUSED u8* arg1, UNUSED s8 arg2) {
    Gfx macro[] = { gsSPSetGeometryMode(G_CULL_BACK) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_clear_geometry_mode(Gfx* arg0, UNUSED u8* arg1, UNUSED s8 arg2) {
    Gfx macro[] = { gsSPClearGeometryMode(G_CULL_BACK) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_cull_displaylist(Gfx* arg0, UNUSED u8* arg1, UNUSED s8 arg2) {
    Gfx macro[] = { gsSPCullDisplayList(0, 7) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_combine_mode1(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetCombineMode(G_CC_MODULATERGBA, G_CC_MODULATERGBA) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_combine_mode2(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetCombineMode(G_CC_MODULATERGBDECALA, G_CC_MODULATERGBDECALA) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_combine_mode_shade(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_combine_mode4(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetCombineMode(G_CC_MODULATERGBDECALA, G_CC_MODULATERGBDECALA) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_combine_mode5(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_render_mode_opaque(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_render_mode_tex_edge(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_render_mode_translucent(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetRenderMode(G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_render_mode_opaque_decal(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetRenderMode(G_RM_AA_ZB_OPA_DECAL, G_RM_AA_ZB_OPA_DECAL) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_render_mode_translucent_decal(Gfx* arg0, UNUSED u8* arg1, UNUSED uintptr_t arg2) {
    Gfx macro[] = { gsDPSetRenderMode(G_RM_AA_ZB_XLU_DECAL, G_RM_AA_ZB_XLU_DECAL) };
    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_tile_sync(Gfx* gfx, u8* args, s8 opcode) {
    Gfx tileSync[] = { gsDPTileSync() };
    uintptr_t temp_a0;
    uintptr_t lo;
    uintptr_t hi;

    s32 width;
    s32 height;
    s32 fmt;
    s32 siz;
    s32 line;
    s32 tmem;
    s32 cms;
    s32 masks;
    s32 cmt;
    s32 maskt;
    s32 lrs;
    s32 lrt;
    UNUSED s32 pad[4];

    tmem = 0;
    switch (opcode) {
        case 26:
            width = 32;
            height = 32;
            fmt = 0;
            break;
        case 44:
            width = 32;
            height = 32;
            fmt = 0;
            tmem = 256;
            break;
        case 27:
            width = 64;
            height = 32;
            fmt = 0;
            break;
        case 28:
            width = 32;
            height = 64;
            fmt = 0;
            break;
        case 29:
            width = 32;
            height = 32;
            fmt = 3;
            break;
        case 30:
            width = 64;
            height = 32;
            fmt = 3;
            break;
        case 31:
            width = 32;
            height = 64;
            fmt = 3;
            break;
    }

    // Set arguments

    siz = G_IM_SIZ_16b_BYTES;
    line = ((((width * 2) + 7) >> 3));

    temp_a0 = args[sPackedSeekPosition++];
    cms = temp_a0 & 0xF;
    masks = (temp_a0 & 0xF0) >> 4;

    temp_a0 = args[sPackedSeekPosition++];
    cmt = temp_a0 & 0xF;
    maskt = (temp_a0 & 0xF0) >> 4;

    // Generate gfx

    gfx[sGfxSeekPosition].words.w0 = tileSync->words.w0;
    gfx[sGfxSeekPosition].words.w1 = tileSync->words.w1;
    sGfxSeekPosition++;

    lo = ((uintptr_t) (uint8_t) G_SETTILE << 24) | (fmt << 21) | (siz << 19) | (line << 9) | tmem;
    hi = ((cmt) << 18) | ((maskt) << 14) | ((cms) << 8) | ((masks) << 4);

    gfx[sGfxSeekPosition].words.w0 = lo;
    gfx[sGfxSeekPosition].words.w1 = hi;
    sGfxSeekPosition++;

    lrs = (width - 1) << 2;
    lrt = (height - 1) << 2;

    lo = ((uintptr_t) (uint8_t) G_SETTILESIZE << 24);
    hi = (lrs << 12) | lrt;

    gfx[sGfxSeekPosition].words.w0 = lo;
    gfx[sGfxSeekPosition].words.w1 = hi;
    sGfxSeekPosition++;
}

void unpack_tile_load_sync(Gfx* gfx, u8* args, s8 opcode) {
    UNUSED uintptr_t var;
    Gfx tileSync[] = { gsDPTileSync() };
    Gfx loadSync[] = { gsDPLoadSync() };

    uintptr_t arg;
    uintptr_t lo;
    uintptr_t hi;
    uintptr_t addr;
    uintptr_t width;
    uintptr_t height;
    uintptr_t fmt;
    uintptr_t siz;
    uintptr_t tmem;
    uintptr_t tile;

    switch (opcode) {
        case 32:
            width = 32;
            height = 32;
            fmt = 0;
            break;
        case 33:
            width = 64;
            height = 32;
            fmt = 0;
            break;
        case 34:
            width = 32;
            height = 64;
            fmt = 0;
            break;
        case 35:
            width = 32;
            height = 32;
            fmt = 3;
            break;
        case 36:
            width = 64;
            height = 32;
            fmt = 3;
            break;
        case 37:
            width = 32;
            height = 64;
            fmt = 3;
            break;
    }

    // Set arguments

    // Waa?
    var = args[sPackedSeekPosition];
    // Generates a texture address.
    addr = SEGMENT_ADDR(0x05, args[sPackedSeekPosition++] << 11);
    sPackedSeekPosition++;
    arg = args[sPackedSeekPosition++];
    siz = G_IM_SIZ_16b;
    tmem = (arg & 0xF);
    tile = (arg & 0xF0) >> 4;

    // Generate gfx

    lo = ((uintptr_t) (uint8_t) G_SETTIMG << 24) | (fmt << 21) | (siz << 19);
    gfx[sGfxSeekPosition].words.w0 = lo;
    gfx[sGfxSeekPosition].words.w1 = addr;
    sGfxSeekPosition++;

    gfx[sGfxSeekPosition].words.w0 = tileSync->words.w0;
    gfx[sGfxSeekPosition].words.w1 = tileSync->words.w1;
    sGfxSeekPosition++;

    lo = ((uintptr_t) (uint8_t) G_SETTILE << 24) | (fmt << 21) | (siz << 19) | tmem;
    hi = tile << 24;

    gfx[sGfxSeekPosition].words.w0 = lo;
    gfx[sGfxSeekPosition].words.w1 = hi;
    sGfxSeekPosition++;

    gfx[sGfxSeekPosition].words.w0 = loadSync->words.w0;
    gfx[sGfxSeekPosition].words.w1 = loadSync->words.w1;
    sGfxSeekPosition++;

    lo = (uintptr_t) (uint8_t) G_LOADBLOCK << 24;
    hi = (tile << 24) | (MIN((width * height) - 1, 0x7FF) << 12) | CALC_DXT(width, G_IM_SIZ_16b_BYTES);

    gfx[sGfxSeekPosition].words.w0 = lo;
    gfx[sGfxSeekPosition].words.w1 = hi;
    sGfxSeekPosition++;
}

void unpack_texture_on(Gfx* arg0, UNUSED u8* args, UNUSED s8 arg2) {
    Gfx macro[] = { gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON) };

    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_texture_off(Gfx* arg0, UNUSED u8* args, UNUSED s8 arg2) {
    Gfx macro[] = { gsSPTexture(0x1, 0x1, 0, G_TX_RENDERTILE, G_OFF) };

    arg0[sGfxSeekPosition].words.w0 = macro->words.w0;
    arg0[sGfxSeekPosition].words.w1 = macro->words.w1;
    sGfxSeekPosition++;
}

void unpack_vtx1(Gfx* gfx, u8* args, UNUSED s8 arg2) {
    uintptr_t temp_t7;
    uintptr_t temp_t7_2;

    uintptr_t temp = args[sPackedSeekPosition++];
    uintptr_t temp2 = ((args[sPackedSeekPosition++] << 8) | temp) * 0x10;

    temp = args[sPackedSeekPosition++];
    temp_t7 = temp & 0x3F;
    temp = args[sPackedSeekPosition++];
    temp_t7_2 = temp & 0x3F;

    gfx[sGfxSeekPosition].words.w0 =
        ((uintptr_t) (uint8_t) G_VTX << 24) | (temp_t7_2 * 2 << 16) | (((temp_t7 << 10) + ((0x10 * temp_t7) - 1)));
    gfx[sGfxSeekPosition].words.w1 = 0x04000000 + temp2;
    sGfxSeekPosition++;
}

void unpack_vtx2(Gfx* gfx, u8* args, s8 arg2) {
    uintptr_t temp_t9;
    uintptr_t temp_v1;
    uintptr_t temp_v2;

    temp_v1 = args[sPackedSeekPosition++];
    temp_v2 = ((args[sPackedSeekPosition++] << 8) | temp_v1) * 0x10;

    temp_t9 = arg2 - 50;

    gfx[sGfxSeekPosition].words.w0 = ((uintptr_t) (uint8_t) G_VTX << 24) | ((temp_t9 << 10) + (((temp_t9) * 0x10) - 1));
    gfx[sGfxSeekPosition].words.w1 = 0x4000000 + temp_v2;
    sGfxSeekPosition++;
}

void unpack_triangle(Gfx* gfx, u8* args, UNUSED s8 arg2) {
    uintptr_t temp_v0;
    uintptr_t phi_a0;
    uintptr_t phi_a2;
    uintptr_t phi_a3;

    temp_v0 = args[sPackedSeekPosition++];

    if (gIsMirrorMode) {
        phi_a3 = temp_v0 & 0x1F;
        phi_a2 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_a2 |= (temp_v0 & 3) * 8;
        phi_a0 = (temp_v0 >> 2) & 0x1F;
    } else {
        phi_a0 = temp_v0 & 0x1F;
        phi_a2 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_a2 |= (temp_v0 & 3) * 8;
        phi_a3 = (temp_v0 >> 2) & 0x1F;
    }
    gfx[sGfxSeekPosition].words.w0 = ((uintptr_t) (uint8_t) G_TRI1 << 24);
    gfx[sGfxSeekPosition].words.w1 = ((phi_a0 * 2) << 16) | ((phi_a2 * 2) << 8) | (phi_a3 * 2);
    sGfxSeekPosition++;
}

void unpack_quadrangle(Gfx* gfx, u8* args, UNUSED s8 arg2) {
    uintptr_t temp_v0;
    uintptr_t phi_t0;
    uintptr_t phi_a3;
    uintptr_t phi_a0;
    uintptr_t phi_t2;
    uintptr_t phi_t1;
    uintptr_t phi_a2;

    temp_v0 = args[sPackedSeekPosition++];

    if (gIsMirrorMode) {
        phi_t0 = temp_v0 & 0x1F;
        phi_a3 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_a3 |= (temp_v0 & 3) * 8;
        phi_a0 = (temp_v0 >> 2) & 0x1F;
    } else {
        phi_a0 = temp_v0 & 0x1F;
        phi_a3 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_a3 |= (temp_v0 & 3) * 8;
        phi_t0 = (temp_v0 >> 2) & 0x1F;
    }

    temp_v0 = args[sPackedSeekPosition++];

    if (gIsMirrorMode) {
        phi_a2 = temp_v0 & 0x1F;
        phi_t1 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_t1 |= (temp_v0 & 3) * 8;
        phi_t2 = (temp_v0 >> 2) & 0x1F;
    } else {
        phi_t2 = temp_v0 & 0x1F;
        phi_t1 = (temp_v0 >> 5) & 7;
        temp_v0 = args[sPackedSeekPosition++];
        phi_t1 |= (temp_v0 & 3) * 8;
        phi_a2 = (temp_v0 >> 2) & 0x1F;
    }
    gfx[sGfxSeekPosition].words.w0 =
        ((uintptr_t) (uint8_t) G_TRI2 << 24) | ((phi_a0 * 2) << 16) | ((phi_a3 * 2) << 8) | (phi_t0 * 2);
    gfx[sGfxSeekPosition].words.w1 = ((phi_t2 * 2) << 16) | ((phi_t1 * 2) << 8) | (phi_a2 * 2);
    sGfxSeekPosition++;
}

void unpack_spline_3D(Gfx* gfx, u8* arg1, UNUSED s8 arg2) {
    uintptr_t temp_v0;
    uintptr_t phi_a0;
    uintptr_t phi_t0;
    uintptr_t phi_a3;
    uintptr_t phi_a2;

    temp_v0 = arg1[sPackedSeekPosition++];

    if (gIsMirrorMode != 0) {
        phi_a0 = temp_v0 & 0x1F;
        phi_a2 = ((temp_v0 >> 5) & 7);
        temp_v0 = arg1[sPackedSeekPosition++];
        phi_a2 |= ((temp_v0 & 3) * 8);
        phi_a3 = (temp_v0 >> 2) & 0x1F;
        phi_t0 = ((temp_v0 >> 7) & 1);
        temp_v0 = arg1[sPackedSeekPosition++];
        phi_t0 |= (temp_v0 & 0xF) * 2;
    } else {
        phi_t0 = temp_v0 & 0x1F;
        phi_a3 = ((temp_v0 >> 5) & 7);
        temp_v0 = arg1[sPackedSeekPosition++];
        phi_a3 |= ((temp_v0 & 3) * 8);
        phi_a2 = (temp_v0 >> 2) & 0x1F;
        phi_a0 = ((temp_v0 >> 7) & 1);
        temp_v0 = arg1[sPackedSeekPosition++];
        phi_a0 |= (temp_v0 & 0xF) * 2;
    }
    gfx[sGfxSeekPosition].words.w0 = ((uintptr_t) (uint8_t) G_QUAD << 24);
    gfx[sGfxSeekPosition].words.w1 = ((phi_a0 * 2) << 24) | ((phi_t0 * 2) << 16) | ((phi_a3 * 2) << 8) | (phi_a2 * 2);
    sGfxSeekPosition++;
}

UNUSED void func_802A9AEC(void) {
}

/**
 * Unpacks course packed displaylists by iterating through each byte of the packed file.
 * Each packed displaylist entry has an opcode and any number of arguments.
 * The opcodes range from 0 to 87 which are used to run the relevant unpack function.
 * The file pointer increments when arguments are used. This way,
 * displaylist_unpack will always read an opcode and not an argument by accident.
 *
 * @warning opcodes that do not contain a definition in the switch are ignored. If an undefined opcode
 * contained arguments the unpacker might try to unpack those arguments.
 * This issue is prevented so long as the packed file adheres to correct opcodes and unpack code
 * increments the file pointer the correct number of times.
 */
void displaylist_unpack(uintptr_t* data) {
	set_segment_base_addr(0x7, (void*) data);
}

struct UnkStr_802AA7C8 {
    u8* unk0;
    uintptr_t unk4;
    uintptr_t unk8;
    uintptr_t unkC;
};

const char __attribute__((aligned(32))) coursenames[20][32] = {
"mario_raceway",
"choco_mountain",
"bowsers_castle",
"banshee_boardwalk",
"yoshi_valley",
"frappe_snowland",
"koopa_troopa_beach",
"royal_raceway",
"luigi_raceway",
"moo_moo_farm",
"toads_turnpike",
"kalimari_desert",
"sherbet_land",
"rainbow_road",
"wario_stadium",
"block_fort",
"skyscraper",
"double_deck",
"dks_jungle_parkway",
"big_donut",
};

char *get_course_name(s16 course) {
    return coursenames[course];
}
extern char *fnpre;

void decompress_textures(UNUSED u32* arg0) {
    char *courseName = get_course_name(gCurrentCourseId);
	sprintf(texfn, "%s/dc_data/%s_tex.bin", fnpre, courseName);
    FILE *file = fopen(texfn, "rb");

    if (!file) {
        perror("fopen");
	    exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

	long toread = filesize;
    long didread = 0;

    while(didread < toread) {
        long rv = fread(&SEG5_BUF[didread], 1, toread - didread, file);
        if (rv == -1) { printf("FILE IS FUCKED\n"); exit(-1); }
	    toread -= rv;
    	didread += rv;
    }
    fclose(file);

    set_segment_base_addr(0x5, (void*)SEG5_BUF);
}
static inline uint32_t Swap32(uint32_t val)
{
	return ((((val)&0xff000000) >> 24) | (((val)&0x00ff0000) >> 8) |
		(((val)&0x0000ff00) << 8) | (((val)&0x000000ff) << 24));
}
//extern uint8_t TEMP_DECODE_BUF[131072];
u32 max_size = 0;
void* decompress_segments(u8* start, u8 *target) {
    mio0decode(segmented_to_virtual(start), (u8*) target);
    return (void*) target;
}
/**
 * @brief Loads & DMAs course data. Vtx, textures, displaylists, etc.
 * @param courseId
 */
extern uint8_t __attribute__((aligned(32))) COURSE_BUF[146464];
extern u8 __attribute__((aligned(32))) UNPACK_BUF[51008];

u32 packoffs[20] = {
0x00009754,// g       .data	00000000 d_course_mario_raceway_packed
0x0000a130,// g       .data	00000000 d_course_choco_mountain_packed
0x0000e350,// g       .data	00000000 d_course_bowsers_castle_packed
0x000069f4,// g       .data	00000000 d_course_banshee_boardwalk_packed
0x00007d54,// g       .data	00000000 d_course_yoshi_valley_packed
0x00009d88,// g       .data	00000000 d_course_frappe_snowland_packed
0x0000faec,// g       .data	00000000 d_course_koopa_troopa_beach_packed
0x0000ec44,// g       .data	00000000 d_course_royal_raceway_packed
0x00009760,// g       .data	00000000 d_course_luigi_raceway_packed
0x0000d998,// g       .data	00000000 d_course_moo_moo_farm_packed
0x0000a75c,// g       .data	00000000 d_course_toads_turnpike_packed
0x0000b520,// g       .data	00000000 d_course_kalimari_desert_packed
0x00004924,// g       .data	00000000 d_course_sherbet_land_packed
0x00005b4c,// g       .data	00000000 d_course_rainbow_road_packed
0x0000aa08,// g       .data	00000000 d_course_wario_stadium_packed
0x000018cc,// g       .data	00000000 d_course_block_fort_packed
0x00001700,// g       .data	00000000 d_course_skyscraper_packed
0x00000cc0,// g       .data	00000000 d_course_double_deck_packed
0x0000a4f8,// g       .data	00000000 d_course_dks_jungle_parkway_packed
0x00001c74,// g       .data	00000000 d_course_big_donut_packed
};
extern void nuke_everything(void);
#if 0
extern u16 d_course_rainbow_road_static_tluts[];
extern u16 d_tlut_rainbow_road_neon_luigi[];
extern u16 d_tlut_rainbow_road_neon_dk[];
extern u16 d_tlut_rainbow_road_neon_yoshi[];
extern u16 d_tlut_rainbow_road_neon_bowser[];
extern u16 d_tlut_rainbow_road_neon_wario[];
extern u16 d_tlut_rainbow_road_neon_toad[];
#endif



//int max_unpack = 0;
u8* load_course(s32 courseId) {
    u32 vertexCount;
    nuke_everything();

    vertexCount = gCourseTable[courseId].vertexCount;
#if 1
    memset(COURSE_BUF, 0, sizeof(COURSE_BUF));
    memset(COMP_VERT_BUF, 0, sizeof(COMP_VERT_BUF));
    memset(DECOMP_VERT_BUF, 0, sizeof(DECOMP_VERT_BUF));
    memset(UNPACK_BUF, 0, sizeof(UNPACK_BUF));
#endif
    char *courseName = get_course_name(gCurrentCourseId);
	// open course data
#if 1
	sprintf(texfn, "%s/dc_data/%s_data.bin", fnpre, courseName);
#else
    sprintf(texfn, "/cd/dc_data/%s_data.bin", courseName);
#endif
    printf("opening %s\n", texfn);
	FILE *file = fopen(texfn, "rb");
    if (!file) {
        perror("fopen");
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    printf("Filesize %d\n", filesize);
	fseek(file, 0, SEEK_SET);

	long toread = filesize;
    long didread = 0;

    while(didread < toread) {
        long rv = fread(&COURSE_BUF[didread], 1, toread - didread, file);
        if (rv == -1) { printf("FILE IS FUCKED\n"); exit(-1); }
        toread -= rv;
        didread += rv;
    }
    fclose(file);
    file = NULL;

    set_segment_base_addr(6, COURSE_BUF);

	// get the verts and the packed
#if 1
    sprintf(texfn, "%s/dc_data/%s_geography.bin", fnpre,courseName);
#else
    sprintf(texfn, "/cd/dc_data/%s_geography.bin", courseName);
#endif
	file = fopen(texfn, "rb");
    if (!file) {
        perror("fopen");
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    rewind(file);

    {
        long toread = packoffs[gCurrentCourseId];
        long didread = 0;

        while(didread < toread) {

            long rv = fread(&COMP_VERT_BUF[didread], 1, toread - didread, file);
            if (rv == -1) { printf("FILE IS FUCKED\n"); exit(-1); }
            toread -= rv;
            didread += rv;
        }
    }
  
    {
        long toread = filesize - packoffs[gCurrentCourseId];
//        if (toread > max_unpack) {
   //         max_unpack = toread;
     //       printf("max_unpack %d\n", max_unpack);
       // }
        long didread = 0;

        while(didread < toread) {

        //	uint8_t *tmpbuf = malloc(filesize);
            long rv = fread(&UNPACK_BUF[didread], 1, toread - didread, file);
            if (rv == -1) { printf("FILE IS FUCKED\n"); exit(-1); }
            toread -= rv;
            didread += rv;
        }

        fclose(file);
        file = NULL;

        set_segment_base_addr(0xF, (void*) COMP_VERT_BUF);
        decompress_vtx(COMP_VERT_BUF, DECOMP_VERT_BUF);

        displaylist_unpack(UNPACK_BUF);
        printf("unpack to %08x\n", (uintptr_t*) UNPACK_BUF);
    }
    decompress_textures(0);
#if 0
if(gCurrentCourseId == 13) {
extern u8 d_course_rainbow_road_static_textures[][4096];

// 1 through 5
for (int n=0;n<6;n++) {
    u16* tlut_ptr = (u16*) segmented_to_virtual(d_course_rainbow_road_static_textures[n]);
        for (int i = 0; i < 2048; i++) {
            uint16_t np = tlut_ptr[i];
            np = (np << 8) | ((np >> 8) & 0xff);
            tlut_ptr[i] = np;
        }
}
}
#endif
    return COMP_VERT_BUF;
}
