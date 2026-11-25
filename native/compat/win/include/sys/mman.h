/*
 * xbased on Windows: mmap for the core's own anonymous and file-backed
 * memory (guest memory itself is core/vm_win.c's). compat/win/win.c.
 */
#ifndef NC_WIN_MMAN_H
#define NC_WIN_MMAN_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE  0x8000
#define MAP_FAILED ((void *)-1)

void *nc_mmap(void *addr, size_t len, int prot, int flags, int fd, int64_t off);
int nc_munmap(void *addr, size_t len);
int nc_mprotect(void *addr, size_t len, int prot);
#define mmap nc_mmap
#define munmap nc_munmap
#define mprotect nc_mprotect

#ifdef __cplusplus
}
#endif
#endif
