/**
 * PSVitaman - Unit Tests (utils, ini parser, cJSON)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/utils.h"
#include "../src/ini.h"
#include "../src/cJSON.h"

static int test_ini_handler(void* user, const char* section, const char* name, const char* value) {
    char *out = (char*)user;
    if (strcmp(section, "spotify") == 0 && strcmp(name, "client_id") == 0) {
        strcpy(out, value);
    }
    return 1;
}

void test_utils_base64(void) {
    const char *plain = "hello:world";
    char encoded[64];
    bool res = utils_base64_encode((const unsigned char*)plain, strlen(plain), encoded, sizeof(encoded));
    assert(res == true);
    assert(strcmp(encoded, "aGVsbG86d29ybGQ=") == 0);
    printf("[PASS] utils_base64_encode: '%s' -> '%s'\n", plain, encoded);
}

void test_utils_format_time(void) {
    char buf[32];
    utils_format_time_ms(0, buf, sizeof(buf));
    assert(strcmp(buf, "00:00") == 0);

    utils_format_time_ms(65000, buf, sizeof(buf));
    assert(strcmp(buf, "01:05") == 0);

    utils_format_time_ms(3665000, buf, sizeof(buf));
    assert(strcmp(buf, "01:01:05") == 0);
    printf("[PASS] utils_format_time_ms passed\n");
}

void test_ini_string_parsing(void) {
    const char *ini_data = 
        "# Spotify test config\n"
        "[spotify]\n"
        "client_id = test_client_123\n"
        "client_secret = test_secret_456\n"
        "refresh_token = test_token_789\n";

    char extracted_id[64] = {0};
    int err = ini_parse_string(ini_data, test_ini_handler, extracted_id);
    assert(err == 0);
    assert(strcmp(extracted_id, "test_client_123") == 0);
    printf("[PASS] ini_parse_string: successfully parsed section and key '%s'\n", extracted_id);
}

void test_cjson_parsing(void) {
    const char *json_data = "{\"is_playing\":true,\"progress_ms\":42000,\"item\":{\"name\":\"Cassette Love\",\"duration_ms\":180000}}";
    cJSON *root = cJSON_Parse(json_data);
    assert(root != NULL);

    cJSON *playing = cJSON_GetObjectItem(root, "is_playing");
    assert(playing && cJSON_IsTrue(playing));

    cJSON *item = cJSON_GetObjectItem(root, "item");
    assert(item != NULL);

    cJSON *name = cJSON_GetObjectItem(item, "name");
    assert(name && strcmp(name->valuestring, "Cassette Love") == 0);

    cJSON_Delete(root);
    printf("[PASS] cJSON: successfully parsed Spotify playback JSON\n");
}

int main(void) {
    printf("=== Running PSVitaman Unit Tests ===\n");
    test_utils_base64();
    test_utils_format_time();
    test_ini_string_parsing();
    test_cjson_parsing();
    printf("=== ALL UNIT TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
