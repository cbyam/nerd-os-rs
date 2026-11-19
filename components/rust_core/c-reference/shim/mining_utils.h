#pragma once
/* Declarations for coinbase_decoder.c; implemented by c-mining/mining_utils.cpp. */
#include <stddef.h>
#include <stdint.h>
size_t hex2bin(const char *hex, uint8_t *bin, size_t bin_len);
size_t bin2hex(const uint8_t *buf, size_t buflen, char *hex, size_t hexlen);
