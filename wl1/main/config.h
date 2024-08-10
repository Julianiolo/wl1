#ifndef __WL1_CONFIG_H__
#define __WL1_CONFIG_H__

#include "esp_log.h"

// TODO
#define WL1_PIN_SDA 0
#define WL1_PIN_SCL 0

#define WL1_PIN_MISO 0
#define WL1_PIN_MOSI 0
#define WL1_PIN_CLK 0

#define WL1_PIN_SD_CS GPIO_NUM_0


// Internet:
#define WL1_ASWER_TIME 1000


#define PRIuSIZE "u"

#define WL1_DEBUG 1

#if WL1_DEBUG
#define WL1_DLOG(_msg_) ESP_LOGD("WL1", _msg_)
#define WL1_DLOGF(_msg_, ...) ESP_LOGD("WL1", _msg_, __VA_ARGS__)
#else
#define WL1_DLOG(_msg_) // do nothing
#define WL1_DLOGF(_msg_, ...) // do nothing
#endif

struct ERR {
    uint8_t error_code = 0;
    uint8_t errno_num = 0;
};

#endif