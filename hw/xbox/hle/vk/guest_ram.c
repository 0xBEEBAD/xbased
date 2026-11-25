/*
 * xbased HLE GPU — the upload arena and the readback landing buffer (guest
 * RAM is copied in and out, never imported).
 */
#include "hw/xbox/hle/base.h"

#include "hw/xbox/hle/hle.h"
#include "hw/xbox/hle/mem/guestmem.h"
#include "hw/xbox/hle/vk/guest_ram.h"

HleVkArena g_harena;
HleVkArena g_hland;
bool g_hland_coherent;

#define T (g_hvk.t)

int hle_vk_arena_init(uint32_t size)
{
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (T.vkCreateBuffer(g_hvk.dev, &bci, NULL, &g_harena.buf) != VK_SUCCESS) {
        return -1;
    }
    VkMemoryRequirements req;
    T.vkGetBufferMemoryRequirements(g_hvk.dev, g_harena.buf, &req);
    VkMemoryPropertyFlags hv = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    /* The decoder copies every draw's vertices and indices in. On Windows
     * into CPU-cached memory (a discrete GPU reads it over the bus; an
     * integrated one's is its own): device-local host-visible memory there
     * is the GPU's VRAM through the BAR, and the copies crossing the bus
     * held Conker's decoder to 225 fps uncapped, 317 from cached memory.
     * XBASED_HLE_ARENA=vram|sys|cached chooses (comparisons). */
    const char *ae = getenv("XBASED_HLE_ARENA");
    if (!ae) {
        ae = "cached";
    }
    int mt = -1;
    if (ae && !strcmp(ae, "cached")) {
        mt = hle_vk_memtype(req.memoryTypeBits, hv | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0);
    } else if (!ae || !strcmp(ae, "vram")) {
        mt = hle_vk_memtype(req.memoryTypeBits, hv | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    }
    if (mt < 0) {
        mt = hle_vk_memtype(req.memoryTypeBits, hv, 0);
    }
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = (uint32_t)mt };
    if (mt < 0 || T.vkAllocateMemory(g_hvk.dev, &mai, NULL, &g_harena.mem) != VK_SUCCESS) {
        return -1;
    }
    T.vkBindBufferMemory(g_hvk.dev, g_harena.buf, g_harena.mem, 0);
    T.vkMapMemory(g_hvk.dev, g_harena.mem, 0, size, 0, (void **)&g_harena.map);
    hle_arena_init(&g_harena.alloc, size);
    hle_log("xbased-hle-back", "upload arena: %u MiB, memtype %d flags 0x%x", size >> 20, mt,
            g_hvk.mem.memoryTypes[mt].propertyFlags);
    return g_harena.map ? 0 : -1;
}

int hle_vk_land_init(uint32_t size)
{
    /* readbacks land here rather than in the write-combined arena (a
     * swizzled target's texels are swizzled into guest RAM on the CPU,
     * pb/pbdraw.c) */
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VkBuffer buf;
    if (T.vkCreateBuffer(g_hvk.dev, &bci, NULL, &buf) != VK_SUCCESS) {
        return 0;
    }
    VkMemoryRequirements req;
    T.vkGetBufferMemoryRequirements(g_hvk.dev, buf, &req);
    VkMemoryPropertyFlags hc = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    int mt = hle_vk_memtype(req.memoryTypeBits, hc | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0);
    g_hland_coherent = mt >= 0;
    if (mt < 0) {
        mt = hle_vk_memtype(req.memoryTypeBits, hc, 0);
    }
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = (uint32_t)mt };
    VkDeviceMemory mem;
    uint8_t *map = NULL;
    if (mt < 0 || T.vkAllocateMemory(g_hvk.dev, &mai, NULL, &mem) != VK_SUCCESS) {
        T.vkDestroyBuffer(g_hvk.dev, buf, NULL);
        hle_log("xbased-hle-back", "readback landing: no host-cached memory, the arena serves");
        return 0;
    }
    T.vkBindBufferMemory(g_hvk.dev, buf, mem, 0);
    T.vkMapMemory(g_hvk.dev, mem, 0, VK_WHOLE_SIZE, 0, (void **)&map);
    if (!map) {
        T.vkFreeMemory(g_hvk.dev, mem, NULL);
        T.vkDestroyBuffer(g_hvk.dev, buf, NULL);
        return 0;
    }
    g_hland.mem = mem;
    g_hland.map = map;
    hle_arena_init(&g_hland.alloc, size);
    g_hland.buf = buf;                  /* last: the front reads it as "ready" */
    hle_log("xbased-hle-back", "readback landing: %u MiB host-cached (type %d, %scoherent)",
            size >> 20, mt, g_hland_coherent ? "" : "not ");
    return 0;
}

