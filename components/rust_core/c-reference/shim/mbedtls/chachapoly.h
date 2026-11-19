#pragma once
/* See sha256.h: host stand-ins. */
#include <stddef.h>
typedef struct {
    unsigned char key[32];
} mbedtls_chachapoly_context;
void mbedtls_chachapoly_init(mbedtls_chachapoly_context *ctx);
int mbedtls_chachapoly_setkey(mbedtls_chachapoly_context *ctx, const unsigned char key[32]);
int mbedtls_chachapoly_encrypt_and_tag(mbedtls_chachapoly_context *ctx, size_t length, const unsigned char nonce[12],
                                       const unsigned char *aad, size_t aad_len, const unsigned char *input,
                                       unsigned char *output, unsigned char tag[16]);
int mbedtls_chachapoly_auth_decrypt(mbedtls_chachapoly_context *ctx, size_t length, const unsigned char nonce[12],
                                    const unsigned char *aad, size_t aad_len, const unsigned char tag[16],
                                    const unsigned char *input, unsigned char *output);
void mbedtls_chachapoly_free(mbedtls_chachapoly_context *ctx);
