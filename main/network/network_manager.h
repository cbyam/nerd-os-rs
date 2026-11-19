#pragma once

// The network (Rust, components/rust_core/src/network.rs): what the C++
// code still reads.
extern "C" {
// the station MAC ("" before WiFi is up)
const char *network_mac_addr(void);
// the access point's RSSI (dBm), -90 if not connected
int network_wifi_rssi(void);
}
