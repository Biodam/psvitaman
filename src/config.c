/**
 * PSVitaman - Configuration Manager Implementation
 */

#include "config.h"
#include "ini.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/io/stat.h>
#endif

static int ini_config_handler(void* user, const char* section, const char* name, const char* value) {
    AppConfig* config = (AppConfig*)user;

    if (strcmp(section, "spotify") == 0) {
        if (strcmp(name, "client_id") == 0) {
            strncpy(config->client_id, value, sizeof(config->client_id) - 1);
            config->client_id[sizeof(config->client_id) - 1] = '\0';
        } else if (strcmp(name, "client_secret") == 0) {
            strncpy(config->client_secret, value, sizeof(config->client_secret) - 1);
            config->client_secret[sizeof(config->client_secret) - 1] = '\0';
        } else if (strcmp(name, "refresh_token") == 0) {
            strncpy(config->refresh_token, value, sizeof(config->refresh_token) - 1);
            config->refresh_token[sizeof(config->refresh_token) - 1] = '\0';
        }
    }
    return 1;
}

bool config_create_template(const char *filepath) {
#if defined(__psp2__) || defined(__VITA__)
    sceIoMkdir(CONFIG_DIR_PATH, 0777);
#else
    #ifdef _WIN32
    _mkdir(CONFIG_DIR_PATH);
    #else
    mkdir(CONFIG_DIR_PATH, 0777);
    #endif
#endif

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "# PSVitaman - Spotify Remote Configuration\n");
    fprintf(f, "# Set up your Spotify Developer App credentials below.\n");
    fprintf(f, "# See README.md or run 'python tools/get_token.py' on PC for setup.\n\n");
    fprintf(f, "[spotify]\n");
    if (strlen(DEFAULT_SPOTIFY_CLIENT_ID) > 0) {
        fprintf(f, "client_id = %s\n", DEFAULT_SPOTIFY_CLIENT_ID);
    } else {
        fprintf(f, "client_id = YOUR_SPOTIFY_CLIENT_ID\n");
    }
    if (strlen(DEFAULT_SPOTIFY_CLIENT_SECRET) > 0) {
        fprintf(f, "client_secret = %s\n", DEFAULT_SPOTIFY_CLIENT_SECRET);
    } else {
        fprintf(f, "client_secret = YOUR_SPOTIFY_CLIENT_SECRET\n");
    }
    if (strlen(DEFAULT_SPOTIFY_REFRESH_TOKEN) > 0) {
        fprintf(f, "refresh_token = %s\n", DEFAULT_SPOTIFY_REFRESH_TOKEN);
    } else {
        fprintf(f, "refresh_token = YOUR_INITIAL_REFRESH_TOKEN\n");
    }

    fclose(f);
    return true;
}

bool config_save(const AppConfig *config) {
    if (!config) return false;

#if defined(__psp2__) || defined(__VITA__)
    sceIoMkdir(CONFIG_DIR_PATH, 0777);
#else
    #ifdef _WIN32
    _mkdir(CONFIG_DIR_PATH);
    #else
    mkdir(CONFIG_DIR_PATH, 0777);
    #endif
#endif

    FILE *f = fopen(CONFIG_FILE_PATH, "w");
    if (!f) return false;

    fprintf(f, "# PSVitaman - Spotify Remote Configuration\n");
    fprintf(f, "# Generated automatically via QR Phone Pairing\n\n");
    fprintf(f, "[spotify]\n");
    fprintf(f, "client_id = %s\n", config->client_id);
    fprintf(f, "client_secret = %s\n", config->client_secret);
    fprintf(f, "refresh_token = %s\n", config->refresh_token);

    fclose(f);
    return true;
}

bool config_load(AppConfig *config) {
    if (!config) return false;

    memset(config, 0, sizeof(AppConfig));

    /* Pre-populate compiled-in defaults if defined at build time */
    if (strlen(DEFAULT_SPOTIFY_CLIENT_ID) > 0) {
        strncpy(config->client_id, DEFAULT_SPOTIFY_CLIENT_ID, sizeof(config->client_id) - 1);
    }
    if (strlen(DEFAULT_SPOTIFY_CLIENT_SECRET) > 0) {
        strncpy(config->client_secret, DEFAULT_SPOTIFY_CLIENT_SECRET, sizeof(config->client_secret) - 1);
    }
    if (strlen(DEFAULT_SPOTIFY_REFRESH_TOKEN) > 0) {
        strncpy(config->refresh_token, DEFAULT_SPOTIFY_REFRESH_TOKEN, sizeof(config->refresh_token) - 1);
    }

    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        /* Config file does not exist; create template with pre-filled defaults */
        config_create_template(CONFIG_FILE_PATH);
        config->template_created = true;

        /* If complete credentials were baked into the build, validate immediately */
        if (strlen(config->client_id) > 0 &&
            strlen(config->refresh_token) > 0 &&
            strstr(config->client_id, "YOUR_") == NULL &&
            strstr(config->refresh_token, "YOUR_") == NULL) {
            config->is_valid = true;
            return true;
        }

        config->is_valid = false;
        return false;
    }
    fclose(f);

    if (ini_parse(CONFIG_FILE_PATH, ini_config_handler, config) < 0) {
        config->is_valid = false;
        return false;
    }

    /* Fallback to compiled defaults if fields are empty or placeholders in config.ini */
    if ((strlen(config->client_id) == 0 || strstr(config->client_id, "YOUR_") != NULL) &&
        strlen(DEFAULT_SPOTIFY_CLIENT_ID) > 0) {
        strncpy(config->client_id, DEFAULT_SPOTIFY_CLIENT_ID, sizeof(config->client_id) - 1);
    }
    if ((strlen(config->client_secret) == 0 || strstr(config->client_secret, "YOUR_") != NULL) &&
        strlen(DEFAULT_SPOTIFY_CLIENT_SECRET) > 0) {
        strncpy(config->client_secret, DEFAULT_SPOTIFY_CLIENT_SECRET, sizeof(config->client_secret) - 1);
    }
    if ((strlen(config->refresh_token) == 0 || strstr(config->refresh_token, "YOUR_") != NULL) &&
        strlen(DEFAULT_SPOTIFY_REFRESH_TOKEN) > 0) {
        strncpy(config->refresh_token, DEFAULT_SPOTIFY_REFRESH_TOKEN, sizeof(config->refresh_token) - 1);
    }

    /* Check if required credentials are valid */
    if (strlen(config->client_id) > 0 &&
        strlen(config->refresh_token) > 0 &&
        strstr(config->client_id, "YOUR_") == NULL &&
        strstr(config->refresh_token, "YOUR_") == NULL) {
        config->is_valid = true;
        return true;
    }

    config->is_valid = false;
    return false;
}
