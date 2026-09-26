/**
 * PSVitaman - Application Error Handling Subsystem
 */

#ifndef PSVITAMAN_ERROR_H
#define PSVITAMAN_ERROR_H

#include <stdbool.h>

typedef enum {
    APP_ERR_NONE               = 0x0000,
    APP_ERR_WIFI_DISCONNECTED  = 0x1001,
    APP_ERR_SPOTIFY_AUTH       = 0x1002,
    APP_ERR_SPOTIFY_PREMIUM    = 0x1003,
    APP_ERR_SPOTIFY_NO_DEVICE  = 0x1004,
    APP_ERR_SPOTIFY_RATE_LIMIT = 0x1005,
    APP_ERR_SPOTIFY_TIMEOUT    = 0x1006,
    APP_ERR_HTTP_SERVER_FAIL   = 0x1007,
    APP_ERR_GENERIC_EXCEPTION  = 0x1008
} AppErrorCode;

typedef struct {
    AppErrorCode code;
    char title[64];
    char message[256];
    char action_hint[128];
    bool is_active;
} AppError;

/* Initialize error subsystem */
void error_init(void);

/* Free error subsystem resources */
void error_cleanup(void);

/* Set an active application error */
void error_set(AppErrorCode code, const char *title, const char *message, const char *action_hint);

/* Clear active error state */
void error_clear(void);

/* Check if an error modal should be displayed */
bool error_is_active(void);

/* Get snapshot of current active error */
void error_get(AppError *out_error);

/* Get hex string for error code (e.g., "0x1002") */
const char *error_code_to_str(AppErrorCode code);

#endif /* PSVITAMAN_ERROR_H */
