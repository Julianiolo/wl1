#include "sd.h"

#include <errno.h>
#include <string.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "../config.h"

#define TAG "wl1/sd"

// somewhat based on https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/main/sd_card_example_main.c

static sdmmc_card_t *card;
static sdmmc_host_t host;

uint8_t sd::init() {
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };
    
    ESP_LOGI(TAG, "Initializing SD card");

    host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = WL1_PIN_MOSI,
        .miso_io_num = WL1_PIN_MISO,
        .sclk_io_num = WL1_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return 1;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = WL1_PIN_SD_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    // TODO
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return 2;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    return 0;
}

uint8_t sd::deinit() {
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free((spi_host_device_t)host.slot);

    return 0;
}

uint8_t sd::appendToFile(const char *filename, const uint8_t *data, size_t size) {
    FILE* f = fopen(filename, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s: %s", filename, strerror(errno));
        return 1;
    }
    uint8_t error = 0;
    const int n = fwrite(data, size, 1, f);
    if(n != 1) {
        ESP_LOGE(TAG, "Failed to write data to file: %s: %s", filename, strerror(errno));
        error = 2;
    }
    if(fflush(f) == EOF) {
        ESP_LOGE(TAG, "Failed to flush to file: %s: %s", filename, strerror(errno));
        error = 3;
    }
    // TODO: do we need a fsync?
    if(fclose(f) == EOF) {
        ESP_LOGE(TAG, "Failed to close to file: %s: %s", filename, strerror(errno));
        error = 4;
    }
    return error;
}
