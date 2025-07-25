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
#define RING_BUFFER_MAX_BYTES (SND_STREAM_BUFFER_MAX * 2 /* / 2 */)

// --- Global State for Dreamcast Audio Backend ---
// Handle for the sound stream
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; 
// The main audio buffer
static uint8_t cb_buf_internal[RING_BUFFER_MAX_BYTES] __attribute__((aligned(32))); 
static void *const cb_buf = cb_buf_internal;
static bool audio_started = false;

typedef enum {
    AUDIO_STATUS_RUNNING,
    AUDIO_STATUS_DONE
} audio_thread_status_t;

volatile audio_thread_status_t g_audio_thread_status = AUDIO_STATUS_DONE;
static kthread_t *g_audio_poll_thread_handle = NULL;
//static kthread_t *g_audio_gen_thread_handle = NULL;

// The dedicated audio polling thread: This continuously calls snd_stream_poll()
// to allow KOS to invoke the audio_callback when data is needed.
void* snd_thread(UNUSED void* arg) {

#if defined(OUTPUT)
    printf("AUDIO_THREAD: Started polling thread.\n"); // Debug print, uncomment if needed
#endif
    while(g_audio_thread_status == AUDIO_STATUS_RUNNING) {
        if (shnd != SND_STREAM_INVALID && audio_started) {
            snd_stream_poll(shnd);
        }
        thd_pass(); // Sleep briefly to avoid busy-waiting, yielding CPU
    }

#if defined(OUTPUT)
    printf("AUDIO_THREAD: Polling thread shutting down.\n"); // Debug print, uncomment if needed
#endif

    return NULL;
}

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
#define TEMP_BUF_SIZE ((SND_STREAM_BUFFER_MAX/*  / 2 */) * NUM_BUFFER_BLOCKS)
static uint8_t __attribute__((aligned(32))) temp_buf[TEMP_BUF_SIZE];
static unsigned int temp_buf_sel = 0;

void mute_stream(void) {
    snd_stream_volume(shnd, 0); // Set maximum volume
}

void unmute_stream(void) {
    snd_stream_volume(shnd, 160); // Set maximum volume
}

void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested_bytes, int *samples_returned_bytes) {
    size_t samples_requested = samples_requested_bytes / 4;
    size_t samples_avail_bytes = cb_read_data(temp_buf + ((SND_STREAM_BUFFER_MAX) * temp_buf_sel) , samples_requested_bytes);
    
    *samples_returned_bytes = samples_requested_bytes;
    size_t samples_returned = samples_avail_bytes / 4;
    
    /*@Note: This is more correct, fill with empty audio */
    if (samples_avail_bytes < (unsigned)samples_requested_bytes) {
        memset(temp_buf + ((SND_STREAM_BUFFER_MAX) * temp_buf_sel) + samples_avail_bytes, 0, (samples_requested_bytes - samples_avail_bytes));
    }
    
    temp_buf_sel += 1;
    if (temp_buf_sel >= NUM_BUFFER_BLOCKS) {
        temp_buf_sel = 0;
    }
    
    return (void*)(temp_buf + ((SND_STREAM_BUFFER_MAX) * temp_buf_sel));
}
mutex_t reset_mutex;

static bool audio_dc_init(void) {
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }

    thd_set_hz(300);
    
    mutex_init(&reset_mutex, MUTEX_TYPE_NORMAL);

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
    shnd = snd_stream_alloc(audio_callback, (SND_STREAM_BUFFER_MAX / 8));
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: Stream allocation failure!\n");
        snd_stream_destroy(shnd);
        return false;
    }

#if 0
    // Start the dedicated audio polling thread
    g_audio_thread_status = AUDIO_STATUS_RUNNING;
    g_audio_poll_thread_handle = thd_create(false, snd_thread, NULL);
    if (!g_audio_poll_thread_handle) {
        printf("ERROR: Failed to create audio polling thread!\n");
        fflush(stdout);
        return false;
    }
#endif
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


void runtime_reset(void) {
    mutex_lock(&reset_mutex);

    snd_stream_volume(shnd, 0);
    audio_started = false;
    // --- Initial Pre-fill of Ring Buffer with Silence ---
    sq_clr(cb_buf_internal, sizeof(cb_buf_internal));
    sq_clr(temp_buf, sizeof(temp_buf));
    if (!cb_init(RING_BUFFER_MAX_BYTES)) {
        printf("CB INIT FAILURE!\n");
    }
    snd_stream_volume(shnd, 160);

    mutex_unlock(&reset_mutex);
}

static void audio_dc_play(const uint8_t *buf, size_t len) {
    mutex_lock(&reset_mutex);
 
    size_t ring_data_available = cb_get_used();
    size_t written = cb_write_data(buf, len);

    if ((!audio_started) && (ring_data_available > (SND_STREAM_BUFFER_MAX / 3))) {
        audio_started = true;
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
    }

    if (audio_started) {
        snd_stream_poll(shnd);
    }

    mutex_unlock(&reset_mutex);
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
