#ifndef __WL1_SENSORS_H__
#define __WL1_SENSORS_H__

#include "config.h"

#include "Arduino.h"

#define WL1_MEASUREMENT_BUFFER_SIZE 256 // TODO

struct Measurement {
    uint32_t timestamp;

    
    struct VEML3328_M {
        uint8_t measurements_present; // TODO

        // stores the meas_ctx_t
        uint8_t config;

        // actual data
        uint16_t clear;
        uint16_t r;
        uint16_t g;
        uint16_t b;
        uint16_t ir;
    } veml3328; // size: 1+1+5*2 = 12

    struct VEML7700_M {
        uint8_t measurements_present; // TODO

        /*
            Bits:
        */
        uint8_t config; // TODO

        // actual data
        uint16_t white;
        uint16_t ambient;
    } veml7700; // size: 1+1+2*2 = 6

    struct LTR390UV01_M {
        uint8_t config; // TODO

        /*
            both 20 bit max
            MSB set means value is not present, rest of bits indicate error
        */
        uint32_t al; // ambient value
        uint32_t uv;
    } ltr390uv01; // size: 1+2*4 = 9

    struct TMP102_M {
        /*
            produces 12 or 13 bit values;
            MSB set means no value present, rest of bits indicate error
        */
        uint16_t raw_value;
    } tmp102; // size: 2 = 2

    struct AS7341_M {
        // TODO
    } as7341;

    struct AS5600_M {
        // maybe need a config?
        /*
            produces 12 bit values
            MSB set means no value present, rest of bits indicate error
        */
        uint16_t angle;
    } as5600; // size: 2

    struct BME680_M {

    } bme680;

    struct WindSpeed_M {
        uint16_t ticks;
    } windSpeed;

    struct RainGauge_M {
        uint16_t ticks;
    } rain_gauge;

    uint16_t battery_adc;
    uint16_t solar_adc;

    ERR last_sd_error;
    ERR last_net_error;
};

Measurement measure(uint32_t timestamp);

void measurement_to_buf(Measurement* meas, uint8_t* buf);

#endif