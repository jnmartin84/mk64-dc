
#include <kos.h>
#include <ultra64.h>
#include "save.h"



int32_t ControllerPakStatus;
extern char *fnpre;
static char texfn[256];
static char frames[2*512];

int32_t FilesUsed = 0;
int32_t Pak_Memory = 0;
int32_t Pak_Size = 0;
uint8_t *Pak_Data;
dirent_t __attribute__((aligned(32))) FileState[200];
static char full_fn[20];

char *get_vmu_fn(maple_device_t *vmudev, char *fn) {
	if (fn)
		sprintf(full_fn, "/vmu/%c%d/%s", 'a'+vmudev->port, vmudev->unit, fn);
	else
		sprintf(full_fn, "/vmu/%c%d", 'a'+vmudev->port, vmudev->unit);

	return full_fn;
}

int vmu_status(int channel) {
	maple_device_t *vmudev = NULL;

	ControllerPakStatus = 0;
	FilesUsed = -1;

	vmudev = maple_enum_type(channel, MAPLE_FUNC_MEMCARD);
	if (!vmudev)
		return PFS_ERR_NOPACK;

	file_t d;
	dirent_t *de;

	d = fs_open(get_vmu_fn(vmudev, NULL), O_RDONLY | O_DIR);
	if(-1 == d)
		return PFS_ERR_ID_FATAL;

	Pak_Memory = 200;

	memset(FileState, 0, sizeof(dirent_t)*200);

	FilesUsed = 0;
	int FileCount = 0;
	while (NULL != (de = fs_readdir(d))) {
		if (strcmp(de->name, ".") == 0)
			continue;
		if (strcmp(de->name, "..") == 0)
			continue;

		memcpy(&FileState[FileCount++], de, sizeof(dirent_t));			
		Pak_Memory -= (de->size / 512);
		FilesUsed += 1;
	}

	fs_close(d);

	ControllerPakStatus = 1;

	return 0;
}

int vmu_file_exists(const char *name) {
	maple_device_t *vmudev = NULL;
	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev)
        return 0;
    file_t d = fs_open(get_vmu_fn(vmudev, name), O_RDONLY | O_META);
	if (-1 == d) {
        return 0;
    } else {
        fs_close(d);
        return 1;
    }
}