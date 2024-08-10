#include "hashing.h"

#include <cstring>

#include "mbedtls/sha256.h"
#include "esp_log.h"

#define TAG "hashing"

void hsh::hash_bytes(const uint8_t* data, size_t len, uint8_t* result) {
    std::memset(result, 0, 32);  // clear to 0 to have defined state after call even if hashing failed
    
    int res = mbedtls_sha256(data, len, result, 0);
    if(res != 0) {
        ESP_LOGE(TAG, "could not hash data (data=%p,len=%u): %d", data, len, res);
    }
}