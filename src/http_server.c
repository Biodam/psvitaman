/**
 * PSVitaman - Embedded HTTP Server for Phone-Only QR OAuth Pairing
 */

#include "http_server.h"
#include "config.h"
#include "logger.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#else
#include <pthread.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#endif

static volatile bool s_running = false;
static volatile bool s_auth_received = false;
static int s_port = HTTP_SERVER_DEFAULT_PORT;
static AppConfig *s_config = NULL;
static int s_server_sock = -1;

#if defined(__psp2__) || defined(__VITA__)
static SceUID s_http_thid = -1;
#else
static pthread_t s_http_thread;
#endif

static const char S_HTML_SUCCESS[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>PSVitaman - Paired!</title>\n"
    "  <style>\n"
    "    body { background: #121622; color: #fff; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; text-align: center; padding: 40px 20px; margin: 0; }\n"
    "    .card { background: #1c2230; border: 2px solid #00d26a; border-radius: 16px; padding: 32px 24px; max-width: 380px; margin: 20px auto; box-shadow: 0 8px 32px rgba(0,0,0,0.6); }\n"
    "    .icon { font-size: 48px; margin-bottom: 12px; }\n"
    "    h1 { color: #00d26a; font-size: 22px; margin: 0 0 10px 0; letter-spacing: 1px; }\n"
    "    p { color: #ccc; font-size: 15px; line-height: 1.5; margin: 10px 0; }\n"
    "    .badge { display: inline-block; background: #00d26a; color: #121622; font-weight: bold; font-size: 14px; padding: 8px 20px; border-radius: 20px; margin-top: 16px; text-transform: uppercase; letter-spacing: 0.5px; }\n"
    "    .tape { font-size: 12px; color: #7f8c9d; margin-top: 24px; border-top: 1px solid #2a3446; padding-top: 16px; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"card\">\n"
    "    <div class=\"icon\">&#x2728;</div>\n"
    "    <h1>PSVITAMAN PAIRED!</h1>\n"
    "    <p>Your Spotify credentials have been received and saved to your PS Vita.</p>\n"
    "    <div class=\"badge\">You can close this tab</div>\n"
    "    <p class=\"tape\">PSVitaman &bull; C-90 High Bias Stereo</p>\n"
    "  </div>\n"
    "</body>\n"
    "</html>\n";

static void url_decode(const char *src, char *dst, size_t dst_len) {
    size_t i = 0, j = 0;
    while (src[i] && j + 1 < dst_len) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            int hex_val = 0;
            if (sscanf(&src[i + 1], "%2x", &hex_val) == 1) {
                dst[j++] = (char)hex_val;
                i += 3;
                continue;
            }
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
            continue;
        }
        dst[j++] = src[i++];
    }
    dst[j] = '\0';
}

static bool extract_param(const char *haystack, const char *key, char *out_val, size_t max_len) {
    if (!haystack || !key || !out_val || max_len == 0) return false;
    out_val[0] = '\0';

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *pos = strstr(haystack, pattern);
    if (!pos) {
        /* Check JSON format "key":"value" */
        snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        pos = strstr(haystack, pattern);
        if (pos) {
            const char *val_start = strchr(pos, ':');
            if (val_start) {
                val_start++;
                while (*val_start == ' ' || *val_start == '\"') val_start++;
                const char *val_end = strchr(val_start, '\"');
                if (!val_end) val_end = strchr(val_start, '}');
                if (val_end && (size_t)(val_end - val_start) < max_len) {
                    size_t len = val_end - val_start;
                    strncpy(out_val, val_start, len);
                    out_val[len] = '\0';
                    return true;
                }
            }
        }
        return false;
    }

    pos += strlen(pattern);
    char raw[512] = {0};
    size_t i = 0;
    while (*pos && *pos != '&' && *pos != ' ' && *pos != '\r' && *pos != '\n' && i + 1 < sizeof(raw)) {
        raw[i++] = *pos++;
    }
    raw[i] = '\0';

    url_decode(raw, out_val, max_len);
    return (strlen(out_val) > 0);
}

