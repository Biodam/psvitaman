/**
 * PSVitaman - Persistent File & Debug Logger Implementation
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#define DEFAULT_LOG_PATH "ux0:data/psvitaman/psvitaman.log"
#else
#include <pthread.h>
#define DEFAULT_LOG_PATH "./psvitaman.log"
#endif

#define MAX_LOG_SIZE (512 * 1024) /* 512 KB max before rotation */

static FILE *s_log_file = NULL;
static char s_log_path[256] = {0};

#if defined(__psp2__) || defined(__VITA__)
static SceUID s_mutex = -1;
#else
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void lock_log(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_mutex >= 0) {
        sceKernelLockMutex(s_mutex, 1, NULL);
    }
#else
    pthread_mutex_lock(&s_mutex);
#endif
}

static void unlock_log(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_mutex >= 0) {
        sceKernelUnlockMutex(s_mutex, 1);
    }
#else
    pthread_mutex_unlock(&s_mutex);
#endif
}

static void rotate_log_if_needed(void) {
    if (!s_log_file) return;

    long size = ftell(s_log_file);
    if (size >= MAX_LOG_SIZE) {
        fclose(s_log_file);
        s_log_file = NULL;

        char old_path[300];
        snprintf(old_path, sizeof(old_path), "%s.old", s_log_path);
        remove(old_path);
        rename(s_log_path, old_path);

        s_log_file = fopen(s_log_path, "a");
    }
}

bool log_init(const char *override_path) {
    if (s_log_file) return true;

#if defined(__psp2__) || defined(__VITA__)
    if (s_mutex < 0) {
        s_mutex = sceKernelCreateMutex("psvitaman_log_mtx", 0, 0, NULL);
    }
    /* Ensure target directory exists on PS Vita memory card */
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/psvitaman", 0777);
#endif

    if (override_path && strlen(override_path) > 0) {
        strncpy(s_log_path, override_path, sizeof(s_log_path) - 1);
    } else {
        strncpy(s_log_path, DEFAULT_LOG_PATH, sizeof(s_log_path) - 1);
    }
    s_log_path[sizeof(s_log_path) - 1] = '\0';

    s_log_file = fopen(s_log_path, "a");
    if (!s_log_file) {
        /* Fallback to current directory if ux0 is inaccessible */
        strncpy(s_log_path, "./psvitaman.log", sizeof(s_log_path) - 1);
        s_log_file = fopen(s_log_path, "a");
    }

    if (s_log_file) {
        log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "========================================");
        log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "PSVitaman Logger Started - Build " __DATE__ " " __TIME__);
        log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "Target log file: %s", s_log_path);
        log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "========================================");
        return true;
    }

    return false;
}

void log_close(void) {
    lock_log();
    if (s_log_file) {
        log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "PSVitaman Logger Shutting Down");
        fclose(s_log_file);
        s_log_file = NULL;
    }
    unlock_log();

#if defined(__psp2__) || defined(__VITA__)
    if (s_mutex >= 0) {
        sceKernelDeleteMutex(s_mutex);
        s_mutex = -1;
    }
#endif
}

const char *log_get_path(void) {
    return s_log_path;
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    lock_log();

    /* Check file rotation */
    rotate_log_if_needed();

    /* Calendar timestamp & millisecond offset */
    time_t raw_time = time(NULL);
    struct tm *ti = localtime(&raw_time);
    char time_str[32];
    if (ti) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ti);
    } else {
        snprintf(time_str, sizeof(time_str), "1970-01-01 00:00:00");
    }

    int millis = 0;
#if defined(__psp2__) || defined(__VITA__)
    millis = (int)((sceKernelGetProcessTimeWide() / 1000) % 1000);
#endif

    const char *lvl_str = "INFO";
    switch (level) {
        case LOG_LEVEL_DEBUG: lvl_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  lvl_str = "INFO";  break;
        case LOG_LEVEL_WARN:  lvl_str = "WARN";  break;
        case LOG_LEVEL_ERROR: lvl_str = "ERROR"; break;
    }

    /* Strip directory from filename for cleaner logs */
    const char *base_file = file;
    const char *p1 = strrchr(file, '/');
    const char *p2 = strrchr(file, '\\');
    if (p1 && p1 + 1 > base_file) base_file = p1 + 1;
    if (p2 && p2 + 1 > base_file) base_file = p2 + 1;

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    /* Output to stdout / console */
    printf("[%s.%03d] [%s] [%s:%d] %s\n", time_str, millis, lvl_str, base_file, line, msg_buf);

    /* Output to file and flush immediately */
    if (s_log_file) {
        fprintf(s_log_file, "[%s.%03d] [%s] [%s:%d] %s\n", time_str, millis, lvl_str, base_file, line, msg_buf);
        fflush(s_log_file);
    }

    unlock_log();
}
