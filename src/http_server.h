/**
 * PSVitaman - Embedded HTTP Server for Phone-Only QR OAuth Pairing
 */

#ifndef PSVITAMAN_HTTP_SERVER_H
#define PSVITAMAN_HTTP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include "config.h"

#define HTTP_SERVER_DEFAULT_PORT 8888

/* Start background pairing HTTP server thread */
bool http_server_start(int port, AppConfig *config);

/* Stop the background pairing HTTP server */
void http_server_stop(void);

/* Check if the server is currently running */
bool http_server_is_running(void);

/* Check if valid Spotify credentials were received and saved (edge-triggered, clears on read) */
bool http_server_has_received_auth(void);

/* Explicitly clear the auth received flag */
void http_server_clear_auth_received(void);

/* Retrieve local IP address string (e.g., "192.168.1.100").
 * Returns true if active IP found, false otherwise (defaults to "127.0.0.1"). */
bool http_server_get_local_ip(char *ip_out, size_t max_len);

#endif /* PSVITAMAN_HTTP_SERVER_H */
