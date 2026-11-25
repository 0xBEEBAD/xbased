/*
 * xbased HLE GPU — runtime state and the host-facing API.
 */
#ifndef HW_XBOX_HLE_HLE_H
#define HW_XBOX_HLE_HLE_H

#include "hw/xbox/hle/core/xbe.h"

#define HLE_MAX_LIB_RANGES 8

typedef struct HleTitle {
    uint64_t fp;
    HleXbeInfo xbe;
    /* the D3D library's sections (diagnostics) */
    uint32_t lib_lo[HLE_MAX_LIB_RANGES], lib_hi[HLE_MAX_LIB_RANGES];
    unsigned nlib;
} HleTitle;

typedef struct HleRuntime {
    HleTitle title;
} HleRuntime;

extern HleRuntime g_hle;

/* Why the HLE GPU could not start (the Vulkan driver's shortfall), for the
 * user; empty while it can. */
extern char g_hle_unavailable[256];

void hle_init(void);
void hle_log(const char *tag, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

/* Title lifecycle (core/title.c): the XBE mapped now, in push-buffer mode
 * (vCPU thread, before its code runs). */
void hle_title_start(void);
void hle_title_stop(void);
bool hle_title_in_lib(uint32_t va);

#endif
