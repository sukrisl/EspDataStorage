#include <cstring>

#include "esp_log.h"

#include "SPIFlash.h"

static const char* TAG = "SPI_flash";

esp_err_t SPIFlash::initSPIbus() {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    spiBusConfig.miso_io_num = SPI2_IOMUX_PIN_NUM_MISO;
    spiBusConfig.mosi_io_num = SPI2_IOMUX_PIN_NUM_MOSI;
    spiBusConfig.sclk_io_num = SPI2_IOMUX_PIN_NUM_CLK;
    spiBusConfig.quadwp_io_num = SPI2_IOMUX_PIN_NUM_WP;
    spiBusConfig.quadhd_io_num = SPI2_IOMUX_PIN_NUM_HD;
#else
    spiBusConfig.miso_io_num = SPI3_IOMUX_PIN_NUM_MISO;
    spiBusConfig.mosi_io_num = SPI3_IOMUX_PIN_NUM_MOSI;
    spiBusConfig.sclk_io_num = SPI3_IOMUX_PIN_NUM_CLK;
    spiBusConfig.quadwp_io_num = -1;
    spiBusConfig.quadhd_io_num = -1;
#endif

    return spi_bus_initialize(spiHost, &spiBusConfig, SPI_DMA_CH_AUTO);
}

esp_err_t SPIFlash::addFlashDevice() {
    flashConfig = {
        .host_id = spiHost,
#ifdef CONFIG_IDF_TARGET_ESP32S3
        .cs_io_num = SPI2_IOMUX_PIN_NUM_CS,
        .io_mode = SPI_FLASH_QIO,
#else
        .cs_io_num = SPI3_IOMUX_PIN_NUM_CS,
        .io_mode = SPI_FLASH_DIO,
#endif
        .speed = ESP_FLASH_80MHZ,
        .input_delay_ns = 10,
        .cs_id = 0,
    };

    return spi_bus_add_flash_device(&device, &flashConfig);
}

bool SPIFlash::registerPartition(const char* label, size_t size) {
    // TODO: automatic offset

    esp_err_t ret = esp_partition_register_external(
        device, 0x1000, size, label, ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &partition
    );

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
    ESP_LOGI(TAG, "offset:      0x%x", verifiedPartition->address);
    ESP_LOGI(TAG, "size:        0x%x", verifiedPartition->size);

    return (ret == ESP_OK);
}

bool SPIFlash::install() {
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

    ret = initSPIbus();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus for SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    ret = addFlashDevice();
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

    ESP_LOGI(TAG, "Flash installed, size: %d", device->size);

    return esp_flash_chip_driver_initialized(device);
}

bool SPIFlash::uninstall() {
    return true;
}