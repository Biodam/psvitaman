/**
 * PSVitaman - Lightweight INI parser
 */

#ifndef PSVITAMAN_INI_H
#define PSVITAMAN_INI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ini_handler)(void* user, const char* section, const char* name, const char* value);

/* Parse given INI-style file. Return 0 on success, line number of first error on parse error, or -1 on file error. */
int ini_parse(const char* filename, ini_handler handler, void* user);

/* Parse given INI-style string. Same return values as ini_parse. */
int ini_parse_string(const char* string, ini_handler handler, void* user);

#ifdef __cplusplus
}
#endif

#endif /* PSVITAMAN_INI_H */
