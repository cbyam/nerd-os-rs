#pragma once
/* mbedTLS stand-ins for the host builds of the C reference code, implemented
 * in c-reference/src/lib.rs with the RustCrypto crates. The contexts are
 * opaque storage for the Rust state. */
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    unsigned long long storage[64];
} mbedtls_sha256_context;
int mbedtls_sha256(const unsigned char *input, size_t ilen, unsigned char *output, int is224);
void mbedtls_sha256_init(mbedtls_sha256_context *ctx);
int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224);
int mbedtls_sha256_update(mbedtls_sha256_context *ctx, const unsigned char *input, size_t ilen);
int mbedtls_sha256_finish(mbedtls_sha256_context *ctx, unsigned char *output);
void mbedtls_sha256_free(mbedtls_sha256_context *ctx);
#ifdef __cplusplus
}
#endif
