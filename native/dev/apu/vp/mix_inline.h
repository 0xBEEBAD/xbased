/*
 * Inline voice mixer helpers for the MCPX APU's voice processor: the
 * per-voice multiply-add into mixbins (vp.c voice_process's inner loop),
 * written so the compiler vectorizes it.
 *
 * The header is self-contained: it defines NUM_SAMPLES_PER_FRAME locally if
 * the surrounding build hasn't (apu_regs.h does).
 */
#ifndef HW_XBOX_MCPX_APU_VP_MIX_INLINE_H
#define HW_XBOX_MCPX_APU_VP_MIX_INLINE_H

#ifndef NUM_SAMPLES_PER_FRAME
#define NUM_SAMPLES_PER_FRAME 32
#endif

/* dst += g * src over a frame */
static inline void mix_one_bin(float *dst, const float *src, float g) {
    for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
        dst[i] += g * src[i];
    }
}

/* The same with a gain per sample (a route's gain ramping across the
 * frame, vp.c route_gain_ramp). */
static inline void mix_one_bin_ramp(float *dst, const float *src, const float *g) {
    for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
        dst[i] += g[i] * src[i];
    }
}

#endif /* HW_XBOX_MCPX_APU_VP_MIX_INLINE_H */