static void handle_client(int client_sock, const char *request) {
    char response[4096];
    memset(response, 0, sizeof(response));

    /* 1. CORS Preflight OPTIONS */
    if (strncmp(request, "OPTIONS", 7) == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Access-Control-Allow-Headers: *\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: close\r\n\r\n");
#if defined(__psp2__) || defined(__VITA__)
        sceNetSend(client_sock, response, strlen(response), 0);
#else
        send(client_sock, response, strlen(response), 0);
#endif
        return;
    }

    /* 2. Health check ping */
    if (strncmp(request, "GET /status", 11) == 0 || strncmp(request, "GET /ping", 9) == 0) {
        const char *body = "{\"status\":\"ready\",\"app\":\"psvitaman\"}";
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Content-Length: %u\r\n"
                 "Connection: close\r\n\r\n%s",
                 (unsigned int)strlen(body), body);
#if defined(__psp2__) || defined(__VITA__)
        sceNetSend(client_sock, response, strlen(response), 0);
#else
        send(client_sock, response, strlen(response), 0);
#endif
        return;
    }

    /* 3. Token pairing endpoint /save */
    if (strstr(request, "/save") != NULL) {
        char refresh_token[512] = {0};
        char client_id[128] = {0};
        char client_secret[128] = {0};

        extract_param(request, "refresh_token", refresh_token, sizeof(refresh_token));
        extract_param(request, "client_id", client_id, sizeof(client_id));
        extract_param(request, "client_secret", client_secret, sizeof(client_secret));

        if (strlen(refresh_token) > 0) {
            LOG_INFO("HTTP /save received valid token from phone client");
            if (s_config) {
                strncpy(s_config->refresh_token, refresh_token, sizeof(s_config->refresh_token) - 1);
                s_config->refresh_token[sizeof(s_config->refresh_token) - 1] = '\0';

                if (strlen(client_id) > 0 && strstr(client_id, "YOUR_") == NULL) {
                    strncpy(s_config->client_id, client_id, sizeof(s_config->client_id) - 1);
                    s_config->client_id[sizeof(s_config->client_id) - 1] = '\0';
                } else if (strlen(s_config->client_id) == 0 || strstr(s_config->client_id, "YOUR_") != NULL) {
                    strncpy(s_config->client_id, DEFAULT_SPOTIFY_CLIENT_ID, sizeof(s_config->client_id) - 1);
                }

                if (strlen(client_secret) > 0 && strstr(client_secret, "YOUR_") == NULL) {
                    strncpy(s_config->client_secret, client_secret, sizeof(s_config->client_secret) - 1);
                }

                s_config->is_valid = true;
                config_save(s_config);
                LOG_INFO("Config saved successfully to ux0:data/psvitaman/config.ini");
            }

            s_auth_received = true;

            /* Send Success HTML Response */
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=utf-8\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: *\r\n"
                     "Content-Length: %u\r\n"
                     "Connection: close\r\n\r\n%s",
                     (unsigned int)strlen(S_HTML_SUCCESS), S_HTML_SUCCESS);

#if defined(__psp2__) || defined(__VITA__)
            sceNetSend(client_sock, response, strlen(response), 0);
#else
            send(client_sock, response, strlen(response), 0);
#endif
            return;
        }

        /* Missing token */
        LOG_WARN("HTTP /save request rejected: missing refresh_token");
        const char *err_body = "{\"error\":\"Missing refresh_token parameter\"}";
        snprintf(response, sizeof(response),
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Content-Length: %u\r\n"
                 "Connection: close\r\n\r\n%s",
                 (unsigned int)strlen(err_body), err_body);
#if defined(__psp2__) || defined(__VITA__)
        sceNetSend(client_sock, response, strlen(response), 0);
#else
        send(client_sock, response, strlen(response), 0);
#endif
        return;
    }

    /* 4. Not Found */
    const char *nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
#if defined(__psp2__) || defined(__VITA__)
    sceNetSend(client_sock, nf, strlen(nf), 0);
#else
    send(client_sock, nf, strlen(nf), 0);
#endif
}

