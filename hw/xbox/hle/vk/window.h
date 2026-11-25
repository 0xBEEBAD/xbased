/*
 * xbased HLE GPU — the window the presenter draws into (vk/swapchain.c), as
 * the front end keeps it (native/app/win_main.c): on Windows its HWND, on
 * Linux its SDL window, and its size in pixels as the last resize left it.
 */
#ifndef HW_XBOX_HLE_VK_WINDOW_H
#define HW_XBOX_HLE_VK_WINDOW_H

#include <stdint.h>

typedef struct HleWindow {
    void *hinstance;
    void *hwnd;
    void *sdl_window;   /* Linux: the SDL window (SDL_Vulkan_CreateSurface) */
    volatile int32_t width, height;
} HleWindow;

#endif
