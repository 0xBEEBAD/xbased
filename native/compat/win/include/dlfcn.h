/* xbased on Windows: dladdr on GetModuleHandleEx (compat/win/win.c). */
#ifndef NC_WIN_DLFCN_H
#define NC_WIN_DLFCN_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;
int dladdr(const void *addr, Dl_info *info);
#ifdef __cplusplus
}
#endif
#endif