#if defined(__psp2__) || defined(__VITA__)
static int http_server_thread_func(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    s_server_sock = sceNetSocket("psvitaman_httpd", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (s_server_sock < 0) {
        LOG_ERROR("HTTP server: sceNetSocket failed: 0x%X", s_server_sock);
        error_set(APP_ERR_HTTP_SERVER_FAIL, "Pairing Server Error", "Failed to create HTTP socket.", "Press [X] to dismiss");
        s_running = false;
        return -1;
    }

    int opt = 1;
    sceNetSetsockopt(s_server_sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, &opt, sizeof(opt));

    struct SceNetSockaddrIn server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SCE_NET_AF_INET;
    server_addr.sin_port = sceNetHtons(s_port);
    server_addr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);

    if (sceNetBind(s_server_sock, (const struct SceNetSockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("HTTP server: sceNetBind failed on port %d", s_port);
        error_set(APP_ERR_HTTP_SERVER_FAIL, "Pairing Server Error", "Failed to bind pairing server port 8888.", "Press [X] to dismiss");
        sceNetSocketClose(s_server_sock);
        s_server_sock = -1;
        s_running = false;
        return -1;
    }

    if (sceNetListen(s_server_sock, 4) < 0) {
        LOG_ERROR("HTTP server: sceNetListen failed");
        sceNetSocketClose(s_server_sock);
        s_server_sock = -1;
        s_running = false;
        return -1;
    }

    LOG_INFO("HTTP pairing server listening on port %d", s_port);

    while (s_running) {
        struct SceNetSockaddrIn client_addr;
        unsigned int client_len = sizeof(client_addr);
        int client_sock = sceNetAccept(s_server_sock, (struct SceNetSockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (!s_running) break;
            sceKernelDelayThread(20000); /* 20ms */
            continue;
        }

        char req_buf[4096];
        memset(req_buf, 0, sizeof(req_buf));
        int recvd = sceNetRecv(client_sock, req_buf, sizeof(req_buf) - 1, 0);
        if (recvd > 0) {
            handle_client(client_sock, req_buf);
        }
        sceNetSocketClose(client_sock);
    }

    if (s_server_sock >= 0) {
        sceNetSocketClose(s_server_sock);
        s_server_sock = -1;
    }
    return 0;
}
#else
static void *http_server_thread_func(void *arg) {
    (void)arg;
    LOG_INFO("HTTP server stub started (host build)");
    while (s_running) {
        usleep(100000);
    }
    return NULL;
}
#endif

bool http_server_start(int port, AppConfig *config) {
    if (s_running) return true;

    s_port = (port > 0) ? port : HTTP_SERVER_DEFAULT_PORT;
    s_config = config;
    s_auth_received = false;
    s_running = true;

    LOG_INFO("Starting HTTP pairing server on port %d...", s_port);

#if defined(__psp2__) || defined(__VITA__)
    s_http_thid = sceKernelCreateThread("psvitaman_httpd", http_server_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
    if (s_http_thid < 0) {
        LOG_ERROR("HTTP server: sceKernelCreateThread failed: 0x%X", s_http_thid);
        s_running = false;
        return false;
    }
    if (sceKernelStartThread(s_http_thid, 0, NULL) < 0) {
        LOG_ERROR("HTTP server: sceKernelStartThread failed");
        s_running = false;
        return false;
    }
#else
    if (pthread_create(&s_http_thread, NULL, http_server_thread_func, NULL) != 0) {
        s_running = false;
        return false;
    }
#endif

    return true;
}

void http_server_stop(void) {
    if (!s_running) return;
    s_running = false;
    s_auth_received = false;

#if defined(__psp2__) || defined(__VITA__)
    /* 100ms grace period to allow client response transmission to finish cleanly */
    sceKernelDelayThread(100000);
    if (s_server_sock >= 0) {
        sceNetSocketClose(s_server_sock);
        s_server_sock = -1;
    }
    if (s_http_thid >= 0) {
        sceKernelWaitThreadEnd(s_http_thid, NULL, NULL);
        s_http_thid = -1;
    }
#else
    usleep(100000);
    pthread_join(s_http_thread, NULL);
#endif

    LOG_INFO("HTTP pairing server stopped successfully");
}

bool http_server_is_running(void) {
    return s_running;
}

bool http_server_has_received_auth(void) {
    if (s_auth_received) {
        s_auth_received = false;
        return true;
    }
    return false;
}

void http_server_clear_auth_received(void) {
    s_auth_received = false;
}

bool http_server_get_local_ip(char *ip_out, size_t max_len) {
    if (!ip_out || max_len == 0) return false;

#if defined(__psp2__) || defined(__VITA__)
    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));
    int ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
    if (ret >= 0 && strlen(info.ip_address) > 0 && strcmp(info.ip_address, "0.0.0.0") != 0) {
        strncpy(ip_out, info.ip_address, max_len - 1);
        ip_out[max_len - 1] = '\0';
        return true;
    }
#endif

    strncpy(ip_out, "127.0.0.1", max_len - 1);
    ip_out[max_len - 1] = '\0';
    return false;
}
