#include <kos.h>
#include <assert.h>

#include "nintendo_192.h"
#include "logo.xbm"
/* includes */
#include "mario0000.xbm"
#include "mario0001.xbm"
#include "mario0002.xbm"
#include "mario0003.xbm"
#include "mario0004.xbm"
#include "mario0005.xbm"
#include "mario0006.xbm"
#include "mario0007.xbm"
#include "mario0008.xbm"
#include "mario0009.xbm"
#include "mario0010.xbm"
#include "mario0011.xbm"
#include "mario0012.xbm"
#include "mario0013.xbm"
#include "mario0014.xbm"
#include "mario0015.xbm"
#include "mario0016.xbm"
#include "mario0017.xbm"
#include "mario0018.xbm"
#include "mario0019.xbm"
#include "mario0020.xbm"
#include "mario0021.xbm"
#include "mario0022.xbm"
#include "mario0023.xbm"
#include "mario0024.xbm"
#include "mario0025.xbm"
#include "mario0026.xbm"
#include "mario0027.xbm"
#include "mario0028.xbm"
#include "mario0029.xbm"
#include "mario0030.xbm"
#include "mario0031.xbm"
#include "mario0032.xbm"
#include "mario0033.xbm"
#include "mario0034.xbm"
#include "mario0035.xbm"
#include "mario0036.xbm"
#include "mario0037.xbm"
#include "mario0038.xbm"
#include "mario0039.xbm"
#include "mario0040.xbm"
#include "mario0041.xbm"
#include "mario0042.xbm"
#include "mario0043.xbm"
#include "mario0044.xbm"
#include "mario0045.xbm"
#include "mario0046.xbm"
#include "mario0047.xbm"
#include "mario0048.xbm"
#include "mario0049.xbm"
#include "mario0050.xbm"

/* array of pointers */
char *mario_xbms[] = {
    mario0000_bits,
    mario0001_bits,
    mario0002_bits,
    mario0003_bits,
    mario0004_bits,
    mario0005_bits,
    mario0006_bits,
    mario0007_bits,
    mario0008_bits,
    mario0009_bits,
    mario0010_bits,
    mario0011_bits,
    mario0012_bits,
    mario0013_bits,
    mario0014_bits,
    mario0015_bits,
    mario0016_bits,
    mario0017_bits,
    mario0018_bits,
    mario0019_bits,
    mario0020_bits,
    mario0021_bits,
    mario0022_bits,
    mario0023_bits,
    mario0024_bits,
    mario0025_bits,
    mario0026_bits,
    mario0027_bits,
    mario0028_bits,
    mario0029_bits,
    mario0030_bits,
    mario0031_bits,
    mario0032_bits,
    mario0033_bits,
    mario0034_bits,
    mario0035_bits,
    mario0036_bits,
    mario0037_bits,
    mario0038_bits,
    mario0039_bits,
    mario0040_bits,
    mario0041_bits,
    mario0042_bits,
    mario0043_bits,
    mario0044_bits,
    mario0045_bits,
    mario0046_bits,
    mario0047_bits,
    mario0048_bits,
    mario0049_bits,
    mario0050_bits
};

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

void fixup_vmu_icons(void) {
	for(int i=0;i<51;i++) {
		fix_xbm(mario_xbms[i]);
	}
    fix_xbm(logo_bits);
}
static int ever_fixup = 0;
void draw_vmu_icon(int index) {
	maple_device_t *vmudev = NULL;
	if (!ever_fixup) {
		fixup_vmu_icons();
		ever_fixup++;
	}
    for (int i=0;i<4;i++) {
        vmudev = maple_enum_type(i, MAPLE_FUNC_LCD);
        if (!vmudev)
            continue;

        // draw on the first vmu screen found
        vmu_draw_lcd(vmudev, mario_xbms[index]);
    }
}

char *fnpre;

uint8_t progbuf[9000000] = {0};
int main(int argc, char **argv) {
    //void *subelf;
	maple_device_t *vmudev = NULL;
    for (int i=0;i<4;i++) {
        vmudev = maple_enum_type(i, MAPLE_FUNC_LCD);
        if (!vmudev)
            continue;

        // draw on the first vmu screen found
        vmu_draw_lcd(vmudev, progbuf);
    }

    dbgio_enable();
    dbglog_set_level(0);
    dbgio_dev_select("fb");
    dbgio_printf("\n\n\n\n\n\n\n                 Loading...\n");
    thd_sleep(375);
    FILE* fntest = fopen("/pc/dc_data/common_data.bin", "rb");
    if (NULL == fntest) {
        fntest = fopen("/cd/dc_data/common_data.bin", "rb");
        assert(fntest);
        fnpre = "/cd";
    } else {
        fnpre = "/pc";
    }
    dbgio_printf("                 Please wait...\n");
    dbgio_disable();
    thd_sleep(375);

    char pixs[3];

#define RGB565(r,g,b)  ( ((r & 0xF8) << 8) | \
                         ((g & 0xFC) << 3) | \
                         ((b & 0xF8) >> 3) )

    int y=0;
    int x=0;
    char binfn[256];
    sprintf(binfn, "%s/mk64.bin", fnpre);

    /* Map the sub-elf */
    //ssize_t se_size = fs_load(binfn, &subelf);
    file_t bin_file = fs_open(binfn, O_RDONLY);
    if (-1 == bin_file) {
        assert(0 == 1);
    }

    size_t full_bin_size = fs_seek(bin_file, 0, SEEK_END);
    size_t bin_rem_size = full_bin_size;
    size_t bin_chunk_size = full_bin_size / height;

    size_t bin_read = 0;
    fs_seek(bin_file, 0, SEEK_SET);
    while (bin_rem_size > bin_chunk_size) {
        ssize_t rv = fs_read(bin_file, (void *)progbuf + bin_read, (196851));
        if (rv == -1)
            assert(0 == 1);
        bin_read += rv;
        bin_rem_size -= rv;
        for (x=0;x<width;x++) {
            HEADER_PIXEL(header_data, pixs);

            uint16_t packed = RGB565(pixs[0],pixs[1],pixs[2]);
            vram_s[((200+y)*640) + (x + 180+24+24-16)] = packed;
        }
	    y++;
        draw_vmu_icon(y % 51);
        thd_sleep(20);
    }
    fs_read(bin_file, (void *)progbuf + bin_read, bin_rem_size);

    for (int i=0;i<16;i++) {
        for (x=0;x<width;x++) {
            vram_s[((200+y)*640) + (x + 180+24+24-16)] = 0;
        }
	    y++;
        draw_vmu_icon(y % 51);
        thd_sleep(75);
    }

    for (int i=0;i<4;i++) {
        vmudev = maple_enum_type(i, MAPLE_FUNC_LCD);
        if (!vmudev)
            continue;

        // draw on the first vmu screen found
        vmu_draw_lcd(vmudev, logo_bits);
    }


    assert(progbuf);
//    dbgio_printf("             ...\n");
    dbgio_disable();
    /* Tell exec to replace us */
    arch_exec(progbuf, full_bin_size);

    /* Shouldn't get here */
    assert_msg(false, "exec call failed");

    return 0;
}
