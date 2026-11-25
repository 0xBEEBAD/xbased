/*
 * xbased HLE GPU — the upload arena and the readback landing buffer.
 */
#ifndef HW_XBOX_HLE_VK_GUEST_RAM_H
#define HW_XBOX_HLE_VK_GUEST_RAM_H

#include "hw/xbox/hle/vk/vkdev.h"
#include "hw/xbox/hle/front/arena.h"

typedef struct HleVkArena {
    VkBuffer buf;
    VkDeviceMemory mem;
    uint8_t *map;
    HleArena alloc;
} HleVkArena;

extern HleVkArena g_harena;

/* Readbacks that land in guest memory (hle_tl_land) go through host-cached
 * memory the CPU reads fast; the arena's is write-combined (a 640x480 frame
 * took ~25 ms to read). g_hland.buf NULL: no cached type, the arena serves.
 * Not coherent: invalidated before the CPU reads it. */
extern HleVkArena g_hland;
extern bool g_hland_coherent;

int hle_vk_arena_init(uint32_t size);
int hle_vk_land_init(uint32_t size);

#endif
