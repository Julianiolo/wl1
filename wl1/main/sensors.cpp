#include "sensors.h"

#include "esp_log.h"

#include "sensors_internal.h"

#include "veml3328_Arduino/veml3328_Arduino.h"
#include "veml7700_Arduino/veml7700_Arduino.h"
#include "TCA9548/TCA9548.h"

#include "CPP_Utils/src/comps/ByteStreamWriter.h"

#define TAG "Sens"

TCA9548 i2c_multiplexer(0b1110000);
#define I2CMULT_CHANNEL_VEML3328 0
#define I2CMULT_CHANNEL_VEML7700 1

VEML3328 veml3328;
VEML7700 veml7700;

static int i2c_multiplexer_select_channel(uint8_t channel) {
    int error = TCA9548_OK;
    
    for(uint8_t i = 0; i<3; i++) {
        i2c_multiplexer.selectChannel(channel);

        error = i2c_multiplexer.getError();
        if(error == TCA9548_OK)
            return TCA9548_OK;
    }

    ESP_LOGE(TAG, "i2c multiplexer could not select channel %d: %d", channel, error);
    
    return error;
}

static uint8_t veml3328_setConfig(VEML3328::Config config) {
    uint8_t ret;
    for(uint8_t i = 0; i<3; i++) {
        ret = veml3328.setConfig(config);
        if(ret == 0) return ret;

        // error
        ESP_LOGE(TAG, "[TRY %d]veml3328 config could not be set: %d", i, ret);
    }
    return ret;
}
static uint8_t veml3328_setup(void) {
    uint8_t ret;
    ret = veml3328.begin(&Wire, VEML3328_DEF_ADDR, false); // don't wake up yet
    if(ret != 0) {  // should never be able to happen, since we dont wake up
        ESP_LOGE(TAG, "veml3328 could not be initialized: %d", ret);
    }

    VEML3328::Config config = VEML3328::Config::fromCtx(VEML3328::auto_steps[1].ctx);
    config.setSD(VEML3328::SD_Awake); // wake up
    config.setMode(VEML3328::Mode_AF); // manual trigger
    config.setCS(VEML3328::CS_All); // enable all channels

    // default config
    // config.setGain(VEML3328::Gain_1x);
    // config.setDG(VEML3328::DG_1x);
    // config.setIT(VEML3328::IT_50ms);
    // config.setSens(VEML3328::Sens_HIGH);

    ret = veml3328_setConfig(config);
    
    return ret;
}

static uint8_t veml3328_trigger_measurement(void) {
    VEML3328::Config config;
    config.setTrig(VEML3328::Trig_Trigger);  // trigger measurement
    uint8_t ret = veml3328.setConfig(config);
    if(ret != 0) { // error
        ESP_LOGE(TAG, "veml3328 Trigger could not be set: %d", ret);
    }
    return ret;
}

static uint8_t veml3328_wait_for_result(void) {
    uint8_t error = 0;
    for(uint8_t i = 0; i < 250; i++) {
        const VEML3328::Config config = veml3328.getConfig(&error);
        if(error != 0) {
            if(config.getTrig() == VEML3328::Trig_NoTrigger)
                return 0;
        }
        delay(10);
    }
    if(error == 0)
        return 8; // timeout
    return error;
}

