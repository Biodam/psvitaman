/**
 * PSVitaman - Spotify Web API Client Implementation
 * 
 * Uses MbedTLS over SceNet sockets for modern, secure HTTPS communication
 * with full TLS 1.2/1.3 and AES-GCM ciphersuite support.
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
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>

#ifndef MBEDTLS_ERR_NET_SEND_FAILED
#define MBEDTLS_ERR_NET_SEND_FAILED -0x004E
#endif
#ifndef MBEDTLS_ERR_NET_RECV_FAILED
#define MBEDTLS_ERR_NET_RECV_FAILED -0x004C
#endif
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

static mbedtls_x509_crt s_cacert;
static mbedtls_entropy_context s_entropy;
static mbedtls_ctr_drbg_context s_ctr_drbg;
static bool s_mbedtls_ready = false;

static int mbedtls_net_send_cb(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    int ret = sceNetSend(fd, buf, (unsigned int)len, 0);
    if (ret < 0) {
        int err = *sceNetErrnoLoc();
        if (err == SCE_NET_EAGAIN || err == SCE_NET_EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return ret;
}

static int mbedtls_net_recv_cb(void *ctx, unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    int ret = sceNetRecv(fd, buf, (unsigned int)len, 0);
    if (ret < 0) {
        int err = *sceNetErrnoLoc();
        if (err == SCE_NET_EAGAIN || err == SCE_NET_EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        if (err == SCE_NET_ETIMEDOUT) {
            return MBEDTLS_ERR_SSL_TIMEOUT;
        }
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return ret;
}

static int safe_strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s2) {
        char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (char)(*s1 + 32) : *s1;
        char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (char)(*s2 + 32) : *s2;
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        s1++;
        s2++;
        n--;
    }
    return n ? ((unsigned char)*s1 - (unsigned char)*s2) : 0;
}

static const char *find_case_insensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return haystack;
    for (const char *h = haystack; *h; h++) {
        if (safe_strncasecmp(h, needle, needle_len) == 0) {
            return h;
        }
    }
    return NULL;
}

static char *decode_chunked_body(const char *src, size_t src_len, size_t *out_len) {
    char *dest = malloc(src_len + 1);
    if (!dest) {
        if (out_len) *out_len = 0;
        return NULL;
    }

    const char *p = src;
    const char *end = src + src_len;
    size_t written = 0;

    while (p < end) {
        char *chunk_end = NULL;
        unsigned long chunk_size = strtoul(p, &chunk_end, 16);
        if (chunk_end == p) break;

        p = chunk_end;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
        if (p < end && *p == '\n') p++;

        if (chunk_size == 0) {
            break;
        }

        if (p + chunk_size > end) {
            chunk_size = (size_t)(end - p);
        }

        memcpy(dest + written, p, chunk_size);
        written += chunk_size;
        p += chunk_size;

        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }

    dest[written] = '\0';
    if (out_len) *out_len = written;
    return dest;
}

static bool parse_url(const char *url, char *host, size_t host_size, int *port,
                      char *path, size_t path_size, bool *is_https) {
    if (!url || !host || !port || !path || !is_https) return false;
    *is_https = true;
    *port = 443;

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        *is_https = true;
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        *is_https = false;
        *port = 80;
        p += 7;
    }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = colon - p;
        if (hlen >= host_size) hlen = host_size - 1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else if (slash) {
        size_t hlen = slash - p;
        if (hlen >= host_size) hlen = host_size - 1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
    } else {
        strncpy(host, p, host_size - 1);
        host[host_size - 1] = '\0';
    }

    if (slash) {
        strncpy(path, slash, path_size - 1);
        path[path_size - 1] = '\0';
    } else {
        strncpy(path, "/", path_size - 1);
        path[path_size - 1] = '\0';
    }

    return true;
}

static int connect_socket(const char *host, int port) {
    SceNetInAddr addr;
    memset(&addr, 0, sizeof(addr));

    if (sceNetInetPton(SCE_NET_AF_INET, host, &addr) <= 0) {
        int rid = sceNetResolverCreate("psvitaman_resolver", NULL, 0);
        if (rid < 0) {
            LOG_ERROR("sceNetResolverCreate failed: 0x%08x", rid);
            return -1;
        }

        int res = sceNetResolverStartNtoa(rid, host, &addr, 5000000, 3, 0);
        sceNetResolverDestroy(rid);
        if (res < 0) {
            LOG_ERROR("DNS resolution failed for '%s': 0x%08x", host, res);
            return -1;
        }
    }

    int sock = sceNetSocket("spotify_sock", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("sceNetSocket failed: 0x%08x", sock);
        return -1;
    }

    int timeout_usec = 8 * 1000 * 1000;
    sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &timeout_usec, sizeof(timeout_usec));
    sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &timeout_usec, sizeof(timeout_usec));

    SceNetSockaddrIn sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = SCE_NET_AF_INET;
    sin.sin_port = sceNetHtons((unsigned short)port);
    sin.sin_addr = addr;

    int ret = sceNetConnect(sock, (SceNetSockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
        LOG_ERROR("sceNetConnect failed to %s:%d: 0x%08x", host, port, ret);
        sceNetSocketClose(sock);
        return -1;
    }

    return sock;
}

static bool do_http_request(const char *url, HttpMethodType method, const char *auth_header,
                           const char *content_type, const void *post_data, size_t post_len,
                           HttpResponseBuffer *out_buf, int *out_http_status) {
    if (!url) return false;
    if (out_http_status) *out_http_status = 0;
    if (out_buf) {
        out_buf->data = NULL;
        out_buf->size = 0;
    }

    if (!s_mbedtls_ready) {
        LOG_ERROR("do_http_request: mbedtls is not initialized");
        return false;
    }

    char host[128] = {0};
    char path[512] = {0};
    int port = 443;
    bool is_https = true;
    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path), &is_https)) {
        LOG_ERROR("do_http_request: failed to parse URL '%s'", url);
        return false;
    }

    int sock = connect_socket(host, port);
    if (sock < 0) {
        LOG_ERROR("do_http_request: failed to connect TCP socket to %s:%d", host, port);
        return false;
    }

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    bool ssl_active = false;

    if (is_https) {
        int ret = mbedtls_ssl_config_defaults(&conf,
                                             MBEDTLS_SSL_IS_CLIENT,
                                             MBEDTLS_SSL_TRANSPORT_STREAM,
                                             MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            char err_buf[128];
            mbedtls_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("mbedtls_ssl_config_defaults failed: -0x%04x (%s)", -ret, err_buf);
            sceNetSocketClose(sock);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            return false;
        }

        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&conf, &s_cacert, NULL);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &s_ctr_drbg);

        ret = mbedtls_ssl_setup(&ssl, &conf);
        if (ret != 0) {
            char err_buf[128];
            mbedtls_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("mbedtls_ssl_setup failed: -0x%04x (%s)", -ret, err_buf);
            sceNetSocketClose(sock);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            return false;
        }

        ret = mbedtls_ssl_set_hostname(&ssl, host);
        if (ret != 0) {
            char err_buf[128];
            mbedtls_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("mbedtls_ssl_set_hostname failed: -0x%04x (%s)", -ret, err_buf);
            sceNetSocketClose(sock);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            return false;
        }

        mbedtls_ssl_set_bio(&ssl, &sock, mbedtls_net_send_cb, mbedtls_net_recv_cb, NULL);

        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char err_buf[128];
                mbedtls_strerror(ret, err_buf, sizeof(err_buf));
                LOG_ERROR("mbedtls_ssl_handshake failed with %s: -0x%04x (%s)", host, -ret, err_buf);
                sceNetSocketClose(sock);
                mbedtls_ssl_free(&ssl);
                mbedtls_ssl_config_free(&conf);
                return false;
            }
        }

        uint32_t vrfy_flags = mbedtls_ssl_get_verify_result(&ssl);
        if (vrfy_flags != 0) {
            char vrfy_buf[512];
            mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", vrfy_flags);
            LOG_ERROR("SSL certificate verification failed for %s (flags 0x%08x):\n%s", host, vrfy_flags, vrfy_buf);
            mbedtls_ssl_close_notify(&ssl);
            sceNetSocketClose(sock);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            return false;
        }

        LOG_INFO("TLS connection verified with %s (%s, %s)",
                 host, mbedtls_ssl_get_version(&ssl), mbedtls_ssl_get_ciphersuite(&ssl));
        ssl_active = true;
    }

    const char *method_str = "GET";
    if (method == HTTP_REQ_POST) method_str = "POST";
    else if (method == HTTP_REQ_PUT) method_str = "PUT";

    char content_length_hdr[64] = {0};
    if (post_data && post_len > 0) {
        snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", post_len);
    } else if (method == HTTP_REQ_POST || method == HTTP_REQ_PUT) {
        snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: 0\r\n");
    }

    char req_buf[2048];
    int req_hdr_len = snprintf(req_buf, sizeof(req_buf),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: PSVitaman/1.0 (PS Vita; ARM)\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "%s%s%s"
        "%s%s%s"
        "%s"
        "\r\n",
        method_str, path, host,
        (auth_header && *auth_header) ? "Authorization: " : "",
        (auth_header && *auth_header) ? auth_header : "",
        (auth_header && *auth_header) ? "\r\n" : "",
        (content_type && *content_type) ? "Content-Type: " : "",
        (content_type && *content_type) ? content_type : "",
        (content_type && *content_type) ? "\r\n" : "",
        content_length_hdr
    );

    /* Send Request Headers */
    size_t total_sent = 0;
    while (total_sent < (size_t)req_hdr_len) {
        int sent = 0;
        if (ssl_active) {
            sent = mbedtls_ssl_write(&ssl, (const unsigned char *)req_buf + total_sent, req_hdr_len - total_sent);
        } else {
            sent = sceNetSend(sock, req_buf + total_sent, req_hdr_len - total_sent, 0);
        }
        if (sent <= 0) {
            if (sent == MBEDTLS_ERR_SSL_WANT_READ || sent == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            LOG_ERROR("Failed sending HTTP request headers to %s", host);
            if (ssl_active) mbedtls_ssl_close_notify(&ssl);
            sceNetSocketClose(sock);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            return false;
        }
        total_sent += sent;
    }

    /* Send Request Body */
    if (post_data && post_len > 0) {
        total_sent = 0;
        while (total_sent < post_len) {
            int sent = 0;
            if (ssl_active) {
                sent = mbedtls_ssl_write(&ssl, (const unsigned char *)post_data + total_sent, post_len - total_sent);
            } else {
                sent = sceNetSend(sock, (const char *)post_data + total_sent, post_len - total_sent, 0);
            }
            if (sent <= 0) {
                if (sent == MBEDTLS_ERR_SSL_WANT_READ || sent == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
                LOG_ERROR("Failed sending HTTP request body to %s", host);
                if (ssl_active) mbedtls_ssl_close_notify(&ssl);
                sceNetSocketClose(sock);
                mbedtls_ssl_free(&ssl);
                mbedtls_ssl_config_free(&conf);
                return false;
            }
            total_sent += sent;
        }
    }

    /* Read Response */
    size_t raw_capacity = 4096;
    size_t raw_size = 0;
    char *raw_resp = malloc(raw_capacity);
    if (!raw_resp) {
        LOG_ERROR("Out of memory allocating HTTP response buffer");
        if (ssl_active) mbedtls_ssl_close_notify(&ssl);
        sceNetSocketClose(sock);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        return false;
    }

    unsigned char chunk[2048];
    while (1) {
        int bytes_read = 0;
        if (ssl_active) {
            bytes_read = mbedtls_ssl_read(&ssl, chunk, sizeof(chunk));
        } else {
            bytes_read = sceNetRecv(sock, chunk, sizeof(chunk), 0);
        }

        if (bytes_read > 0) {
            if (raw_size + bytes_read + 1 > raw_capacity) {
                raw_capacity = (raw_size + bytes_read + 1) * 2;
                char *new_buf = realloc(raw_resp, raw_capacity);
                if (!new_buf) {
                    LOG_ERROR("Out of memory reallocating HTTP response buffer");
                    break;
                }
                raw_resp = new_buf;
            }
            memcpy(raw_resp + raw_size, chunk, bytes_read);
            raw_size += bytes_read;
            raw_resp[raw_size] = '\0';
        } else if (bytes_read == 0 || bytes_read == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        } else if (bytes_read == MBEDTLS_ERR_SSL_WANT_READ || bytes_read == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        } else {
            char err_buf[128];
            mbedtls_strerror(bytes_read, err_buf, sizeof(err_buf));
            LOG_ERROR("Error reading HTTP response from %s: -0x%04x (%s)", host, -bytes_read, err_buf);
            break;
        }
    }

    if (ssl_active) {
        mbedtls_ssl_close_notify(&ssl);
    }
    sceNetSocketClose(sock);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);

    if (raw_size == 0) {
        LOG_ERROR("Empty response received from %s", host);
        free(raw_resp);
        return false;
    }

    const char *hdr_end = strstr(raw_resp, "\r\n\r\n");
    if (!hdr_end) {
        LOG_ERROR("Malformed HTTP response from %s (no header boundary)", host);
        free(raw_resp);
        return false;
    }

    int status_code = 0;
    if (sscanf(raw_resp, "HTTP/%*d.%*d %d", &status_code) == 1) {
        if (out_http_status) *out_http_status = status_code;
    }

    LOG_INFO("HTTP %d response from %s (%zu bytes)", status_code, host, raw_size);

    const char *body_start = hdr_end + 4;
    size_t body_len = raw_size - (body_start - raw_resp);

    if (out_buf) {
        bool is_chunked = false;
        const char *te = find_case_insensitive(raw_resp, "Transfer-Encoding:");
        if (te && te < hdr_end && find_case_insensitive(te, "chunked")) {
            is_chunked = true;
        }

        if (is_chunked) {
            size_t decoded_len = 0;
            out_buf->data = decode_chunked_body(body_start, body_len, &decoded_len);
            out_buf->size = decoded_len;
        } else {
            out_buf->data = malloc(body_len + 1);
            if (out_buf->data) {
                memcpy(out_buf->data, body_start, body_len);
                out_buf->data[body_len] = '\0';
                out_buf->size = body_len;
            } else {
                out_buf->size = 0;
            }
        }
    }

    free(raw_resp);
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
#if defined(__psp2__) || defined(__VITA__)
    if (s_mbedtls_ready) return true;

    mbedtls_x509_crt_init(&s_cacert);
    mbedtls_entropy_init(&s_entropy);
    mbedtls_ctr_drbg_init(&s_ctr_drbg);

    const char *pers = "psvitaman_spotify";
    int ret = mbedtls_ctr_drbg_seed(&s_ctr_drbg, mbedtls_entropy_func, &s_entropy,
                                    (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        char err_buf[128];
        mbedtls_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("mbedtls_ctr_drbg_seed failed: -0x%04x (%s)", -ret, err_buf);
        return false;
    }

    FILE *f = fopen("app0:assets/cacert.pem", "rb");
    if (!f) {
        LOG_ERROR("Failed to open app0:assets/cacert.pem");
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        LOG_ERROR("app0:assets/cacert.pem is empty or invalid size: %ld", fsize);
        fclose(f);
        return false;
    }

    unsigned char *pem_buf = malloc(fsize + 1);
    if (!pem_buf) {
        LOG_ERROR("Out of memory allocating %ld bytes for CA bundle", fsize);
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(pem_buf, 1, fsize, f);
    fclose(f);
    pem_buf[read_bytes] = '\0';

    ret = mbedtls_x509_crt_parse(&s_cacert, pem_buf, read_bytes + 1);
    free(pem_buf);

    if (ret < 0) {
        char err_buf[128];
        mbedtls_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("mbedtls_x509_crt_parse failed: -0x%04x (%s)", -ret, err_buf);
        return false;
    }

    LOG_INFO("MbedTLS initialized with Root CA certificate bundle (%zu bytes)", read_bytes);
    s_mbedtls_ready = true;
#endif
    return true;
}

void spotify_cleanup(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_mbedtls_ready) {
        mbedtls_x509_crt_free(&s_cacert);
        mbedtls_ctr_drbg_free(&s_ctr_drbg);
        mbedtls_entropy_free(&s_entropy);
        s_mbedtls_ready = false;
    }
#endif
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
