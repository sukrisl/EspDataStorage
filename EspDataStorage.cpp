#include <cstring>

#include "esp_log.h"

#include "EspDataStorage.h"

static const char* TAG = "EspDataStorage";

bool EspDataStorage::initExtFlash() {
    ESP_LOGI(TAG, "Initializing external SPI flash");
    esp_err_t ret;

    extFlashSpiBusCfg.miso_io_num = SPI3_IOMUX_PIN_NUM_MISO;
    extFlashSpiBusCfg.mosi_io_num = SPI3_IOMUX_PIN_NUM_MOSI;
    extFlashSpiBusCfg.sclk_io_num = SPI3_IOMUX_PIN_NUM_CLK;
    extFlashSpiBusCfg.quadwp_io_num = -1;
    extFlashSpiBusCfg.quadhd_io_num = -1;

    ret = spi_bus_initialize(extFlashSpihost, &extFlashSpiBusCfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus for external SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    extFlashCfg = {
        .host_id = extFlashSpihost,
        .cs_io_num = SPI3_IOMUX_PIN_NUM_CS,
        .io_mode = SPI_FLASH_DIO,
        .speed = ESP_FLASH_40MHZ,
        .input_delay_ns = 0,
        .cs_id = 0,
    };

    ret = spi_bus_add_flash_device(&extFlashDev, &extFlashCfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI bus external SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_flash_init(extFlashDev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize external SPI flash, error: %s", esp_err_to_name(ret));
        return false;
    }

    uint32_t id;
    esp_flash_read_id(extFlashDev, &id);
    ESP_LOGI(TAG, "Initialized external Flash, size=%d KB, ID=0x%x", extFlashDev->size / 1024, id);

    return esp_flash_chip_driver_initialized(extFlashDev);
}

bool EspDataStorage::createPartitionOnExtFlash(const char* label, const char* basePath, size_t size) {
    esp_err_t ret;

    // TODO: automatic offset
    ret = esp_partition_register_external(
        extFlashDev, 0, size, label, ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &extFlashPartition
    );

    if (extFlashPartition == NULL) {
        return false;
    }

    ESP_LOGI(TAG, "part_label: %s", extFlashPartition->label);
    ESP_LOGI(TAG, "start_addr: 0x%x", extFlashPartition->address);
    ESP_LOGI(TAG, "size: 0x%x", extFlashPartition->size);

    extFlashFsConf = {
        .base_path = basePath,
        .partition_label = label,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    ret = esp_vfs_littlefs_register(&extFlashFsConf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }

        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(extFlashFsConf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        return false;
    }

    extFlashBasePath = basePath;
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    return true;
}

bool EspDataStorage::init(DataStorageOption_t option) {
    initExtFlash();
    return true;
}

bool EspDataStorage::printFileContent(const char* filepath) {
    FILE *f = fopen(filepath, "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        fclose(f);
        return false;
    }

    char line[100];
    while (fgets(line, sizeof(line), f)) {
        char *pos = strchr(line, '\n');
        if (pos) { *pos = '\0'; }
        ESP_LOGI(TAG, "Read from file: %s", line);
    }

    fclose(f);
    return true;
}

bool EspDataStorage::appendDataToFile(const char* filepath, const char* data) {
    FILE *f = fopen(filepath, "a");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        fclose(f);
        return false;
    }

    fprintf(f, "%s\n", data);
    fclose(f);

    return true;
}