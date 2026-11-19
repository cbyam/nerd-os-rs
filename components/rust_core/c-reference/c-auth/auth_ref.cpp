/* The former authentication code, verbatim, for the differential tests
 * (tests/http_auth.rs). Member functions became free functions (ref_*);
 * now()/now_ms() and the request's address, Origin header and hostname are
 * parameters. Sources: main/otp/otp.cpp, main/http_server/http_utils.cpp,
 * main/http_server/http_cors.cpp (before ASIC step H1), and lwIP's
 * ip4addr_aton()/ipaddr_addr() for inet_addr() on the device. */
#include <arpa/inet.h>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

#include "mbedtls/md.h"

/*** RFC4648 Base32 (uppercase, no padding) ***/
static const char B32_ALPH[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// Pack a minimal payload to avoid JSON parsing.
#pragma pack(push, 1)
struct SessionPayload
{
    uint32_t iat; // issue-at (unix)
    uint32_t exp; // expiry (unix)
    uint32_t bid; // boot-id (random per boot)
};
#pragma pack(pop)

static bool parse_otp6(const std::string &s, uint32_t &out)
{
    // Accept exactly 6 digits (ignore spaces)
    out = 0;
    int cnt = 0;
    for (char c : s) {
        if (c == ' ')
            continue;
        if (!std::isdigit((unsigned char) c))
            return false;
        out = out * 10u + (uint32_t) (c - '0');
        ++cnt;
        if (cnt > 6)
            return false;
    }
    return cnt == 6;
}

/*** Base32 (RFC4648, uppercase, no padding) decode; returns bytes written or 0 on error ***/
static size_t base32_decode(const char *in, size_t inlen, uint8_t *out, size_t outcap)
{
    auto val = [](int ch) -> int {
        if (ch >= 'A' && ch <= 'Z')
            return ch - 'A';
        if (ch >= 'a' && ch <= 'z')
            return ch - 'a';
        if (ch >= '2' && ch <= '7')
            return ch - '2' + 26;
        return -1;
    };
    uint32_t buf = 0;
    int bits = 0;
    size_t o = 0;
    for (size_t i = 0; i < inlen; ++i) {
        int v = val((unsigned char) in[i]);
        if (v < 0)
            continue; // skip whitespace/padding if any
        buf = (buf << 5) | (uint32_t) v;
        bits += 5;
        if (bits >= 8) {
            if (o >= outcap)
                return 0;
            out[o++] = (uint8_t) ((buf >> (bits - 8)) & 0xFF);
            bits -= 8;
        }
    }
    return o;
}

static std::string base32_encode(const uint8_t *in, size_t len)
{
    std::string out;
    out.reserve((len * 8 + 4) / 5);
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < len; ++i) {
        buf = (buf << 8) | in[i];
        bits += 8;
        while (bits >= 5) {
            out.push_back(B32_ALPH[(buf >> (bits - 5)) & 0x1F]);
            bits -= 5;
        }
    }
    if (bits > 0) {
        out.push_back(B32_ALPH[(buf << (5 - bits)) & 0x1F]);
    }
    return out;
}

static bool hmac_sha256(const uint8_t *key, size_t keylen, const uint8_t *msg, size_t mlen, uint8_t mac32[32])
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info)
        return false;
    return mbedtls_md_hmac(info, key, keylen, msg, mlen, mac32) == 0;
}

/*** HOTP (RFC4226) using HMAC-SHA1 ***/
static bool hotp_sha1(const uint8_t *key, size_t keylen, uint64_t counter, uint32_t &out_code, int digits = 6)
{
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = (uint8_t) (counter & 0xFF);
        counter >>= 8;
    }

    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!info)
        return false;

    uint8_t mac[20];
    if (mbedtls_md_hmac(info, key, keylen, msg, sizeof(msg), mac) != 0)
        return false;

    int off = mac[19] & 0x0F;
    uint32_t bin = ((mac[off] & 0x7F) << 24) | ((mac[off + 1] & 0xFF) << 16) | ((mac[off + 2] & 0xFF) << 8) | (mac[off + 3] & 0xFF);
    uint32_t mod = 1;
    for (int i = 0; i < digits; i++)
        mod *= 10;
    out_code = bin % mod;
    return true;
}

