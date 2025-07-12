/*
 * File: dc_audio_kos.c
 * Project: sm64-port
 * Author: Hayden Kowalchuk (hayden@hkowsoftware.com)
 * -----
 * Copyright (c) 2025 Hayden Kowalchuk
 */

#if 1
//defined(TARGET_DC)

#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "audio_dc.h"
#include "macros.h"

//#define OUTPUT

// --- Configuration ---
#define DC_AUDIO_CHANNELS (2) // Stereo
#define DC_STEREO_AUDIO ( DC_AUDIO_CHANNELS == 2)
#define DC_AUDIO_FREQUENCY (26800) // Sample rate for the AICA (32kHz)
#define RING_BUFFER_MAX_BYTES (SND_STREAM_BUFFER_MAX * 2)

// --- Global State for Dreamcast Audio Backend ---
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; // Handle for the sound stream
static uint8_t cb_buf_internal[RING_BUFFER_MAX_BYTES] __attribute__((aligned(32))); // The main audio buffer
static void *const cb_buf = cb_buf_internal;
//static spinlock_t g_audio_spinlock; // Spinlock to protect concurrent access to ring buffer
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

    while(1) { //g_audio_thread_status == AUDIO_STATUS_RUNNING) {
        if (shnd != SND_STREAM_INVALID && audio_started) {
            snd_stream_poll(shnd);
        }
        thd_sleep(1);//sleep(1); // Sleep briefly to avoid busy-waiting, yielding CPU
    }

#if defined(OUTPUT)
    printf("AUDIO_THREAD: Polling thread shutting down.\n"); // Debug print, uncomment if needed
#endif

    return NULL;
}

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

typedef struct {
    uint8_t *buf;
    uint32_t  cap;    // power-of-two
    /* atomic_uint */uint32_t head; // next write pos
    /* atomic_uint */uint32_t tail; // next read pos
} ring_t;

static ring_t cb_ring;
static ring_t *r = &cb_ring;

static bool cb_init(size_t capacity) {
    // round capacity up to power of two
    r->cap = 1u << (32 - __builtin_clz(capacity - 1));
    r->buf = cb_buf; //aligned_alloc(32, r->cap);
    if (!r->buf) return false;
    r->head = 0;//atomic_store(&r->head, 0);
    r->tail = 0;//atomic_store(&r->tail, 0);
    return true;
}

//static int16_t *get_

static size_t cb_write_data(const void *src, size_t n) {
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;//atomic_load(&r->tail);
    uint32_t free = r->cap - (head - tail);
    if (n > free) return 0;
    uint32_t idx = head & (r->cap - 1);
    uint32_t first = MIN(n, r->cap - idx);
    memcpy(r->buf + idx, src, first);
//    if (n - first) {
//        printf("had to do a split copy for %d bytes\n", n-first);
    memcpy(r->buf, (uint8_t*)src + first, n - first);
//    }
    r->head = head + n;//atomic_store(&r->head, head + n);
    return n;
}

static size_t cb_read_data(void *dst, size_t n) {
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;atomic_load(&r->tail);
    uint32_t avail = head - tail;
    if (n > avail) return 0;
    uint32_t idx = tail & (r->cap - 1);
    uint32_t first = MIN(n, r->cap - idx);
    memcpy(dst, r->buf + idx, first);
    memcpy((uint8_t*)dst + first, r->buf, n - first);
//    atomic_store(&r->tail, tail + n);
    r->tail = tail + n;
    return n;
}

// Calculates and returns the number of bytes currently in the ring buffer
static size_t cb_get_used(void) {
    // Atomically load both head and tail to get a consistent snapshot.
    // The order of loads might matter in some weak memory models,
    // but for head-tail diff, generally not.
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;//atomic_load(&r->tail);
    
           // The number of used bytes is simply the difference between head and tail.
           // This works because head and tail are continuously incrementing indices,
           // and effectively handle wrap-around due to the (head - tail) arithmetic.
    return head - tail;
}

// You might also want a function to get free space:
static size_t cb_get_free(void) {
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;//atomic_load(&r->tail);
    return r->cap - (head - tail);
}

// And optionally, if you need to check if it's full or empty
static bool cb_is_empty(void) {
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;//atomic_load(&r->tail);
    return head == tail;
}

static bool cb_is_full(void) {
    uint32_t head = r->head;//atomic_load(&r->head);
    uint32_t tail = r->tail;//atomic_load(&r->tail);
    // A power-of-two ring buffer designed this way typically means
    // full when (head - tail) == cap
    return (head - tail) == r->cap;
}

