#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif
#include <string.h>
#include <PR/ultratypes.h>
#include <PR/os_message.h>
#include <PR/os_pi.h>
#include <PR/os_vi.h>
#include <PR/os_time.h>
#include <PR/libultra.h>

#include <kos.h>
#include <ultra64.h>
#include "save.h"

char* get_vmu_fn(maple_device_t* vmudev, char* fn);
void* segmented_to_virtual(void* addr);

u64 osClockRate = 62500000;
void n64_memcpy(void* dst, const void* src, size_t size);

#if 0
s32 osPiStartDma(UNUSED OSIoMesg* mb, UNUSED s32 priority, UNUSED s32 direction, uintptr_t devAddr, void* vAddr,
                 size_t nbytes, UNUSED OSMesgQueue* mq) {
    void* vdevAddr = segmented_to_virtual((void*) devAddr);
    n64_memcpy(vAddr, (const void*) vdevAddr, nbytes);
    return 0;
}
#endif

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}

s32 osSendMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}

s32 osRecvMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg* msg, UNUSED s32 flag) {
    return 0;
}

extern mutex_t mq_mutex;
extern OSMesgQueue* D_800EA3B4;
s32 AosSendMesg(OSMesgQueue* mq, OSMesg msg, UNUSED s32 flag) {
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }

    index = (mq->first + mq->validCount) % mq->msgCount;

    mq->msg[index] = msg;
    mq->validCount++;

    return 0;
}

s32 AosRecvMesg(OSMesgQueue* mq, OSMesg* msg, UNUSED s32 flag) {
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

void osSetTime(UNUSED OSTime time) {
    ;
}

OSTime osGetTime(void) {
    return 0;
}

void osWritebackDCacheAll(void) {
    ;
}

void osWritebackDCache(UNUSED void* a, UNUSED size_t b) {
    ;
}

void osInvalDCache(UNUSED void* a, UNUSED size_t b) {
    ;
}

void osInvalICache(UNUSED void* a, UNUSED size_t b) {
    ;
}

u32 osGetCount(void) {
    static u32 counter = 0;
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
static uint8_t icondata[512 * 2];

struct state_pak {
    OSPfsState state;
    file_t file;
    char filename[64];
};

struct state_pak openFile[16] = { 0 };

int fileIndex = 0;

static uint8_t eeprom_block[512];

#include <kos.h>

static file_t eeprom_file = -1;
static mutex_t eeprom_lock;
static int eeprom_init = 0;

// thanks @zcrc
#include <kos/oneshot_timer.h>
/* 2s timer, to delay closing the VMU file.
 * This is because the emulator might open/modify/close often, and we want the
 * VMU VFS driver to only write to the VMU once we're done modifying the file. */
static oneshot_timer_t* timer;

void eeprom_flush(UNUSED void* arg) {
    mutex_lock_scoped(&eeprom_lock);

    if (eeprom_file != -1) {
        fs_close(eeprom_file);
        eeprom_file = -1;
    }
}
#include "save_data.h"
extern SaveData gSaveData;
extern s32 D_800DC5AC;
s32 osEepromProbe(UNUSED OSMesgQueue* mq) {
    maple_device_t* vmudev = NULL;

    if (!eeprom_init) {
        mutex_init(&eeprom_lock, MUTEX_TYPE_NORMAL);
        eeprom_init = 1;
        timer = oneshot_timer_create(eeprom_flush, NULL, 2000);
    }

    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return 0;
    }
    vid_border_color(255, 0, 255);
    eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDONLY | O_META);
    if (-1 == eeprom_file) {
        eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_CREAT | O_META);
        if (-1 == eeprom_file) {
            vid_border_color(0, 0, 0);
            return 1;
        }
        vid_border_color(255, 255, 0);
        vmu_pkg_t pkg;
        memset(eeprom_block, 0, 512);
        memset(&pkg, 0, sizeof(vmu_pkg_t));
        strcpy(pkg.desc_short, "Records");
        strcpy(pkg.desc_long, "Mario Kart 64");
        strcpy(pkg.app_id, "Mario Kart 64");
        sprintf(texfn, "%s/kart.ico", fnpre);
        pkg.icon_cnt = 2;
        pkg.icon_data = icondata;
        pkg.icon_anim_speed = 5;
        pkg.data_len = 512;
        pkg.data = &gSaveData;
        vmu_pkg_load_icon(&pkg, texfn);
        uint8_t* pkg_out;
        ssize_t pkg_size;
        vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
        if (!pkg_out || pkg_size <= 0) {
            fs_close(eeprom_file);
            eeprom_file = -1;
            vid_border_color(0, 0, 0);
            return 0;
        }
        fs_write(eeprom_file, pkg_out, pkg_size);
        free(pkg_out);
        oneshot_timer_reset(timer);
        // see menus.c : 443 for real initialization
        func_800B46D0();
        D_800DC5AC = 0;
    } else {
        fs_close(eeprom_file);
        eeprom_file = -1;
        eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_META);
        if (-1 == eeprom_file) {
            vid_border_color(0, 0, 0);
            return EEPROM_TYPE_4K;
        }

        oneshot_timer_reset(timer);
    }

    vid_border_color(0, 0, 0);
    return EEPROM_TYPE_4K;
}
uint8_t* vmu_load_data(int channel, const char* name, uint8_t* outbuf, uint32_t* bytes_read);

