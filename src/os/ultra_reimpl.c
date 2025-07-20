// #include <stdio.h>
// #include <string.h>
// #include "lib/src/libultra_internal.h"
#define UNUSED
#include <string.h>
#include <PR/ultratypes.h>
#include <PR/os_message.h>
#include <PR/os_pi.h>
#include <PR/os_vi.h>
#include <PR/os_time.h>
#include <PR/libultra.h>
// #include "macros.h"
#include <kos.h>
#include <ultra64.h>
#include "save.h"
char* get_vmu_fn(maple_device_t* vmudev, char* fn);
// extern OSMgrArgs piMgrArgs;
void* segmented_to_virtual(void* addr);

u64 osClockRate = 62500000;
void n64_memcpy(void* dst, const void* src, size_t size);

s32 osPiStartDma(UNUSED OSIoMesg* mb, UNUSED s32 priority, UNUSED s32 direction, uintptr_t devAddr, void* vAddr,
                 size_t nbytes, UNUSED OSMesgQueue* mq) {
    void* vdevAddr = segmented_to_virtual((void*) devAddr);
    n64_memcpy(vAddr, (const void*) vdevAddr, nbytes);
    return 0;
}

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}

void osSetEventMesg(UNUSED OSEvent e, UNUSED OSMesgQueue* mq, UNUSED OSMesg msg) {
}
s32 osJamMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}

s32 osSendMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg msg, UNUSED s32 flag) {
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
s32 osRecvMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg* msg, UNUSED s32 flag) {
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
extern OSMesgQueue* D_800EA3B4;
s32 AosSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flag) {
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }

    index = (mq->first + mq->validCount) % mq->msgCount;

    mq->msg[index] = msg;
    mq->validCount++;

    return 0;
}

s32 AosRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flag) {
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

uintptr_t osVirtualToPhysical(void* addr) {
    return (uintptr_t) addr;
}

void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode* mode) {
}
void osViSetEvent(UNUSED OSMesgQueue* mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViBlack(UNUSED u8 active) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void* vaddr) {
}
void osSetTime(OSTime time) {
}
OSTime osGetTime(void) {
    return 0;
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void* a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void* a, UNUSED size_t b) {
}
void osInvalICache(UNUSED void* a, UNUSED size_t b) {
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

extern char* fnpre;
static char texfn[256];
static char icondata[512 * 2];

struct state_pak {
    OSPfsState state;
    file_t file;
    char filename[64];
};

struct state_pak openFile[16] = { 0 };

int fileIndex = 0;

static char eeprom_block[512];
#include <kos.h>
static file_t eeprom_file = -1;
static mutex_t eeprom_lock;
static int eeprom_init = 0;
// thanks @zcrc
#include <kos/oneshot_timer.h>
/* 2s timer, to delay closing the VMU file.
 * This is because the emulator might open/modify/close often, and we want the
 * VMU VFS driver to only write to the VMU once we're done modifying the file. */
static oneshot_timer_t *timer;

void eeprom_flush(void *arg) {
	mutex_lock_scoped(&eeprom_lock);

    if (eeprom_file != -1) {
        fs_close(eeprom_file);
        eeprom_file = -1;
    }
}

s32 osEepromProbe(OSMesgQueue* mq) {
	maple_device_t *vmudev = NULL;

    if (!eeprom_init) {
        mutex_init(&eeprom_lock, MUTEX_TYPE_NORMAL);
        eeprom_init = 1;
        timer = oneshot_timer_create(eeprom_flush, NULL, 2000);
    }

    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev) {
		//dbgio_printf("eeprom probe: could not enum\n");
        //vid_border_color(255,0,0);
		return 0;
	}
    //printf("EEPROM PROBE:\n");
    //vid_border_color(0,0,255);
    eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDONLY | O_META);
	if (-1 == eeprom_file) {
        //printf("\tmk64.rec did not exist on VMU a1\n");
        eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_CREAT | O_META);
		if (-1 == eeprom_file) {
			//printf("\tcant open mk64.rec for rdwr|creat\n");
            //vid_border_color(255,0,0);
			return 1;
		}

        vmu_pkg_t pkg;
        memset(eeprom_block, 0, 512);
        memset(&pkg, 0, sizeof(vmu_pkg_t));
        strcpy(pkg.desc_short,"Records");
        strcpy(pkg.desc_long, "Mario Kart 64");
        strcpy(pkg.app_id, "Mario Kart 64");
        sprintf(texfn, "%s/kart.ico", fnpre);
        pkg.icon_cnt = 2;
        pkg.icon_data = icondata;
        pkg.icon_anim_speed = 5;
        pkg.data_len = 512;
        pkg.data = eeprom_block;
        vmu_pkg_load_icon(&pkg, texfn);
        uint8_t *pkg_out;
        ssize_t pkg_size;
	    vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
	    if (!pkg_out || pkg_size <= 0) {
		    //printf("vmu_pkg_build failed\n");
		    fs_close(eeprom_file);
            eeprom_file = -1;
            //vid_border_color(255,0,0);
		    return 0;
	    }
        //vid_border_color(0,255,0);
        fs_write(eeprom_file, pkg_out, pkg_size);
        free(pkg_out);
        //printf("\tcreated mk64.rec\n");
        oneshot_timer_reset(timer);
    } else {
        //printf("\teeprom file existed on vmu a1\n");
        fs_close(eeprom_file);
        eeprom_file = -1;
        eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_META);
		if (-1 == eeprom_file) {
			//printf("\tcant open mk64.rec for rdwr\n");
            //vid_border_color(255,0,0);
			return EEPROM_TYPE_4K;
		}

        oneshot_timer_reset(timer);
    }

    //vid_border_color(0,0,0);
    //printf("successfully returning from EEPROM probe\n");
    return EEPROM_TYPE_4K;
}
uint8_t* vmu_load_data(int channel, const char* name, uint8_t* outbuf, uint32_t* bytes_read);

