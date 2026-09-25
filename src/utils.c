/**
 * PSVitaman - Utilities & Helpers Implementation
 */

#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool utils_base64_encode(const unsigned char *input, size_t input_len, char *output, size_t max_out) {
    if (!input || !output) return false;

    size_t output_len = 4 * ((input_len + 2) / 3);
    if (output_len + 1 > max_out) return false;

    size_t i = 0, j = 0;
    while (i < input_len) {
        size_t rem = input_len - i;
        uint32_t octet_a = input[i++];
        uint32_t octet_b = (rem > 1) ? input[i++] : 0;
        uint32_t octet_c = (rem > 2) ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = (rem > 1) ? base64_table[(triple >> 6) & 0x3F] : '=';
        output[j++] = (rem > 2) ? base64_table[triple & 0x3F] : '=';
    }

    output[output_len] = '\0';
    return true;
}

void utils_format_time_ms(int ms, char *output, size_t max_out) {
    if (!output || max_out < 6) return;

    if (ms < 0) ms = 0;
    int total_sec = ms / 1000;
    int sec = total_sec % 60;
    int min = (total_sec / 60) % 60;
    int hr = total_sec / 3600;

    if (hr > 0) {
        snprintf(output, max_out, "%02d:%02d:%02d", hr, min, sec);
    } else {
        snprintf(output, max_out, "%02d:%02d", min, sec);
    }
}

void utils_safe_strncpy(char *dest, const char *src, size_t max_len) {
    if (!dest || max_len == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < max_len; ) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 32 && c <= 126) {
            /* Standard safe ASCII printable characters */
            dest[j++] = (char)c;
            i++;
        } else if (c == '\t') {
            dest[j++] = ' ';
            i++;
        } else if (c >= 0xC0 && c <= 0xDF) {
            /* 2-byte UTF-8 sequence */
            i += 1;
            if (src[i] != '\0') i++;
            dest[j++] = '?';
        } else if (c >= 0xE0 && c <= 0xEF) {
            /* 3-byte UTF-8 sequence */
            i += 1;
            if (src[i] != '\0') i++;
            if (src[i] != '\0') i++;
            dest[j++] = ' ';
        } else if (c >= 0xF0) {
            /* 4-byte UTF-8 sequence (emojis) */
            i += 1;
            if (src[i] != '\0') i++;
            if (src[i] != '\0') i++;
            if (src[i] != '\0') i++;
            dest[j++] = ' ';
        } else {
            /* Control characters < 32 or stray bytes */
            i++;
        }
    }
    dest[j] = '\0';
}
