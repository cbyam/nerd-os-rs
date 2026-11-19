/*
 * Responses of the mempool API (implemented in Rust,
 * components/rust_core/src/apis.rs), read with ArduinoJson's rules.
 */

#ifndef APIS_H
#define APIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APIS_PRICE = 0,        /* /api/v1/prices: {"USD": ...} */
    APIS_BLOCK_HEIGHT = 1, /* /api/blocks/tip/height: a number */
    APIS_HASHRATE = 2,     /* /api/v1/mining/hashrate/3d: currentHashrate, currentDifficulty */
    APIS_FEES = 3,         /* /api/v1/fees/recommended: hourFee, halfHourFee, fastestFee */
} apis_type_t;

typedef struct {
    uint32_t price_usd;
    uint32_t block_height;
    uint64_t net_hash;       /* EH/s */
    uint64_t net_difficulty; /* T */
    uint32_t hour_fee;
    uint32_t half_hour_fee;
    uint32_t fastest_fee;
} apis_values_t;

/* Parses a response into the fields of its type; false (values unchanged)
 * for invalid JSON. */
bool apis_parse(apis_type_t type, const char *body, size_t len, apis_values_t *values);

#ifdef __cplusplus
}
#endif

#endif /* APIS_H */