static int reopen_vmu_eeprom(void) {
    maple_device_t* vmudev = NULL;
    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        //vid_border_color(255, 0, 0);
        return 1;
    }

    eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_META);
    return 0;
}

s32 osEepromLongRead(OSMesgQueue* mq, unsigned char address, unsigned char* buffer, s32 length) {
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }
    
    //printf("EEPROM READ:\n");
    //vid_border_color(0,0,255);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);
        //printf("\tKOS claims mk64.rec exists on vmu A1 with size: %d\n", size);
        if (size != 2048) {
            fs_close(eeprom_file);
            //printf("\tbut the size was wrong (%d, expect 2048)\n", size);
            //vid_border_color(255,0,0);
            return 1;
        }
        // skip header
        //vid_border_color(0,255,0);
        fs_seek(eeprom_file, (512*3) + (address * 8), SEEK_SET);
        ssize_t rv = fs_read(eeprom_file, buffer, length);
        if (rv != length) {
            //printf("\tcould not read %d bytes from mk64.rec\n", length);
            //vid_border_color(255,0,0);
            return 1;
        }

        //vid_border_color(0,0,0);
        //printf("success reading EEPROM file\n");
        oneshot_timer_reset(timer);
        return 0;
    } else {
        //vid_border_color(255,0,0);
        return 1;
    }
}

s32 osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

s32 osEepromLongWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer, s32 length) {
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    //printf("EEPROM WRITE:\n");
    //vid_border_color(0,0,255);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);
        //printf("\tKOS claims mk64.rec exists on vmu A1 with size: %d\n", size);
        if (size != 2048) {
            fs_close(eeprom_file);
            //printf("\tbut the size was wrong (%d, expect 2048)\n", size);
            //vid_border_color(255,0,0);
            return 1;
        }
        // skip header
        //vid_border_color(0,0,255);

        fs_seek(eeprom_file, (512*3) + (address * 8), SEEK_SET);
        ssize_t rv = fs_write(eeprom_file, buffer, length);
        if (rv != length) {
            //printf("\tcould not write %d bytes to mk64.rec\n", length);
            //vid_border_color(255,0,0);
            return 1;
        }

        //vid_border_color(0,0,0);
        //printf("success writing EEPROM file\n");
        oneshot_timer_reset(timer);
        return 0;
    } else {
        //vid_border_color(255,0,0);
        return 1;
    }
}

s32 osEepromWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}
static vmu_pkg_t ghostpkg;
s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    maple_device_t* vmudev = NULL;
    //printf("%s\n",__func__);

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_ERR_NOPACK;

    int rv = fs_unlink(get_vmu_fn(vmudev, "mk64.gho"));
    if (rv)
        return PFS_ERR_ID_FATAL;

    return PFS_NO_ERROR;
}

static uint8_t __attribute__((aligned(32))) tempblock[32768];//64 * 512];

