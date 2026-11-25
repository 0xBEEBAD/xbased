#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "resample.h"

/*
 * The VP's own mix to 16-bit samples (libsamplerate's
 * src_float_to_short_array). The voices' resampler is the
 * 8-point Lagrange interpolator in vp.c (voice_resample).
 */
void src_float_to_short_array(const float *in, short *out, int len)
{
    if (!in || !out || len <= 0) {
        return;
    }

    for (int i = 0; i < len; ++i) {
        float v = in[i];
        if (v > 1.0f) {
            v = 1.0f;
        } else if (v < -1.0f) {
            v = -1.0f;
        }
        out[i] = (short)(v * 32767.0f);
    }
}
