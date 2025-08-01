#include <ultra64.h>
#include <macros.h>
#include <mk64.h>
#include <stdarg.h>
#include <string.h>
#include "../crash_screen.h"
#include "crash_screen_enhancement.h"

s32 _Printf(char* (*prout)(char*, const char*, size_t), char* dst, const char* fmt, va_list args);

u32 crashScreenFont2[7 * 9 + 1] = {
    0x70871c30, 0x8988a250, 0x88808290, 0x88831c90, 0x888402f8, 0x88882210, 0x71cf9c10, 0xf9cf9c70, 0x8228a288,
    0xf200a288, 0x0bc11c78, 0x0a222208, 0x8a222288, 0x71c21c70, 0x23c738f8, 0x5228a480, 0x8a282280, 0x8bc822f0,
    0xfa282280, 0x8a28a480, 0x8bc738f8, 0xf9c89c08, 0x82288808, 0x82088808, 0xf2ef8808, 0x82288888, 0x82288888,
    0x81c89c70, 0x8a08a270, 0x920da288, 0xa20ab288, 0xc20aaa88, 0xa208a688, 0x9208a288, 0x8be8a270, 0xf1cf1cf8,
    0x8a28a220, 0x8a28a020, 0xf22f1c20, 0x82aa0220, 0x82492220, 0x81a89c20, 0x8a28a288, 0x8a28a288, 0x8a289488,
    0x8a2a8850, 0x894a9420, 0x894aa220, 0x70852220, 0xf8011000, 0x08020800, 0x10840400, 0x20040470, 0x40840400,
    0x80020800, 0xf8011000, 0x70800000, 0x88822200, 0x08820400, 0x108f8800, 0x20821000, 0x00022200, 0x20800020
};

extern u64 osClockRate;

u8 gCrashScreenCharToGlyph[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 41, -1, -1, -1, 43, -1, -1, 37, 38, -1, 42, -1, 39, 44, -1, 0,  1,  2,  3,
    4,  5,  6,  7,  8,  9,  36, -1, -1, -1, -1, 40, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
};

char* gCauseDesc[18] = {
    "Interrupt",
    "TLB modification",
    "TLB exception on load",
    "TLB exception on store",
    "Address error on load",
    "Address error on store",
    "Bus error on inst.",
    "Bus error on data",
    "System call exception",
    "Breakpoint exception",
    "Reserved instruction",
    "Coprocessor unusable",
    "Arithmetic overflow",
    "Trap exception",
    "Virtual coherency on inst.",
    "Floating point exception",
    "Watchpoint exception",
    "Virtual coherency on data",
};

char* gFpcsrDesc[6] = {
    "Unimplemented operation", "Invalid operation", "Division by zero", "Overflow", "Underflow", "Inexact operation",
};

void crash_screen_draw_glyph_enhancement(s32 x, s32 y, s32 glyph) {
    const u32* data;
    u16* ptr;
    u32 bit;
    u32 rowMask;
    s32 i, j;

    data = &crashScreenFont2[glyph / 5 * 7];
    ptr = pFramebuffer + SCREEN_WIDTH * y + x;

    for (i = 0; i < 7; i++) {
        bit = 0x80000000U >> ((glyph % 5) * 6);
        rowMask = *data++;

        for (j = 0; j < 6; j++) {
            *ptr++ = (bit & rowMask) ? 0xffff : 1;
            bit >>= 1;
        }
        ptr += SCREEN_WIDTH - 6;
    }
}

void crash_screen_draw_rect(s32 x, s32 y, s32 w, s32 h) {
    u16* ptr;
    s32 i, j;

    ptr = pFramebuffer + SCREEN_WIDTH * y + x;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            // 0xe738 = 0b1110011100111000
            *ptr = ((*ptr & 0xe738) >> 2) | 1;
            ptr++;
        }
        ptr += SCREEN_WIDTH - w;
    }
}

static char* write_to_buf(char* buffer, const char* data, size_t size) {
    return (char*) memcpy(buffer, data, size) + size;
}

void crash_screen_print(s32 x, s32 y, const char* fmt, ...) {
    char* ptr;
    u32 glyph;
    s32 size;
    char buf[0x100];

    va_list args;
    va_start(args, fmt);

    size = _Printf(write_to_buf, buf, fmt, args);

    if (size > 0) {
        ptr = buf;

        while (size > 0) {

            glyph = gCrashScreenCharToGlyph[*ptr & 0x7f];

            if (glyph != 0xff) {
                crash_screen_draw_glyph_enhancement(x, y, glyph);
            }

            size--;

            ptr++;
            x += 6;
        }
    }

    va_end(args);
}

void crash_screen_sleep(UNUSED s32 ms) {
    ;
}

void crash_screen_print_float_reg(UNUSED s32 x, UNUSED s32 y, UNUSED s32 regNum,
                                  UNUSED void* addr) {
    ;
}

void crash_screen_print_fpcsr(UNUSED u32 fpcsr) {
    ;
}

void crash_screen_draw(UNUSED OSThread* thread) {
    ;
}
