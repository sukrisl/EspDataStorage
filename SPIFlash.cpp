#include "SPIFlash.h"

#include <esp_log.h>

#include <cstring>

static const char* TAG = "SPIFlash";

esp_err_t SPIFlash::initSPIbus(int miso, int mosi, int clk) {
    spiBusConfig.miso_io_num = miso;
    spiBusConfig.mosi_io_num = mosi;
    spiBusConfig.sclk_io_num = clk;
    spiBusConfig.quadwp_io_num = -1;
    spiBusConfig.quadhd_io_num = -1;
    return spi_bus_initialize(spiHost, &spiBusConfig, SPI_DMA_CH_AUTO);
}

esp_err_t SPIFlash::addFlashDevice(int cs) {
    flashConfig = {
        .host_id = spiHost,
        .cs_io_num = cs,
        .io_mode = SPI_FLASH_DIO,
        .speed = ESP_FLASH_40MHZ,
        .input_delay_ns = 0,
        .cs_id = 0,
    };
    return spi_bus_add_flash_device(&device, &flashConfig);
}

bool SPIFlash::registerPartition(const char* label, size_t size) {
    // TODO: automatic offset

    esp_err_t ret = esp_partition_register_external(
        device, 0x1000, size, label, ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &partition);

    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to register partition: %s", esp_err_to_name(ret));
        return false;
    }

    const esp_partition_t* verifiedPartition = esp_partition_verify(partition);
    if (verifiedPartition == NULL) {
        ESP_LOGE(TAG, "Partition verification failed");
        return false;
    }

    ESP_LOGI(TAG, "Successfully registered storage partition");
    ESP_LOGI(TAG, "part_label:  %s", verifiedPartition->label);
    ESP_LOGI(TAG, "offset:      0x%lx", verifiedPartition->address);
    ESP_LOGI(TAG, "size:        0x%lx", verifiedPartition->size);

    return (ret == ESP_OK);
}

bool SPIFlash::install(int miso, int mosi, int clk, int cs) {
    ESP_LOGI(TAG, "Initializing SPI flash");

    info.status = STORAGE_DEVICE_OFFLINE;
    info.type = STORAGE_DEVICE_TYPE_UNKNOWN;
    info.capacity = 0;

    esp_err_t ret;
#ifdef CONFIG_IDF_TARGET_ESP32S3
    spiHost = SPI2_HOST;
#else
    spiHost = SPI3_HOST;
#endif

    ret = initSPIbus(miso, mosi, clk);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus for SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    ret = addFlashDevice(cs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI bus SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_flash_init(device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    uint32_t flash_id;
    esp_flash_read_id(device, &flash_id);

    info.status = STORAGE_DEVICE_ONLINE;
    info.type = STORAGE_DEVICE_TYPE_FLASH;
    info.capacity = device->size;

    ESP_LOGI(TAG, "Flash installed, size: %lu", device->size);

    return esp_flash_chip_driver_initialized(device);
}

bool SPIFlash::uninstall() {
    return true;
}