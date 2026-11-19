/*
 * Job math shared by the Stratum protocols (implemented in Rust,
 * components/rust_core/src/mining.rs).
 */

#ifndef MINING_RS_H
#define MINING_RS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The hash fields of a bm_job: for hashing (merkle_root, prev_block_hash)
 * and for the ASIC drivers (*_be). */
typedef struct
{
    uint8_t merkle_root[32];
    uint8_t merkle_root_be[32];
    uint8_t prev_block_hash[32];
    uint8_t prev_block_hash_be[32];
} mining_job_hashes_t;

/* Difficulty of the block header hash for nonce (0 hash: infinity), from the
 * hashing-order fields of a job. Same result as the former test_nonce_value(). */
double mining_nonce_difficulty(uint32_t version, const uint8_t prev_block_hash[32], const uint8_t merkle_root[32],
                               uint32_t ntime, uint32_t nbits, uint32_t nonce);

#ifdef __cplusplus
}
#endif

#endif /* MINING_RS_H */