/*** TOTP verify with +/-1 window and 3-bit replay mask ***/
static bool totp_verify_window(const uint8_t *key, size_t keylen, time_t now, int period, int digits, uint32_t user_code,
                               int64_t &io_base_step, uint8_t &io_mask)
{
    int64_t step = now / period;

    // Slide window forward if time advanced beyond base+1
    if (io_base_step == 0 || step > io_base_step + 1) {
        io_base_step = step;
        io_mask = 0;
    } else if (step < io_base_step - 1) {
        return false; // far too old
    }

    for (int off = -1; off <= 1; ++off) {
        int64_t s = step + off;
        int idx = (int) (s - (io_base_step - 1)); // 0,1,2 map to base-1, base, base+1
        if (idx < 0 || idx > 2)
            continue;
        if (io_mask & (1u << idx))
            continue; // replay

        uint32_t code = 0;
        if (!hotp_sha1(key, keylen, (uint64_t) s, code, digits))
            return false;
        if (code == user_code) {
            io_mask |= (1u << idx); // mark used
            return true;
        }
    }
    return false;
}

// OTP::mintSessionToken (m_sessKey, m_bootId, now() as parameters)
static std::string mintSessionToken(const uint8_t m_sessKey[32], uint32_t m_bootId, time_t ts, uint32_t ttl_sec)
{
    if (!ttl_sec) {
        return {};
    }

    SessionPayload pl{};
    pl.iat = (uint32_t) ts;
    pl.exp = (uint32_t) (ts + ttl_sec);
    pl.bid = m_bootId;

    // Encode payload as Base32 (reuse your encoder)
    const uint8_t *pbytes = reinterpret_cast<const uint8_t *>(&pl);
    std::string payload_b32 = base32_encode(pbytes, sizeof(pl));

    // HMAC-SHA256 over the Base32 payload (exact string)
    uint8_t mac[32];
    if (!hmac_sha256(m_sessKey, 32, (const uint8_t *) payload_b32.data(), payload_b32.size(), mac)) {
        return {};
    }
    // sig_b32 has length of `ceil(32 / 5 * 8) = 52`
    std::string sig_b32 = base32_encode(mac, 32);

    std::string token;
    token.reserve(payload_b32.size() + 1 + sig_b32.size());
    token.append(payload_b32);
    token.push_back('.');
    token.append(sig_b32);
    return token;
}

// OTP::verifySessionToken (m_sessKey, m_bootId, now() as parameters)
static bool verifySessionToken(const uint8_t m_sessKey[32], uint32_t m_bootId, time_t ts, const std::string &token)
{
    if (token.empty())
        return false;

    // 1) Split "payload.sig"
    const char *dot = (const char *) memchr(token.data(), '.', token.size());
    if (!dot)
        return false;
    size_t p_len = (size_t) (dot - token.data());
    size_t s_len = token.size() - p_len - 1;
    if (p_len == 0 || s_len == 0)
        return false;

    // 2) Base32-decode payload and signature
    uint8_t pbuf[32]; // SessionPayload is 4+4+4 = 12 bytes
    size_t plen = base32_decode(token.data(), p_len, pbuf, sizeof(pbuf));
    if (plen != sizeof(SessionPayload))
        return false;

    uint8_t sref[32];
    size_t sref_len = base32_decode(dot + 1, s_len, sref, sizeof(sref));
    if (sref_len != 32)
        return false;

    // 3) Recompute HMAC over base32(payload)
    uint8_t mac[32];
    if (!hmac_sha256(m_sessKey, 32, (const uint8_t *) token.data(), p_len, mac)) {
        return false;
    }

    if (memcmp(mac, sref, 32) != 0)
        return false;

    // 4) Check exp/iat/time and boot-id
    const SessionPayload *pl = reinterpret_cast<const SessionPayload *>(pbuf);

    if ((time_t) pl->exp <= ts)
        return false;
    if ((time_t) pl->iat > ts + 300)
        return false; // future issue time? reject with 5 min skew

    if (pl->bid != m_bootId)
        return false; // token minted on prior boot -> reject

    return true;
}

// --- http_utils.cpp rate limit (now_ms() as parameter) ---

#define RL_FAIL_LIMIT      5            // allowed wrong tries in window
#define RL_WINDOW_SEC      60           // 1 min window
#define RL_BLOCK_SEC       300          // 5 min blocking time

static uint64_t timestamps[RL_FAIL_LIMIT] = {};
static uint64_t block_exp_time = 0;

// return true if currently blocked, false otherwise
static bool isBlocked(uint64_t ts) {
    // if block_exp_time is set and lies in the future -> we are blocked
    if (block_exp_time && block_exp_time > ts) {
        return true;
    }
    return false;
}