static void cb_clear(void){}

extern uint32_t GetSystemTimeLow(void);


// --- KOS Stream Audio Callback (Consumer): Called by KOS when the AICA needs more data ---
#define NUM_BUFFER_BLOCKS (2)
static uint8_t temp_buf[SND_STREAM_BUFFER_MAX * NUM_BUFFER_BLOCKS]  __attribute__((aligned(32)));
static unsigned int temp_buf_sel = 0;
void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested_bytes, int *samples_returned_bytes) {

    size_t samples_requested = samples_requested_bytes / 4;
    size_t samples_avail_bytes = cb_read_data(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel) , samples_requested_bytes);
    
    *samples_returned_bytes = samples_requested_bytes;
    size_t samples_returned = samples_avail_bytes / 4;

#if defined(OUTPUT)
    printf("AUDIOCB_DBG: Entry: avail=%zu, free=%zu, cap=%zu, buf=%u, Req=%d, Ret=%zu (Rq=%zu, Rt=%zu)\n",
           ring_data_available, ring_data_free, cb_capacity, temp_buf_sel, samples_requested_bytes, samples_avail_bytes, samples_requested, samples_returned  );
    fflush(stdout);
#endif
    
    /*@Note: This is more correct, fill with empty audio */
    if(samples_avail_bytes < (unsigned)samples_requested_bytes){
        memset(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel) + samples_avail_bytes, 0, (samples_requested_bytes - samples_avail_bytes));
        
        // Underrun debug message (already present, but keeping for context)
    //    printf("AUDIOCB: Underrun! Avail: %zu/%u, Req=%d, Ret=%zu (Rq=%zu, Rt=%zu). Silent: %zu .\n",
      //         samples_avail_bytes, SND_STREAM_BUFFER_MAX, samples_requested_bytes, samples_avail_bytes, samples_requested, samples_returned, samples_requested_bytes - samples_avail_bytes);
        //fflush(stdout);
    }
    
    temp_buf_sel+=1;
    if(temp_buf_sel >= NUM_BUFFER_BLOCKS) { temp_buf_sel = 0; }
    
    return (void*)(temp_buf + (SND_STREAM_BUFFER_MAX * temp_buf_sel));
}

static bool audio_dc_init(void) {
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        fflush(stdout);
        return false;
    }
    thd_set_hz(120);
    
    // --- Initial Pre-fill of Ring Buffer with Silence ---
    sq_clr(cb_buf_internal, sizeof(cb_buf_internal));
    sq_clr(temp_buf, sizeof(temp_buf));
    if(!cb_init(RING_BUFFER_MAX_BYTES)) {
        printf("CB INIT FAILURE!\n");
        fflush(stdout);
        return false;
    }
    
    printf("Dreamcast Audio: Initialized. Ring buffer size: %u bytes.\n",
           (unsigned int)RING_BUFFER_MAX_BYTES);
    
    // Allocate the sound stream with KOS
    shnd = snd_stream_alloc(audio_callback, SND_STREAM_BUFFER_MAX / 8);
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: Stream allocation failure!\n");
        fflush(stdout);
        snd_stream_destroy(shnd);
        return false;
    }
    snd_stream_volume(shnd, 252); // Set maximum volume
    
    // Start the dedicated audio polling thread
//    g_audio_thread_status = AUDIO_STATUS_RUNNING;

    #if 0
    kthread_attr_t main_attr;
    main_attr.create_detached = 0;
	main_attr.prio = 14;
	main_attr.label = "snd_thread";
    g_audio_poll_thread_handle = thd_create_ex(&main_attr, snd_thread, NULL);
    #endif
#if 0
    g_audio_poll_thread_handle = thd_create(false, snd_thread, NULL);

    if (!g_audio_poll_thread_handle) {
        printf("ERROR: Failed to create audio polling thread!\n");
        fflush(stdout);
        return false;
    }
#endif
    printf("Sound init complete!\n");
    
    return true;
}

static int audio_dc_buffered(void) {
    return 1088;
}

static int audio_dc_get_desired_buffered(void) {
    return 1100;
}

