#ifndef __WL1_DATA_H__
#define __WL1_DATA_H__

#include <cinttypes>
#include <utility> // for std::pair

#include "../config.h"
#include "sensors.h"


#define WL1_DP_SIZE (WL1_MEASUREMENT_BUFFER_SIZE+32)

namespace wl1 {
    struct DataPoint {
        uint8_t data[WL1_DP_SIZE];

        uint32_t getTimestamp() const;
    };

    /*
        buf needs to be 8+1+3+1 bytes long
    */
    void get_dp_filename(char* buf, uint32_t timestamp);
    void dp_add_hash(DataPoint* point);

    void add_dp(const DataPoint& point);
    std::pair<ERR, ERR> store_data(bool send_net);
}

#endif