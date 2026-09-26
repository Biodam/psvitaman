/**
 * PSVitaman - Spotify Web API Client Implementation
 * 
 * Uses PS Vita native SceHttp / SceSsl for 100% reliable hardware-accelerated
 * HTTPS communication without OpenSSL ABI incompatibility or memory leaks.
 */

#include "spotify.h"
#include "utils.h"
#include "cJSON.h"
#include "logger.h"
#include "error.h"
#include "try_catch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/libssl.h>
#else
#include <curl/curl.h>
#endif

typedef enum {
    HTTP_REQ_GET = 0,
    HTTP_REQ_POST = 1,
    HTTP_REQ_PUT = 2
} HttpMethodType;

typedef struct {
    char *data;
    size_t size;
} HttpResponseBuffer;

#if defined(__psp2__) || defined(__VITA__)

static bool do_http_request(const char *url, HttpMethodType method, const char *auth_header,
                           const char *content_type, const void *post_data, size_t post_len,
                           HttpResponseBuffer *out_buf, int *out_http_status) {
    if (!url) return false;
    if (out_http_status) *out_http_status = 0;

    int tmpl = sceHttpCreateTemplate("PSVitaman/1.0 (PSVita; ARM)", SCE_HTTP_VERSION_1_1, SCE_TRUE);
    if (tmpl < 0) {
        LOG_ERROR("sceHttpCreateTemplate failed: 0x%08x", tmpl);
        return false;
    }

    sceHttpSetConnectTimeOut(tmpl, 8 * 1000 * 1000);
    sceHttpSetSendTimeOut(tmpl, 8 * 1000 * 1000);
    sceHttpSetRecvTimeOut(tmpl, 8 * 1000 * 1000);
    sceHttpSetResolveTimeOut(tmpl, 8 * 1000 * 1000);

    int conn = sceHttpCreateConnectionWithURL(tmpl, url, SCE_FALSE);
    if (conn < 0) {
        LOG_ERROR("sceHttpCreateConnectionWithURL failed for %s: 0x%08x", url, conn);
        sceHttpDeleteTemplate(tmpl);
        return false;
    }

    int sce_method = SCE_HTTP_METHOD_GET;
    if (method == HTTP_REQ_POST) sce_method = SCE_HTTP_METHOD_POST;
    else if (method == HTTP_REQ_PUT) sce_method = SCE_HTTP_METHOD_PUT;

    int req = sceHttpCreateRequestWithURL(conn, sce_method, url, post_len);
    if (req < 0) {
        LOG_ERROR("sceHttpCreateRequestWithURL failed for %s: 0x%08x", url, req);
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tmpl);
        return false;
    }

    if (auth_header && strlen(auth_header) > 0) {
        sceHttpAddRequestHeader(req, "Authorization", auth_header, SCE_HTTP_HEADER_ADD);
    }
    if (content_type && strlen(content_type) > 0) {
        sceHttpAddRequestHeader(req, "Content-Type", content_type, SCE_HTTP_HEADER_ADD);
    }

    int send_res = sceHttpSendRequest(req, post_data, (unsigned int)post_len);
    if (send_res < 0) {
        LOG_ERROR("sceHttpSendRequest failed for URL %s: 0x%08x", url, send_res);
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tmpl);
        return false;
    }

    int status = 0;
    sceHttpGetStatusCode(req, &status);
    if (out_http_status) *out_http_status = status;

    if (out_buf) {
        out_buf->data = malloc(1);
        out_buf->size = 0;
        if (out_buf->data) out_buf->data[0] = '\0';

        unsigned char chunk[2048];
        int bytes_read = 0;
        while ((bytes_read = sceHttpReadData(req, chunk, sizeof(chunk) - 1)) > 0) {
            char *new_ptr = realloc(out_buf->data, out_buf->size + bytes_read + 1);
            if (!new_ptr) {
                LOG_ERROR("Out of memory reading HTTP response");
                break;
            }
            out_buf->data = new_ptr;
            memcpy(out_buf->data + out_buf->size, chunk, bytes_read);
            out_buf->size += bytes_read;
            out_buf->data[out_buf->size] = '\0';
        }
    }

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tmpl);
    return true;
}

