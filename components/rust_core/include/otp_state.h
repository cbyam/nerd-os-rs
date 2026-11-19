/*
 * The OTP state (implemented in Rust, components/rust_core/src/otp.rs):
 * TOTP secret, session key and boot ID, enabled and enrollment state,
 * persisted in the configuration; TOTP check with replay protection,
 * session tokens, the otpauth URI of an enrollment. Thread-safe.
 */

#ifndef OTP_STATE_H
#define OTP_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void otp_load(void);
void otp_save(void);
bool otp_is_enabled(void);
void otp_set_enabled(bool enabled);
bool otp_is_initialized(void);
bool otp_enrollment_active(void);
void otp_disable_enrollment(void);

/* 0: started, uri holds the otpauth URI for the QR code; 1: already active;
 * 2: OTP enabled; -1: the URI does not fit. */
int otp_start_enrollment(const char *hostname, const char *mac, const char *model, char *uri, size_t cap);
bool otp_validate(const char *token, int64_t now, bool time_synced);
/* the token's length, 0 without a session key */
size_t otp_mint(uint32_t ttl, int64_t now, char *out, size_t cap);
bool otp_verify_session(const char *token, size_t len, int64_t now);
/* A protected request: session token (NULL if absent; not with force) or
 * TOTP (NULL if absent), behind the rate limit; without OTP (and without
 * force) everything passes. 0 ok, 1 blocked, 2 failed (counted). */
int otp_request_check(bool force, const char *session, const char *totp, int64_t now, bool time_synced,
                      uint64_t now_ms);
/* The TOTP login of /api/v2/otp/session, the same outcomes. */
int otp_login_check(const char *totp, int64_t now, bool time_synced, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* OTP_STATE_H */
