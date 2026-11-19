/* host build: the firmware's register-level I2C API (cpp-shim/i2c_stub.cpp) */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "driver/i2c.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t i2c_master_register_read(uint8_t device_address, uint8_t reg_addr, uint8_t *data, size_t len);
esp_err_t i2c_master_register_write_byte(uint8_t device_address, uint8_t reg_addr, uint8_t data);
esp_err_t i2c_master_register_write_word(uint8_t device_address, uint8_t reg_addr, uint16_t data);
#ifdef __cplusplus
}
#endif
