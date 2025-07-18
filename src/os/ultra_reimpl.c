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

s32 osEepromProbe(OSMesgQueue* mq) {
	maple_device_t *vmudev = NULL;
	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev) {
		dbgio_printf("eeprom probe: could not enum\n");
		return 0;
	}
    file_t d = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDONLY | O_META);
	if (!d) {
//		if (Pak_Memory < 5){
//			printf("no %s file and not enough space\n", name);
//			return 0;
//		}
		d = fs_open(get_vmu_fn(vmudev, "mk64.rec"), O_RDWR | O_CREAT | O_META);
		if (!d) {
			printf("cant open %s for rdwr|creat\n", "mk64.rec");
			return 0;
		}

        vmu_pkg_t pkg;
        memset(eeprom_block, 0, 512);
        memset(&pkg, 0, sizeof(vmu_pkg_t));
        strcpy(pkg.desc_short,"Records");
        strcpy(pkg.desc_long, "Saved Settings");
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
		    printf("vmu_pkg_build failed\n");
		    fs_close(d);
		    return 0;
	    }
        fs_write(d, pkg_out, pkg_size);
		fs_close(d);
        free(pkg_out);
    } else {
		fs_close(d);
	}

    return EEPROM_TYPE_4K;
}
uint8_t* vmu_load_data(int channel, const char* name, uint8_t* outbuf, uint32_t* bytes_read);

s32 osEepromLongRead(OSMesgQueue* mq, unsigned char address, unsigned char* buffer, s32 length) {
    u8 content[512];
    uint32_t bsize;
    uint8_t* rv = vmu_load_data(0, "mk64.rec", content, &bsize);
    if (!rv || bsize != 512) {
        return -1;
    }

    memcpy(buffer, content + address * 8, length);
    return 0;
}

s32 osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

#define _EEPROM 0
#define _GHOST 1
uint32_t vmu_store_data(int channel, const char* name, int itype, void* bytes, int32_t len);

s32 osEepromLongWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer, s32 length) {
    u8 content[512] = { 0 };
    if (address != 0 || length != 512) {
        osEepromLongRead(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, length);

    return !vmu_store_data(0, "mk64.rec", _EEPROM, content, 512);
}

s32 osEepromWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}
static vmu_pkg_t ghostpkg;
s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    maple_device_t* vmudev = NULL;
    printf("%s\n",__func__);

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_ERR_NOPACK;

    int rv = fs_unlink(get_vmu_fn(vmudev, "mk64.gho"));
    if (rv)
        return PFS_ERR_ID_FATAL;

    return PFS_NO_ERROR;
}

