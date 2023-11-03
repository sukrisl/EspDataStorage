#pragma once

#include <driver/spi_common.h>
#include <esp_flash.h>
#include <esp_flash_spi_init.h>
#include <esp_partition.h>

#include "StorageDevice.h"
#include "esp_littlefs.h"

class SPIFlash : public StorageDevice {
   private:
    esp_flash_t* device;
    spi_host_device_t spiHost;
    spi_bus_config_t spiBusConfig;
    esp_flash_spi_device_config_t flashConfig;

    const esp_partition_t* partition;

    esp_err_t initSPIbus();
    esp_err_t addFlashDevice();

   public:
    bool install() override;
    bool uninstall() override;
    bool registerPartition(const char* label, size_t size) override;
};