// most pragmatic approach of /some/ bruteforce protection
static bool rateLimit(uint64_t ts) {
    // If a block existed but has expired, clear it and reset failure history.
    if (block_exp_time && block_exp_time <= ts) {
        block_exp_time = 0;
        memset(timestamps, 0, sizeof(timestamps));
    }

    // Rotate the timestamps array one position towards the end (keep newest at index 0).
    // Walk backwards to avoid overwriting values before they are copied.
    for (int i = RL_FAIL_LIMIT - 1; i > 0; i--) {
        timestamps[i] = timestamps[i - 1];
    }
    timestamps[0] = ts; // store newest failure timestamp at index 0

    // If we have a full buffer of failure timestamps, check the time span
    // between the newest and the oldest entry. If it's within the window,
    // set the block expiry and deny further attempts.
    if (timestamps[RL_FAIL_LIMIT - 1] != 0) {
        uint64_t delta = timestamps[0] - timestamps[RL_FAIL_LIMIT - 1]; // newest - oldest
        if (delta < RL_WINDOW_SEC * 1000ull) {
            // block for RL_BLOCK_SEC (store expiry as ms)
            block_exp_time = ts + (RL_BLOCK_SEC * 1000ull);
            // optional: clear history immediately to avoid repeated re-blocking logic
            memset(timestamps, 0, sizeof(timestamps));
            return false; // blocked
        }
    }

    return true; // allowed
}

// --- lwIP ip4addr_aton() / ipaddr_addr(), the inet_addr() of the device ---

typedef uint32_t u32_t;
typedef uint8_t u8_t;
#define lwip_isdigit(c) isdigit((unsigned char) (c))
#define lwip_isxdigit(c) isxdigit((unsigned char) (c))
#define lwip_islower(c) islower((unsigned char) (c))
#define lwip_isspace(c) isspace((unsigned char) (c))

static int ip4addr_aton(const char *cp, u32_t *addr)
{
  u32_t val;
  u8_t base;
  char c;
  u32_t parts[4];
  u32_t *pp = parts;

  c = *cp;
  for (;;) {
    if (!lwip_isdigit(c)) {
      return 0;
    }
    val = 0;
    base = 10;
    if (c == '0') {
      c = *++cp;
      if (c == 'x' || c == 'X') {
        base = 16;
        c = *++cp;
      } else {
        base = 8;
      }
    }
    for (;;) {
      if (lwip_isdigit(c)) {
        if((base == 8) && ((u32_t)(c - '0') >= 8))
          break;
        val = (val * base) + (u32_t)(c - '0');
        c = *++cp;
      } else if (base == 16 && lwip_isxdigit(c)) {
        val = (val << 4) | (u32_t)(c + 10 - (lwip_islower(c) ? 'a' : 'A'));
        c = *++cp;
      } else {
        break;
      }
    }
    if (c == '.') {
      if (pp >= parts + 3) {
        return 0;
      }
      *pp++ = val;
      c = *++cp;
    } else {
      break;
    }
  }
  if (c != '\0' && !lwip_isspace(c)) {
    return 0;
  }
  switch (pp - parts + 1) {
    case 0:
      return 0;       /* initial nondigit */
    case 1:             /* a -- 32 bits */
      break;
    case 2:             /* a.b -- 8.24 bits */
      if (val > 0xffffffUL) {
        return 0;
      }
      if (parts[0] > 0xff) {
        return 0;
      }
      val |= parts[0] << 24;
      break;
    case 3:             /* a.b.c -- 8.8.16 bits */
      if (val > 0xffff) {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff)) {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16);
      break;
    case 4:             /* a.b.c.d -- 8.8.8.8 bits */
      if (val > 0xff) {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff) || (parts[2] > 0xff)) {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
      break;
    default:
      break;
  }
  if (addr) {
    *addr = htonl(val);
  }
  return 1;
}

static u32_t lwip_inet_addr(const char *cp)
{
  u32_t val;
  if (ip4addr_aton(cp, &val)) {
    return val;
  }
  return INADDR_NONE;
}

// --- http_cors.cpp ---

// Returns true if the IPv4 address (in network byte order) is in RFC1918 private ranges
static bool ip_in_private_range(uint32_t address_be)
{
    uint32_t ip = ntohl(address_be); // convert to host byte order for compare

    // 10.0.0.0 - 10.255.255.255
    if (ip >= 0x0A000000 && ip <= 0x0AFFFFFF)
        return true;

    // 172.16.0.0 - 172.31.255.255
    if (ip >= 0xAC100000 && ip <= 0xAC1FFFFF)
        return true;

    // 192.168.0.0 - 192.168.255.255
    if (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF)
        return true;

    return false;
}

// Parse dotted IPv4 string (no port) to network-byte-order uint32_t
static uint32_t get_origin_ip(const char *host_without_port)
{
    return lwip_inet_addr(host_without_port); // network byte order
}

