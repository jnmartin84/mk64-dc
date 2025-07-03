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
#define SAMPLES_HIGH (544)
//#define DC_AUDIO_SAMPLES_DESIRED (SAMPLES_HIGH * DC_AUDIO_GIVEN_BUFFERS * 2 /* to help pad space */)
#define DC_AUDIO_SAMPLES_DESIRED (0x12000 / 4)

/* Double Buffer */
snd_stream_hnd_t shnd = -1;
//extern int gProcessAudio;

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
    void *snd_buf = snd_buffer_internal + audio_frames_generated_cur * (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);
        snd_buf += (SAMPLES_HIGH * DC_AUDIO_CHANNELS * sizeof(short));
        create_next_audio_buffer(snd_buf, SAMPLES_HIGH);


    audio_frames_generated_total += 2;
    audio_frames_generated_cur += 2;

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
    thd_pass();
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
//#endif
