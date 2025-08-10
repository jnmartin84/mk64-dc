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

#include "buffer_sizes.h"
extern uint8_t __attribute__((aligned(32))) COURSE_BUF[COURSE_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) UNPACK_BUF[UNPACK_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) SEG4_BUF[SEG4_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) SEG5_BUF[SEG5_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) COMP_VERT_BUF[COMP_VERT_BUF_SIZE];
extern uint8_t __attribute__((aligned(32))) DECOMP_VERT_BUF[DECOMP_VERT_BUF_SIZE];

static char __attribute__((aligned(32))) texfn[256];

s32 sGfxSeekPosition;
s32 sPackedSeekPosition;

struct UnkStruct_802B8CD4 D_802B8CD4[] = { 0 };
s32 D_802B8CE4 = 0; // pad
s32 memoryPadding[2];

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

// borrowed from skmp / DCA3
// thanks
extern const char etext[];
__attribute__((noinline)) void stacktrace() {
	uint32 sp=0, pr=0;
	__asm__ __volatile__(
		"mov	r15,%0\n"
		"sts	pr,%1\n"
		: "+r" (sp), "+r" (pr)
		:
		: );
	printf("[ %08X ", (uintptr_t)pr);
	int found = 0;
	if(!(sp & 3) && sp > 0x8c000000 && sp < _arch_mem_top) {
		char** sp_ptr = (char**)sp;
		for (int so = 0; so < 16384; so++) {
			if ((uintptr_t)(&sp_ptr[so]) >= _arch_mem_top) {
				//printf("(@@%08X) ", (uintptr_t)&sp_ptr[so]);
				break;
			}
			if (sp_ptr[so] > (char*)0x8c000000 && sp_ptr[so] < etext) {
				uintptr_t addr = (uintptr_t)(sp_ptr[so]);
				// candidate return pointer
				if (addr & 1) {
					// dbglog(DBG_CRITICAL, "Stack trace: %p (@%p): misaligned\n", (void*)sp_ptr[so], &sp_ptr[so]);
					continue;
				}

				uint16_t* instrp = (uint16_t*)addr;

				uint16_t instr = instrp[-2];
				// BSR or BSRF or JSR @Rn ?
				if (((instr & 0xf000) == 0xB000) || ((instr & 0xf0ff) == 0x0003) || ((instr & 0xf0ff) == 0x400B)) {
					printf("%08X ", (uintptr_t)instrp);
					if (found++ > 24) {
						//printf("(@%08X) ", (uintptr_t)&sp_ptr[so]);
						break;
					}
				} else {
					// dbglog(DBG_CRITICAL, "%p:%04X ", instrp, instr);
				}
			} else {
				// dbglog(DBG_CRITICAL, "Stack trace: %p (@%p): out of range\n", (void*)sp_ptr[so], &sp_ptr[so]);
			}
		}
		printf("]\n");
	} else {
		printf("%08x ]\n", (uintptr_t)sp);
	}
}

/**
 * @brief converts an RSP segment + offset address to a normal memory address
 */
void* segmented_to_virtual(const void* addr) {
    uintptr_t uip_addr = (uintptr_t) addr;

    /*
        going from player select to map select hits this for some stuff
        0x2000 range
        Sherbet Land has 0x00000004, 0x00000009, 0x0000000b
        if (uip_addr < 0x02000000) {
            printf("tried to use NULL-ish addr %08x as segmented addr\n", uip_addr);
            stacktrace();
        }
    */

    if ((uip_addr >= 0x8c010000) && (uip_addr <= 0x8cffffff)) {
        return uip_addr;
    }

    size_t segment = (uintptr_t) uip_addr >> 24;

    // investigate why this hits on Sherbet Land 4 player attract mode demo
    /* if (segment < 0x2) {
        printf("%08x converts to bad segment %02x %08x\n", addr, segment, uip_addr);
    } */
#if DEBUG
    if (segment > 0xf) {
        printf("%08x converts to bad segment %02x %08x\n", (uintptr_t) addr, segment, (uintptr_t) uip_addr);
        printf("\n");
        stacktrace();
        printf("\n");
        while (1) {}
        exit(-1);
    }
#endif
    /* if (gSegmentTable[segment] == 0) {
        printf("segment %02x has null base address, original address %08x\n", segment, uip_addr);
    } */

    size_t offset = (uintptr_t) uip_addr & 0x00FFFFFF;
    return (void*) ((gSegmentTable[segment] + offset));
}

