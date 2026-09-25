/**
 * PSVitaman - Spotify Web API Client Implementation
 */

#include "spotify.h"
#include "utils.h"
#include "cJSON.h"
#include "logger.h"
#include "error.h"
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
    /*
     * On PS Vita, VitaSDK's precompiled libcurl was built against OpenSSL 1.0 headers
     * while the toolchain links OpenSSL 1.1. When CURLOPT_SSL_VERIFYPEER is enabled,
     * Curl_ssl_setup_x509_store attempts to cache and traverse the X509_STORE using OpenSSL 1.0
     * struct offsets (store->objs), reading garbage and calling sk_pop_free(), which corrupts
     * the dlmalloc heap bins and leads to Data Abort crashes (C2-12828-1).
     *
     * Disabling peer verification bypasses Curl_ssl_setup_x509_store entirely, preserving
     * full TLS transport encryption while preventing the OpenSSL 1.0/1.1 ABI crash.
     */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
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
    if (!client_id || !refresh_token || !access_token_out)
        return false;
    if (strlen(client_id) == 0 || strlen(refresh_token) == 0)
        return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    MemoryBuffer chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    char *escaped_token = curl_easy_escape(curl, refresh_token, 0);
    char post_fields[1024];

    /* If client_secret is provided, use Basic Auth header; otherwise PKCE mode */
    if (client_secret && strlen(client_secret) > 0 && strstr(client_secret, "YOUR_") == NULL) {
        char creds[300];
        snprintf(creds, sizeof(creds), "%s:%s", client_id, client_secret);
        char b64_creds[512];
        utils_base64_encode((const unsigned char *)creds, strlen(creds), b64_creds, sizeof(b64_creds));

        char auth_header[600];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Basic %s", b64_creds);
        headers = curl_slist_append(headers, auth_header);

        snprintf(post_fields, sizeof(post_fields), "grant_type=refresh_token&refresh_token=%s", escaped_token);
    } else {
        char *escaped_id = curl_easy_escape(curl, client_id, 0);
        snprintf(post_fields, sizeof(post_fields), "grant_type=refresh_token&refresh_token=%s&client_id=%s",
                 escaped_token, escaped_id);
        curl_free(escaped_id);
    }
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
                    *expires_in_out = (exp && cJSON_IsNumber(exp)) ? exp->valueint : 3600;
                }
                success = true;
                LOG_INFO("Spotify token refreshed successfully, valid for %d s", expires_in_out ? *expires_in_out : 3600);
            }
            cJSON_Delete(json);
        }
    } else {
        if (res != CURLE_OK) {
            LOG_ERROR("Spotify token refresh curl error: %s", curl_easy_strerror(res));
            error_set(APP_ERR_SPOTIFY_TIMEOUT, "Spotify Network Timeout",
                      "Could not connect to Spotify auth server. Check your Wi-Fi.",
                      "Press [X] to dismiss");
        } else {
            LOG_ERROR("Spotify token refresh failed: HTTP %ld", http_code);
            if (http_code == 400 || http_code == 401) {
                error_set(APP_ERR_SPOTIFY_AUTH, "Spotify Auth Expired",
                          "Your Spotify authorization token is invalid or expired. Please re-pair your account.",
                          "Press [SELECT] to pair phone | [X] Dismiss");
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
                state->progress_ms = (progress && cJSON_IsNumber(progress)) ? progress->valueint : 0;

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
                    if (vol && cJSON_IsNumber(vol)) state->volume_percent = vol->valueint;
                }

                cJSON *item = cJSON_GetObjectItem(json, "item");
                if (item) {
                    cJSON *name = cJSON_GetObjectItem(item, "name");
                    if (name && cJSON_IsString(name)) {
                        utils_safe_strncpy(state->track_name, name->valuestring, sizeof(state->track_name));
                    }

                    cJSON *duration = cJSON_GetObjectItem(item, "duration_ms");
                    if (duration && cJSON_IsNumber(duration)) state->duration_ms = duration->valueint;

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
            LOG_WARN("Spotify API 401 Unauthorized - token expired");
            error_set(APP_ERR_SPOTIFY_AUTH, "Spotify Auth Expired",
                      "Authorization token expired or invalid. Please re-pair your account.",
                      "Press [SELECT] to pair phone | [X] Dismiss");
        } else if (http_code == 403) {
            LOG_ERROR("Spotify API 403 Forbidden - Spotify Premium required");
            error_set(APP_ERR_SPOTIFY_PREMIUM, "Spotify Premium Required",
                      "Spotify Web API restricts player control to Premium subscribers.",
                      "Press [X] to dismiss");
        } else if (http_code == 429) {
            LOG_WARN("Spotify API 429 Rate Limit exceeded");
            error_set(APP_ERR_SPOTIFY_RATE_LIMIT, "API Rate Limit Exceeded",
                      "Spotify is temporarily throttling requests. Auto-recovering shortly.",
                      "Press [X] to dismiss");
        }
    } else {
        state->network_error = true;
        utils_safe_strncpy(state->error_message, curl_easy_strerror(res), sizeof(state->error_message));
        LOG_WARN("Spotify get_playback failed: %s", state->error_message);
        error_set(APP_ERR_SPOTIFY_TIMEOUT, "Spotify Network Timeout",
                  "Failed to reach Spotify API. Please verify your Wi-Fi connection.",
                  "Press [X] to dismiss");
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

    if (res == CURLE_OK) {
        if (http_code >= 200 && http_code < 300) {
            return true;
        } else if (http_code == 404) {
            LOG_WARN("Spotify command (%s %s) failed: HTTP 404 No Active Device", method, url);
            error_set(APP_ERR_SPOTIFY_NO_DEVICE, "No Active Spotify Device",
                      "Spotify did not find an active playback device. Start music on phone, PC, or speaker first.",
                      "Press [X] to dismiss");
        } else if (http_code == 403) {
            LOG_ERROR("Spotify command (%s %s) failed: HTTP 403 Premium Required", method, url);
            error_set(APP_ERR_SPOTIFY_PREMIUM, "Spotify Premium Required",
                      "Spotify Web API restricts player control to Premium subscribers.",
                      "Press [X] to dismiss");
        } else {
            LOG_WARN("Spotify command (%s %s) returned HTTP %ld", method, url, http_code);
        }
    } else {
        LOG_ERROR("Spotify command (%s %s) curl error: %s", method, url, curl_easy_strerror(res));
    }

    return false;
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
