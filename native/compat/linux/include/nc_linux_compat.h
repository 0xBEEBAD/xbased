/*
 * xbased on Linux: what an older glibc doesn't declare, for the release built
 * on one (compat/linux/linux.c defines them there). Every core source
 * includes it (cmake/linux.cmake nc_linux_compat).
 */
#ifndef NC_LINUX_COMPAT_H
#define NC_LINUX_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GLIBC__) && !__GLIBC_PREREQ(2, 36)
#ifdef __cplusplus
extern "C" {
#endif
void arc4random_buf(void *buf, size_t n);
uint32_t arc4random(void);
uint32_t arc4random_uniform(uint32_t bound);
#ifdef __cplusplus
}
#endif
#endif

#if defined(__GLIBC__) && !__GLIBC_PREREQ(2, 38)
#ifdef __cplusplus
extern "C" {
#endif
size_t strlcpy(char *dst, const char *src, size_t n);
#ifdef __cplusplus
}
#endif
#endif

#endif
