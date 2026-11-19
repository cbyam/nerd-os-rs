/* The former parsing of the mempool API responses, verbatim from
 * main/tasks/apis_task.cpp (before the port); the members of APIsFetcher
 * are the fields of RefApis. */
#include <cstdint>

#include "ArduinoJson.h"

#define ESP_LOGI(...) ((void) 0)
static const char *TAG = "apis";

struct RefApis {
    uint32_t m_bitcoinPrice;
    uint32_t m_blockHeigh;
    uint64_t m_netHash;
    uint64_t m_netDifficulty;
    uint32_t m_hourFee;
    uint32_t m_halfHourFee;
    uint32_t m_fastestFee;
};

static RefApis *self;
#define m_bitcoinPrice self->m_bitcoinPrice
#define m_blockHeigh self->m_blockHeigh
#define m_netHash self->m_netHash
#define m_netDifficulty self->m_netDifficulty
#define m_hourFee self->m_hourFee
#define m_halfHourFee self->m_halfHourFee
#define m_fastestFee self->m_fastestFee

// Parse Bitcoin price
static bool parseBitcoinPrice(JsonDocument &doc) {
    m_bitcoinPrice = doc["USD"].as<uint32_t>();
    ESP_LOGI(TAG, "Bitcoin price in USD: %lu", m_bitcoinPrice);
    return true;
}

// Parse Bloack Height
static bool parseBlockHeight(JsonDocument &doc) {
    m_blockHeigh = doc.as<uint32_t>();
    ESP_LOGI(TAG, "Current block: %lu", m_blockHeigh);
    return true;
}

static bool parseHashrate(JsonDocument &doc) {
    double rawHash = doc["currentHashrate"].as<double>();  // Read as floating point
    double rawDiff = doc["currentDifficulty"].as<double>();  // Read as floating point

    m_netHash = static_cast<uint64_t>(rawHash / 1e18); // Convert to EH/s
    m_netDifficulty = static_cast<uint64_t>(rawDiff / 1e12); // Convert to T

    ESP_LOGI(TAG, "Network hash: %llu EH/s", m_netHash);
    ESP_LOGI(TAG, "Network difficulty: %llu T", m_netDifficulty);

    return true;
}


static bool parseFees(JsonDocument &doc) {
    m_hourFee = doc["hourFee"].as<uint32_t>();
    m_halfHourFee = doc["halfHourFee"].as<uint32_t>();
    m_fastestFee = doc["fastestFee"].as<uint32_t>();
    ESP_LOGI(TAG, "Network fees: %lu, %lu, %lu", m_hourFee, m_halfHourFee, m_fastestFee);
    return true;
}



extern "C" bool ref_apis_parse(int type, const char *buf, RefApis *values)
{
    self = values;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        return false;
    }
    switch (type) {
    case 0: return parseBitcoinPrice(doc);
    case 1: return parseBlockHeight(doc);
    case 2: return parseHashrate(doc);
    default: return parseFees(doc);
    }
}