s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer) {
    //printf("%s(%s,%d,%d)\n",__func__, flag ? "WRITE" : "READ", offset, size_in_bytes);
    //printf("openFile[%d].file == %d .filename == %s\n", file_no, openFile[file_no].file, openFile[file_no].filename);
    if (openFile[file_no].file == -1) {
        return PFS_ERR_INVALID;
    }

    if (flag == PFS_READ) {
        //        fseek(openFile[file_no].file, offset, SEEK_SET);
        //        fread(data_buffer, size_in_bytes, 1, openFile[file_no].file);
        fs_seek(openFile[file_no].file, (512*3) + offset, SEEK_SET);
        ssize_t res = fs_read(openFile[file_no].file, data_buffer, size_in_bytes);

        if (res != size_in_bytes) {
            //printf("could not read it right\n");
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_ERR_BAD_DATA;
        }
        // dont need to close/reopen, didnt write
    } else {
        //        fseek(openFile[file_no].file, offset, SEEK_SET);
        //        fwrite(data_buffer, size_in_bytes, 1, openFile[file_no].file);
        fs_seek(openFile[file_no].file, (512*3) + offset, SEEK_SET);
        ssize_t rv = fs_write(openFile[file_no].file, data_buffer, size_in_bytes);
        if (rv != size_in_bytes) {
            //printf("could not write it right\n");
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_CORRUPTED;
        }

        fs_close(openFile[file_no].file);
        openFile[file_no].file = fs_open(openFile[file_no].filename, O_RDWR | O_META); // fopen(filename, "w+");
        if (-1 == openFile[file_no].file) {
            //printf("write was fucked upon reopen attempt\n");
            return PFS_CORRUPTED;
        }
    }
    return PFS_NO_ERROR;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, int file_size_in_bytes,
                      s32* file_no) {
    maple_device_t* vmudev = NULL;
    //printf("%s(%s) %d\n", __func__, game_name, file_size_in_bytes);

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_NO_PAK_INSERTED;

    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    *file_no = fileIndex++;
    openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META);
    if (-1 == openFile[*file_no].file) {
        //printf("couldnt open file in allocate\n");
        return PFS_INVALID_DATA;
    }

    strcpy(openFile[*file_no].filename, filename);

    vmu_pkg_t newpkg;
    memset(&newpkg, 0, sizeof(vmu_pkg_t));
    memset(tempblock, 0, 32768);
    strcpy(newpkg.desc_short, "Ghost Data");
    strcpy(newpkg.desc_long, "Mario Kart 64");
    strcpy(newpkg.app_id, "Mario Kart 64");
    newpkg.data = tempblock;
    newpkg.data_len = 32768;
    newpkg.icon_data = icondata;
    newpkg.icon_cnt = 2;
    newpkg.icon_anim_speed = 5;
    sprintf(texfn, "%s/ghost.ico", fnpre);
    vmu_pkg_load_icon(&newpkg, texfn);
    ssize_t* pkg_size;
    u8* pkg_out;
    vmu_pkg_build(&newpkg, &pkg_out, &pkg_size);
    //printf("built ghostdata package\n");
    ssize_t rv = fs_write(openFile[*file_no].file, pkg_out, pkg_size);
    if (rv != pkg_size) {
            fs_close(openFile[*file_no].file);
            free(pkg_out);
            return PFS_CORRUPTED;
    }
    fs_close(openFile[*file_no].file);
    free(pkg_out);
    openFile[*file_no].file = fs_open(openFile[*file_no].filename, O_RDWR | O_META);
    if (-1 == openFile[*file_no].file) {
        return PFS_CORRUPTED;
    }
    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, game_name);
    strcpy(openFile[*file_no].state.ext_name, ext_name);
    return PFS_NO_ERROR;
}

extern int vmu_status(int channel);

s32 osPfsIsPlug(OSMesgQueue* queue, u8* pattern) {
    //printf("%s\n",__func__);
    *pattern = 0;
    if (!vmu_status(0))
        *pattern = 1;
    return 1;
}
s32 osPfsInit(OSMesgQueue* queue, OSPfs* pfs, int channel) {
    //printf("%s(%d)\n",__func__,channel);
    int rv = vmu_status(channel);
    if (rv) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->queue =  queue;
    if (channel != 0) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->channel = channel;
    pfs->status = 0;
    pfs->status |= PFS_INITIALIZED;
    return PFS_NO_ERROR;
}

s32 osPfsNumFiles(OSPfs* pfs, s32* max_files, s32* files_used) {
    //printf("%s\n",__func__);
    *max_files = 16;
    *files_used = fileIndex;
    return 0;
}

s32 osPfsFileState(OSPfs* pfs, s32 file_no, OSPfsState* state) {
    //printf("%s\n",__func__);
    return PFS_NO_ERROR;
}
extern int32_t Pak_Memory;

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used) {
    //printf("%s\n",__func__);
    vmu_status(pfs->channel);

    *bytes_not_used = Pak_Memory * 512;//0x10000;
    return PFS_NO_ERROR;
}

s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no) {
    maple_device_t* vmudev = NULL;
    //printf("%s(%d,%s)\n", __func__, pfs->channel, game_name);

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        //printf("couldn't get vmu for pfs->channel %d\n", pfs->channel);
        return PFS_NO_PAK_INSERTED;
    }
    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    for (size_t i = 0; i < 16; i++) {
        if (openFile[i].state.game_code == game_code && openFile[i].state.company_code == company_code &&
            strcmp(openFile[i].state.game_name, game_name) == 0 && strcmp(openFile[i].state.ext_name, ext_name) == 0) {
            *file_no = i;
            return PFS_NO_ERROR;
        }
    }
    //printf("\tdidn't find open file in cache\n");
    *file_no = fileIndex++;

    openFile[*file_no].file = fs_open(filename, O_RDONLY | O_META);
    strcpy(openFile[*file_no].filename, filename);
    if (-1 == openFile[*file_no].file) {
        //printf("\t\tcouldn't find it on vmu either\n");
        return PFS_ERR_INVALID;
    } else {
        fs_close(openFile[*file_no].file);
        openFile[*file_no].file = -1;
        openFile[*file_no].file = fs_open(filename, O_RDWR | O_META);
        if (-1 == openFile[*file_no].file) {
            //printf("\t\tcouldn't re-open read-write\n");
            return PFS_CORRUPTED;
        }
    }

    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, game_name);
    strcpy(openFile[*file_no].state.ext_name, ext_name);
    return PFS_NO_ERROR;
}