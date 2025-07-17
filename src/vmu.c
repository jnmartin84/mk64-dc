
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

int vmu_status(void) {
	maple_device_t *vmudev = NULL;

	ControllerPakStatus = 0;
	FilesUsed = -1;

	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev)
		return PFS_ERR_NOPACK;

	file_t d;
	dirent_t *de;

	d = fs_open(get_vmu_fn(vmudev, NULL), O_RDONLY | O_DIR);
	if(!d)
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

 /*    maple_device_t *vmudev = NULL;
	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev) {
        ControllerPakStatus = 0;
		return 0;
	}
    ControllerPakStatus = 1;
    return 1;
 */
}

int vmu_file_exists(const char *name) {
	maple_device_t *vmudev = NULL;
	vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (!vmudev)
        return 0;
    file_t d = fs_open(get_vmu_fn(vmudev, name), O_RDONLY | O_META);
	if (!d) {
        return 0;
    } else {
        fs_close(d);
        return 1;
    }
}

uint8_t *vmu_load_data(int channel, const char *name, uint8_t *outbuf, uint32_t *bytes_read) {
	ssize_t size;
	maple_device_t *vmudev = NULL;
	uint8_t *data;

	ControllerPakStatus = 0;

	vmudev = maple_enum_type(channel, MAPLE_FUNC_MEMCARD);
	if (!vmudev) {
		printf("could not enum\n");
		*bytes_read = 0;
		return NULL;
	}

	file_t d = fs_open(get_vmu_fn(vmudev, name), O_RDONLY | O_META);
	if (!d) {
		printf("could not fs_open %s\n", get_vmu_fn(vmudev, name));
		*bytes_read = 0;
		return NULL;
	}

	size = fs_total(d);
	data = calloc(1, size);

	if (!data) {
		fs_close(d);
 		*bytes_read = 0;
		dbgio_printf("platform_load_userdata: could not calloc data\n");
		return NULL;
	}

	vmu_pkg_t pkg;
	memset(&pkg, 0, sizeof(pkg));
	ssize_t res = fs_read(d, data, size);

	if (res < 0) {
		fs_close(d);
 		*bytes_read = 0;
		printf("could not fs_read\n");
		return NULL;
	}
	ssize_t total = res;
	while (total < size) {
		res = fs_read(d, data + total, size - total);
		if (res < 0) {
			fs_close(d);
			*bytes_read = 0;
			printf("could not fs_read\n");
			return NULL;
		}
		total += res;
	}

	if (total != size) {
		fs_close(d);
 		*bytes_read = 0;
		printf("total != size\n");
		return NULL;
	}

	fs_close(d);

	if(vmu_pkg_parse(data, size, &pkg) < 0) {
		free(data);
 		*bytes_read = 0;
		printf("could not vmu_pkg_parse\n");
		return NULL;
	}

	uint8_t *bytes = outbuf;//mem_temp_alloc(pkg.data_len);
	if (!bytes) {
		free(data);
 		*bytes_read = 0;
		printf("could not mem_temp_alloc bytes\n");
		return NULL;
	}

	memcpy(bytes, pkg.data, pkg.data_len);
	ControllerPakStatus = 1;
	free(data);

	*bytes_read = pkg.data_len;

	return bytes;
}

uint32_t vmu_store_data(int channel, const char *name, int itype, void *bytes, int32_t len) {
	uint8 *pkg_out;
	ssize_t pkg_size;
	maple_device_t *vmudev = NULL;

	ControllerPakStatus = 0;

	vmudev = maple_enum_type(channel, MAPLE_FUNC_MEMCARD);
	if (!vmudev) {
		dbgio_printf("vmu_store_data: could not enum\n");
		return 0;
	}

	vmu_pkg_t pkg;
	memset(&pkg, 0, sizeof(vmu_pkg_t));
	strcpy(pkg.desc_short,"Mario Kart 64");
	strcpy(pkg.desc_long, "Mario Kart 64");
	strcpy(pkg.app_id, "Mario Kart 64");
/*    if (itype == 0) {
        sprintf(texfn, "%s/kart.ico", fnpre);
    } else { // GHOST
        sprintf(texfn, "%s/ghost.ico", fnpre);
    }
	pkg.icon_cnt = 2;
	pkg.icon_data = frames;
	pkg.icon_anim_speed = 4;
*/
    pkg.icon_cnt = 0;
    pkg.icon_data = NULL;
    pkg.icon_anim_speed = 0;
    pkg.data_len = len;
	pkg.data = bytes;
//    vmu_pkg_load_icon(&pkg, texfn);

	file_t d = fs_open(get_vmu_fn(vmudev, name), O_RDONLY | O_META);
	if (!d) {
		if (Pak_Memory < 60){
			printf("no %s file and not enough space\n", name);
			return 0;
		}
		d = fs_open(get_vmu_fn(vmudev, name), O_RDWR | O_CREAT | O_META);
		if (!d) {
			printf("cant open %s for rdwr|creat\n", name);
			return 0;
		}
	} else {
		fs_close(d);
		d = fs_open(get_vmu_fn(vmudev, name), O_WRONLY | O_META);
		if (!d) {
			printf("could not open file %s\n", name);
			return 0;
		}
	}

	vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
	if (!pkg_out || pkg_size <= 0) {
		printf("vmu_pkg_build failed\n");
		fs_close(d);
		return 0;
	}

	ssize_t rv = fs_write(d, pkg_out, pkg_size);
	ssize_t total = rv;
	while (total < pkg_size) {
		rv = fs_write(d, pkg_out + total, pkg_size - total);
		if (rv < 0) {
			printf("could not fs_write\n");
			fs_close(d);
			return -2;
		}
		total += rv;
	}

	fs_close(d);

	free(pkg_out);

	if (total == pkg_size) {
		ControllerPakStatus = 1;
		return len;
	} else {
	    return 0;
	}
}