#include "Arduino.h"
#include "Wire.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <cstdio>  // snprintf
#include <cinttypes> // PRIu16 etc.

#include "DS3231/DS3231.h"

/*
maybe need to set this, although we probably won't use it:

wl1/managed_components/espressif__arduino-esp32/libraries/WiFiClientSecure/src/ssl_client.cpp:24:4: warning: #warning "Please call `idf.py menuconfig` then go to Component config -> mbedTLS -> TLS Key Exchange Methods -> Enable pre-shared-key ciphersuites and then check `Enable PSK based cyphersuite modes`. Save and Quit." [-Wcpp]
   24 | #  warning "Please call `idf.py menuconfig` then go to Component config -> mbedTLS -> TLS Key Exchange Methods -> Enable pre-shared-key ciphersuites and then check `Enable PSK based cyphersuite modes`. Save and Quit."
      |    ^~~~~~~

*/

#include "config.h"
#include "sensors.h"
#include "sending/sending.h"
#include "data.h"


#define TAG "WL1"

DS3231 ds3231;

wl1::DataPoint meas_dp;

RTC_DATA_ATTR ERR last_sd_error;
RTC_DATA_ATTR ERR last_net_error;

struct Preferences {
    uint8_t send_every;
} preferences;

esp_sleep_wakeup_cause_t wakeup_reason;

static bool init_nvs() {
    esp_err_t ret;
    for(uint8_t i = 0; i<3; i++) {
        ret = nvs_flash_init();
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "nvs_flash_init failed: %d", ret);
        }
    }
    return ret == ESP_OK;
}

static bool init_wire(void) {
    for(uint8_t i = 0; i<3; i++) {
        if(Wire.begin(WL1_PIN_SDA, WL1_PIN_SCL)) {
            return true;
        }
        delay(10);
    }
    return false;
}

static void wakeup_reason_check(void) {
    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_UNDEFINED: { // not sure if this is the right wakeup reason for normal powerup
            WL1_DLOG("wakeup_reason undef");
        } break;
        default: { // any type of deep sleep wakeup (hopefully?)
            WL1_DLOGF("wakeup_reason %d", (int)wakeup_reason);
        } break;
    }
}


static void goto_sleep() {
    // TODO
}

extern "C" void app_main() {
    initArduino();

    wakeup_reason_check();

    const bool inited_nvs = init_nvs();

    if(!inited_nvs) {
        ESP_LOGE(TAG, "NVS could not be initialized :((");
    }

    const bool inited_wire = init_wire();

    if(!inited_wire) {
        ESP_LOGE(TAG, "Wire could not be initialized :((");
        // TODO inited_wire error handling
    }

    uint8_t ds3231_error = 1;
    const DateTime curr_time = RTClib::now(Wire, &ds3231_error);
    if(ds3231_error != 0) {
        ESP_LOGE(TAG, "DS32321 Error: %d", ds3231_error);
        // TODO error handling
    }
    const uint32_t utc_timestamp = curr_time.unixtime();

    const bool send_data = curr_time.minute() % preferences.send_every == 0;

    uint8_t init_wifi_error = 0;
    if(send_data) {
        init_wifi_error = sending::init_wifi();
    }

    Measurement meas = measure(utc_timestamp);
    meas.last_sd_error = last_sd_error;
    meas.last_net_error = last_net_error;
    
    measurement_to_buf(&meas, meas_dp.data);
    wl1::dp_add_hash(&meas_dp);

    wl1::add_dp(meas_dp);

    auto store_res = wl1::store_data(send_data && !init_wifi_error);
    last_sd_error = store_res.first;
    last_net_error = store_res.second;

    goto_sleep();
}
