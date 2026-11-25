/*
 * xbased native core — the debug switches (debug.xbased.*), for dev builds
 * and investigations: props.ini beside the executable (name=value lines, '#'
 * comments, read again when it changes), values set while running over the
 * file's, else the environment, the name upper-cased with '.' as '_'
 * (debug.xbased.perf: DEBUG_XBASED_PERF). Any thread.
 */
#ifndef NATIVE_CORE_PROPS_H
#define NATIVE_CORE_PROPS_H

#define NC_PROP_NAME_MAX 32
#define NC_PROP_VALUE_MAX 92

#ifdef __cplusplus
extern "C" {
#endif

/* name's value in value (NC_PROP_VALUE_MAX bytes; empty when unset), its length. */
int nc_prop_get(const char *name, char *value);
/* A value set while running, over props.ini's (NULL: unset). */
void nc_prop_set(const char *name, const char *value);

#ifdef __cplusplus
}
#endif

#endif