void move_segment_table_to_dmem(void) {
    /* s32 i;
    for (i = 0; i < 16; i++) {
        gSPSegment(gDisplayListHead++, i, gSegmentTable[i]);
    } */
}

void n64_memcpy(void* dst, const void* src, size_t size) {
    uint8_t* bdst = (uint8_t*) dst;
    uint8_t* bsrc = (uint8_t*) src;
    uint16_t *sdst = (uint16_t *)dst;
    uint16_t *ssrc = (uint16_t *)src;
    uint32_t* wdst = (uint32_t*) dst;
    uint32_t* wsrc = (uint32_t*) src;

    int size_to_copy = size;
    int words_to_copy = size_to_copy >> 2;
    int shorts_to_copy = size_to_copy >> 1;
    int bytes_to_copy = size_to_copy - (words_to_copy<<2);
    int sbytes_to_copy = size_to_copy - (shorts_to_copy<<1);

    __builtin_prefetch(bsrc);
    if ((!(((uintptr_t)bdst | (uintptr_t)bsrc) & 3))) {
        while (words_to_copy--) {
            if ((words_to_copy & 3) == 0) {
                __builtin_prefetch(bsrc + 16);
            }
            *wdst++ = *wsrc++;
        }

        bdst = (uint8_t*) wdst;
        bsrc = (uint8_t*) wsrc;

        switch (bytes_to_copy) {
            case 0:
                return;
            case 1:
                goto n64copy1;
            case 2:
                goto n64copy2;
            case 3:
                goto n64copy3;
            case 4:
                goto n64copy4;
            case 5:
                goto n64copy5;
            case 6:
                goto n64copy6;
            case 7:
                goto n64copy7;
        }
    }  else if ((!(((uintptr_t)bdst | (uintptr_t)bsrc) & 1))) {
        while (shorts_to_copy--) {
            *sdst++ = *ssrc++;
        }

        bdst = (uint8_t*) sdst;
        bsrc = (uint8_t*) ssrc;

        switch (sbytes_to_copy) {
            case 0:
                return;
            case 1:
                goto n64copy1;
            case 2:
                goto n64copy2;
            case 3:
                goto n64copy3;
            case 4:
                goto n64copy4;
            case 5:
                goto n64copy5;
            case 6:
                goto n64copy6;
            case 7:
                goto n64copy7;
        }
    } else {
        while (words_to_copy > 0) {
            uint8_t b1, b2, b3, b4;
            b1 = *bsrc++;
            b2 = *bsrc++;
            b3 = *bsrc++;
            b4 = *bsrc++;
#if 1
            asm volatile("" : : : "memory");
#endif
            *bdst++ = b1; //*bsrc++;
            *bdst++ = b2; //*bsrc++;
            *bdst++ = b3; //*bsrc++;
            *bdst++ = b4; //*bsrc++;

            words_to_copy--;
        }

        switch (bytes_to_copy) {
            case 0:
                return;
            case 1:
                goto n64copy1;
            case 2:
                goto n64copy2;
            case 3:
                goto n64copy3;
            case 4:
                goto n64copy4;
            case 5:
                goto n64copy5;
            case 6:
                goto n64copy6;
            case 7:
                goto n64copy7;
        }
    }

n64copy7:
    *bdst++ = *bsrc++;
n64copy6:
    *bdst++ = *bsrc++;
n64copy5:
    *bdst++ = *bsrc++;
n64copy4:
    *bdst++ = *bsrc++;
n64copy3:
    *bdst++ = *bsrc++;
n64copy2:
    *bdst++ = *bsrc++;
n64copy1:
    *bdst++ = *bsrc++;
    return;
}

void gfx_texture_cache_invalidate(void* arg);
extern u8 *ROVING_SEG3_BUF;
extern uint8_t __attribute__((aligned(32))) OTHER_BUF[OTHER_BUF_SIZE];