static uint8_t veml3328_get_value(VEML3328::channel_t channel, uint16_t* result) {
    uint8_t error = 0;
    for(uint8_t i = 0; i < 3; i++) {
        const uint16_t val = veml3328.getChannelValue(channel, &error);
        if(error != 0) {
            *result = val;
            return 0;
        }
        delay(1);
    }
    if(error == 0)
        return 9; // timeout
    return error;
}
// returns the measurements_present bit
static uint8_t veml3328_get_value_err_handled(VEML3328::channel_t channel, uint16_t* result) {
    uint16_t v;
    uint8_t ret = veml3328_get_value(channel, &v);
    if(ret == 0) {
        *result = v;
        return 1<<(channel-VEML3328::CHANNEL_CLEAR);
    }else{
        *result = ret;
        return 0;
    }
}
static void veml3328_start_auto_reading(uint8_t& error_veml3328) {
    // TODO error handling
    i2c_multiplexer_select_channel(I2CMULT_CHANNEL_VEML3328);

    // veml3328 setup
    {
        uint8_t ret = veml3328_setup();
        if(ret != 0) {
            error_veml3328 = ret;
            ESP_LOGE(TAG, "veml3328 config could not be set: %d", ret);
        }
    }

    if(!error_veml3328) {
        uint8_t ret = veml3328_trigger_measurement();
        if(ret != 0) { // error
            error_veml3328 = ret | (1<<4);
            ESP_LOGE(TAG, "veml3328 first trig failed: %d", ret);
        }
    }
}
static void veml3328_start_actual_reading(uint8_t& error_veml3328, Measurement* meas) {
    i2c_multiplexer_select_channel(I2CMULT_CHANNEL_VEML3328);
    if(!error_veml3328) {
        uint8_t ret = veml3328_wait_for_result();
        if(ret != 0) { // error
            error_veml3328 = ret | (2<<4);
            ESP_LOGE(TAG, "veml3328 first wait failed: %d", ret);
        }
    }

    if(!error_veml3328) {
        uint16_t v;
        uint8_t ret = veml3328_get_value(VEML3328::CHANNEL_CLEAR, &v);
        if(ret == 0) {
            meas->veml3328.config = veml3328.getFittingConfig(v);
            auto conf = VEML3328::Config::fromCtx(meas->veml3328.config);
            uint8_t ret2 = veml3328_setConfig(conf);
            if(ret2 != 0) {
                error_veml3328 = ret2 | (4<<4);
            }
        }else{
            error_veml3328 = ret | (3<<4);
            ESP_LOGE(TAG, "veml3328 first get value failed: %d", ret);
        }
    }

    if(!error_veml3328) {
        uint8_t ret = veml3328_trigger_measurement();
        if(ret != 0) { // error
            error_veml3328 = ret | (5<<4);
            ESP_LOGE(TAG, "veml3328 2nd trig failed: %d", ret);
        }
    }
}
static void veml3328_get_data(uint8_t& error_veml3328, Measurement* meas) {
    i2c_multiplexer_select_channel(I2CMULT_CHANNEL_VEML3328);
    if(!error_veml3328) {
        uint8_t ret = veml3328_wait_for_result();
        if(ret != 0) { // error
            error_veml3328 = ret | (6<<4);
            ESP_LOGE(TAG, "veml3328 2nd trig failed: %d", ret);
        }
    }

    meas->veml3328.measurements_present = 0;
    if(!error_veml3328) {
        meas->veml3328.measurements_present |= veml3328_get_value_err_handled(VEML3328::CHANNEL_CLEAR, &meas->veml3328.clear);
        meas->veml3328.measurements_present |= veml3328_get_value_err_handled(VEML3328::CHANNEL_RED,   &meas->veml3328.r);
        meas->veml3328.measurements_present |= veml3328_get_value_err_handled(VEML3328::CHANNEL_GREEN, &meas->veml3328.g);
        meas->veml3328.measurements_present |= veml3328_get_value_err_handled(VEML3328::CHANNEL_BLUE,  &meas->veml3328.b);
        meas->veml3328.measurements_present |= veml3328_get_value_err_handled(VEML3328::CHANNEL_IR,    &meas->veml3328.ir);
    }else{
        meas->veml3328.clear = error_veml3328;
        meas->veml3328.r = meas->veml3328.g = meas->veml3328.b = meas->veml3328.ir = 0;
    }
}

