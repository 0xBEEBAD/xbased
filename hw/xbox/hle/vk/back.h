/*
 * xbased HLE GPU — back end: render thread, timeline, surfaces.
 * No Vulkan types; callable from any HLE module.
 */
#ifndef HW_XBOX_HLE_VK_BACK_H
#define HW_XBOX_HLE_VK_BACK_H

#include <stdbool.h>
#include <stdint.h>

/* Vulkan device (floor), arena, ring, timeline, threads. 0 = ok; negative:
 * no usable device (the title can't run). */
int hle_back_start(void);
bool hle_back_running(void);
void hle_back_kick(void);
/* The guest blocks on the timeline: pending fence / kick boundaries are
 * submitted at once (vCPU thread). */
void hle_back_urgent(void);

/* Timeline value = GPU time. Any thread. */
uint64_t hle_tl_completed(void);
uint64_t hle_tl_submitted(void);
void hle_tl_wait(uint64_t value);
/* The device lost (hle_vk_lose): every value counts as reached, so no
 * thread waits on the GPU any more. */
void hle_tl_lose(void);
/* len bytes the GPU copies into the arena at arena_off go to guest memory at
 * pa once it reaches value, before any thread sees the value (a readback
 * without zero-copy guest RAM). False if too many wait (nothing queued). */
bool hle_tl_land(uint64_t value, uint32_t arena_off, uint32_t pa, uint32_t len);
/* The back end recorded the copy into arena_off (rows of row bytes, pitch
 * apart): those land (else nothing does). */
void hle_tl_land_copied(uint32_t arena_off, uint32_t pitch, uint32_t row, uint32_t rows);
/* More landings at value, made in the order queued (vk/timeline.c), each
 * reported to done(token, landed) on the thread that lands it, the timeline
 * lock held (no landing queued from it): a target's w x h texels of bpp
 * bytes the GPU packs at off of the landing buffer (the arena without one),
 * swizzled into guest memory at pa (dropped unless the back end recorded
 * the copy, hle_tl_land_copied); rows of row bytes copied in guest memory,
 * spitch / dpitch apart (a 2D blit behind a landing it reads); a note.
 * Queued at a value the landings went through already, made at once.
 * False: too many wait (hle_tl_land_room: whether n more fit; the decode
 * thread queues them all; hle_tl_land_oldest: the lowest value queued, 0 if
 * none, what makes room). */
typedef void (*HleLandDone)(uint32_t token, bool landed);
bool hle_tl_land_room(unsigned n);
uint64_t hle_tl_land_oldest(void);
bool hle_tl_land_swz(uint64_t value, uint32_t off, uint32_t pa, uint32_t w, uint32_t h, uint32_t bpp,
                     HleLandDone done, uint32_t token);
bool hle_tl_land_copy(uint64_t value, uint32_t spa, uint32_t spitch, uint32_t dpa, uint32_t dpitch,
                      uint32_t row, uint32_t rows, HleLandDone done, uint32_t token);
bool hle_tl_land_note(uint64_t value, HleLandDone done, uint32_t token);
/* Guest memory [pa, pa + len) was freed (any thread): the landings queued
 * into it are dropped (their done hears "not landed"). */
void hle_tl_land_freed(uint32_t pa, uint32_t len);

typedef struct HleBackStats {
    uint64_t cmds, clears, passes, submits, swaps, readbacks, draws;
    uint64_t ring_hw;
    uint64_t readback_skips;        /* READBACKs not performed (format/zero-copy) */
    uint64_t copies, copy_skips;    /* CopyRects performed / not (vk/back.c cmd_copy) */
    uint64_t uploads, upload_skips; /* guest texels into surfaces / not (cmd_upload) */
    uint64_t zreads, zread_skips;   /* depth buffers read as colour / not (vk/zread.c) */
    uint64_t nopass;                /* draws dropped: neither target nor depth buffer usable */
    uint32_t last_color, last_rt;   /* last colour clear of the presented surface (M2) */
} HleBackStats;
void hle_back_draw_diag(void);      /* logs the draw-feature counters */

/* Frames the render thread finished in this process (its swaps; after a
 * savestate's load, the frames since it). Any thread. */
uint64_t hle_back_swap_count(void);

#endif
