
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

// do all of the swapping needed to draw with vmu_draw_lcd
void fix_xbm(unsigned char *p)
{
    unsigned char tmp[6*32];
	for (int i = 31; i > -1; i--) {
		memcpy(&tmp[(31 - i) * 6], &p[i * 6], 6);
	}

	memcpy(p, tmp, 6 * 32);

	for (int j = 0; j < 32; j++) {
		for (int i = 0; i < 6; i++) {
			uint8_t tmpb = p[(j * 6) + (5 - i)];
			tmp[(j * 6) + i] = tmpb;
		}
	}

	memcpy(p, tmp, 6 * 32);
}

#include "mario.xbm"
#include "luigi.xbm"
#include "yoshi.xbm"
#include "dk.xbm"
#include "wario.xbm"
#include "peach.xbm"
#include "toad.xbm"
#include "bowser.xbm"
#if 1
//defined(SONIC_BUILD)
#include "sonic.xbm"
const char *xbms[] = { mario_xbm, luigi_xbm, yoshi_xbm,
                    sonic_xbm, dk_xbm, wario_xbm,
					peach_xbm, bowser_xbm };
#else
const char *xbms[] = { mario_xbm, luigi_xbm, yoshi_xbm,
                    todd_xbm, dk_xbm, wario_xbm,
					peach_xbm, bowser_xbm };
#endif
void fixup_vmu_icons(void) {
	for(int i=0;i<8;i++) {
		fix_xbm(xbms[i]);
	}
}
static int ever_fixup = 0;
void draw_vmu_icon(int controller, int charid) {
	file_t d;
	char tmpfn[256];
	char contid[4] = {'a','b','c','d'};
	maple_device_t *vmudev = NULL;
	if (!ever_fixup) {
		fixup_vmu_icons();
		ever_fixup++;
	}
	vmudev = maple_enum_type(controller, MAPLE_FUNC_LCD);
	if (!vmudev)
		return;

	sprintf(tmpfn, "/vmu/%c1/", contid[controller]);
	d = fs_open(tmpfn, O_RDONLY | O_DIR);
	if(-1 == d)
		return;

	fs_close(d);

	// draw on the first vmu screen found
	vmu_draw_lcd(vmudev, xbms[charid]);
}