//
//  log.c
//  CMS50F
//
//  Created by Oliver Epper on 18.12.22.
//

#include "log.h"
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <math.h>

#define PREFIX(stream)  do { TIMESTAMP; fprintf(stream, "%s (%s:%s:%d): ", date_and_time, filename, function, line); } while (0)
#define POSTFIX(stream) do { fprintf(stream, "\n"); } while (0)

#define TIMESTAMP       char date_and_time[32] = {0}; do { \
    struct timeval tv; \
    gettimeofday(&tv, NULL); \
    char millis[8] = {0}; \
    strftime(date_and_time, sizeof(date_and_time), "%b-%d %H:%M:%S", localtime(&tv.tv_sec)); \
    snprintf(millis, sizeof(millis), ".%-*.0f", 3, round((float)tv.tv_usec / 1000)); \
    strncat(date_and_time, millis, sizeof(date_and_time) - strlen(date_and_time) - 1); \
} while (0)

void report(FILE *stream, const char *filename, const char *function, int line, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PREFIX(stream);
    vfprintf(stream, format, vargs);
    POSTFIX(stream);
    va_end(vargs);
}

void dump_buffer(FILE *stream, const char *filename, const char *function, int line, const char *format, unsigned char *buffer, size_t length)
{
    PREFIX(stream);
    for (int i = 0; i < length - 1; ++i) {
        fprintf(stream, format, buffer[i]);
        fprintf(stream, " ");
    }
    fprintf(stream, format, buffer[length - 1]);
    POSTFIX(stream);
}

