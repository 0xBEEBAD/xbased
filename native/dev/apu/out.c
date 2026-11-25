/*
 * xbased native core — the APU's output: 48 kHz 16-bit stereo or 5.1 on an
 * SDL3 audio stream.
 *
 * The frame thread pushes each EP frame (256 samples) into a ring; the
 * stream's callback drains it. One producer and one consumer, so the ring is
 * two counters: the producer publishes its writes with the head (release),
 * the callback its reads with the tail. With no output device, a thread
 * drains the ring at the device's rate instead (the sound goes nowhere, the
 * APU keeps its timing).
 */
#include "native/dev/apu/apu_int.h"

#include <SDL3/SDL.h>

#define OUT_RATE 48000
/* The channels out: 2, or 6 for 5.1 (nc_audio_set_channels: FL FR C LFE BL BR,
 * SDL's order and the Xbox's mixbins'); a sample frame: 16 bits each. */
int g_apu_out_channels = 2;
int16_t g_apu_frame6[256][6];
#define OUT_FRAME_BYTES (2 * g_apu_out_channels)

/* The channels out (2 or 6), before the APU starts: SDL mixes six down to
 * the device's speakers when it has fewer. */
void nc_audio_set_channels(int n)
{
    g_apu_out_channels = n == 6 ? 6 : 2;
}

static struct {
    uint8_t *buf;
    uint32_t cap;
    uint64_t head, tail;     /* bytes written, bytes read */
    int samples;             /* the callback's size, in frames */
    pthread_cond_t *wake;
    pthread_mutex_t mu;      /* the stream's pause, app_paused */
    bool app_paused;
    uint32_t callbacks, short_callbacks;
    uint32_t asked_max;      /* the most bytes one callback asked for */
    uint32_t fed_callbacks;  /* callbacks since the first frame came, to 64 (fill) */
} g_out = { .mu = PTHREAD_MUTEX_INITIALIZER };

int apu_out_queued(void)
{
    return (int)(apu_ld_acq(&g_out.head) - apu_ld_acq(&g_out.tail));
}

void apu_out_push(const void *frame, int bytes)
{
    const uint64_t head = g_out.head;
    assert(head + (uint64_t)bytes - apu_ld_acq(&g_out.tail) <= g_out.cap);
    const uint32_t at = (uint32_t)(head % g_out.cap);
    const uint32_t first = MIN((uint32_t)bytes, g_out.cap - at);
    memcpy(g_out.buf + at, frame, first);
    memcpy(g_out.buf, (const uint8_t *)frame + first, (uint32_t)bytes - first);
    apu_st_rel(&g_out.head, head + (uint64_t)bytes);
}

static int pop(uint8_t *out, int bytes)
{
    const uint64_t tail = g_out.tail;
    const int n = (int)MIN((uint64_t)bytes, apu_ld_acq(&g_out.head) - tail);
    const uint32_t at = (uint32_t)(tail % g_out.cap);
    const uint32_t first = MIN((uint32_t)n, g_out.cap - at);
    memcpy(out, g_out.buf + at, first);
    memcpy(out + first, g_out.buf, (uint32_t)n - first);
    apu_st_rel(&g_out.tail, tail + (uint64_t)n);
    return n;
}

int apu_out_asked_max(void)
{
    return (int)apu_ld(&g_out.asked_max);
}

void apu_out_stats(uint32_t *callbacks, uint32_t *short_callbacks)
{
    *callbacks = apu_xchg(&g_out.callbacks, 0);
    *short_callbacks = apu_xchg(&g_out.short_callbacks, 0);
}

/* One callback's worth: what the ring has, the rest silence (a short
 * callback). Its size is the device's, whatever was asked at the open:
 * PulseAudio and PipeWire can take larger periods than 512 frames, so the
 * frame thread keeps its watermarks from the largest callback seen
 * (apu_out_asked_max). It never waits: the callback runs on the device's
 * real-time thread, and the frame thread keeps two to four callbacks queued
 * (apu.c's watermarks). Paused (nc_pause_set), silence, the ring kept: the frame
 * thread stops at its high watermark. */
static void fill(uint8_t *stream, int bytes)
{
    if (nc_paused()) {
        memset(stream, 0, (size_t)bytes);
        return;
    }
    /* the device's own first fills (SDL asks for its whole buffer as it
     * starts) left out of asked_max: its periods are what the ring is for */
    if (g_out.fed_callbacks < 64) {
        g_out.fed_callbacks += apu_ld_acq(&g_out.head) != 0;
    } else if ((uint32_t)bytes > apu_ld(&g_out.asked_max)) {
        apu_st(&g_out.asked_max, (uint32_t)bytes);
    }
    const int n = pop(stream, bytes);
    apu_inc(&g_out.callbacks);
    if (n < bytes) {
        apu_inc(&g_out.short_callbacks);
        memset(stream + n, 0, (size_t)(bytes - n));
    }
    pthread_cond_broadcast(g_out.wake);
}

