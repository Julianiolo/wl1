#include "sensors.h"



Measurement measure(uint32_t timestamp) {
    Measurement meas;
    meas.timestamp = timestamp;

    

    return meas;
}

void measurement_to_buf(Measurement* meas, uint8_t* buf) {
    uint8_t* ptr = buf;

}