static int reopen_vmu_eeprom(void) {
    maple_device_t* vmudev = NULL;
    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return 1;
    }

    eeprom_file = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_META);
    return (eeprom_file == -1);
}

s32 osEepromLongRead(UNUSED OSMesgQueue* mq, unsigned char address, unsigned char* buffer,
                     s32 length) {
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    vid_border_color(0, 255, 0);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);

        if (size != 2048) {
            fs_close(eeprom_file);
            vid_border_color(0, 0, 0);
            return 1;
        }

        // skip header
        fs_seek(eeprom_file, (512 * 3) + (address * 8), SEEK_SET);
        ssize_t rv = fs_read(eeprom_file, buffer, length);
        if (rv != length) {
            vid_border_color(0, 0, 0);
            return 1;
        }

        vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        return 0;
    } else {
        vid_border_color(0, 0, 0);
        return 1;
    }
}

s32 osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

s32 osEepromLongWrite(UNUSED OSMesgQueue* mq, unsigned char address, unsigned char* buffer,
                      s32 length) {
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    vid_border_color(0, 0, 255);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);
        if (size != 2048) {
            vid_border_color(0, 0, 0);
            fs_close(eeprom_file);
            return 1;
        }
        // skip header
        fs_seek(eeprom_file, (512 * 3) + (address * 8), SEEK_SET);
        ssize_t rv = fs_write(eeprom_file, buffer, length);
        if (rv != length) {
            vid_border_color(0, 0, 0);
            return 1;
        }

        vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        return 0;
    } else {
        vid_border_color(0, 0, 0);
        return 1;
    }
}

s32 osEepromWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}

s32 osPfsDeleteFile(OSPfs* pfs, UNUSED u16 company_code, UNUSED u32 game_code,
                    UNUSED u8* game_name, UNUSED u8* ext_name) {
    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_ERR_NOPACK;
    vid_border_color(255, 0, 0);

    int rv = fs_unlink(get_vmu_fn(vmudev, "mk64.gho"));

    vid_border_color(0, 0, 0);

    if (rv)
        return PFS_ERR_ID_FATAL;

    return PFS_NO_ERROR;
}

// I would stack allocate this but I really would like to guarantee that
// the file/header creation can't fail from a lack of available memory
static uint8_t __attribute__((aligned(32))) tempblock[32768];

