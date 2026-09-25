/**
 * PSVitaman - Application Error Handling Subsystem Implementation
 */

#include "error.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/threadmgr.h>
#else
#include <pthread.h>
#endif

static AppError s_current_error = {0};

#if defined(__psp2__) || defined(__VITA__)
static SceUID s_err_mutex = -1;
#else
static pthread_mutex_t s_err_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void lock_err(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_err_mutex >= 0) {
        sceKernelLockMutex(s_err_mutex, 1, NULL);
    }
#else
    pthread_mutex_lock(&s_err_mutex);
#endif
}

static void unlock_err(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_err_mutex >= 0) {
        sceKernelUnlockMutex(s_err_mutex, 1);
    }
#else
    pthread_mutex_unlock(&s_err_mutex);
#endif
}

void error_init(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_err_mutex < 0) {
        s_err_mutex = sceKernelCreateMutex("psvitaman_err_mtx", 0, 0, NULL);
    }
#endif
    lock_err();
    memset(&s_current_error, 0, sizeof(s_current_error));
    unlock_err();
}

void error_cleanup(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_err_mutex >= 0) {
        sceKernelDeleteMutex(s_err_mutex);
        s_err_mutex = -1;
    }
#endif
}

void error_set(AppErrorCode code, const char *title, const char *message, const char *action_hint) {
    lock_err();

    s_current_error.code = code;
    if (title) {
        strncpy(s_current_error.title, title, sizeof(s_current_error.title) - 1);
        s_current_error.title[sizeof(s_current_error.title) - 1] = '\0';
    } else {
        s_current_error.title[0] = '\0';
    }

    if (message) {
        strncpy(s_current_error.message, message, sizeof(s_current_error.message) - 1);
        s_current_error.message[sizeof(s_current_error.message) - 1] = '\0';
    } else {
        s_current_error.message[0] = '\0';
    }

    if (action_hint) {
        strncpy(s_current_error.action_hint, action_hint, sizeof(s_current_error.action_hint) - 1);
        s_current_error.action_hint[sizeof(s_current_error.action_hint) - 1] = '\0';
    } else {
        s_current_error.action_hint[0] = '\0';
    }

    s_current_error.is_active = (code != APP_ERR_NONE);

    unlock_err();

    if (code != APP_ERR_NONE) {
        LOG_ERROR("[0x%04X] %s: %s (%s)", (unsigned int)code,
                  s_current_error.title, s_current_error.message, s_current_error.action_hint);
    }
}

void error_clear(void) {
    lock_err();
    if (s_current_error.is_active) {
        LOG_INFO("Cleared active error [0x%04X]", (unsigned int)s_current_error.code);
        memset(&s_current_error, 0, sizeof(s_current_error));
    }
    unlock_err();
}

bool error_is_active(void) {
    lock_err();
    bool active = s_current_error.is_active;
    unlock_err();
    return active;
}

void error_get(AppError *out_error) {
    if (!out_error) return;
    lock_err();
    *out_error = s_current_error;
    unlock_err();
}

const char *error_code_to_str(AppErrorCode code) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned int)code);
    return buf;
}
