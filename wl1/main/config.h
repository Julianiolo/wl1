#ifndef __WL1_CONFIG_H__
#define __WL1_CONFIG_H__

#include "esp_log.h"

// TODO
#define WL1_PIN_SDA 0
#define WL1_PIN_SCL 0


#define WL1_DEBUG 1

#if WL1_DEBUG
#define WL1_DLOG(_msg_) ESP_LOGD("WL1", _msg_)
#define WL1_DLOGF(_msg_, ...) ESP_LOGD("WL1", _msg_, __VA_ARGS__)
#else
#define WL1_DLOG(_msg_) // do nothing
#define WL1_DLOGF(_msg_, ...) // do nothing
#endif

#endif