// starting address for this texture COMPRESSED is gNextFree+arg2
// starting address for this texture DECOMPRESSED is gNextFree
void mio0decode_noinval(const unsigned char *in, unsigned char *out);

u8* dma_textures(u8 texture[], UNUSED size_t arg1, size_t arg2) {
    u8* temp_v0;
    temp_v0 = (u8*) ROVING_SEG3_BUF;
    arg2 = ALIGN16(arg2);
    mio0decode_noinval((u8 *)texture, temp_v0);
    ROVING_SEG3_BUF += arg2;
    return temp_v0;
}

void func_802A86A8(CourseVtx* data, u32 arg1) {
    CourseVtx* courseVtx = data;
    Vtx* vtx;
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
void mio0decode_noinval(const unsigned char *in, unsigned char *out);

void decompress_vtx(CourseVtx* arg0, u32 vertexCount, void *target) {
	u8* vtxCompressed;	
	vtxCompressed = (u8*)segmented_to_virtual(arg0);

    mio0decode_noinval(vtxCompressed, (u8*) target);
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

char __attribute__((aligned(32))) coursenames[20][32] = {
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
    printf("loaded course textures\n");
}

void mio0decode_noinval(const unsigned char *in, unsigned char *out);

void* decompress_segments(u8* start, u8 *target) {
    mio0decode_noinval(segmented_to_virtual(start), (u8*) target);
    return (void*) target;
}

/**
 * @brief Loads & DMAs course data. Vtx, textures, displaylists, etc.
 * @param courseId
 */
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

void mute_stream(void);
void unmute_stream(void);

u8* load_course(s32 courseId) {
    u32 vertexCount;
//    mute_stream();
    nuke_everything();

    vertexCount = gCourseTable[courseId].vertexCount;

    memset(COURSE_BUF, 0, sizeof(COURSE_BUF));
    memset(COMP_VERT_BUF, 0, sizeof(COMP_VERT_BUF));
    memset(DECOMP_VERT_BUF, 0, sizeof(DECOMP_VERT_BUF));
    memset(UNPACK_BUF, 0, sizeof(UNPACK_BUF));

    char *courseName = get_course_name(gCurrentCourseId);
	// open course data
	sprintf(texfn, "%s/dc_data/%s_data.bin", fnpre, courseName);
    //printf("opening %s\n", texfn);
	FILE *file = fopen(texfn, "rb");
    if (!file) {
        perror("fopen");
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    //printf("Filesize %d\n", filesize);
	fseek(file, 0, SEEK_SET);

	long toread = filesize;
    long didread = 0;

    while (didread < toread) {
        long rv = fread(&COURSE_BUF[didread], 1, toread - didread, file);
        if (rv == -1) {
            printf("FILE IS FUCKED\n");
            exit(-1);
        }
        toread -= rv;
        didread += rv;
    }
    fclose(file);
    file = NULL;

    set_segment_base_addr(6, COURSE_BUF);

	// get the verts and the packed
    sprintf(texfn, "%s/dc_data/%s_geography.bin", fnpre, courseName);
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

        while (didread < toread) {
            long rv = fread(&COMP_VERT_BUF[didread], 1, toread - didread, file);
            if (rv == -1) {
                printf("FILE IS FUCKED\n");
                exit(-1);
            }
            toread -= rv;
            didread += rv;
        }
    }
  
    {
        long toread = filesize - packoffs[gCurrentCourseId];
        long didread = 0;

        while (didread < toread) {
            long rv = fread(&UNPACK_BUF[didread], 1, toread - didread, file);
            if (rv == -1) {
                    printf("FILE IS FUCKED\n");
                    exit(-1);
            }
            toread -= rv;
            didread += rv;
        }

        fclose(file);
        file = NULL;

        set_segment_base_addr(0xF, (void*) COMP_VERT_BUF);
        decompress_vtx(COMP_VERT_BUF, vertexCount, DECOMP_VERT_BUF);

        displaylist_unpack(UNPACK_BUF);
        //printf("unpack to %08x\n", (uintptr_t*) UNPACK_BUF);
    }
    decompress_textures(0);
//    unmute_stream();
    if (strncmp("/cd", fnpre, 3) == 0) {
        cdrom_spin_down();
    }
    return COMP_VERT_BUF;
}
