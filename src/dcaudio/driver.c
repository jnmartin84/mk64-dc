#if 0
#include "macros.h"
#include "audio_api.h"
#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>
void n64_memcpy(void *dst, const void *src, size_t size);
void n64_memset(void *dst, uint8_t val, size_t size);

#define DC_AUDIO_CHANNELS 2
#define DC_STEREO_AUDIO (1)
#define DC_AUDIO_FREQUENCY 26800
#define DC_AUDIO_GIVEN_BUFFERS (2)
#define SAMPLES_HIGH (448)
//#define DC_AUDIO_SAMPLES_DESIRED (SAMPLES_HIGH * DC_AUDIO_GIVEN_BUFFERS * 2 /* to help pad space */)
#define DC_AUDIO_SAMPLES_DESIRED (0x12000 / 4)

/* Double Buffer */
snd_stream_hnd_t shnd = -1;
//extern int gProcessAudio;

void mute_stream(void) {
    snd_stream_volume(shnd, 0); // Set maximum volume
}

void unmute_stream(void) {
    snd_stream_volume(shnd, 124); // Set maximum volume
}



/* 0x12000 bytes */
uint16_t snd_buffer_internal[DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS] __attribute__((aligned(64)));
static int audio_frames_generated_total;
static int audio_frames_generated_cur;

extern void create_next_audio_buffer(uint16_t *samples, uint32_t num_samples);
void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested, int *samples_returned) {
    const int backup_req = samples_requested;
    /* Catch-22, can be used to play more samples, but then takes longer, leading to needing more samples */
#if 0
    void *snd_buf = snd_buffer_internal;
    do {
        //printf("left to generate %d/%d bytes\n", samples_requested, backup_req);
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
        snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
        samples_requested -= (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
    } while (samples_requested >= (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short)));
    create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
    snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));

    //int ret = backup_req-samples_requested;
#endif
    int ret = backup_req;
    *samples_returned = ret;
#ifdef DEBUG
    void *buf = snd_buffer_internal;
    printf("%s:%d asked for %d and gave %d @ %x with %d left \n", __func__, __LINE__, backup_req, ret, (unsigned int) buf, samples_requested);
    fflush(stdout);
#endif
    audio_frames_generated_cur = 0;
    return snd_buffer_internal;
}

static bool audio_dc_init(void) {
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        fflush(stdout);
        return false;
    }

    shnd = snd_stream_alloc(audio_callback, SND_STREAM_BUFFER_MAX / 2 /* 0xFFFF */);
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: creation failure!");
        fflush(stdout);
        return false;
    }

    n64_memset(snd_buffer_internal, 0, DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS * 2);
    audio_frames_generated_total = 0;
    audio_frames_generated_cur = 0;
    return true;
}

static int audio_dc_buffered(void) {
    return 0;
}

static int audio_dc_get_desired_buffered(void) {
    /* This is more than 1088 */
    return 1100;
}
static int stream_started = 0;
/* We do our own cycle and processing of audio */
static void audio_dc_play(UNUSED const uint8_t *buf, UNUSED size_t len) {
    void *snd_buf = snd_buffer_internal + audio_frames_generated_cur * (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));

//    if (gProcessAudio) {
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
        snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
//    } else {
//        sq_clr(snd_buffer_internal, sizeof(snd_buffer_internal));
//    }

    audio_frames_generated_total += 2;
    audio_frames_generated_cur += 2;

    if (audio_frames_generated_total > 15) {
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
        stream_started = 1;
    }

    if (stream_started) {
    int ret = snd_stream_poll(shnd);
    if (ret) {
        printf("SND: %d\n", ret);
        fflush(stdout);
    }
    }
    thd_pass();
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
#endif

#if 1
/*
 * File: dc_audio_kos.c
 * Project: sm64-port
 * Author: Hayden Kowalchuk (hayden@hkowsoftware.com)
 * -----
 * Copyright (c) 2025 Hayden Kowalchuk
 */

/*
 * Modifications by jnmartin84
 */

#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "audio_dc.h"
#include "macros.h"

// --- Configuration ---
// Stereo
#define DC_AUDIO_CHANNELS (2) 
#define DC_STEREO_AUDIO ( DC_AUDIO_CHANNELS == 2)
// Sample rate for the AICA (32kHz)
#define DC_AUDIO_FREQUENCY (26800) 
#define RING_BUFFER_MAX_BYTES (SND_STREAM_BUFFER_MAX * 2)

// --- Global State for Dreamcast Audio Backend ---
// Handle for the sound stream
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; 
// The main audio buffer
static uint8_t cb_buf_internal[RING_BUFFER_MAX_BYTES] __attribute__((aligned(32))); 
static void *const cb_buf = cb_buf_internal;
static bool audio_started = false;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

typedef struct {
    uint8_t *buf;
    uint32_t cap; // power-of-two
    uint32_t head; // next write pos
    uint32_t tail; // next read pos
} ring_t;

static ring_t cb_ring;
static ring_t *r = &cb_ring;