s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes,
                       u8* data_buffer) {
    if (openFile[file_no].file == -1) {
        return PFS_ERR_INVALID;
    }

    if (flag == PFS_READ) {
        vid_border_color(0, 255, 0);
        fs_seek(openFile[file_no].file, (512 * 3) + offset, SEEK_SET);
        ssize_t res = fs_read(openFile[file_no].file, data_buffer, size_in_bytes);

        if (res != size_in_bytes) {
            vid_border_color(0, 0, 0);
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_ERR_BAD_DATA;
        }
        // dont need to close/reopen, didnt write
    } else {
        vid_border_color(0, 0, 255);
        fs_seek(openFile[file_no].file, (512 * 3) + offset, SEEK_SET);
        ssize_t rv = fs_write(openFile[file_no].file, data_buffer, size_in_bytes);
        if (rv != size_in_bytes) {
            vid_border_color(0, 0, 0);
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_CORRUPTED;
        }

        fs_close(openFile[file_no].file);
        openFile[file_no].file = fs_open(openFile[file_no].filename, O_RDWR | O_META);
        if (-1 == openFile[file_no].file) {
            vid_border_color(0, 0, 0);
            return PFS_CORRUPTED;
        }
    }
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
                      UNUSED int file_size_in_bytes, s32* file_no) {
    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return PFS_NO_PAK_INSERTED;
    }
    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    *file_no = fileIndex++;
    openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META);
    if (-1 == openFile[*file_no].file) {
        return PFS_INVALID_DATA;
    }

    strcpy(openFile[*file_no].filename, filename);
    vid_border_color(255, 255, 0);
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
    ssize_t pkg_size;
    u8* pkg_out;
    vmu_pkg_build(&newpkg, &pkg_out, &pkg_size);
    ssize_t rv = fs_write(openFile[*file_no].file, pkg_out, pkg_size);
    if (rv != pkg_size) {
        vid_border_color(0, 0, 0);
        fs_close(openFile[*file_no].file);
        free(pkg_out);
        return PFS_CORRUPTED;
    }
    fs_close(openFile[*file_no].file);
    free(pkg_out);
    openFile[*file_no].file = fs_open(openFile[*file_no].filename, O_RDWR | O_META);
    if (-1 == openFile[*file_no].file) {
        vid_border_color(0, 0, 0);
        return PFS_CORRUPTED;
    }
    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, (char*) game_name);
    strcpy(openFile[*file_no].state.ext_name, (char*) ext_name);
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
}

extern int vmu_status(int channel);

s32 osPfsIsPlug(UNUSED OSMesgQueue* queue, u8* pattern) {
    *pattern = 0;
    if (!vmu_status(0)) {
        *pattern = 1;
    }
    return 1;
}

s32 osPfsInit(UNUSED OSMesgQueue* queue, OSPfs* pfs, int channel) {
    int rv = vmu_status(channel);
    if (rv) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->queue = queue;
    if (channel != 0) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->channel = channel;
    pfs->status = 0;
    pfs->status |= PFS_INITIALIZED;
    return PFS_NO_ERROR;
}

s32 osPfsNumFiles(UNUSED OSPfs* pfs, s32* max_files, s32* files_used) {
    *max_files = 16;
    *files_used = fileIndex;
    return 0;
}

s32 osPfsFileState(UNUSED OSPfs* pfs, UNUSED s32 file_no, UNUSED OSPfsState* state) {
    return PFS_NO_ERROR;
}

extern int32_t Pak_Memory;

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used) {
    vmu_status(pfs->channel);

    *bytes_not_used = Pak_Memory * 512;
    return PFS_NO_ERROR;
}

void __osPfsCloseAllFiles(void) {
    for (size_t i = 0; i < 16; i++) {
        if (openFile[i].file != -1) {
            fs_close(openFile[i].file);
        }
    }
}

s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
                  s32* file_no) {
    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return PFS_NO_PAK_INSERTED;
    }

    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    vid_border_color(255, 0, 255);

    for (size_t i = 0; i < 16; i++) {
        if (openFile[i].state.game_code == game_code && 
            openFile[i].state.company_code == company_code &&
            strcmp(openFile[i].state.game_name, (char*) game_name) == 0 &&
            strcmp(openFile[i].state.ext_name, (char*) ext_name) == 0) {
            *file_no = i;
            vid_border_color(0, 0, 0);
            return PFS_NO_ERROR;
        }
    }

    *file_no = fileIndex++;

    openFile[*file_no].file = fs_open(filename, O_RDONLY | O_META);
    strcpy(openFile[*file_no].filename, filename);
    if (-1 == openFile[*file_no].file) {
        vid_border_color(0, 0, 0);
        return PFS_ERR_INVALID;
    } else {
        fs_close(openFile[*file_no].file);
        openFile[*file_no].file = -1;
        openFile[*file_no].file = fs_open(filename, O_RDWR | O_META);
        if (-1 == openFile[*file_no].file) {
            vid_border_color(0, 0, 0);
            return PFS_CORRUPTED;
        }
    }

    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, (char*) game_name);
    strcpy(openFile[*file_no].state.ext_name, (char*) ext_name);
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
}