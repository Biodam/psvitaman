/**
 * PSVitaman - Persistent File & Debug Logger
 * Writes timestamped diagnostic logs to ux0:data/psvitaman/psvitaman.log
 */

#ifndef PSVITAMAN_LOGGER_H
#define PSVITAMAN_LOGGER_H

#include <stdbool.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

/* Initialize the logger subsystem, ensuring ux0:data/psvitaman exists */
bool log_init(const char *override_path);

/* Flush and close log file handle */
void log_close(void);

/* Write a formatted log message with timestamp, level, and source location */
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

/* Get current log file path */
const char *log_get_path(void);

#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* PSVITAMAN_LOGGER_H */