static void audio_dc_play(/* const */ uint8_t *buf, size_t len) {
    //spinlock_lock(&g_audio_spinlock);
    
    size_t ring_data_available = cb_get_used();
    size_t written = cb_write_data(buf, len);

    //printf("AUDIOGEN_DBG: used=%zu .\n", ring_data_available);
            
    if(!audio_started && ring_data_available >= (SND_STREAM_BUFFER_MAX)){
        audio_started = true;
     //   printf("AUDIOGEN_DBG: Start.\n");
       // fflush(stdout);
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
    }
    //dc-tool-ip -t 192.168.137.202 -x build/us_dc/sm64.us.f3dex2e
    //clear && /Applications/Flycast.app/Contents/MacOS/Flycast "`realpath ./build/us_dc/sm64-port.cdi`";
    
    if(written == 0){
       // printf("AUDIOGEN_DBG: Buffer full! clearing audio. Gen %zu Wrote %zu \n", len / 4, written / 4 );
       // fflush(stdout);
        cb_clear();
       // return;
    }
    if(audio_started)
        snd_stream_poll(shnd);
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
#endif

#if 0
//#if defined(TARGET_DC)

#include "macros.h"
#include "audio_api.h"
#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>

#define DC_AUDIO_CHANNELS 2
#define DC_STEREO_AUDIO (1)
#define DC_AUDIO_FREQUENCY 32000
#define DC_AUDIO_GIVEN_BUFFERS (2)
#define SAMPLES_HIGH (533)
//#define DC_AUDIO_SAMPLES_DESIRED (SAMPLES_HIGH * DC_AUDIO_GIVEN_BUFFERS * 2 /* to help pad space */)
#define DC_AUDIO_SAMPLES_DESIRED (0x12000 / 4)

/* Double Buffer */
snd_stream_hnd_t shnd = -1;
//extern int gProcessAudio;

/* 0x12000 bytes */
uint16_t snd_buffer_internal[DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS *2] __attribute__((aligned(64)));

static int audio_frames_generated_total;
static int audio_frames_generated_cur;

extern void create_next_audio_buffer(uint16_t *samples, uint32_t num_samples);
void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested, int *samples_returned) {
    const int backup_req = samples_requested;
    /* Catch-22, can be used to play more samples, but then takes longer, leading to needing more samples */
#if 1
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

    shnd = snd_stream_alloc(audio_callback, SND_STREAM_BUFFER_MAX  /* 0xFFFF */);
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: creation failure!");
        fflush(stdout);
        return false;
    }

    memset(snd_buffer_internal, 0xff, DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS * 2);
    audio_frames_generated_total = 0;
    audio_frames_generated_cur = 0;

    //snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
    //snd_stream_volume(shnd, 255);	

    return 1;
}

static int audio_dc_buffered(void) {
    return 0;
}

static int audio_dc_get_desired_buffered(void) {
    /* This is more than 1088 */
    return 1100;
}
    int stream_started = 0;

/* We do our own cycle and processing of audio */
static void audio_dc_play(void) {

//printf("a f g c %d\n", audio_frames_generated_cur);
/* 
    if((unsigned)(DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS) < (unsigned)(audio_frames_generated_cur * (SAMPLES_HIGH * DC_AUDIO_CHANNELS))) {
        printf("over by %u\n",(unsigned)(audio_frames_generated_cur * (SAMPLES_HIGH * DC_AUDIO_CHANNELS)) -  (unsigned)(DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS));
//        audio_frames_generated_cur = audio_frames_generated_total = 0;
        printf("audio frame overflow\n");
    } */

    void *snd_buf = snd_buffer_internal + audio_frames_generated_cur * (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
  //  printf("curnt: %08x\n", snd_buf);
   // printf("start: %08x\n", snd_buffer_internal);

    //if ((uintptr_t)snd_buf > (uintptr_t)(snd_buffer_internal + (DC_AUDIO_SAMPLES_DESIRED * DC_AUDIO_CHANNELS * 2))) {
  //  printf("buf - start == %d\n", (uintptr_t)snd_buf - (uintptr_t)snd_buffer_internal);
//}
    create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
        snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
        snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
  //      create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
    //    snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
      //  create_next_audio_buffer(snd_buf, SAMPLES_HIGH);


    audio_frames_generated_total += 2;
    audio_frames_generated_cur += 2;
#if 1
    if (!stream_started && audio_frames_generated_total > 15) {
        printf("START THE STREAMS\n");
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
        snd_stream_volume(shnd, 255);		
        stream_started = 1;
    }
    
    if (stream_started) {
    int ret = snd_stream_poll(shnd);
    if (ret) {
    printf("SND: %d\n", ret);
        fflush(stdout);
    }
    }
#endif
    thd_pass();
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
//#endif
#endif