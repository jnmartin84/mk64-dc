/*
 * AICA hardware-mixed voice driver (Dreamcast) for MK64.
 * Ported from the OoT AICA driver. Drives AICA hardware channels from the
 * finalized NoteSubEu set each frame via the KOS command queue; samples staged
 * into ARAM from a resident offline-transcoded Yamaha-ADPCM pool. Stock KOS
 * firmware only (snd_sh4_to_aica is the one non-public symbol, for live UPDATE).
 */

#include "internal.h"
#include "heap.h"
#include "load.h"
#include "data.h"
#include "aica_synth.h"

#ifdef __DREAMCAST__

#include <stdio.h>
#include <stdlib.h>
#include <dc/sound/sound.h>
#include <dc/spu.h>
#include <dc/sound/aica_comm.h>

extern int snd_sh4_to_aica(void* packet, uint32_t size);

#include "aica_sample_table.h"

/* Resident AICA-ADPCM pool base. Loaded by setup_audio_data() in main.c. */
const unsigned char* gAicaAdpcmPoolBase = NULL;
static u8* sTblBase = NULL;        /* shared audiotables blob base (gAlTbl->seqArray[0].offset) */

#define NUM_AICA_CHANNELS 64
#define MAX_VOICES 64
#define ARAM_CACHE_ENTRIES 192
#define AICA_LEN_MAX 65534
#define WAVE_SAMPLE_COUNT 64
#define NUM_WAVEFORMS 6
#define NUM_HARMONICS 4
#define KEY_EMPTY 0xFFFFFFFFu
#define SYNTH_KEY(wf, h) (0x80000000u | ((u32)(wf) << 4) | (u32)(h))

typedef struct {
    u32 key;
    u32 aram;
    u32 len;
    s32 refs;
    u32 lru;
} AramEntry;

static AramEntry sCache[ARAM_CACHE_ENTRIES];
static u32 sTick;
static s8 sChanFree[NUM_AICA_CHANNELS];
static s32 sChanFreeTop;

typedef struct {
    s8 channel;
    u8 active;
    u32 sampleKey;
    AramEntry* entry;
} Voice;
static Voice sVoices[MAX_VOICES];
static u32 sWaveAram[NUM_WAVEFORMS][NUM_HARMONICS];

typedef struct {
    u32 base, type, length, loop, loopstart, loopend;
    u8 downsample_shift;
} Resolved;


