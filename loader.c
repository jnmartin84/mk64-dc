#include <kos.h>
#include <assert.h>

#include "nintendo_192.h"

char *fnpre;

uint8_t progbuf[8400000];

int main(int argc, char **argv) {
    //void *subelf;

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
        ssize_t rv = fs_read(bin_file, (void *)progbuf + bin_read, (195165));
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
        thd_sleep(20);
    }
    fs_read(bin_file, (void *)progbuf + bin_read, bin_rem_size);

    for (int i=0;i<16;i++) {
        for (x=0;x<width;x++) {
            vram_s[((200+y)*640) + (x + 180+24+24-16)] = 0;
        }
	y++;
        thd_sleep(75);
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
