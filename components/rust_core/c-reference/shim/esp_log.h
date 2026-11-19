#pragma once
#define ESP_LOGE(tag, ...) ((void) (tag))
#define ESP_LOGW(tag, ...) ((void) (tag))
#define ESP_LOGI(tag, ...) ((void) (tag))
#define ESP_LOGD(tag, ...) ((void) (tag))
#define ESP_LOGV(tag, ...) ((void) (tag))
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, len, level) ((void) (tag))
#define ESP_LOG_BUFFER_HEX(tag, buf, len) ((void) (tag))