static int sample_lookup(u32 key, const AicaSampleDesc** out) {
    s32 lo = 0, hi = AICA_SAMPLE_COUNT - 1;
    while (lo <= hi) {
        s32 mid = (lo + hi) >> 1;
        u32 k = gAicaSampleTable[mid].src_offset;
        if (k == key) { *out = &gAicaSampleTable[mid]; return 1; }
        if (k < key) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

static AramEntry* cache_find(u32 key) {
    s32 i;
    for (i = 0; i < ARAM_CACHE_ENTRIES; i++)
        if (sCache[i].key == key) return &sCache[i];
    return NULL;
}

static AramEntry* cache_acquire(u32 key, u32 pool_offset, u32 byte_len) {
    AramEntry* e = cache_find(key);
    s32 i;
    if (e) { e->refs++; e->lru = sTick; return e; }
    u32 aram = (u32)snd_mem_malloc(byte_len);
    if (aram == 0) {
        AramEntry* victim = NULL;
        for (i = 0; i < ARAM_CACHE_ENTRIES; i++)
            if (sCache[i].key != KEY_EMPTY && sCache[i].refs == 0)
                if (!victim || sCache[i].lru < victim->lru) victim = &sCache[i];
        if (victim) {
            snd_mem_free(victim->aram);
            victim->key = KEY_EMPTY; victim->aram = 0;
            aram = (u32)snd_mem_malloc(byte_len);
        }
        if (aram == 0) return NULL;
    }
    spu_memload_sq(aram, (void*)(gAicaAdpcmPoolBase + pool_offset), (byte_len + 31) & ~31);
    e = cache_find(KEY_EMPTY);
    if (!e) { snd_mem_free(aram); return NULL; }
    e->key = key; e->aram = aram; e->len = byte_len; e->refs = 1; e->lru = sTick;
    return e;
}

static void cache_release(AramEntry* e) { if (e && e->refs > 0) e->refs--; }

/* The KOS aica_freq firmware converts Hz -> AICA (octave, mantissa) via
   freq_lo = (freq<<10)/freq_base, but freq_lo overflows its 10 bits when freq
   reaches 2*freq_base (which happens just below each 44100/2^n boundary because
   freq_base is the integer-truncated 44100>>k). That makes the firmware drop an
   octave -> the voice plays at HALF speed. Mirror the firmware's octave search
   and clamp freq to the last representable value so it never lands in that gap. */
static u32 fix_aica_freq_gap(u32 freq) {
    u32 base = 5644800;   /* 44100 * 128 */
    s32 hi = 7;
    while (freq < base && hi > -8) { base >>= 1; hi--; }
    if (base != 0 && freq >= 2 * base) freq = 2 * base - 1;
    return freq;
}

static u32 calc_freq(struct NoteSubEu* sub, u8 shift) {
    u32 freq = ((u32)sub->resamplingRateFixedPoint * (u32)gAudioBufferParameters.frequency) >> 15;
    if (sub->hasTwoAdpcmParts) freq <<= 1;
    freq >>= shift;
    if (freq == 0) freq = 1;
    return fix_aica_freq_gap(freq);
}

static u32 calc_vol(struct NoteSubEu* sub) {
    u32 l = sub->targetVolLeft, r = sub->targetVolRight;
    u32 m = (l > r) ? l : r;
    u32 v = m >> 4;
    return v > 255 ? 255 : v;
}

/* AICA pan (DIPAN) attenuates the opposite channel in ~3 dB steps (matching the
   documented DISDL/EFSDL send-level table). KOS calc_aica_pan maps our 0..255
   pan to a 0..15 step amount: left  -> 127 - amount*8, right -> 128 + amount*8.
   So convert the L/R balance to dB and pick the matching step count, instead of
   the old linear formula which over-attenuated off-center voices. */
static u32 calc_pan(struct NoteSubEu* sub) {
    s32 l = sub->targetVolLeft, r = sub->targetVolRight;
    s32 hi, lo, amount;
    int left;

    if (l == r) return 128;
    if (l > r) { hi = l; lo = r; left = 1; } else { hi = r; lo = l; left = 0; }

    if (lo <= 0) {
        amount = 15;
    } else {
        /* amount = round( 20*log10(hi/lo) / 3 ), via successive 3 dB (x0.7079)
           ratio thresholds -- no float / libm needed. */
        amount = 0;
        while (amount < 15 && (s64)lo * 1000 < (s64)hi * 708) { /* lo/hi < 0.708 ^? */
            lo = (s32)(((s64)lo * 1000) / 708); /* step up by 3 dB */
            amount++;
        }
    }
    return left ? (u32)(127 - amount * 8) : (u32)(128 + amount * 8);
}

static int resolve(struct NoteSubEu* sub, u32* outKey, Resolved* res, AramEntry** outEntry) {
    *outEntry = NULL;
    res->downsample_shift = 0;

    if (sub->isSyntheticWave) {
        s16* addr = sub->sound.samples;
        s32 wf = -1; u32 h = 0, w;
        for (w = 0; w < NUM_WAVEFORMS; w++) {
            if (addr >= gWaveSamples[w] && addr < gWaveSamples[w] + NUM_HARMONICS * WAVE_SAMPLE_COUNT) {
                wf = (s32)w;
                h = (u32)(addr - gWaveSamples[w]) / WAVE_SAMPLE_COUNT;
                break;
            }
        }
        if (wf < 0) return 0;
        if (h >= NUM_HARMONICS) h = NUM_HARMONICS - 1;
        *outKey = SYNTH_KEY(wf, h);
        res->base = sWaveAram[wf][h];
        res->type = AICA_SM_16BIT;
        res->length = WAVE_SAMPLE_COUNT;
        res->loop = 1; res->loopstart = 0; res->loopend = WAVE_SAMPLE_COUNT;
        return 1;
    }

    if (gAicaAdpcmPoolBase == NULL || sub->sound.audioBankSound == NULL) return 0;
    {
        struct AudioBankSample* s = sub->sound.audioBankSound->sample;
        if (s == NULL || s->sampleAddr == NULL) return 0;
        u32 key = (u32)s->sampleAddr - (u32)sTblBase;
        const AicaSampleDesc* d;
        if (!sample_lookup(key, &d)) return 0;
        AramEntry* e = cache_acquire(key, d->pool_offset, d->byte_len);
        if (!e) return 0;
        *outEntry = e; *outKey = key;
        res->base = e->aram;
        res->type = AICA_SM_ADPCM;
        res->length = d->nsamples > AICA_LEN_MAX ? AICA_LEN_MAX : d->nsamples;
        res->loop = d->loop_flag;
        res->loopstart = d->loop_start;
        res->loopend = d->loop_end ? d->loop_end : res->length;
        res->downsample_shift = d->downsample_shift;
        return 1;
    }
}

static void chan_start(s32 ch, const Resolved* r, u32 freq, u32 vol, u32 pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_START;
    chan->base = r->base; chan->type = r->type; chan->length = r->length;
    chan->loop = r->loop; chan->loopstart = r->loopstart; chan->loopend = r->loopend;
    chan->freq = freq; chan->vol = vol; chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

static void chan_update(s32 ch, u32 freq, u32 vol, u32 pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_VOL | AICA_CH_UPDATE_SET_PAN;
    chan->freq = freq; chan->vol = vol; chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

static void chan_stop(s32 ch) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_STOP;
    snd_sh4_to_aica(tmp, cmd->size);
}

static s32 chan_alloc(void) { return sChanFreeTop ? sChanFree[--sChanFreeTop] : -1; }
static void chan_release(s32 ch) { if (sChanFreeTop < NUM_AICA_CHANNELS) sChanFree[sChanFreeTop++] = (s8)ch; }

static void voice_stop(s32 i) {
    Voice* v = &sVoices[i];
    if (v->channel >= 0) { chan_stop(v->channel); chan_release(v->channel); }
    cache_release(v->entry);
    v->channel = -1; v->active = 0; v->entry = NULL; v->sampleKey = KEY_EMPTY;
}

void AicaSynth_Init(void) {
    s32 i, ch; u32 w, h;
    for (i = 0; i < ARAM_CACHE_ENTRIES; i++) { sCache[i].key = KEY_EMPTY; sCache[i].aram = 0; sCache[i].refs = 0; }
    for (i = 0; i < MAX_VOICES; i++) { sVoices[i].channel = -1; sVoices[i].active = 0; sVoices[i].entry = NULL; sVoices[i].sampleKey = KEY_EMPTY; }
    sChanFreeTop = 0;
    for (ch = NUM_AICA_CHANNELS - 1; ch >= 0; ch--) sChanFree[sChanFreeTop++] = (s8)ch;

    /* gAicaAdpcmPoolBase is loaded earlier by setup_audio_data() in main.c. */
    sTblBase = (u8*)gAlTbl->seqArray[0].offset;

    for (w = 0; w < NUM_WAVEFORMS; w++) {
        u32 bytes = NUM_HARMONICS * WAVE_SAMPLE_COUNT * sizeof(s16);
        u32 aram = (u32)snd_mem_malloc(bytes);
        spu_memload_sq(aram, (void*)gWaveSamples[w], (bytes + 31) & ~31);
        for (h = 0; h < NUM_HARMONICS; h++)
            sWaveAram[w][h] = aram + h * WAVE_SAMPLE_COUNT * sizeof(s16);
    }
}

void AicaSynth_Update(void) {
    s32 numNotes = gMaxSimultaneousNotes;
    s32 base, i;

    if (gAicaAdpcmPoolBase == NULL && sTblBase == NULL) return; /* not initialized yet */
    sTick++;
    if (numNotes > MAX_VOICES) numNotes = MAX_VOICES;
    base = gMaxSimultaneousNotes * (gAudioBufferParameters.updatesPerFrame - 1);

    static u32 sDbgFrame = 0;
    s32 nEnabled = 0, nResolved = 0, nStarted = 0, nFail = 0, nSynth = 0;
    u32 dbgVolMax = 0, dbgVolMin = 999;

    for (i = 0; i < numNotes; i++) {
        struct NoteSubEu* sub = &gNoteSubsEu[base + i];
        Voice* v = &sVoices[i];
        u32 key, freq, vol, pan;
        Resolved res;
        AramEntry* entry;
        int retrigger;

        if (!sub->enabled) { if (v->active) voice_stop(i); continue; }
        nEnabled++;
        if (sub->isSyntheticWave) nSynth++;
        if (!resolve(sub, &key, &res, &entry)) { nFail++; if (v->active) voice_stop(i); continue; }
        nResolved++;

        freq = calc_freq(sub, res.downsample_shift);
        vol = calc_vol(sub);
        pan = calc_pan(sub);
        if (vol > dbgVolMax) dbgVolMax = vol;
        if (vol < dbgVolMin) dbgVolMin = vol;

        retrigger = (!v->active) || (v->sampleKey != key) || sub->needsInit;
        if (retrigger) {
            s32 chn;
            if (v->active) voice_stop(i);
            chn = chan_alloc();
            if (chn < 0) { cache_release(entry); continue; }
            v->channel = chn; v->active = 1; v->sampleKey = key; v->entry = entry;
            chan_start(chn, &res, freq, vol, pan);
            nStarted++;
            {
                static int dbgStarts = 0;
                if (dbgStarts < 48) {
                    printf("AICAstart key=%lX synth=%d vol=%lu pan=%lu freq=%lu len=%lu loop=%lu shift=%d\n",
                           (unsigned long)key, sub->isSyntheticWave, (unsigned long)vol,
                           (unsigned long)pan, (unsigned long)freq, (unsigned long)res.length,
                           (unsigned long)res.loop, res.downsample_shift);
                    dbgStarts++;
                }
            }
        } else {
            if (entry) cache_release(entry);
            chan_update(v->channel, freq, vol, pan);
        }
    }

    if ((++sDbgFrame & 0x3F) == 0 && nEnabled) {
        printf("AICAdbg en=%ld synth=%ld res=%ld fail=%ld start=%ld vol[%lu..%lu]\n",
               (long)nEnabled, (long)nSynth, (long)nResolved, (long)nFail,
               (long)nStarted, (unsigned long)dbgVolMin, (unsigned long)dbgVolMax);
    }
}

#else
void AicaSynth_Init(void) {}
void AicaSynth_Update(void) {}
#endif
