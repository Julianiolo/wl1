#ifndef __WL1_SENDING_H__
#define __WL1_SENDING_H__

#include <cstdint>
#include <cstddef> // for size_t

namespace sending {
    uint8_t init_wifi();

    uint8_t wait_wifi_init_done();

    /*
        returns a socket connected to whatever was specified in nvs (see init_wifi).
        the socket has non_blocking enabled to make checking for messages possible
    */
    uint8_t connect(int* sock);

    uint8_t send_data(int sock, const uint8_t* data, size_t len);
    enum {
        RECV_END = 1,
        RECV_NO_DATA = 2
    };
    uint8_t recv_data(int sock, uint8_t* data, size_t len, size_t* recv_len);

    /*
        closes the connection of the socket
    */
    uint8_t disconnect(int sock);
};

#endif