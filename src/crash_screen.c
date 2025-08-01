#include <ultra64.h>
#include <macros.h>
#include <mk64.h>
#include <stdarg.h>
#include <string.h>

#include "crash_screen.h"
#include "main.h"

#ifdef CRASH_SCREEN_ENHANCEMENT
#include "debug/crash_screen_enhancement.h"
#endif

OSThread D_80162790;
ALIGNED8 u8 gDebugThreadStack[0x400];
OSMesgQueue D_80162D40;
OSMesg D_80162D58;
u16* pFramebuffer;
s32 sButtonSequenceIndex;

#define DRAW_CODE 0xFFFF
#define CHARACTER_DASH 16

extern void osSetEventMesg(OSEvent, OSMesgQueue*, OSMesg);
extern s32 osRecvMesg(OSMesgQueue*, OSMesg*, s32);

s32 sCounter = 0;

u8 crashScreenFont[][8] = {
#include "textures/crash_screen/crash_screen_font.ia1.inc.c"
};

u16 sCrashScreenButtonSequence[] = { L_TRIG, U_JPAD, L_JPAD,   D_JPAD,   R_JPAD,
                                     R_TRIG, L_TRIG, B_BUTTON, A_BUTTON, DRAW_CODE };

void crash_screen_draw_glyph(UNUSED u16* framebuffer, UNUSED s32 x, UNUSED s32 y,
                             UNUSED s32 glyph) {
    ;
}

void crash_screen_draw_square(u16* framebuffer);

#define WHITE_COLOUR 0xFFFF
#define RED_COLOUR 0xF801

// (x,y) of top left pixel of square
#define SQUARE_X 40
#define SQUARE_Y 40

#define SQUARE_SIZE_X 6
#define SQUARE_SIZE_Y 6

// width of the square's border being drawn.
#define BORDER_WIDTH 1

#define SQUARE_X2 SQUARE_X + SQUARE_SIZE_Y
#define SQUARE_Y2 SQUARE_Y + SQUARE_SIZE_X

// Here's to you, Yoshimoto, for writing this stupid function. 3 years. 3 years to match.
void crash_screen_draw_square(UNUSED u16* framebuffer) {
    ;
}

/**
 * Draws three black boxes then prints crash info in the following format:
 * Line 1: threadId - address of faulted instruction - error code
 * Line 2: Address in the return address register
 * Line 3: Machine code of faulted instruction
 *
 * The R4300i manual discusses exceptions in more depth.
 *
 * @param framebuffer
 * @param faulted thread
 **/

//                     0xRGBA
#define BLACK_COLOUR 0x0001

void crash_screen_draw_info(UNUSED u16* framebuffer, UNUSED OSThread* thread) {
    ;
}

OSThread* get_faulted_thread(void) {
    return NULL;
}

/**
 * @brief Retrieves faulted thread and displays debug info after the user inputs the button sequence.
 * Button sequence: L, Up, Left, Down, Right, R, L, B, A
 **/
void thread9_crash_screen(UNUSED void* arg0) {
    ;
}

void crash_screen_set_framebuffer(UNUSED u16* framebuffer) {
    ;
}

extern void thread9_crash_screen(void*);

void create_debug_thread(void) {
    ;
}

void start_debug_thread(void) {
    ;
}
