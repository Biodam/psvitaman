/**
 * PSVitaman - Utilities & Helpers
 */

#ifndef PSVITAMAN_UTILS_H
#define PSVITAMAN_UTILS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base64 encode data into output buffer (null-terminated) */
bool utils_base64_encode(const unsigned char *input, size_t input_len, char *output, size_t max_out);

/* Format milliseconds into MM:SS or HH:MM:SS */
void utils_format_time_ms(int ms, char *output, size_t max_out);

/* Safe string copy with guaranteed null-termination */
void utils_safe_strncpy(char *dest, const char *src, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* PSVITAMAN_UTILS_H */
