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

#include <kos.h>

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

#if 0
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
#endif

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
    gSegmentTable[segment] = (uintptr_t) addr;
    return gSegmentTable[segment];
}

/**
 * @brief Returns the physical memory location of a segment.
 * @param permits segment numbers from 0x0 to 0xF.
 */
void* get_segment_base_addr(s32 segment) {
    return (void*) (gSegmentTable[segment]);
}
/**
 * @brief converts an RSP segment + offset address to a normal memory address
 */
void* segmented_to_virtual(const void* addr) {
	uintptr_t uip_addr = (uintptr_t)addr;

    if ((uip_addr >= 0x8c010000) && (uip_addr <= 0x8cffffff)) {
		return uip_addr;
	} 

    size_t segment = (uintptr_t) uip_addr >> 24;

    // investigate why this hits on Sherbet Land 4 player attract mode demo
//    if (segment < 0x2) {
//        printf("%08x converts to bad segment %02x %08x\n", addr, segment, uip_addr);
//    }
    
    if(segment > 0xf) {
        printf("%08x converts to bad segment %02x %08x\n", (uintptr_t)addr, segment, (uintptr_t)uip_addr);
            printf("\n");
        arch_stk_trace(0);
            printf("\n");
            while(1){}
        exit(-1);
	}

    size_t offset = (uintptr_t) uip_addr & 0x00FFFFFF;
    return (void*) ((gSegmentTable[segment] + offset));
}

void move_segment_table_to_dmem(void) {
//    s32 i;

//	//printf(__func__);
//	//printf("\n");

//    for (i = 0; i < 16; i++) {
//        gSPSegment(gDisplayListHead++, i, gSegmentTable[i]);
//    }
}

UNUSED void func_802A7D54(s32 arg0, s32 arg1) {
    gD_80150158[arg0].unk0 = arg0;
    gD_80150158[arg0].unk8 = arg1;
}

/**
 * @brief Allocate and DMA.
 */
void* load_data(uintptr_t startAddr, uintptr_t endAddr, uintptr_t target) {
    void* allocated;
    uintptr_t size = endAddr - startAddr;
    dma_copy((u8*) target, (u8*) startAddr, size);
    return (void*) target;
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

void gfx_texture_cache_invalidate(void* arg);
// starting address for this texture COMPRESSED is gNextFree+arg2
// starting address for this texture DECOMPRESSED is gNextFree
extern u8 *ROVING_SEG3_BUF;

u8* dma_textures(u8 texture[], size_t arg1, size_t arg2) {
    u8* temp_v0;
    void* temp_a0;
    temp_v0 = (u8*) ROVING_SEG3_BUF;
    temp_a0 = temp_v0 + arg2;
    arg1 = ALIGN16(arg1);
    arg2 = ALIGN16(arg2);
    dma_copy(temp_a0, texture, arg1);
    mio0decode((u8*) temp_a0, temp_v0);
    gfx_texture_cache_invalidate(temp_v0);
    ROVING_SEG3_BUF += arg2;
    return temp_v0;
}

void func_802A86A8(CourseVtx* data, u32 arg1) {
    CourseVtx* courseVtx = data;
    Vtx* vtx;
    s32 tmp = ALIGN16(arg1 * 0x10);
    u32 i;
    s8 temp_a0;
    s8 temp_a3;
    s8 flags;

    vtx = (Vtx*) SEG4_BUF;

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

void decompress_vtx(CourseVtx* arg0, u32 vertexCount, void *target) {
	u8* vtxCompressed;	
	vtxCompressed = (u8*)segmented_to_virtual(arg0);

    mio0decode(vtxCompressed, (u8*) target);
    func_802A86A8((CourseVtx*) target, vertexCount);
    set_segment_base_addr(4, (void*) SEG4_BUF);
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
        decompress_vtx(COMP_VERT_BUF, vertexCount, DECOMP_VERT_BUF);

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
