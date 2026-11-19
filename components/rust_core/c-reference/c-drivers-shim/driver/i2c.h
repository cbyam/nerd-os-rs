/* host build: the ESP-IDF I2C command-link API, recorded by
 * cpp-shim/i2c_stub.cpp as SMBus operations on a fake register file */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_ERR_NOT_FOUND
#define ESP_ERR_NOT_FOUND 0x105
#endif
#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT 0x107
#endif

typedef int i2c_port_t;
typedef void *i2c_cmd_handle_t;
typedef enum { I2C_MASTER_ACK = 0, I2C_MASTER_NACK = 1, I2C_MASTER_LAST_NACK = 2 } i2c_ack_type_t;
#define I2C_MASTER_WRITE 0
#define I2C_MASTER_READ 1
typedef uint32_t TickType_t;
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
#endif
#define GPIO_NUM_10 10

#ifdef __cplusplus
extern "C" {
#endif
i2c_cmd_handle_t i2c_cmd_link_create(void);
void i2c_cmd_link_delete(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_start(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool ack_en);
esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd, uint8_t *data, i2c_ack_type_t ack);
esp_err_t i2c_master_read(i2c_cmd_handle_t cmd, uint8_t *data, size_t len, i2c_ack_type_t ack);
esp_err_t i2c_master_cmd_begin(i2c_port_t port, i2c_cmd_handle_t cmd, TickType_t ticks);
#ifdef __cplusplus
}
#endif
