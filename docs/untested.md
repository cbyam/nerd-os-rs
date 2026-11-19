# Not tested on hardware

These parts of the Rust port were not run on hardware. They are covered
only by host tests: unit tests, and differential tests against the former
C/C++ code where those exist. Each entry names the PR, what is missing and
what a test would need. When an entry has been tested, delete it.

## Stratum

- **TLS pool connection** (`src/stratum_transport.rs`, `Kind::Tls`).
  No pool with `stratum+ssl` was configured, so only TCP (SV1) and Noise
  (SV2) ran on hardware. Needs a TLS pool, for example a TLS port of
  public-pool, with `stratumtls` set.
- **Failover switching** (`src/pool_manager.rs`). Only dual pool mode
  ran on the Q1373. Unit tests cover the switching to the secondary pool
  through the reconnect timer and the switching back when the primary
  returns. Needs failover mode with a primary that goes away, for example
  a `socat` relay to ckpool that is stopped and started again.
- **Coinbase verification fault**. This raises the board error when
  all configured pools are blocked by the verification. Needs a pool
  whose coinbase does not pay the configured address, or a deliberately
  wrong user address with verification mode 1.

## Display / system task

- **Error overlay** (`src/system.rs`). It is shown when the board
  error is set and hidden again when it clears. Needs a board error, for
  example the coinbase verification fault above, or a lowered overheat
  limit (only with the user's OK).
- **Found block overlay**. It is shown when the found-block counter
  increases. Needs a found block.
- **CAN slave label**. The display shows "CAN ID: n" or "CAN: ..."
  instead of the IP address. Needs a board in CAN slave mode.
- **Boards without a hashrate counter**. The current hashrate comes
  from the 10 min share average on these boards. Only the Q1373, which has
  a counter, was tested after S4.

## Boards

- **Boards other than the Q1373 and the NerdAxeGaia** (`src/board.rs`).
  All twelve boards build, and the init sequence on the Q1373 and the Gaia
  is the same, line for line, as before. Not run on hardware:
  - NerdQAxe+ and NerdOCTAXE+: control pins on GPIOs, chip temperatures
    from the ASICs every 15 s.
  - NerdQAxe++ including the rev7 TPS546 detection, NerdEKO and
    NerdHAXE-γ.
  - NerdQX: TMP451 mux on GPIO2/3; without the mux the limits drop to
    1150 mV / 495 MHz.
  - NerdOCTAXE-γ: VR detection on GPIO3, two TMP451 muxes.
  - Q1370: TMP451 mux on the FXL6408.
  - NerdAxe: DS4432U DAC with INA260.
  - NerdAxeGamma.

  Each needs a flash and a comparison with the previous firmware (log of
  the init sequence, `/api/system/info`).
- **Ethernet HAT detection on the NerdQAxe family**: the W5500 probe
  on the HAT pins, skipped when the CAN extension or a TMP451 mux uses
  them. Needs a NerdQAxe with an ethernet HAT.
- **CAN extension detection and slave pin on the NerdQAxe+ family**.
  Needs the CAN HAT.

## CAN

- **CAN master receive** and **CAN slave protocol**.
  Only differential tests ran. Needs a CAN setup with a
  master and at least one slave board.

## Power / boards

- **rev7 TPS546 path**. Only the NerdAxe path ran on
  hardware (Gamma). The rev7 path is covered by differential tests only.
  Needs a NerdQAxe++ rev7.

## OTA

- **Factory OTA with `keep_config=false`** (`src/ota_factory_task.rs`):
  the NVS erase after the update. Never run on the test boards (it wipes
  the configuration). The update itself with `keep_config=true` ran on the
  Q1373 (GitHub release v1.1.2).

## Network

- **Captive portal DNS from an access point client** (`src/dns.rs`).
  This was tested with `dig` over the LAN, where the server also answers
  with 192.168.4.1. Needs a phone or laptop on the device's access point
  (no WiFi configured).
- **WiFi health log, RSSI line** (`src/wifi_health.rs`). Tested on the
  Q1373 over Ethernet (lwIP counters, "WiFi not connected"); the
  RSSI / SSID / channel line needs a board on WiFi.

## CAN (`src/can.rs`, `src/http_can.rs`)

Tested on the Q1373 as a CAN master without other nodes:
- the TWAI driver starts;
- MASTER_BOOT is queued;
- the status lines appear;
- the settings commands are queued;
- the swarm API (nodes JSON, actions, PATCH, DELETE incl. errors) matches the C++ code.

Untested (needs a second board on the bus):
- slave negotiation (HELLO/ASSIGN, registry in NVS);
- slave jobs, nonces, telemetry and config;
- the settings commands applied on a slave;
- a bus-off recovery.

## Network (`src/w5500.rs`, `src/network.rs`, PRs N1/N2)

Tested on the Q1373 with the cable plugged in:
- W5500 setup, link up and DHCP over ETH;
- WiFi start (identical driver init) and the station connecting to the configured AP;
- the AP switched off and WiFi stopped once ETH has its IP;
- the default netif, MAC, RSSI and status texts.

Tested with the user on the Q1373 (2026-11-19):
- ETH cable pulled and plugged back (WiFi fallback and back to ETH);
- the access point portal with an empty NVS (AP up, captive DNS, WiFi saved through the portal, station connected after the reboot).

Still to test:
- the WiFi scan in the portal (`/api/v2/wifi/scan` with the AP active);
- the WiFi reconnect backoff after a lost AP;
- a board without Ethernet;
- an ETH init failure (cleanup path).

## GitHub updater on the Rust repository (`webflasher-rs`)

The web UI's updater lists the releases of `shufps/nerd-os-rs`, and the factory update only accepts URLs under that repository's release downloads (`http_ota.rs`, host-tested); the CI uploads to `webflasher-rs/` in the R2 bucket. Switching between the Rust and the C firmware is meant to go through the web flasher only.

Still to test (needs a first release of the Rust repository):
- the updater listing the `-rs` releases and the factory update from one of them;
- the CI upload into `webflasher-rs/`.