static uint8_t veml7700_setup(void) {
    uint8_t ret;
    ret = veml7700.begin(&Wire, VEML7700_DEF_ADDR, false); // don't wake up yet
    if(ret != 0) {  // should never be able to happen, since we dont wake up
        ESP_LOGE(TAG, "veml7700 could not be initialized: %d", ret);
    }

    VEML7700::Config config;
    config.setSD(VEML7700::SD_Awake); // wake up

    // default config
    config.setGain(VEML7700::Gain_2x);
    config.setIT(VEML7700::IT_50ms);
    config.setPers(VEML7700::Pers_1);
    config.setInt(VEML7700::Int_Disabled); // disable interrupts

    for(uint8_t i = 0; i<3; i++) {
        ret = veml7700.setConfig(config);
        if(ret == 0) return ret;

        // error
        ESP_LOGE(TAG, "[TRY %d]veml7700 config could not be set: %d", i, ret);
    }

    VEML7700::PsConfig ps_config;
    ps_config.setPSM_EN(VEML7700::PSM_EN_Disabled);
    ps_config.setPSM(VEML7700::PSM_Mode4);

    for(uint8_t i = 0; i<3; i++) {
        ret = veml7700.setPsConfig(ps_config);
        if(ret == 0) return ret;

        // error
        ESP_LOGE(TAG, "[TRY %d]veml7700 ps_config could not be set: %d", i, ret);
    }
    return ret;
}
static uint8_t veml7700_trigger_measurement(void) {
    // uuuhm..., is there even a way to trigger a measurement?
    return 0;
}
static uint8_t veml7700_get_value(VEML7700::channel_t channel, uint16_t* result) {
    uint8_t error = 0;
    for(uint8_t i = 0; i < 3; i++) {
        const uint16_t val = veml7700.getChannelValue(channel, &error);
        if(error != 0) {
            *result = val;
            return 0;
        }
        delay(1);
    }
    if(error == 0)
        return 9; // timeout
    return error;
}
static uint8_t veml7700_get_value_err_handled(VEML7700::channel_t channel, uint16_t* result) {
    uint16_t v;
    uint8_t ret = veml7700_get_value(channel, &v);
    if(ret == 0) {
        *result = v;
        return 1<<(channel-VEML7700::CHANNEL_WHITE);
    }else{
        *result = ret;
        return 0;
    }
}
static void veml7700_start_auto_reading(uint8_t& error_veml7700) {
    // veml7700 setup
    i2c_multiplexer_select_channel(I2CMULT_CHANNEL_VEML7700);
    {
        uint8_t ret = veml7700_setup();
        if(ret != 0) {
            error_veml7700 = ret | (1 << 4);
            ESP_LOGE(TAG, "veml7700 config could not be set: %d", ret);
        }
    }

    if(!error_veml7700) {
        uint8_t ret = veml7700_trigger_measurement();
        if(ret != 0) {
            error_veml7700 = ret | (2 << 4);
            ESP_LOGE(TAG, "veml7700 first trig failed: %d", ret);
        }
    }
}
static void veml7700_start_actual_reading(uint8_t& error_veml7700, Measurement* meas) {
    // TODO
}
static void veml7700_get_data(uint8_t& error_veml7700, Measurement* meas) {
    i2c_multiplexer_select_channel(I2CMULT_CHANNEL_VEML7700);

    // TODO: wait for measurement

    meas->veml7700.measurements_present = 0;
    if(!error_veml7700) {
        meas->veml7700.measurements_present |= veml7700_get_value_err_handled(VEML7700::CHANNEL_WHITE, &meas->veml7700.white);
        meas->veml7700.measurements_present |= veml7700_get_value_err_handled(VEML7700::CHANNEL_ALS, &meas->veml7700.ambient);
    }else {
        meas->veml7700.ambient = error_veml7700;
        meas->veml7700.white = 0;
    }
}

Measurement measure(uint32_t timestamp) {
    Measurement meas;
    meas.timestamp = timestamp;

    if(!i2c_multiplexer.begin()) {
        ESP_LOGE(TAG, "i2c multiplexer could not be started: %d", i2c_multiplexer.getError());
    }

    uint8_t error_veml3328 = 0;
    uint8_t error_veml7700 = 0;

    veml3328_start_auto_reading(error_veml3328);
    veml7700_start_auto_reading(error_veml7700);

    // TODO do something else

    veml3328_start_actual_reading(error_veml3328, &meas);
    veml7700_start_actual_reading(error_veml7700, &meas);

    // TODO do something else
    
    veml3328_get_data(error_veml3328, &meas);
    veml7700_get_data(error_veml7700, &meas);



    return meas;
}

void measurement_to_buf(Measurement* meas, uint8_t* buf) {
    ByteStreamWriter wbs(buf, WL1_MEASUREMENT_BUFFER_SIZE, false);
    wbs.write(meas->timestamp);

    wbs.write(meas->veml3328.measurements_present);
    wbs.write(meas->veml3328.config);
    wbs.write(meas->veml3328.clear);
    wbs.write(meas->veml3328.r);
    wbs.write(meas->veml3328.g);
    wbs.write(meas->veml3328.b);
    wbs.write(meas->veml3328.ir);

    wbs.write(meas->veml7700.measurements_present);
    wbs.write(meas->veml7700.config);
    wbs.write(meas->veml7700.white);
    wbs.write(meas->veml7700.ambient);

    // TODO
}