/* SDL follows the default device itself (a headset plugged in, the default
 * changed in Windows' settings): one stream for the run. Its callback asks
 * for what the device will play next; the ring gives it, padded with
 * silence. */
static SDL_AudioStream *g_sdl;
static float g_volume = 1.0f;

static void SDLCALL on_sdl(void *user, SDL_AudioStream *s, int additional, int total)
{
    static uint8_t buf[16384];
    while (additional > 0) {
        int n = MIN(additional, (int)sizeof(buf));
        n -= n % OUT_FRAME_BYTES;
        if (n <= 0) {
            break;
        }
        fill(buf, n);
        SDL_PutAudioStreamData(s, buf, n);
        additional -= n;
    }
}

/* No device at all: the ring drained at the device's rate, without sound. */
static void *drain_thread(void *arg)
{
    static uint8_t buf[16384];
    const int len = MIN(g_out.samples * OUT_FRAME_BYTES, (int)sizeof(buf));
    const int64_t period_ns = (int64_t)(len / OUT_FRAME_BYTES) * 1000000000LL / OUT_RATE;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int64_t next = (int64_t)t0.tv_sec * 1000000000LL + t0.tv_nsec;
    for (;;) {
        next += period_ns;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        const int64_t left = next - ((int64_t)now.tv_sec * 1000000000LL + now.tv_nsec);
        if (left > 0) {
            usleep((unsigned long long)(left / 1000));
        }
        fill(buf, len);
    }
    return NULL;
}

int apu_out_open(int capacity_bytes, int samples, pthread_cond_t *wake)
{
    g_out.buf = calloc(1, (size_t)capacity_bytes);
    g_out.cap = (uint32_t)capacity_bytes;
    g_out.samples = samples;
    g_out.wake = wake;

    char frames[16];
    snprintf(frames, sizeof(frames), "%d", samples);
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames);
    const SDL_AudioSpec spec = {SDL_AUDIO_S16LE, g_apu_out_channels, OUT_RATE};
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        g_sdl = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, on_sdl, NULL);
    }
    if (!g_sdl) {
        nc_log("xbased-apu", "no output stream (SDL: %s); draining without sound", SDL_GetError());
        pthread_t t;
        apu_thread_start(&t, "apu.drain", drain_thread, NULL, false);
    } else {
        pthread_mutex_lock(&g_out.mu);
        if (!g_out.app_paused) {
            SDL_ResumeAudioStreamDevice(g_sdl);
        }
        SDL_SetAudioStreamGain(g_sdl, g_volume);
        pthread_mutex_unlock(&g_out.mu);
        /* what the device gives: PipeWire runs every client at the graph's
         * quantum, smaller than asked when another client or the system's
         * settings want it (128 frames, 2.7 ms, on one Linux machine) */
        const SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(g_sdl);
        SDL_AudioSpec got = {0};
        int got_frames = 0;
        SDL_GetAudioDeviceFormat(dev, &got, &got_frames);
        nc_log("xbased-apu", "output stream: %s (%s), %d frames a callback asked, %d channels; "
                            "the device: %d frames a period, %d channels, %d Hz",
               SDL_GetAudioDeviceName(dev), SDL_GetCurrentAudioDriver(), samples, g_apu_out_channels, got_frames,
               got.channels, got.freq);
    }
    return samples * OUT_FRAME_BYTES;
}

/* The output's gain (0-1, the front end's volume), before or after it opens. */
void nc_audio_set_volume(float v)
{
    g_volume = v < 0 ? 0 : v > 1 ? 1 : v;
    if (g_sdl) {
        SDL_SetAudioStreamGain(g_sdl, g_volume);
    }
}

/* The emulation paused (the window minimized, the front end's pause): the
 * device stops pulling silence meanwhile, and starts again on resuming. */
void nc_audio_set_paused(bool on)
{
    pthread_mutex_lock(&g_out.mu);
    g_out.app_paused = on;
    if (g_sdl) {
        if (on) {
            SDL_PauseAudioStreamDevice(g_sdl);
        } else {
            SDL_ResumeAudioStreamDevice(g_sdl);
        }
    }
    pthread_mutex_unlock(&g_out.mu);
}