static bool cb_init(size_t capacity) {
    // round capacity up to power of two
    r->cap = 1u << (32 - __builtin_clz(capacity - 1));
    r->buf = cb_buf;
    if (!r->buf)
        return false;
    r->head = 0;
    r->tail = 0;
    return true;
}
void n64_memcpy(void *dst, const void *src, size_t size);

static size_t cb_write_data(const void *src, size_t n) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    uint32_t free = r->cap - (head - tail);
    if (n > free)
        return 0;
    uint32_t idx = head & (r->cap - 1);
    uint32_t first = MIN(n, r->cap - idx);
    n64_memcpy(r->buf + idx, src, first);
    n64_memcpy(r->buf, (uint8_t*)src + first, n - first);
    r->head = head + n;
    return n;
}

static size_t cb_read_data(void *dst, size_t n) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;atomic_load(&r->tail);
    uint32_t avail = head - tail;
    if (n > avail) return 0;
    uint32_t idx = tail & (r->cap - 1);
    uint32_t first = MIN(n, r->cap - idx);
    n64_memcpy(dst, r->buf + idx, first);
    n64_memcpy((uint8_t*)dst + first, r->buf, n - first);
    r->tail = tail + n;
    return n;
}

// Calculates and returns the number of bytes currently in the ring buffer
static size_t cb_get_used(void) {
    // Atomically load both head and tail to get a consistent snapshot.
    // The order of loads might matter in some weak memory models,
    // but for head-tail diff, generally not.
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    
    // The number of used bytes is simply the difference between head and tail.
    // This works because head and tail are continuously incrementing indices,
    // and effectively handle wrap-around due to the (head - tail) arithmetic.
    return head - tail;
}

// You might also want a function to get free space:
static size_t cb_get_free(void) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    return r->cap - (head - tail);
}

// And optionally, if you need to check if it's full or empty
static bool cb_is_empty(void) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    return head == tail;
}

static bool cb_is_full(void) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    // A power-of-two ring buffer designed this way typically means
    // full when (head - tail) == cap
    return (head - tail) == r->cap;
}

static void cb_clear(void) {
    ;
}

// --- KOS Stream Audio Callback (Consumer): Called by KOS when the AICA needs more data ---
#define NUM_BUFFER_BLOCKS (2)
#define TEMP_BUF_SIZE (SND_STREAM_BUFFER_MAX * NUM_BUFFER_BLOCKS)
static uint8_t __attribute__((aligned(32))) temp_buf[TEMP_BUF_SIZE];
static unsigned int temp_buf_sel = 0;
void n64_memset(void *dst, uint8_t val, size_t size);
void mute_stream(void) {
    snd_stream_volume(shnd, 0); // Set maximum volume
}

void unmute_stream(void) {
    snd_stream_volume(shnd, 124); // Set maximum volume
}

void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested_bytes, int *samples_returned_bytes) {
    size_t samples_requested = samples_requested_bytes / 4;
    size_t samples_avail_bytes = cb_read_data(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel) , samples_requested_bytes);
    
    *samples_returned_bytes = samples_requested_bytes;
    size_t samples_returned = samples_avail_bytes / 4;
    
    /*@Note: This is more correct, fill with empty audio */
    if (samples_avail_bytes < (unsigned)samples_requested_bytes) {
        n64_memset(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel) + samples_avail_bytes, 0, (samples_requested_bytes - samples_avail_bytes));
        // printf("U\n");
    }
    
    temp_buf_sel += 1;
    if (temp_buf_sel >= NUM_BUFFER_BLOCKS) {
        temp_buf_sel = 0;
    }
    
    return (void*)(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel));
}

static bool audio_dc_init(void) {
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }

    thd_set_hz(120);
    
    // --- Initial Pre-fill of Ring Buffer with Silence ---
    sq_clr(cb_buf_internal, sizeof(cb_buf_internal));
    sq_clr(temp_buf, sizeof(temp_buf));
    if (!cb_init(RING_BUFFER_MAX_BYTES)) {
        printf("CB INIT FAILURE!\n");
        return false;
    }
    
    printf("Dreamcast Audio: Initialized. Ring buffer size: %u bytes.\n",
           (unsigned int)RING_BUFFER_MAX_BYTES);
    
    // Allocate the sound stream with KOS
    shnd = snd_stream_alloc(audio_callback, SND_STREAM_BUFFER_MAX / 8);
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: Stream allocation failure!\n");
        snd_stream_destroy(shnd);
        return false;
    }

    // Set maximum volume
    snd_stream_volume(shnd, 160); 

    printf("Sound init complete!\n");
    
    return true;
}

static int audio_dc_buffered(void) {
    return 1088;
}

static int audio_dc_get_desired_buffered(void) {
    return 1100;
}

static void audio_dc_play(const uint8_t *buf, size_t len) {
    size_t ring_data_available = cb_get_used();
    size_t written = cb_write_data(buf, len);
            
    if (!audio_started && ring_data_available >= (SND_STREAM_BUFFER_MAX)) {
        audio_started = true;
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
    }
    
//    if (written == 0) {
        // printf("O\n");
//        cb_clear();
//    }

    if (audio_started) {
        snd_stream_poll(shnd);
    }
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
#endif