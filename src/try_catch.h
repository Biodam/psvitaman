/**
 * PSVitaman - C Exception Handling (Try / Catch / Throw)
 * 
 * Provides structured exception handling for C on PS Vita using setjmp/longjmp,
 * preventing unhandled thread crashes and capturing granular diagnostic context.
 */

#ifndef PSVITAMAN_TRY_CATCH_H
#define PSVITAMAN_TRY_CATCH_H

#include <setjmp.h>
#include <stdio.h>
#include "logger.h"

typedef struct {
    jmp_buf env;
    int code;
    const char *file;
    int line;
    char message[256];
} ExceptionFrame;

#define TRY(frame) \
    if (((frame).code = setjmp((frame).env)) == 0)

#define CATCH(frame) \
    else

#define THROW(frame, err_code, fmt, ...) do { \
    (frame).code = (err_code); \
    (frame).file = __FILE__; \
    (frame).line = __LINE__; \
    snprintf((frame).message, sizeof((frame).message), fmt, ##__VA_ARGS__); \
    LOG_ERROR("EXCEPTION THROWN at %s:%d (code 0x%04x): %s", __FILE__, __LINE__, (unsigned int)(err_code), (frame).message); \
    longjmp((frame).env, (err_code) ? (err_code) : 1); \
} while (0)

#endif /* PSVITAMAN_TRY_CATCH_H */
