/**
 * PSVitaman - Spotify Web API Client Implementation
 */

#include "spotify.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define CACERT_PATH "app0:assets/cacert.pem"

typedef struct {
    char *data;
    size_t size;
} MemoryBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer *mem = (MemoryBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0; /* out of memory */
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

static void configure_curl_ssl(CURL *curl) {
    /* Set CA bundle if available */
    FILE *f = fopen(CACERT_PATH, "r");
    if (f) {
        fclose(f);
        curl_easy_setopt(curl, CURLOPT_CAINFO, CACERT_PATH);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        /* Fallback for environments where bundle is not present */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PSVitaman/1.0 (PSVita; ARM)");
}

bool spotify_init(void) {
    /* curl_global_init is called in main.c, but verify here */
    return true;
}

void spotify_cleanup(void) {
    /* Cleanup any global state if needed */
}

bool spotify_refresh_token(const char *client_id, const char *client_secret,
                          const char *refresh_token, char *access_token_out,
                          size_t token_max, int *expires_in_out) {
    if (!client_id || !client_secret || !refresh_token || !access_token_out)
        return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    MemoryBuffer chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;

    /* Build Basic Auth Header */
    char creds[300];
    snprintf(creds, sizeof(creds), "%s:%s", client_id, client_secret);
    char b64_creds[512];
    utils_base64_encode((const unsigned char *)creds, strlen(creds), b64_creds, sizeof(b64_creds));

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Basic %s", b64_creds);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    /* Build Post Data */
    char *escaped_token = curl_easy_escape(curl, refresh_token, 0);
    char post_fields[1024];
    snprintf(post_fields, sizeof(post_fields), "grant_type=refresh_token&refresh_token=%s", escaped_token);
    curl_free(escaped_token);

    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    configure_curl_ssl(curl);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    bool success = false;
    if (res == CURLE_OK && http_code == 200 && chunk.data) {
        cJSON *json = cJSON_Parse(chunk.data);
        if (json) {
            cJSON *tok = cJSON_GetObjectItem(json, "access_token");
            cJSON *exp = cJSON_GetObjectItem(json, "expires_in");
            if (tok && cJSON_IsString(tok)) {
                utils_safe_strncpy(access_token_out, tok->valuestring, token_max);
                if (expires_in_out) {
                    *expires_in_out = exp ? exp->valueint : 3600;
                }
                success = true;
            }
            cJSON_Delete(json);
        }
    } else if (res != CURLE_OK) {
        /* Retry with SSL verify disabled if certificate verification failed */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (res == CURLE_OK && http_code == 200 && chunk.data) {
            cJSON *json = cJSON_Parse(chunk.data);
            if (json) {
                cJSON *tok = cJSON_GetObjectItem(json, "access_token");
                cJSON *exp = cJSON_GetObjectItem(json, "expires_in");
                if (tok && cJSON_IsString(tok)) {
                    utils_safe_strncpy(access_token_out, tok->valuestring, token_max);
                    if (expires_in_out) {
                        *expires_in_out = exp ? exp->valueint : 3600;
                    }
                    success = true;
                }
                cJSON_Delete(json);
            }
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (chunk.data) free(chunk.data);

    return success;
}

bool spotify_get_playback(const char *access_token, SpotifyPlaybackState *state) {
    if (!access_token || !state) return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    MemoryBuffer chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.spotify.com/v1/me/player");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    configure_curl_ssl(curl);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    bool success = false;

    if (res == CURLE_OK) {
        state->network_error = false;
        if (http_code == 204) {
            /* Active playback idle / paused on all devices */
            state->is_active = false;
            state->is_playing = false;
            success = true;
        } else if (http_code == 200 && chunk.data) {
            cJSON *json = cJSON_Parse(chunk.data);
            if (json) {
                state->is_active = true;

                cJSON *playing = cJSON_GetObjectItem(json, "is_playing");
                state->is_playing = playing ? cJSON_IsTrue(playing) : false;

                cJSON *progress = cJSON_GetObjectItem(json, "progress_ms");
                state->progress_ms = progress ? progress->valueint : 0;

                cJSON *shuffle = cJSON_GetObjectItem(json, "shuffle_state");
                state->shuffle_state = shuffle ? cJSON_IsTrue(shuffle) : false;

                cJSON *repeat = cJSON_GetObjectItem(json, "repeat_state");
                if (repeat && cJSON_IsString(repeat)) {
                    if (strcmp(repeat->valuestring, "track") == 0)
                        state->repeat_state = REPEAT_TRACK;
                    else if (strcmp(repeat->valuestring, "context") == 0)
                        state->repeat_state = REPEAT_CONTEXT;
                    else
                        state->repeat_state = REPEAT_OFF;
                }

                cJSON *device = cJSON_GetObjectItem(json, "device");
                if (device) {
                    cJSON *dev_name = cJSON_GetObjectItem(device, "name");
                    if (dev_name && cJSON_IsString(dev_name)) {
                        utils_safe_strncpy(state->device_name, dev_name->valuestring, sizeof(state->device_name));
                    }
                    cJSON *vol = cJSON_GetObjectItem(device, "volume_percent");
                    if (vol) state->volume_percent = vol->valueint;
                }

                cJSON *item = cJSON_GetObjectItem(json, "item");
                if (item) {
                    cJSON *name = cJSON_GetObjectItem(item, "name");
                    if (name && cJSON_IsString(name)) {
                        utils_safe_strncpy(state->track_name, name->valuestring, sizeof(state->track_name));
                    }

                    cJSON *duration = cJSON_GetObjectItem(item, "duration_ms");
                    if (duration) state->duration_ms = duration->valueint;

                    cJSON *album = cJSON_GetObjectItem(item, "album");
                    if (album) {
                        cJSON *alb_name = cJSON_GetObjectItem(album, "name");
                        if (alb_name && cJSON_IsString(alb_name)) {
                            utils_safe_strncpy(state->album_name, alb_name->valuestring, sizeof(state->album_name));
                        }
                    }

                    cJSON *artists = cJSON_GetObjectItem(item, "artists");
                    if (artists && cJSON_IsArray(artists)) {
                        state->artist_name[0] = '\0';
                        int count = cJSON_GetArraySize(artists);
                        for (int i = 0; i < count && i < 3; i++) {
                            cJSON *art = cJSON_GetArrayItem(artists, i);
                            cJSON *art_name = cJSON_GetObjectItem(art, "name");
                            if (art_name && cJSON_IsString(art_name)) {
                                if (i > 0) strncat(state->artist_name, ", ", sizeof(state->artist_name) - strlen(state->artist_name) - 1);
                                strncat(state->artist_name, art_name->valuestring, sizeof(state->artist_name) - strlen(state->artist_name) - 1);
                            }
                        }
                    }
                }
                cJSON_Delete(json);
                success = true;
            }
        } else if (http_code == 401) {
            state->auth_error = true;
        }
    } else {
        state->network_error = true;
        utils_safe_strncpy(state->error_message, curl_easy_strerror(res), sizeof(state->error_message));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (chunk.data) free(chunk.data);

    return success;
}

static bool spotify_send_rest_cmd(const char *access_token, const char *url, const char *method) {
    if (!access_token || !url) return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Length: 0");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    configure_curl_ssl(curl);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && (http_code >= 200 && http_code < 300));
}

bool spotify_play(const char *access_token) {
    return spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/play", "PUT");
}

bool spotify_pause(const char *access_token) {
    return spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/pause", "PUT");
}

bool spotify_next(const char *access_token) {
    return spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/next", "POST");
}

bool spotify_previous(const char *access_token) {
    return spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/previous", "POST");
}

bool spotify_set_volume(const char *access_token, int volume_percent) {
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;
    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/volume?volume_percent=%d", volume_percent);
    return spotify_send_rest_cmd(access_token, url, "PUT");
}

bool spotify_set_shuffle(const char *access_token, bool state) {
    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/shuffle?state=%s", state ? "true" : "false");
    return spotify_send_rest_cmd(access_token, url, "PUT");
}

bool spotify_set_repeat(const char *access_token, SpotifyRepeatMode mode) {
    const char *state_str = "off";
    if (mode == REPEAT_CONTEXT) state_str = "context";
    else if (mode == REPEAT_TRACK) state_str = "track";

    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/repeat?state=%s", state_str);
    return spotify_send_rest_cmd(access_token, url, "PUT");
}