#else

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HttpResponseBuffer *mem = (HttpResponseBuffer *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

static bool do_http_request(const char *url, HttpMethodType method, const char *auth_header,
                           const char *content_type, const void *post_data, size_t post_len,
                           HttpResponseBuffer *out_buf, int *out_http_status) {
    if (!url) return false;
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist *headers = NULL;
    if (auth_header) {
        char auth_hdr[600];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: %s", auth_header);
        headers = curl_slist_append(headers, auth_hdr);
    }
    if (content_type) {
        char ct_hdr[128];
        snprintf(ct_hdr, sizeof(ct_hdr), "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, ct_hdr);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    if (method == HTTP_REQ_POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (post_data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (const char *)post_data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_len);
        }
    } else if (method == HTTP_REQ_PUT) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (post_data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (const char *)post_data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_len);
        } else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        }
    }

    if (out_buf) {
        out_buf->data = malloc(1);
        out_buf->size = 0;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)out_buf);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (out_http_status) *out_http_status = (int)http_code;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

#endif

bool spotify_init(void) {
    return true;
}

void spotify_cleanup(void) {
}

bool spotify_refresh_token(const char *client_id, const char *client_secret,
                          const char *refresh_token, char *access_token_out,
                          size_t token_max, int *expires_in_out) {
    LOG_INFO("spotify_refresh_token: entered");
    if (!client_id || !refresh_token || !access_token_out) {
        LOG_ERROR("spotify_refresh_token: invalid NULL argument passed");
        return false;
    }
    if (strlen(client_id) == 0 || strlen(refresh_token) == 0) {
        LOG_ERROR("spotify_refresh_token: client_id or refresh_token is empty");
        return false;
    }

    char escaped_token[512] = {0};
    utils_url_encode(refresh_token, escaped_token, sizeof(escaped_token));

    char post_fields[1024];
    char auth_header[600] = {0};

    if (client_secret && strlen(client_secret) > 0 && strstr(client_secret, "YOUR_") == NULL) {
        LOG_INFO("spotify_refresh_token: using Basic Auth mode");
        char creds[300];
        snprintf(creds, sizeof(creds), "%s:%s", client_id, client_secret);
        char b64_creds[512];
        utils_base64_encode((const unsigned char *)creds, strlen(creds), b64_creds, sizeof(b64_creds));
        snprintf(auth_header, sizeof(auth_header), "Basic %s", b64_creds);
        snprintf(post_fields, sizeof(post_fields), "grant_type=refresh_token&refresh_token=%s", escaped_token);
    } else {
        LOG_INFO("spotify_refresh_token: using PKCE direct mode");
        char escaped_id[128] = {0};
        utils_url_encode(client_id, escaped_id, sizeof(escaped_id));
        snprintf(post_fields, sizeof(post_fields), "grant_type=refresh_token&refresh_token=%s&client_id=%s",
                 escaped_token, escaped_id);
    }

    LOG_INFO("spotify_refresh_token: sending POST to accounts.spotify.com/api/token...");
    HttpResponseBuffer resp = {0};
    int http_status = 0;
    bool ok = do_http_request("https://accounts.spotify.com/api/token",
                              HTTP_REQ_POST,
                              auth_header[0] ? auth_header : NULL,
                              "application/x-www-form-urlencoded",
                              post_fields, strlen(post_fields),
                              &resp, &http_status);

    LOG_INFO("spotify_refresh_token: request finished: success=%s, http_code=%d",
             ok ? "true" : "false", http_status);

    bool success = false;
    if (ok && http_status == 200 && resp.data) {
        cJSON *json = cJSON_Parse(resp.data);
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
        if (!ok) {
            LOG_ERROR("Spotify token refresh network error");
            error_set(APP_ERR_SPOTIFY_TIMEOUT, "Spotify Network Timeout",
                      "Could not connect to Spotify auth server. Check your Wi-Fi.",
                      "Press [X] to dismiss");
        } else {
            LOG_ERROR("Spotify token refresh failed: HTTP %d", http_status);
            if (http_status == 400 || http_status == 401) {
                error_set(APP_ERR_SPOTIFY_AUTH, "Spotify Auth Expired",
                          "Your Spotify authorization token is invalid or expired. Please re-pair your account.",
                          "Press [SELECT] to pair phone | [X] Dismiss");
            }
        }
    }

    if (resp.data) free(resp.data);
    return success;
}

