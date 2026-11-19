# Rust port plan: Stratum v1 and v2

Goal: move the complete Stratum stack (v1 and v2) from C/C++ to Rust in
`components/rust_core`. The firmware must build and mine after every step,
so the port goes one layer at a time. Each step is one PR.

Constraints, which apply to the whole port:

- Rust `std` on ESP-IDF. No Embassy or esp-hal.
- The LVGL display code stays in C/C++.
- Builds run in Docker (`docker/`).

## Approach

**Sans-I/O protocol code.** The protocol logic in Rust never touches
sockets. It takes received bytes or lines and returns messages to send and
events for the rest of the firmware ("new job", "share accepted", ...).
This lets the host tests run complete pool sessions (setup, jobs,
reconnect, malformed input) without hardware, and it keeps the I/O layer
swappable.

**Bottom-up.** First the pure codecs (parsing, encoding, hashing), then the
session state machines on top, and the I/O and FreeRTOS tasks last. Until
the last steps, the existing C++ classes stay in place as thin wrappers that
call into Rust through a C ABI. Their callers do not change.

**Verified against the C/C++ code.** Like the coinbase decoder, every codec
gets a differential test: the old C/C++ implementation is compiled for the
host (ArduinoJson is header-only, so this works for the v1 parser too) and
fed the same random and mutated inputs as the Rust version. Malformed input
has defined, documented behaviour in Rust.

**Memory.** The C++ code allocates protocol buffers in PSRAM (`MALLOC` →
`_malloc_psram`). Rust allocations go through `malloc`, so into internal RAM.
Step 1 adds a global allocator that prefers PSRAM for Rust, using
`heap_caps_malloc_prefer`.

## Steps

| # | Step | Replaces | Verification |
|---|---|---|---|
| 1 | **SV1 codec:** JSON-RPC parsing of all pool messages, request formatting with correct JSON escaping, line framing. PSRAM allocator. | `StratumApi` (`stratum_api.cpp`) except socket I/O; `dispatch()` gets the raw line instead of a `JsonDocument` | differential test against `StratumApi::parse` (C++ with ArduinoJson on the host); hardware test |
| 2 | **SV1 job construction:** coinbase assembly, extranonce2, merkle root, `bm_job` fields | `MiningInfoV1::buildBmJob`, `calculate_merkle_root_hash`, `construct_bm_job` | differential test; known mainnet blocks as vectors; hardware test (accepted shares) |
| 3 | **SV1 session:** message IDs, matching responses to the requests sent, setup/submit request building. The connection loop itself (`protocolLoop`: send, receive, flags) is I/O and moves with step 8. | ID handling in `StratumApi`, fixed-ID classification in the parser | session unit tests (incl. concurrent use), C++-level test against the former `StratumApi`; hardware test |
| 4 | **SV2 codec:** frame header, all message builders and parsers, `sv2_target_to_pdiff` | `components/stratum_v2/sv2_protocol.c` | differential test |
| 5 | **Noise NX:** handshake (EllSwift, Schnorr certificate check), ChaCha20-Poly1305 transport | `components/stratum_v2/sv2_noise.c` | differential test with deterministic keys; handshake against a real SV2 pool |
| 6 | **SV2 session and jobs:** channel setup checks, pending jobs and prevhash, extranonce prefix per job, target, submits; job construction for standard and extended channels. The connection loop and the calls into the manager stay in C++ (events from the session) until steps 7/8. | message handling in `stratum_task_v2.cpp`, `mining_info_v2.cpp` job math | session scenario tests; differential test of the job construction; hardware test against an SV2 pool |
| 7 | **Pool manager decisions:** failover and dual-pool pool selection, job dithering, ASIC difficulty, coinbase verification. The managers keep mutex, tasks, config, alerts and the JSON API and carry out the decisions. | logic in `stratum_manager*.cpp` | differential test against the verbatim former logic (random event sequences); hardware test of failover and dual pool |
| 8 | **I/O and tasks**, in three parts: **8a** v1 connection loop (setup, receive, send loops of `StratumApi`), **8b** v2 connection loop, **8c** the common task loop (config, DNS, connect, reconnect delay, error counting) | `StratumTaskV1/V2::protocolLoop`, `StratumApi` I/O, `stratum_task.cpp` (base) | scripted socket/hook scenario tests; the framing/request differential tests run through the new loops; hardware tests: TCP, TLS, SV2, network loss |

**Status: done.** All eight steps are merged and hardware-tested step by
step; what is left in C++ for Stratum is the transport (`esp_transport`, TLS,
Noise framing calls), the glue to tasks, config, alerts and the JSON API, and
the event handling into the job pipeline. The `StratumApi` class is gone:
`StratumTaskV1` holds the Rust session and line buffer, shares go out with
`stratum_v1_submit()`, the manager parses with `stratum_v1_session_parse()`.

### ASIC drivers (after the Stratum plan)

`components/bm1397` (BM1366/1368/1370/1373), same approach: the former
drivers are compiled for the host with a scripted UART (`c-asic/`,
`cpp-shim/serial_stub.cpp`), and every byte sent to the chips is compared
(`tests/asic.rs`), including whole init sequences per chip.

