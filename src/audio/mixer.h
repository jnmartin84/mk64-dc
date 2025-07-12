#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "PR/abi.h"

#undef aSegment
#undef aClearBuffer
#undef aSetBuffer
#undef aLoadBuffer
#undef aSaveBuffer
#undef aDMEMMove
#undef aMix
#undef aEnvMixer
#undef aResample
#undef aInterleave
#undef aSetVolume
#undef aSetVolume32
#undef aSetLoop
#undef aLoadADPCM
#undef aADPCMdec
#undef aS8Dec
#undef aAddMixer
#undef aDuplicate
#undef aDMEMMove2
#undef aResampleZoh
#undef aDownsampleHalf
#undef aEnvSetup1
#undef aEnvSetup2
#undef aFilter
#undef aHiLoGain
#undef aInterl
#undef aUnkCmd3
#undef aUnkCmd19
#define VIRTUAL_TO_PHYSICAL2(x) (x)
void aClearBufferImpl(uint16_t addr, int nbytes);
void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes);
void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes);
void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr);
void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes);
void aInterleaveImpl(uint16_t left, uint16_t right);
void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes);
void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state);
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state);
void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state);
void aEnvSetup1Impl(uint8_t initial_vol_wet, uint16_t rate_wet, uint16_t rate_left, uint16_t rate_right);
void aEnvSetup2Impl(uint16_t initial_vol_left, uint16_t initial_vol_right);
void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples, bool swap_reverb,
                   bool neg_left, bool neg_right,
                   uint16_t dry_left_addr, uint16_t dry_right_addr,
                   uint16_t wet_left_addr, uint16_t wet_right_addr);
void aSMixImpl(uint16_t in_addr, uint16_t out_addr, uint16_t count);
void aMixImpl(int16_t gain, uint16_t in_addr, uint16_t out_addr, uint16_t count);
void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count);
void aDownsampleHalfImpl(uint16_t n_samples, uint16_t in_addr, uint16_t out_addr);
//void aS8DecImpl(uint8_t flags, ADPCM_STATE state);
//void aResampleZohImpl(uint16_t pitch, uint16_t start_fract);
//void aAddMixerImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr);
//void aDuplicateImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr);
//void aInterlImpl(uint16_t in_addr, uint16_t out_addr, uint16_t n_samples);
//void aFilterImpl(uint8_t flags, uint16_t count_or_buf, int16_t* state_or_filter);
//void aHiLoGainImpl(uint8_t g, uint16_t count, uint16_t addr);
//void aUnkCmd3Impl(uint16_t a, uint16_t b, uint16_t c);
//void aUnkCmd19Impl(uint8_t f, uint16_t count, uint16_t out_addr, uint16_t in_addr);

#define aSegment(pkt, s, b) \
    do {                    \
    } while (0)
#define aClearBuffer(pkt, d, c) aClearBufferImpl(d, c)
#define aLoadBuffer(pkt, s, d, c) aLoadBufferImpl(s, d, c)
#define aSaveBuffer(pkt, s, d, c) aSaveBufferImpl(s, d, c)
#define aLoadADPCM(pkt, c, d) aLoadADPCMImpl(c, d)
#define aSetBuffer(pkt, f, i, o, c) aSetBufferImpl(f, i, o, c)
#define aInterleave(pkt, l, r) aInterleaveImpl(l, r)
#define aDMEMMove(pkt, i, o, c) aDMEMMoveImpl(i, o, c)
#define aSetLoop(pkt, a) aSetLoopImpl(a)
#define aADPCMdec(pkt, f, s) aADPCMdecImpl(f, s)
#define aResample(pkt, f, p, s) aResampleImpl(f, p, s)
#define aEnvSetup1(pkt, initialVolReverb, rampReverb, rampLeft, rampRight) \
    aEnvSetup1Impl(initialVolReverb, rampReverb, rampLeft, rampRight)
#define aEnvSetup2(pkt, initialVolLeft, initialVolRight) aEnvSetup2Impl(initialVolLeft, initialVolRight)
#define aEnvMixer(pkt, inBuf, nSamples, swapReverb, negLeft, negRight, dryLeft, dryRight, wetLeft, wetRight) \
    aEnvMixerImpl(inBuf, nSamples, swapReverb, negLeft, negRight, dryLeft, dryRight, wetLeft, wetRight)
#define aSMix(pkt, g, i, o, c) aSMixImpl(i, o, c)
#define aMix(pkt, g, i, o, c) aMixImpl(g, i, o, c)
#define aDMEMMove2(pkt, t, i, o, c) aDMEMMove2Impl(t, i, o, c)
#define aDownsampleHalf(pkt, nSamples, i, o) aDownsampleHalfImpl(nSamples, i, o)
//#define aS8Dec(pkt, f, s) aS8DecImpl(f, s)
//#define aAddMixer(pkt, s, d, c) aAddMixerImpl(s, d, c)
//#define aDuplicate(pkt, s, d, c) aDuplicateImpl(s, d, c)
//#define aResampleZoh(pkt, pitch, startFract) aResampleZohImpl(pitch, startFract)
//#define aInterl(pkt, dmemi, dmemo, count) aInterlImpl(dmemi, dmemo, count)
//#define aFilter(pkt, f, countOrBuf, addr) aFilterImpl(f, countOrBuf, addr)
//#define aHiLoGain(pkt, g, buflen, i, a4) aHiLoGainImpl(g, buflen, i)
//#define aUnkCmd3(pkt, a1, a2, a3) aUnkCmd3Impl(a1, a2, a3)
//#define aUnkCmd19(pkt, a1, a2, a3, a4) aUnkCmd19Impl(a1, a2, a3, a4)
