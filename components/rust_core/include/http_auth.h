/*
 * Authentication gate of the HTTP API (implemented in Rust,
 * components/rust_core/src/http_auth.rs): TOTP codes, session tokens, the
 * failed-attempt rate limit and the private-network check. Secrets, replay
 * state and the session key stay in the OTP class / NVS.
 */

#ifndef HTTP_AUTH_H
#define HTTP_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RFC 4648 Base32 (uppercase, no padding) into out (NUL-terminated);
 * returns its length, or 0 if it does not fit into cap bytes. */
size_t auth_base32_encode(const uint8_t *data, size_t len, char *out, size_t cap);
/* Base32 decode (either case, other characters skipped); returns the number
 * of bytes, or 0 if they do not fit into cap bytes. */
size_t auth_base32_decode(const char *text, size_t len, uint8_t *out, size_t cap);

/* Checks a 6-digit TOTP code (spaces ignored) against the Base32 secret at
 * Unix time now (30 s steps, +-1 step); a code is accepted once per step.
 * base_step/mask: the replay state, updated in place. */
bool auth_totp_verify(const char *code, const char *secret_b32, int64_t now, int64_t *base_step, uint8_t *mask);

/* Session token for ttl seconds (NUL-terminated into out, 73 bytes + NUL);
 * returns its length, or 0 (ttl 0, out too small). */
size_t auth_session_mint(const uint8_t key[32], uint32_t boot_id, int64_t now, uint32_t ttl, char *out, size_t cap);
/* Valid signature (constant-time compare), not expired, from this boot ID. */
bool auth_session_verify(const uint8_t key[32], uint32_t boot_id, int64_t now, const char *token, size_t len);

/* Failed attempts: 5 within 60 s block for 5 minutes (one limit for all
 * endpoints). auth_rate_limit_failure() returns false if it starts a block. */
bool auth_rate_limit_blocked(uint64_t now_ms);
bool auth_rate_limit_failure(uint64_t now_ms);

/* Whether a request may use the API: always in AP mode; otherwise the
 * requester's IPv4 address (network byte order) must be private, and an
 * Origin header (NULL: none) must name the device (hostname), localhost, a
 * .local host or a private IPv4 address. */
bool auth_network_allowed(bool ap_mode, uint32_t request_ip_be, const char *origin, const char *hostname);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_AUTH_H */