| Step | Content |
|---|---|
| A1 | protocol: frames and CRC, job and nonce ID mapping per chip, response parsing, ticket mask, version rolling register (`src/asic/mod.rs`) |
| A2 | frequency: PLL divider search and the 6.25 MHz ramp |
| A3 | chip init sequences and the driver (`src/asic/driver.rs`, `asic_driver.h`); the UART (`serial.cpp`) behind `asic_uart_t`, the C++ `Asic` class a thin facade, the chip classes only a constructor |
| A4 | optional: `asic_result_task` / `create_jobs_task` |

Hardware tests on the Q1373 (BM1373) first; the other boards later.

### Pool manager (after the HTTP API)

The manager's decisions are in Rust since step 7 (`pool.rs`); its state,
statistics and the message dispatch are still C++. The goal is a thin
facade like the ASIC driver.

| Step | Content |
|---|---|
| M1 | statistics: shares, pool and network difficulty, session and all-time best, found blocks, last response (`src/pool_stats.rs`, `pool_stats.h`) |
| M2 | coinbase processing and verification state: extranonce, decoded coinbase, counters, block reason (`src/pool_coinbase.rs`, `pool_coinbase.h`); the alert, reconnect and fault flag stay C++, driven by the returned outcome |
| M3 | the manager itself (pools, timers, dispatch); `StratumManagerFallback/DualPool` become thin or go away |

### HTTP API: inputs from the network (after the ASIC drivers)

`esp_http_server` stays the transport, and responses that only report
internal state (the GET handlers) stay in C++. What moves to Rust is what
processes data from the network, in this order:

| Step | Content |
|---|---|
| H1 | authentication gate: TOTP, session tokens, failed-attempt rate limit, private-network check (`src/http_auth.rs`, `http_auth.h`) |
| H2 | request bodies, part 1: the settings endpoints (`PATCH /api/system`, `PATCH /api/v2/settings`); JSON read by a port of ArduinoJson's deserializer (`src/ajson.rs`) into typed writes (`src/http_settings.rs`), applied by `settings_apply.cpp` |
| H2b | request bodies, part 2: influx, alert, OTP and CAN node bodies (`src/http_settings.rs`), CAN node IDs and actions from the URI, history query parameters and `allTime` (`src/http_query.rs`, `http_query.h`) |

JSON from outside (pool lines and HTTP bodies) is read with one reader,
`src/ajson.rs`, a port of the ArduinoJson 7.3 deserializer, so it accepts
exactly what the C++ code accepted. Stratum v1 used `serde_json` from step 1
until this consolidation; that was stricter than before (a pool line with
single quotes, unquoted keys or text after the object dropped the
connection) and cost 37 KB of flash. JSON output stays ArduinoJson in C++.
| H3 | OTA: receiving uploads (`src/http_ota.rs`, `http_ota.h`), the factory update body and URL check; the factory image itself is not parsed (fixed offsets), the download stays C++ |
| H4 | not ported: files are served from SPIFFS, which has no directories (a `..` in the URL is only a file name that does not exist), and the WebSocket only receives a close frame |

Other data from outside: the mempool API responses (price, block height,
hashrate, fees; from the internet, any configured server) are parsed in
Rust (`src/apis.rs`, `apis.h`), the HTTP client stays C++.

Tests as before: the former code compiled for the host (verbatim copies in
`c-reference/c-auth/` and later the handlers with a config stand-in that
records every setter call), random and malformed inputs, plus the RFC test
vectors for TOTP and HMAC.

Steps 1 to 3 cover v1 completely, apart from I/O. Steps 4 to 6 do the same
for v2.

### Decision for step 8 (decided 2026-11-16: option A)

There are two options for sockets and TLS:

- **A: Keep the C++ transport** (`esp_transport`, TLS with the pinned CA
  bundle) behind a small C ABI. Rust then owns everything except the socket.
  This needs no new build dependencies.
- **B: esp-idf-svc / esp-idf-sys.** This is idiomatic, but needs bindgen
  against the ESP-IDF headers inside the CMake build, ties us to esp-idf-svc
  releases, and the pinned CA bundle setup has to be rebuilt.

Decided: A. B can follow when other parts of the firmware
(HTTP server, NVS, drivers) need esp-idf-svc anyway. Plain TCP already works
with `std::net` today, because lwIP provides BSD sockets.

### Crypto in step 5 (decided)

libsecp256k1 (EllSwift, Schnorr) is the existing C component, bound through
FFI (`src/secp.rs`); the `secp256k1` crate would bring a second copy with its
precomputed tables. SHA-256 is mbedTLS (hardware accelerated), HMAC/HKDF are
built on it, and ChaCha20-Poly1305 is the RustCrypto crate. The step cost
+6.4 KB.

## Process

- One branch and one PR per step. The PR description records what was
  replaced, how it was verified, the change in firmware size, any new C
  findings and what is left open.
- Merge requirements: host tests green, clippy and rustfmt clean, and the
  firmware builds in `docker/`. A hardware test is noted in the PR. Until
  it is done, the PR says "hardware test pending".
