#ifndef __WL1_SD_H__
#define __WL1_SD_H__

#include <cstdint>
#include "esp_err.h"

#define SD_MOUNT_POINT "/sdcard"

namespace sd {
    uint8_t init();

    uint8_t deinit();

    // sets errno on error
    uint8_t appendToFile(const char *filename, const uint8_t *data, size_t size);
}

#endif