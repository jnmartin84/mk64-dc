#ifndef AICA_SYNTH_H
#define AICA_SYNTH_H

/*
 * AICA hardware-mixed voice driver (Dreamcast) for MK64.
 * Replaces the software synthesis render path: maps each active NoteSubEu to an
 * AICA hardware channel via the KOS command queue. Ported from the OoT driver.
 *
 *   AicaSynth_Update() : call from synthesis_execute, after the
 *                        process_sequences / synthesis_load_note_subs_eu loop.
 *   AicaSynth_Init()   : lazy-called from Update (uploads synthetic wavetables,
 *                        loads adpcm_pool.bin resident).
 */

void AicaSynth_Init(void);
void AicaSynth_Update(void);
/* Clean teardown: stop voices, drain a concurrent Update, free ARAM. Idempotent.
   Auto-registered via atexit() by AicaSynth_Init(); also safe to call directly
   from the game's exit path (before exit()/snd teardown). */
void AicaSynth_Shutdown(void);

/* Base of the resident AICA-ADPCM sample pool; set by setup_audio_data(). */
extern const unsigned char* gAicaAdpcmPoolBase;

#endif /* AICA_SYNTH_H */
