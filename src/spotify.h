/**
 * PSVitaman - Spotify Web API Client
 */

#ifndef PSVITAMAN_SPOTIFY_H
#define PSVITAMAN_SPOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#define SPOTIFY_TRACK_NAME_MAX   256
#define SPOTIFY_ARTIST_NAME_MAX  256
#define SPOTIFY_ALBUM_NAME_MAX   256
#define SPOTIFY_DEVICE_NAME_MAX  128
#define SPOTIFY_URL_MAX          512

typedef enum {
    REPEAT_OFF = 0,
    REPEAT_CONTEXT,
    REPEAT_TRACK
} SpotifyRepeatMode;

typedef struct {
    bool is_active;
    bool is_playing;
    bool shuffle_state;
    SpotifyRepeatMode repeat_state;
    int progress_ms;
    int duration_ms;
    int volume_percent;

    char track_name[SPOTIFY_TRACK_NAME_MAX];
    char artist_name[SPOTIFY_ARTIST_NAME_MAX];
    char album_name[SPOTIFY_ALBUM_NAME_MAX];
    char device_name[SPOTIFY_DEVICE_NAME_MAX];
    char album_art_url[SPOTIFY_URL_MAX];

    uint64_t last_sync_tick;
    bool auth_error;
    bool network_error;
    char error_message[128];
} SpotifyPlaybackState;

/* Initialize Spotify subsystem and curl environment */
bool spotify_init(void);

/* Clean up Spotify subsystem */
void spotify_cleanup(void);

/* Refresh Spotify OAuth access token using refresh_token */
bool spotify_refresh_token(const char *client_id, const char *client_secret,
                          const char *refresh_token, char *access_token_out,
                          size_t token_max, int *expires_in_out);

/* Fetch currently playing / playback state */
bool spotify_get_playback(const char *access_token, SpotifyPlaybackState *state);

/* Transport control commands */
bool spotify_play(const char *access_token);
bool spotify_pause(const char *access_token);
bool spotify_next(const char *access_token);
bool spotify_previous(const char *access_token);
bool spotify_set_volume(const char *access_token, int volume_percent);
bool spotify_set_shuffle(const char *access_token, bool state);
bool spotify_set_repeat(const char *access_token, SpotifyRepeatMode mode);

#endif /* PSVITAMAN_SPOTIFY_H */
