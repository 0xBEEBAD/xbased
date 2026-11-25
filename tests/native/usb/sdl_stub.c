/*
 * usb_test: the SDL audio calls dev/communicator.c makes, without SDL: no
 * device opens, so the headset stays silent (the test drives its USB side).
 */
#include <SDL3/SDL.h>

bool SDL_InitSubSystem(SDL_InitFlags flags)
{
    (void)flags;
    return false;
}

const char *SDL_GetError(void)
{
    return "no SDL in usb_test";
}

SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec *spec,
                                           SDL_AudioStreamCallback callback, void *userdata)
{
    (void)devid, (void)spec, (void)callback, (void)userdata;
    return NULL;
}

SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec)
{
    (void)src_spec, (void)dst_spec;
    return NULL;
}

bool SDL_ClearAudioStream(SDL_AudioStream *stream)
{
    (void)stream;
    return false;
}

int SDL_GetAudioStreamAvailable(SDL_AudioStream *stream)
{
    (void)stream;
    return 0;
}

int SDL_GetAudioStreamData(SDL_AudioStream *stream, void *buf, int len)
{
    (void)stream, (void)buf, (void)len;
    return 0;
}

int SDL_GetAudioStreamQueued(SDL_AudioStream *stream)
{
    (void)stream;
    return 0;
}

bool SDL_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **audio_buf, Uint32 *audio_len)
{
    (void)path, (void)spec, (void)audio_buf, (void)audio_len;
    return false;
}

bool SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len)
{
    (void)stream, (void)buf, (void)len;
    return false;
}

bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream)
{
    (void)stream;
    return false;
}

bool SDL_SetAudioStreamFormat(SDL_AudioStream *stream, const SDL_AudioSpec *src_spec,
                              const SDL_AudioSpec *dst_spec)
{
    (void)stream, (void)src_spec, (void)dst_spec;
    return false;
}

bool SDL_SetAudioStreamPutCallback(SDL_AudioStream *stream, SDL_AudioStreamCallback callback, void *userdata)
{
    (void)stream, (void)callback, (void)userdata;
    return false;
}
