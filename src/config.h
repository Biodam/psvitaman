/**
 * PSVitaman - Configuration Manager
 */

#ifndef PSVITAMAN_CONFIG_H
#define PSVITAMAN_CONFIG_H

#include <stdbool.h>

#define CONFIG_DIR_PATH   "ux0:data/psvitaman"
#define CONFIG_FILE_PATH  "ux0:data/psvitaman/config.ini"

typedef struct {
    char client_id[128];
    char client_secret[128];
    char refresh_token[512];
    bool is_valid;
    bool template_created;
} AppConfig;

/* Load configuration from ux0:data/psvitaman/config.ini.
 * If file does not exist, creates template and sets template_created = true. */
bool config_load(AppConfig *config);

/* Create template config.ini with instructions */
bool config_create_template(const char *filepath);

#endif /* PSVITAMAN_CONFIG_H */
