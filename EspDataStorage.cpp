#include <cstring>

#include "esp_log.h"

#include "SPIFlash.h"
#include "EspDataStorage.h"

static const char* TAG = "EspDataStorage";

bool EspDataStorage::addDevice(uint8_t id, StorageDeviceType_t type) {
    if (type == STORAGE_DEVICE_TYPE_FLASH) {
        std::shared_ptr<SPIFlash> device = std::make_shared<SPIFlash>();

        if (device) {
            if (!device->install()) {
                ESP_LOGE(TAG, "Failed to install flash device");
                return false;
            }

            devices.insert(std::make_pair(id, device));
            return true;
        }
    }
    
    return false;
}

bool EspDataStorage::createPartition(uint8_t partitionID, const char* label, const char* basePath, size_t size) {
    std::shared_ptr<StorageDevice> device = devices[partitionID];
    if (!device) {
        ESP_LOGW(TAG, "Failed to create partition, storage device [%u] not found", partitionID);
        return false;
    }

    if (!device->registerPartition(label, size)) return false;

    esp_vfs_littlefs_conf_t fsConfig = {
        .base_path = basePath,
        .partition_label = label,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&fsConfig);

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
    ret = esp_littlefs_info(fsConfig.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    return false;
}

bool EspDataStorage::print(const char* filepath) {
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

bool EspDataStorage::read(const char* filepath, char* dataDestination, char terminator) {
    FILE *f = fopen(filepath, "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        fclose(f);
        return false;
    }

    char line[100];
    while (fgets(line, sizeof(line), f)) {
        char *pos = strchr(line, terminator);
        strcat(dataDestination, line);
        if (pos) break;
    }

    fclose(f);
    return true;    
}

bool EspDataStorage::append(const char* filepath, const char* data) {
    FILE *f = fopen(filepath, "a");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for append");
        fclose(f);
        return false;
    }

    fprintf(f, "%s\n", data);
    fclose(f);

    return true;
}

bool EspDataStorage::write(const char* filepath, const char* data) {
    FILE *f = fopen(filepath, "w");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        fclose(f);
        return false;
    }

    fprintf(f, "%s\n", data);
    fclose(f);

    return true;
}

bool EspDataStorage::rm(const char* filepath) {
    if (remove(filepath) != 0 ) {
        ESP_LOGW(TAG, "Error deleting file");
        return false;
    }

    ESP_LOGI(TAG, "Successfully delete file %s", filepath);
    return true;
}