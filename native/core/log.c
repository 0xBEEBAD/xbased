/*
 * xbased native core — logging (stderr, the log file).
 */
#include "native/core/nc.h"

#include <stdio.h>
#include <unistd.h>

void nc_log(const char *tag, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s: %s\n", tag, buf);
}

void nc_dump_line(int fd, const char *line, size_t len)
{
    if (fd >= 0) {
        write(fd, line, len);
        return;
    }
    char tag[64] = "xbased-native";
    const char *colon = memchr(line, ':', len);
    if (colon && (size_t)(colon - line) < sizeof(tag)) {
        memcpy(tag, line, (size_t)(colon - line));
        tag[colon - line] = '\0';
        len -= (size_t)(colon - line) + 1;
        line = colon + 1;
    }
    while (len && *line == ' ') {
        line++;
        len--;
    }
    while (len && line[len - 1] == '\n') {
        len--;
    }
    nc_log(tag, "%.*s", (int)len, line);
}