// Extract host from Origin header of form "http://host[:port]/..."
// inplace operation
static const char *extract_origin_host(char *origin)
{
    if (!origin)
        return NULL;

    const char *http_prefix = "http://";
    size_t http_len = strlen(http_prefix);

    // Only accept http:// (device does not serve https)
    if (strncmp(origin, http_prefix, http_len) != 0) {
        return NULL;
    }

    // Move past "http://"
    char *host = origin + http_len;

    // Cut at first slash to drop any path
    char *slash = strchr(host, '/');
    if (slash)
        *slash = '\0';

    // Cut optional :port (IPv4 / hostname form)
    char *colon = strchr(host, ':');
    if (colon)
        *colon = '\0';

    return host; // host is now [hostname OR IPv4 literal] without port/path
}

// Check "localhost"
static bool is_localhost(const char *host_wo_port)
{
    if (!host_wo_port)
        return false;
    return strcmp(host_wo_port, "localhost") == 0;
}

// Check "*.local"
static bool is_local(const char *host_wo_port)
{
    if (!host_wo_port)
        return false;

    const char *suffix = ".local";
    size_t len = strlen(host_wo_port);
    size_t suf_len = strlen(suffix);

    if (len < suf_len)
        return false;

    return strcmp(host_wo_port + len - suf_len, suffix) == 0;
}

// is_network_allowed(): AP state, client address, Origin header (NULL: none,
// at most 127 bytes as read before) and hostname as parameters
static bool is_network_allowed(bool ap_state, uint32_t request_ip_addr, const char *origin_header, const char *hostname)
{
    // AP mode: always allow
    if (ap_state) {
        return true;
    }

    char origin[128];
    uint32_t origin_ip_addr;

    if (origin_header) {
        strncpy(origin, origin_header, sizeof(origin) - 1);
        origin[sizeof(origin) - 1] = '\0';

        const char *host = extract_origin_host(origin);
        if (!host) {
            return false;
        }

        // Check against device hostname (match exact, no port)
        if (strcmp(host, hostname) == 0) {
            return true;
        }

        // Allow localhost / *.local cases
        if (is_localhost(host) || is_local(host)) {
            return true;
        }

        // Else, interpret host as IPv4 literal (must succeed to continue)
        origin_ip_addr = get_origin_ip(host);

    } else {
        // No Origin header → treat the client IP as origin
        origin_ip_addr = request_ip_addr;
    }

    // Final rule: both request IP and origin IP must be private RFC1918
    if (ip_in_private_range(origin_ip_addr) && ip_in_private_range(request_ip_addr)) {
        return true;
    }

    return false;
}

// --- C interface for the tests ---

extern "C" {
size_t ref_auth_base32_encode(const uint8_t *in, size_t len, char *out, size_t cap)
{
    std::string s = base32_encode(in, len);
    if (s.size() + 1 > cap)
        return 0;
    memcpy(out, s.c_str(), s.size() + 1);
    return s.size();
}
size_t ref_auth_base32_decode(const char *in, size_t len, uint8_t *out, size_t cap)
{
    return base32_decode(in, len, out, cap);
}
bool ref_auth_parse_otp6(const char *s, uint32_t *out)
{
    return parse_otp6(std::string(s), *out);
}
bool ref_auth_totp_verify(const uint8_t *key, size_t keylen, int64_t now, uint32_t code, int64_t *base_step,
                          uint8_t *mask)
{
    return totp_verify_window(key, keylen, (time_t) now, 30, 6, code, *base_step, *mask);
}
size_t ref_auth_session_mint(const uint8_t key[32], uint32_t boot_id, int64_t now, uint32_t ttl, char *out, size_t cap)
{
    std::string s = mintSessionToken(key, boot_id, (time_t) now, ttl);
    if (s.size() + 1 > cap)
        return 0;
    memcpy(out, s.c_str(), s.size() + 1);
    return s.size();
}
bool ref_auth_session_verify(const uint8_t key[32], uint32_t boot_id, int64_t now, const char *token, size_t len)
{
    return verifySessionToken(key, boot_id, (time_t) now, std::string(token, len));
}
bool ref_auth_rate_limit_blocked(uint64_t now_ms) { return isBlocked(now_ms); }
bool ref_auth_rate_limit_failure(uint64_t now_ms) { return rateLimit(now_ms); }
void ref_auth_rate_limit_reset(void)
{
    memset(timestamps, 0, sizeof(timestamps));
    block_exp_time = 0;
}
uint32_t ref_auth_inet_addr(const char *s) { return lwip_inet_addr(s); }
bool ref_auth_network_allowed(bool ap_state, uint32_t request_ip_be, const char *origin, const char *hostname)
{
    return is_network_allowed(ap_state, request_ip_be, origin, hostname);
}
}
