/**
 * PSVitaman - Lightweight INI parser implementation
 */

#include "ini.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_SECTION 64
#define MAX_NAME 64

/* Strip whitespace chars from beginning of given string, in place. */
static char* lskip(const char* s) {
    while (*s && isspace((unsigned char)(*s)))
        s++;
    return (char*)s;
}

/* Strip whitespace chars from end of given string, in place. */
static char* rstrip(char* s) {
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';
    return s;
}

/* Return pointer to first char (of needle) in haystack, or NULL. */
static char* find_chars_or_comment(const char* s, const char* chars) {
    int was_space = 0;
    while (*s && (!chars || !strchr(chars, *s))) {
        if (*s == ';' || (was_space && *s == '#'))
            return (char*)s;
        was_space = isspace((unsigned char)(*s));
        s++;
    }
    return (char*)s;
}

/* Custom strncpy that always null-terminates */
static char* strncpy0(char* dest, const char* src, size_t size) {
    if (size > 0) {
        size_t len = strlen(src);
        if (len >= size)
            len = size - 1;
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return dest;
}

int ini_parse_stream(char* (*reader)(char* str, int num, void* stream), void* stream,
                     ini_handler handler, void* user) {
    char line[MAX_LINE];
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

    while (reader(line, MAX_LINE, stream) != NULL) {
        lineno++;
        start = line;
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#' || *start == '\0') {
            /* Comment or blank line */
            continue;
        } else if (*start == '[') {
            /* Section header */
            end = find_chars_or_comment(start + 1, "]");
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            } else if (!error) {
                error = lineno;
            }
        } else {
            /* Key = Value pair */
            end = find_chars_or_comment(start, "=:");
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = end + 1;
                end = find_chars_or_comment(value, NULL);
                if (*end == ';' || *end == '#')
                    *end = '\0';
                value = lskip(rstrip(value));
                strncpy0(prev_name, name, sizeof(prev_name));

                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            } else if (!error) {
                error = lineno;
            }
        }

        if (error)
            break;
    }

    return error;
}

static char* file_reader(char* str, int num, void* stream) {
    return fgets(str, num, (FILE*)stream);
}

int ini_parse(const char* filename, ini_handler handler, void* user) {
    FILE* file = fopen(filename, "r");
    int error;

    if (!file)
        return -1;

    error = ini_parse_stream(file_reader, file, handler, user);
    fclose(file);
    return error;
}

typedef struct {
    const char* ptr;
    size_t num_left;
} ini_string_reader_ctx;

static char* string_reader(char* str, int num, void* stream) {
    ini_string_reader_ctx* ctx = (ini_string_reader_ctx*)stream;
    const char* ctx_ptr = ctx->ptr;
    size_t ctx_num_left = ctx->num_left;
    char* dst = str;

    if (ctx_num_left == 0 || num < 2)
        return NULL;

    while (num > 1 && ctx_num_left != 0) {
        char c = *ctx_ptr++;
        ctx_num_left--;
        *dst++ = c;
        num--;
        if (c == '\n')
            break;
    }

    *dst = '\0';
    ctx->ptr = ctx_ptr;
    ctx->num_left = ctx_num_left;
    return str;
}

int ini_parse_string(const char* string, ini_handler handler, void* user) {
    ini_string_reader_ctx ctx;
    ctx.ptr = string;
    ctx.num_left = strlen(string);
    return ini_parse_stream(string_reader, &ctx, handler, user);
}
