/*
 * xbased on Linux: the thread helpers the Windows build's compat layer has
 * (nc_win_thread_name, _priority), and what an older glibc lacks
 * (arc4random: 2.36, strlcpy: 2.38) for the release built on one.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/random.h>

#if !__GLIBC_PREREQ(2, 36)
void arc4random_buf(void *buf, size_t n)
{
    uint8_t *p = buf;
    while (n) {
        const ssize_t got = getrandom(p, n, 0);
        if (got > 0) {
            p += got;
            n -= (size_t)got;
        }
    }
}

uint32_t arc4random(void)
{
    uint32_t v;
    arc4random_buf(&v, sizeof(v));
    return v;
}

uint32_t arc4random_uniform(uint32_t bound)
{
    if (bound < 2) {
        return 0;
    }
    /* the values below 2^32 mod bound would come up once more often */
    const uint32_t min = -bound % bound;
    for (;;) {
        const uint32_t r = arc4random();
        if (r >= min) {
            return r % bound;
        }
    }
}
#endif

#if !__GLIBC_PREREQ(2, 38)
size_t strlcpy(char *dst, const char *src, size_t n)
{
    const size_t len = strlen(src);
    if (n) {
        const size_t k = len < n - 1 ? len : n - 1;
        memcpy(dst, src, k);
        dst[k] = 0;
    }
    return len;
}
#endif

void nc_win_thread_name(const char *name)
{
    char n[16];
    snprintf(n, sizeof(n), "%s", name);
    pthread_setname_np(pthread_self(), n);
}

void nc_win_thread_priority(int level)
{
    (void)level;    /* raising it needs privileges on Linux; the scheduler's default */
}