static uint8_t __attribute__((aligned(32))) tempblock[32768];//64 * 512];
static uint8_t __attribute__((aligned(32))) temppkg[65536];
static vmu_pkg_t ghostpkg;
s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer) {
    printf("%s(flag %d)\n",__func__, flag);

    printf("openFile[%d].file == %d .filename == %s\n", file_no, openFile[file_no].file, openFile[file_no].filename);

    if (flag == PFS_READ) {
        ssize_t size = fs_total(openFile[file_no].file);
        printf("file size is %d\n", size);
        //        fseek(openFile[file_no].file, offset, SEEK_SET);
        //        fread(data_buffer, size_in_bytes, 1, openFile[file_no].file);
        // file is open, so seek to beginning of it
        fs_seek(openFile[file_no].file, 0, SEEK_SET);
        // get the size of the file
        if (size == 512) {
            vmu_pkg_t newpkg;
            printf("seting up new package\n");
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
            printf("trying to load %s icon\n", texfn);
            vmu_pkg_load_icon(&newpkg, texfn);
            printf("did it\n");
            ssize_t* pkg_size;
            u8* pkg_out;
            vmu_pkg_build(&newpkg, &pkg_out, &pkg_size);
            printf("built package\n");
            fs_write(openFile[file_no].file, pkg_out, pkg_size);
            fs_close(openFile[file_no].file);
            free(pkg_out);
            openFile[file_no].file = fs_open(openFile[file_no].filename, O_RDWR | O_META); // fopen(filename, "w+");
            size = fs_total(openFile[file_no].file);
        }
        // read "size" bytes into buffer
        ssize_t res = fs_read(openFile[file_no].file, temppkg, size);
        if (res != size) {
            printf("couldnt read it right\n");
            return PFS_ERR_BAD_DATA;
        }
        if (vmu_pkg_parse(temppkg, size, &ghostpkg) < 0) {
            printf("couldnt parse it right\n");
            return PFS_ERR_ID_FATAL;
        }
        // copy the data from the file into the data_buffer
        memcpy(data_buffer, ghostpkg.data + offset, size_in_bytes);
        // dont need to close/reopen, didnt write
    } else {
        //        fseek(openFile[file_no].file, offset, SEEK_SET);
        //        fwrite(data_buffer, size_in_bytes, 1, openFile[file_no].file);
        vmu_pkg_t newpkg;

        // file is open, so seek to beginning of it
        fs_seek(openFile[file_no].file, 0, SEEK_SET);
        // get the size of the file
        ssize_t size = fs_total(openFile[file_no].file);
        // read "size" bytes into staging buffer from vmu file
        ssize_t res = fs_read(openFile[file_no].file, temppkg, size);
        if (res != size) {
            printf("couldnt read it right\n");
            return PFS_ERR_BAD_DATA;
        }
        if (vmu_pkg_parse(temppkg, size, &ghostpkg) < 0) {
            printf("couldnt parse it right\n");
            return PFS_ERR_ID_FATAL;
        }

        // take original file contents and put into tempblock
        memcpy(tempblock, ghostpkg.data, 32768);
        printf("copied file contents to temp block\n");

        // now take data to write and copy it to offset in tempblock
        memcpy(tempblock + offset, data_buffer, size_in_bytes);
        printf("updated tempblock with data_buffer\n");

        // update the new package to point to the tempblock and write back to vmu
        memset(&newpkg, 0, sizeof(vmu_pkg_t));
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

        fs_seek(openFile[file_no].file, 0, SEEK_SET);
        fs_write(openFile[file_no].file, pkg_out, pkg_size);
        fs_close(openFile[file_no].file);
            free(pkg_out);
        openFile[file_no].file = fs_open(openFile[file_no].filename, O_RDWR | O_META); // fopen(filename, "w+");
    }
    return PFS_NO_ERROR;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, int file_size_in_bytes,
                      s32* file_no) {
    //    char filename[1024];
    //    sprintf(filename, "/pc/channel_%d_%hu_%hd_%s.sav", pfs->channel, company_code, game_code, game_name);
    maple_device_t* vmudev = NULL;
    printf("%s(%s) %d\n", __func__, game_name, file_size_in_bytes);

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_NO_PAK_INSERTED;

    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    *file_no = fileIndex++;
    openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META); // fopen(filename, "w+");
    strcpy(openFile[*file_no].filename, filename);
    // fwrite("\0", 1, file_size_in_bytes, openFile[*file_no].file);
    if (!openFile[*file_no].file) {
        printf("couldnt open file in allocate\n");
        return PFS_INVALID_DATA;
    }
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
    printf("built package\n");
    fs_write(openFile[*file_no].file, pkg_out, pkg_size);
    fs_close(openFile[*file_no].file);
            free(pkg_out);
    openFile[*file_no].file = fs_open(openFile[*file_no].filename, O_RDWR | O_META); // fopen(filename, "w+");
    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, game_name);
    strcpy(openFile[*file_no].state.ext_name, ext_name);
    return PFS_NO_ERROR;
}

s32 osPfsIsPlug(OSMesgQueue* queue, u8* pattern) {
    printf("%s\n",__func__);
    *pattern = 1;
    return 1;
}

s32 osPfsInit(OSMesgQueue* queue, OSPfs* pfs, int channel) {
    printf("%s\n",__func__);
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
    printf("%s\n",__func__);
    *max_files = 16;
    *files_used = fileIndex;
    return 0;
}

s32 osPfsFileState(OSPfs* pfs, s32 file_no, OSPfsState* state) {
    printf("%s\n",__func__);
    return PFS_NO_ERROR;
}

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used) {
    printf("%s\n",__func__);
    *bytes_not_used = 0x10000;
    return PFS_NO_ERROR;
}

s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no) {
    //    char filename[1024];
    //    sprintf(filename, "/pc/channel_%d_%hu_%hd_%s.sav", pfs->channel, company_code, game_code, game_name);

    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        printf("couldn't get vmu for pfs->channel %d\n", pfs->channel);
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
    printf("didn't find open file\n");
    *file_no = fileIndex++;

    openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META);
    strcpy(openFile[*file_no].filename, filename);
    if (!openFile[*file_no].file) {
/*         printf("\tcouldn't open it for read\n");
        openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META);
        // fopen(filename, "w+");
        if (!openFile[*file_no].file) {
         */    printf("\t\tcouldn't open it for read write create either\n");
            return PFS_INVALID_DATA;
        //}
    } else {
        printf("Created the file with fd %d filename %s\n", openFile[*file_no].file, openFile[*file_no].filename);
    }
    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, game_name);
    strcpy(openFile[*file_no].state.ext_name, ext_name);
    return PFS_NO_ERROR;
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