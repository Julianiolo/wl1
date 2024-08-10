#include "sending.h"

#include "../config.h"

#include <memory>
#include <utility>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "CPP_Utils/src/DataUtils.h"

#define TAG "wl1/sending"

struct ServerAddr{
    char ip[(3+1+3+1+3+1+3)+1];
    uint16_t port;
};
static ServerAddr server_addr;

constexpr ServerAddr default_server_addr = {
    // TODO
};

static EventGroupHandle_t s_wifi_event_group;


static uint8_t get_wifi_nvs_stuff(char* wifi_str_buf, size_t wifi_str_buf_len) {
    esp_err_t err = ESP_OK;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &err);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening nvs handle: %s", err); // TODO maybe save err
        return 1;
    }

    err = handle->get_blob("server_ip", server_addr.ip, sizeof(server_addr.ip));
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting server ip: %d", err);
        server_addr = default_server_addr;
    }else{
        err = handle->get_item("server_port", server_addr.port);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Error getting server port: %d", err);
            server_addr = default_server_addr;
        }
    }

    uint8_t keyNum = 0;
    {
        char keyBuf[16];
        std::snprintf(keyBuf, sizeof(keyBuf), "wifi_cred%d", keyNum);
        err = handle->get_string(keyBuf, wifi_str_buf, wifi_str_buf_len);

        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Error getting wifi string: %d", err);
            return 2;
        }
    }
}

// something along the lines of: https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c
uint8_t sending::init_wifi() {
    char wifi_str_buf[128];
    uint8_t ret = get_wifi_nvs_stuff(wifi_str_buf, sizeof(wifi_str_buf));
    if(ret != 0)
        return ret;

    const char* ssid;
    const char* psswd;
    {
        const char* res = strtok(wifi_str_buf, ";");
        if(res == NULL) {
            ESP_LOGE(TAG, "Error parsing wifi string: %s", wifi_str_buf);
            return 16;
        }
        ssid = wifi_str_buf;
        psswd = res;
    }


    return 0;
}

uint8_t sending::wait_wifi_init_done() {

}

static uint8_t set_nonblocking(int sock) {
    auto flags = fcntl(sock, F_GETFL, 0);
    if(flags < 0)
        return 1;
    
    auto ret = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    if(ret == -1) {
        return 2;
    }

    return 0;
}

// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_client/main/tcp_client_v4.c
uint8_t sending::connect(int* sock_) {
    ESP_LOGI(TAG, "connecting to: %s:%d", server_addr.ip, server_addr.port);

    errno = 0;

    struct sockaddr_in dest_addr;
    {
        const auto ret = inet_pton(AF_INET, server_addr.ip, &dest_addr.sin_addr);
        if(ret != 1) {
            ESP_LOGE(TAG, "inet_pton failed: %d", ret);
            return 1;
        }
    }
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_addr.port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(sock < 0) {
        ESP_LOGE(TAG, "socket failed: %s", strerror(errno));
        return 2;
    }
    ESP_LOGI(TAG, "Socket created");

    {
        const auto ret = connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if (ret != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: %s [%d]", strerror(errno), ret);
            return 3;
        }
    }
    ESP_LOGI(TAG, "Successfully connected");

    {
        const auto ret = set_nonblocking(sock);
        if(ret != 0) {
            return ret | (1<<3);
        }
    }
    
    *sock_ = sock;
    return 0;
}

uint8_t sending::send_data(int sock, const uint8_t* data, size_t len) {
    const auto ret = send(sock, data, len, 0);
    if(ret == -1) {
        ESP_LOGE(TAG, "Failed to send: %s", strerror(errno));
        return 1;
    }
    if(ret != len) {
        ESP_LOGE(TAG, "Failed to send all: %" PRIuSIZE "/%" PRIuSIZE, ret, len);
        return 2;
    }
    
    return 0;
}

uint8_t sending::recv_data(int sock, uint8_t* data, size_t len, size_t* recv_len) {
    const auto ret = recv(sock, data, len, 0);
    if(ret == 0) {
        ESP_LOGW(TAG, "recv: No data present, ending: %s", strerror(errno));
        return RECV_END;
    }

    if(ret == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            ESP_LOGW(TAG, "recv: No data present: %s", strerror(errno));
            return RECV_NO_DATA;
        }else{
            ESP_LOGE(TAG, "Failed to recv: %s", strerror(errno));
            return 3;
        }
    }
    *recv_len = ret;
    return 0;
}

uint8_t sending::disconnect(int sock) {
    if(shutdown(sock, SHUT_RDWR) != 0) {
        ESP_LOGE(TAG, "Failed to shutdown: %s", strerror(errno));
        return 1;
    }

    if(close(sock) != 0) {
        ESP_LOGE(TAG, "Failed to close: %s", strerror(errno));
        return 2;
    }

    return 0;
}