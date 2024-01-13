#include "Arduino.h"
#include "Wire.h"
#include "esp_log.h"

#include "DS3231/DS3231.h"

/*
maybe need to set this, although we probably won't use it:

wl1/managed_components/espressif__arduino-esp32/libraries/WiFiClientSecure/src/ssl_client.cpp:24:4: warning: #warning "Please call `idf.py menuconfig` then go to Component config -> mbedTLS -> TLS Key Exchange Methods -> Enable pre-shared-key ciphersuites and then check `Enable PSK based cyphersuite modes`. Save and Quit." [-Wcpp]
   24 | #  warning "Please call `idf.py menuconfig` then go to Component config -> mbedTLS -> TLS Key Exchange Methods -> Enable pre-shared-key ciphersuites and then check `Enable PSK based cyphersuite modes`. Save and Quit."
      |    ^~~~~~~

*/

#include "config.h"
#include "sensors.h"

#define TAG "WL1"

DS3231 ds3231;

struct Preferences {
    uint8_t send_every;
} preferences;

esp_sleep_wakeup_cause_t wakeup_reason;

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

extern "C" void app_main() {
    initArduino();

    wakeup_reason_check();

    bool inited_wire = init_wire();

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

    if(send_data) {
        // init wifi
    }

    Measurement meas = measure(utc_timestamp);
    (void)meas;

    
    if(send_data) {
        // send data
    }
}