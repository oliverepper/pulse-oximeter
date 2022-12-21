//
//  log.h
//  CMS50F
//
//  Created by Oliver Epper on 18.12.22.
//

#ifndef log_h
#define log_h

#include <stdio.h>

#define LOG_ERROR(format, ...) \
    do { report(stderr, __FILE_NAME__, __FUNCTION__, __LINE__, format, __VA_ARGS__); } while (0)

#ifdef DEBUG
#define LOG_DEBUG(format, ...) \
    do { report(stdout, __FILE_NAME__, __FUNCTION__, __LINE__, format, __VA_ARGS__); } while (0)
#else
#define LOG_DEBUG(format, ...) \
    do {} while (0)
#endif

void report(FILE *stream, const char *filename, const char *function, int line, const char *format, ...);
void dump_buffer(FILE *stream, const char *filename, const char *function, int line, const char *format, unsigned char *buffer, size_t length);


#endif /* log_h */
