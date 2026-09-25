/**
 * PSVitaman - OpenSSL 1.0 to OpenSSL 1.1/3.0 Linker Compatibility Shim
 * 
 * VitaSDK's libcurl.a was compiled with OpenSSL 1.0 legacy function symbols,
 * whereas modern VitaSDK provides OpenSSL 1.1+ where these functions were
 * replaced by macros or OPENSSL_init_* / OPENSSL_sk_* functions.
 * This shim maps the legacy symbols to their modern OpenSSL equivalents.
 */

#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ui.h>
#include <openssl/safestack.h>

/* Undefine macros that OpenSSL 1.1 headers use in place of legacy functions */
#undef EVP_MD_CTX_create
#undef EVP_MD_CTX_destroy
#undef SSLeay
#undef SSL_library_init
#undef SSL_load_error_strings
#undef OPENSSL_add_all_algorithms_noconf
#undef EVP_cleanup
#undef ENGINE_cleanup
#undef ERR_free_strings
#undef CONF_modules_free
#undef SSL_COMP_free_compression_methods
#undef SSLv23_client_method
#undef sk_num
#undef sk_value
#undef sk_pop
#undef sk_pop_free

EVP_MD_CTX *EVP_MD_CTX_create(void) {
    return EVP_MD_CTX_new();
}

void EVP_MD_CTX_destroy(EVP_MD_CTX *ctx) {
    EVP_MD_CTX_free(ctx);
}

unsigned long SSLeay(void) {
    return OpenSSL_version_num();
}

int SSL_library_init(void) {
    return OPENSSL_init_ssl(0, NULL);
}

void SSL_load_error_strings(void) {
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
}

void OPENSSL_add_all_algorithms_noconf(void) {
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
}

void EVP_cleanup(void) {
    /* No-op in OpenSSL 1.1+ */
}

void ENGINE_cleanup(void) {
    /* No-op in OpenSSL 1.1+ */
}

void ERR_free_strings(void) {
    /* No-op in OpenSSL 1.1+ */
}

void CONF_modules_free(void) {
    /* No-op in OpenSSL 1.1+ */
}

void SSL_COMP_free_compression_methods(void) {
    /* No-op in OpenSSL 1.1+ */
}

const SSL_METHOD *SSLv23_client_method(void) {
    return TLS_client_method();
}

int sk_num(const OPENSSL_STACK *st) {
    return OPENSSL_sk_num(st);
}

void *sk_value(const OPENSSL_STACK *st, int i) {
    return OPENSSL_sk_value(st, i);
}

void *sk_pop(OPENSSL_STACK *st) {
    return OPENSSL_sk_pop(st);
}

void sk_pop_free(OPENSSL_STACK *st, void (*func)(void *)) {
    OPENSSL_sk_pop_free(st, (OPENSSL_sk_freefunc)func);
}

UI_METHOD *UI_OpenSSL(void) {
    return (UI_METHOD *)UI_null();
}
