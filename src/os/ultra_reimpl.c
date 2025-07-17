//#include <stdio.h>
//#include <string.h>
//#include "lib/src/libultra_internal.h"
#define UNUSED 
#include <string.h>
#include <PR/ultratypes.h>
#include <PR/os_message.h>
#include <PR/os_pi.h>
#include <PR/os_vi.h>
#include <PR/os_time.h>
#include <PR/libultra.h>
//#include "macros.h"
#include <kos.h>
//extern OSMgrArgs piMgrArgs;
void* segmented_to_virtual(void* addr);

u64 osClockRate = 62500000;
void n64_memcpy(void *dst, const void *src, size_t size);

s32 osPiStartDma(UNUSED OSIoMesg *mb, UNUSED s32 priority, UNUSED s32 direction,
                 uintptr_t devAddr, void *vAddr, size_t nbytes,
                 UNUSED OSMesgQueue *mq) {
	void *vdevAddr = segmented_to_virtual((void *)devAddr);
    n64_memcpy(vAddr, (const void *) vdevAddr, nbytes);
    return 0;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}

void osSetEventMesg(UNUSED OSEvent e, UNUSED OSMesgQueue *mq, UNUSED OSMesg msg) {
}
s32 osJamMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}

s32 osSendMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
#if 0
    //#ifdef VERSION_EU
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }
    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;
#endif
    return 0;
}
s32 osRecvMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg *msg, UNUSED s32 flag) {
#if 0
    if (mq->validCount == 0) {
        return -1;
    }
    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
#endif
    return 0;
}

extern mutex_t mq_mutex;
extern OSMesgQueue *D_800EA3B4;
s32 AosSendMesg( OSMesgQueue *mq,  OSMesg msg,  s32 flag) {
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }

    index = (mq->first + mq->validCount) % mq->msgCount;

    mq->msg[index] = msg;
    mq->validCount++;

    return 0;
}

s32 AosRecvMesg( OSMesgQueue *mq,  OSMesg *msg,  s32 flag) {
    if (mq->validCount == 0) {
        return -1;
    }

    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    return 0;
}

uintptr_t osVirtualToPhysical(void *addr) {
    return (uintptr_t) addr;
}

void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode *mode) {
}
void osViSetEvent(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViBlack(UNUSED u8 active) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void *vaddr) {
}
void osSetTime(OSTime time) {
	
}
OSTime osGetTime(void) {
    return 0;
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void *a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void *a, UNUSED size_t b) {
}
void osInvalICache(UNUSED void *a, UNUSED size_t b) {
}

u32 osGetCount(void) {
    static u32 counter;
    return counter++;
}

s32 osAiSetFrequency(u32 freq) {
    u32 a1;
    s32 a2;
    u32 D_8033491C;

    D_8033491C = 0x02E6D354;
    a1 = D_8033491C / (float) freq + .5f;

    if (a1 < 0x84) {
        return -1;
    }

    a2 = (a1 / 66) & 0xff;
    if (a2 > 16) {
        a2 = 16;
    }

    return D_8033491C / (s32) a1;
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return -1;
}


s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    return -1;
}

s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
	return -1;
}

#if 0
/* file system interface */

s32 osPfsInitPak(OSMesgQueue*, OSPfs*, int);
s32 osPfsRepairId(OSPfs*);
s32 osPfsInit(OSMesgQueue*, OSPfs*, int);
s32 osPfsReFormat(OSPfs*, OSMesgQueue*, int);
s32 osPfsChecker(OSPfs*);
s32 osPfsAllocateFile(OSPfs*, u16, u32, u8*, u8*, int, s32*);
s32 osPfsFindFile(OSPfs*, u16, u32, u8*, u8*, s32*);
s32 osPfsDeleteFile(OSPfs*, u16, u32, u8*, u8*);
s32 osPfsReadWriteFile(OSPfs*, s32, u8, int, int, u8*);
s32 osPfsFileState(OSPfs*, s32, OSPfsState*);
s32 osPfsGetLabel(OSPfs*, u8*, int*);
s32 osPfsSetLabel(OSPfs*, u8*);
s32 osPfsIsPlug(OSMesgQueue*, u8*);
s32 osPfsFreeBlocks(OSPfs*, s32*);
s32 osPfsNumFiles(OSPfs*, s32*, s32*);

int32_t ControllerPakStatus = 1;
int32_t Pak_Memory = 0;

static char full_fn[20];

char *get_vmu_fn(maple_device_t *vmudev, char *fn) {
	if (fn)
		sprintf(full_fn, "/vmu/%c%d/%s", 'a'+vmudev->port, vmudev->unit, fn);
	else
		sprintf(full_fn, "/vmu/%c%d", 'a'+vmudev->port, vmudev->unit);

	return full_fn;
}

/* Description
osPfsAllocateFile creates a new game note (file) in a Controller Pak.
The company_code (company code), game_code (game code), game_name (note name),
ext_name (note extension), and length (size) arguments must be specified as the
information for the game note.
*/
s32 osPfsAllocateFile(OSPfs *pfs,  u16 company_code,  u32 game_code,  u8 *game_name, 
                      u8 *ext_name,  int length,  s32 *file_no) {
	ssize_t size;
	maple_device_t *vmudev = NULL;
	uint8_t *data;

	ControllerPakStatus = 0;

	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);

}

s32 osPfsInitPak(OSMesgQueue* a, OSPfs* b, int c) {
    return 0;
}
#endif