static void parse_track_item(cJSON *item, SpotifyPlaybackState *state) {
    if (!item || !state) return;

    cJSON *name = cJSON_GetObjectItem(item, "name");
    if (name && cJSON_IsString(name)) {
        utils_safe_strncpy(state->track_name, name->valuestring, sizeof(state->track_name));
    }

    cJSON *duration = cJSON_GetObjectItem(item, "duration_ms");
    if (duration && cJSON_IsNumber(duration)) {
        state->duration_ms = duration->valueint;
    }

    cJSON *album = cJSON_GetObjectItem(item, "album");
    if (album) {
        cJSON *alb_name = cJSON_GetObjectItem(album, "name");
        if (alb_name && cJSON_IsString(alb_name)) {
            utils_safe_strncpy(state->album_name, alb_name->valuestring, sizeof(state->album_name));
        }

        /* Check for album art images */
        cJSON *images = cJSON_GetObjectItem(album, "images");
        if (images && cJSON_IsArray(images) && cJSON_GetArraySize(images) > 0) {
            cJSON *img = cJSON_GetArrayItem(images, 0);
            if (img) {
                cJSON *img_url = cJSON_GetObjectItem(img, "url");
                if (img_url && cJSON_IsString(img_url)) {
                    utils_safe_strncpy(state->album_art_url, img_url->valuestring, sizeof(state->album_art_url));
                }
            }
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

static bool spotify_fetch_json(const char *access_token, const char *url, cJSON **out_json, long *out_http_code) {
    if (!access_token || !url || !out_json) return false;
    *out_json = NULL;

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", access_token);

    HttpResponseBuffer resp = {0};
    int http_status = 0;
    bool ok = do_http_request(url, HTTP_REQ_GET, auth_header, NULL, NULL, 0, &resp, &http_status);
    if (out_http_code) *out_http_code = (long)http_status;

    if (ok) {
        if (http_status == 200 && resp.data && resp.size > 0) {
            *out_json = cJSON_Parse(resp.data);
            free(resp.data);
            return (*out_json != NULL);
        }
        if (resp.data) free(resp.data);
        return (http_status >= 200 && http_status < 300);
    }

    if (resp.data) free(resp.data);
    return false;
}

bool spotify_get_playback(const char *access_token, SpotifyPlaybackState *state) {
    if (!access_token || !state) return false;

    cJSON *json = NULL;
    long http_code = 0;

    bool query_res = spotify_fetch_json(access_token, "https://api.spotify.com/v1/me/player", &json, &http_code);

    if (http_code == 200 && json) {
        state->network_error = false;
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
        if (item && !cJSON_IsNull(item)) {
            parse_track_item(item, state);
        }

        cJSON_Delete(json);
        return true;
    }

    if (json) {
        cJSON_Delete(json);
        json = NULL;
    }

    if (http_code == 204) {
        state->network_error = false;
        state->is_playing = false;
        state->is_active = false;

        cJSON *cp_json = NULL;
        long cp_code = 0;
        if (spotify_fetch_json(access_token, "https://api.spotify.com/v1/me/player/currently-playing", &cp_json, &cp_code) && cp_json) {
            cJSON *item = cJSON_GetObjectItem(cp_json, "item");
            if (item && !cJSON_IsNull(item)) {
                parse_track_item(item, state);
            }
            cJSON *progress = cJSON_GetObjectItem(cp_json, "progress_ms");
            if (progress && cJSON_IsNumber(progress)) {
                state->progress_ms = progress->valueint;
            }
            cJSON_Delete(cp_json);
            return true;
        }

        cJSON *dev_json = NULL;
        long dev_code = 0;
        if (spotify_fetch_json(access_token, "https://api.spotify.com/v1/me/player/devices", &dev_json, &dev_code) && dev_json) {
            cJSON *devices = cJSON_GetObjectItem(dev_json, "devices");
            if (devices && cJSON_IsArray(devices) && cJSON_GetArraySize(devices) > 0) {
                cJSON *first_dev = cJSON_GetArrayItem(devices, 0);
                cJSON *dname = cJSON_GetObjectItem(first_dev, "name");
                if (dname && cJSON_IsString(dname)) {
                    snprintf(state->device_name, sizeof(state->device_name), "%s (Idle)", dname->valuestring);
                }
            }
            cJSON_Delete(dev_json);
        }
        return true;
    }

    if (http_code == 401) {
        state->auth_error = true;
        LOG_WARN("Spotify API 401 Unauthorized - token expired");
        error_set(APP_ERR_SPOTIFY_AUTH, "Spotify Auth Expired",
                  "Authorization token expired or invalid. Please re-pair your account.",
                  "Press [SELECT] to pair phone | [X] Dismiss");
        return false;
    }

    if (http_code == 403) {
        LOG_ERROR("Spotify API 403 Forbidden - Spotify Premium required");
        error_set(APP_ERR_SPOTIFY_PREMIUM, "Spotify Premium Required",
                  "Spotify Web API restricts player control to Premium subscribers.",
                  "Press [X] to dismiss");
        return false;
    }

    if (http_code == 429) {
        LOG_WARN("Spotify API 429 Rate Limit exceeded");
        error_set(APP_ERR_SPOTIFY_RATE_LIMIT, "API Rate Limit Exceeded",
                  "Spotify is temporarily throttling requests. Auto-recovering shortly.",
                  "Press [X] to dismiss");
        return false;
    }

    if (!query_res) {
        state->network_error = true;
        LOG_WARN("Spotify get_playback failed (HTTP %ld)", http_code);
        error_set(APP_ERR_SPOTIFY_TIMEOUT, "Spotify Network Timeout",
                  "Failed to reach Spotify API. Please verify your Wi-Fi connection.",
                  "Press [X] to dismiss");
    }

    return false;
}

static bool spotify_send_rest_cmd(const char *access_token, const char *url, HttpMethodType method) {
    if (!access_token || !url) return false;

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", access_token);

    int http_status = 0;
    bool ok = do_http_request(url, method, auth_header, NULL, "", 0, NULL, &http_status);

    if (ok) {
        if (http_status >= 200 && http_status < 300) {
            return true;
        } else if (http_status == 404) {
            LOG_WARN("Spotify command (%s) failed: HTTP 404 No Active Device", url);
        } else if (http_status == 403) {
            LOG_ERROR("Spotify command (%s) failed: HTTP 403 Premium Required", url);
            error_set(APP_ERR_SPOTIFY_PREMIUM, "Spotify Premium Required",
                      "Spotify Web API restricts player control to Premium subscribers.",
                      "Press [X] to dismiss");
        } else {
            LOG_WARN("Spotify command (%s) returned HTTP %d", url, http_status);
        }
    } else {
        LOG_ERROR("Spotify command (%s) network error", url);
    }

    return false;
}

static bool spotify_send_json_cmd(const char *access_token, const char *url, HttpMethodType method, const char *json_body) {
    if (!access_token || !url) return false;

    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", access_token);

    int http_status = 0;
    size_t body_len = json_body ? strlen(json_body) : 0;
    bool ok = do_http_request(url, method, auth_header, "application/json", json_body, body_len, NULL, &http_status);

    if (ok && http_status >= 200 && http_status < 300) {
        return true;
    }

    LOG_WARN("spotify_send_json_cmd (%s) returned HTTP %d", url, http_status);
    return false;
}

static bool spotify_get_best_device(const char *access_token, char *out_device_id, size_t max_len, char *out_name, size_t name_max) {
    if (!access_token || !out_device_id) return false;
    out_device_id[0] = '\0';
    if (out_name) out_name[0] = '\0';

    cJSON *json = NULL;
    long http_code = 0;
    if (!spotify_fetch_json(access_token, "https://api.spotify.com/v1/me/player/devices", &json, &http_code) || !json) {
        LOG_WARN("spotify_get_best_device: failed to fetch devices (HTTP %ld)", http_code);
        return false;
    }

    cJSON *devices = cJSON_GetObjectItem(json, "devices");
    if (!devices || !cJSON_IsArray(devices) || cJSON_GetArraySize(devices) == 0) {
        LOG_WARN("spotify_get_best_device: no registered devices found in Spotify account");
        cJSON_Delete(json);
        return false;
    }

    int dev_count = cJSON_GetArraySize(devices);
    int best_index = -1;

    for (int i = 0; i < dev_count; i++) {
        cJSON *dev = cJSON_GetArrayItem(devices, i);
        if (!dev) continue;

        cJSON *is_active = cJSON_GetObjectItem(dev, "is_active");
        if (is_active && cJSON_IsTrue(is_active)) {
            best_index = i;
            break;
        }

        cJSON *type = cJSON_GetObjectItem(dev, "type");
        if (type && cJSON_IsString(type)) {
            if (strcasecmp(type->valuestring, "Smartphone") == 0 ||
                strcasecmp(type->valuestring, "Computer") == 0) {
                if (best_index < 0) best_index = i;
            }
        }
    }

    if (best_index < 0) {
        best_index = 0;
    }

    cJSON *target = cJSON_GetArrayItem(devices, best_index);
    if (target) {
        cJSON *id = cJSON_GetObjectItem(target, "id");
        if (id && cJSON_IsString(id) && strlen(id->valuestring) > 0) {
            utils_safe_strncpy(out_device_id, id->valuestring, max_len);
            cJSON *name = cJSON_GetObjectItem(target, "name");
            if (name && cJSON_IsString(name) && out_name) {
                utils_safe_strncpy(out_name, name->valuestring, name_max);
            }
            LOG_INFO("spotify_get_best_device: selected device '%s' (id=%s)",
                     out_name ? out_name : "unknown", out_device_id);
            cJSON_Delete(json);
            return true;
        }
    }

    cJSON_Delete(json);
    return false;
}

static bool spotify_transfer_playback(const char *access_token, const char *device_id, bool play) {
    if (!access_token || !device_id || strlen(device_id) == 0) return false;

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"device_ids\":[\"%s\"],\"play\":%s}",
             device_id, play ? "true" : "false");

    LOG_INFO("spotify_transfer_playback: waking up device %s (play=%s)", device_id, play ? "true" : "false");
    return spotify_send_json_cmd(access_token, "https://api.spotify.com/v1/me/player", HTTP_REQ_PUT, payload);
}

bool spotify_resume_playback(const char *access_token) {
    if (!access_token) return false;

    if (spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/play", HTTP_REQ_PUT)) {
        LOG_INFO("spotify_resume_playback: direct play command succeeded");
        return true;
    }

    LOG_INFO("Direct play returned 404. Finding registered Spotify devices...");
    char device_id[128] = {0};
    char device_name[128] = {0};

    if (spotify_get_best_device(access_token, device_id, sizeof(device_id), device_name, sizeof(device_name))) {
        LOG_INFO("Found available device '%s'. Transferring playback with play=true...", device_name);
        if (spotify_transfer_playback(access_token, device_id, true)) {
            LOG_INFO("Successfully resumed playback on '%s'!", device_name);
            return true;
        }

        char play_url[256];
        snprintf(play_url, sizeof(play_url), "https://api.spotify.com/v1/me/player/play?device_id=%s", device_id);
        if (spotify_send_rest_cmd(access_token, play_url, HTTP_REQ_PUT)) {
            LOG_INFO("Resumed playback with explicit device_id query on '%s'!", device_name);
            return true;
        }
    }

    LOG_WARN("Could not resume playback: No active or discoverable Spotify device");
    error_set(APP_ERR_SPOTIFY_NO_DEVICE, "No Active Spotify Device",
              "Spotify could not find an active player. Open Spotify on phone, PC, or speaker.",
              "Press [X] to dismiss");
    return false;
}

bool spotify_play(const char *access_token) {
    return spotify_resume_playback(access_token);
}

bool spotify_pause(const char *access_token) {
    return spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/pause", HTTP_REQ_PUT);
}

bool spotify_next(const char *access_token) {
    if (spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/next", HTTP_REQ_POST)) {
        return true;
    }
    char dev_id[128] = {0};
    if (spotify_get_best_device(access_token, dev_id, sizeof(dev_id), NULL, 0)) {
        char url[256];
        snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/next?device_id=%s", dev_id);
        return spotify_send_rest_cmd(access_token, url, HTTP_REQ_POST);
    }
    return false;
}

bool spotify_previous(const char *access_token) {
    if (spotify_send_rest_cmd(access_token, "https://api.spotify.com/v1/me/player/previous", HTTP_REQ_POST)) {
        return true;
    }
    char dev_id[128] = {0};
    if (spotify_get_best_device(access_token, dev_id, sizeof(dev_id), NULL, 0)) {
        char url[256];
        snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/previous?device_id=%s", dev_id);
        return spotify_send_rest_cmd(access_token, url, HTTP_REQ_POST);
    }
    return false;
}

bool spotify_set_volume(const char *access_token, int volume_percent) {
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;
    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/volume?volume_percent=%d", volume_percent);
    return spotify_send_rest_cmd(access_token, url, HTTP_REQ_PUT);
}

bool spotify_set_shuffle(const char *access_token, bool state) {
    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/shuffle?state=%s", state ? "true" : "false");
    return spotify_send_rest_cmd(access_token, url, HTTP_REQ_PUT);
}

bool spotify_set_repeat(const char *access_token, SpotifyRepeatMode mode) {
    const char *state_str = "off";
    if (mode == REPEAT_CONTEXT) state_str = "context";
    else if (mode == REPEAT_TRACK) state_str = "track";

    char url[128];
    snprintf(url, sizeof(url), "https://api.spotify.com/v1/me/player/repeat?state=%s", state_str);
    return spotify_send_rest_cmd(access_token, url, HTTP_REQ_PUT);
}
