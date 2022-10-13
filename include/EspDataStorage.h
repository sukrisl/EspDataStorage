#pragma once

#include "driver/spi_common.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"

#include "esp_littlefs.h"

typedef enum {
    DATA_STORAGE_INTERNAL_FLASH = 0,
    DATA_STORAGE_EXTERNAL_FLASH,
    DATA_STORAGE_BOTH_FLASH,
} DataStorageOption_t;

class EspDataStorage {
 private:
    spi_host_device_t extFlashSpihost = SPI3_HOST;
    spi_bus_config_t extFlashSpiBusCfg;
    esp_flash_spi_device_config_t extFlashCfg;
    esp_flash_t* extFlashDev;

    const esp_partition_t* extFlashPartition;

    esp_vfs_littlefs_conf_t extFlashFsConf;

    const char* extFlashBasePath;

    bool initExtFlash();

 public:
    bool init(DataStorageOption_t option = DATA_STORAGE_INTERNAL_FLASH);
    bool deinit(DataStorageOption_t option = DATA_STORAGE_INTERNAL_FLASH);

    bool createPartitionOnExtFlash(const char* label, const char* basePath, size_t size = 0xF00000);
    bool listPartition(DataStorageOption_t option = DATA_STORAGE_INTERNAL_FLASH);

    bool printFileContent(const char* filepath);
    bool appendDataToFile(const char* filepath, const char* data);
};