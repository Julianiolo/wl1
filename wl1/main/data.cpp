#include "data.h"
#include <cstring>
#include <array>

#include "Arduino.h" // mainly for millis()
#include "esp_log.h"

#include "sd/sd.h"
#include "hashing/hashing.h"
#include "sending/sending.h"
#ifndef _NO_EXCEPTIONS
#define _NO_EXCEPTIONS
#endif
#include "CPP_Utils/src/comps/ringBuffer.h"
#include "CPP_Utils/src/DataUtils.h"

#include "DS3231/DS3231.h"

// this file needs to define a `uint8_t sec_key[32]` which is the secret key used to sign messages; see gen_sec_key.py
#include "sec_key.h"

#define TAG "wl1/data"

#define DS_LEN 16

struct DataStorage {
    uint32_t sd_ack = 0;
    uint32_t send_ack = 0;
    RingBuffer<wl1::DataPoint, std::array<wl1::DataPoint, DS_LEN>> data;
};

RTC_DATA_ATTR DataStorage ds;

#define SB_LEN 64
uint8_t send_buf[(WL1_MEASUREMENT_BUFFER_SIZE+32)*SB_LEN];

uint32_t wl1::DataPoint::getTimestamp() const {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
}


void wl1::get_dp_filename(char* buf, uint32_t timestamp) {
    DateTime dt(timestamp);
    auto res = std::snprintf(buf, 8+1+3+1, "%04" PRIu16 "%02" PRIu8 "%02" PRIu8 ".bin", dt.year(), dt.month(), dt.day());
    if(res < 0)
        ESP_LOGE(TAG, "Error snprintf for filename failed: %d", res);
}

void wl1::dp_add_hash(DataPoint* point) {
    constexpr size_t buf_len = sizeof(point->data);

    // this marks where the actual measurement data ends in the meas_buf, and the start of where sec_key or hash is stored
    uint8_t* const meas_data_end = point->data + WL1_MEASUREMENT_BUFFER_SIZE;
    
    std::memcpy(meas_data_end, sec_key, 32);

    uint8_t hash_value[32];
    hsh::hash_bytes(point->data, buf_len, hash_value);

    std::memcpy(meas_data_end, hash_value, 32);
}

void wl1::add_dp(const DataPoint& point) {
    ds.data.add(point);
}

static uint8_t save_dp_to_sd(const wl1::DataPoint& point) {
    char filename[8+1+3+1];
    wl1::get_dp_filename(filename, point.getTimestamp());

    uint8_t write_ret = 0;
    for(uint8_t i = 0; i < 3; i++) {
        write_ret = sd::appendToFile(filename, point.data, sizeof(point.data));
        if(write_ret == 0)
            break;
    }

    if(write_ret != 0) {
        ESP_LOGE(TAG, "failed to write to file: %d", write_ret);
    }
    
    return write_ret;
}

static size_t get_ds_start_ind_for_ack(uint32_t ack) {
    size_t start_ind = 0;
    while(start_ind < ds.data.size() && ds.data.get(start_ind).getTimestamp() <= ack)
        start_ind++;
    return start_ind;
}

static uint8_t save_to_sd() {
    const size_t start_ind = get_ds_start_ind_for_ack(ds.sd_ack);
    
    for(size_t i = start_ind; i < ds.data.size(); i++) {
        
        const auto res = save_dp_to_sd(ds.data.get(i));
        if(res != 0) {
            ESP_LOGE(TAG, "failed to save dp @%" PRIuSIZE " %" PRIu32, i, ds.data.get(i).getTimestamp());
            break;  // TODO: maybe not stop?
        }
        ds.sd_ack = ds.data.get(i).getTimestamp();
    }
    return 0;
}

static uint8_t parse_and_respond(const uint8_t* data, size_t len) {
    return 0;
}

static uint8_t send(bool sd_works) {
    int con;
    {
        const auto ret = sending::connect(&con);
        if(ret != 0) {
            ESP_LOGE(TAG, "failed to connect: %d", ret);
            return ret;
        }
    }

    const size_t start_ind = get_ds_start_ind_for_ack(ds.send_ack);

    size_t len = 0;
    for(size_t i = start_ind; i < ds.data.size(); i++) {
        uint8_t* p = &send_buf[WL1_DP_SIZE*i];
        std::memcpy(p, ds.data.get(i).data, WL1_DP_SIZE);
        len++;
    }

    const uint8_t send_res = sending::send_data(con, send_buf, WL1_DP_SIZE*len);
    if(send_res != 0) {
        // TODO: maybe check kind of error
        return send_res + 16;
    }

    uint8_t err = 0;
    {
        const uint32_t start_millis = millis();
        do {
            delay(50);
            size_t len = 0;
            const auto ret = sending::recv_data(con, send_buf, sizeof(send_buf), &len);
            if(ret == sending::RECV_NO_DATA) continue;
            if(ret == sending::RECV_END) {
                err = ret + 32;
                break;
            }

            const auto p_ret = parse_and_respond(send_buf, len);
            if(p_ret != 0) {
                ESP_LOGE(TAG, "send: could not parse data: %d", p_ret);
                // TODO
            }

            break;
        } while(millis() < start_millis+WL1_ASWER_TIME);
    }

    {
        const auto ret = sending::disconnect(con);
        if(ret != 0) {
            ESP_LOGE(TAG, "failed to disconnect: %d", ret);
            // TODO: maybe return error?
        }
    }
    return err;
}

std::pair<ERR, ERR> wl1::store_data(bool send_net) {
    const auto sd_init_error = sd::init();
    if(sd_init_error != 0) {
        ESP_LOGE(TAG, "failed to init sd card: %d", sd_init_error);
    }

    const uint8_t sd_res = save_to_sd();
    const uint8_t sd_errno = errno;

    uint8_t send_res = 0, send_errno = 0;
    if(send_net) {
        send_res = send(sd_init_error == 0);
        send_errno = errno;
    }

    {
        const auto ret = sd::deinit();
        if(ret != 0) {
            ESP_LOGW(TAG, "failed to deinit sd card: %d", ret);
        }
    }
    return {{sd_res, sd_errno}, {send_res, send_errno}};
}
