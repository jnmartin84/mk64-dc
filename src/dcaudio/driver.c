/*
 * File: driver.c
 * Project: mk64-dc
 * Author: Hayden Kowalchuk (hayden@hkowsoftware.com)
 * Modifications by jnmartin84
 *
 * AICA hardware mixing: the KOS sound stream is no longer used. The AICA voice
 * driver (src/audio/aica_synth.c) plays voices directly on the AICA hardware
 * channels. This file only initializes the base sound system (snd_init: uploads
 * the AICA ARM program, sets up the snd_mem/ARAM allocator and the SH4->AICA
 * command queue) which the voice driver relies on.
 */

#include <kos.h>
#include <dc/sound/stream.h>
#include <dc/sound/sound.h>
#include <stdio.h>
#include <stdbool.h>

#include "audio_dc.h"
#include "macros.h"

static bool audio_dc_init(void) {
    /* snd_stream_init() == snd_init() + stream-subsystem init. We do NOT allocate
       or start a stream (no AICA channels used by it), but this matches the OoT
       config that works -- it brings up snd_mem/the AICA output path fully. */
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }
    printf("Sound init complete (AICA hardware mixing, no stream started)!\n");
    return true;
}

static int audio_dc_buffered(void) {
    return 1088;
}

static int audio_dc_get_desired_buffered(void) {
    return 1100;
}

/* No software-mixed output is produced anymore (AICA mixes in hardware), so
   there is nothing to push to a stream. */
static void audio_dc_play(UNUSED const uint8_t *buf, UNUSED size_t len) {
}

/* Retained as no-ops: still referenced by src/racing/memory.c. */
void mute_stream(void) {
}

void unmute_stream(void) {
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
