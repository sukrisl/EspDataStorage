#include "EspDataStorage.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "SPIFlash.h"

#define TIMEOUT_MS 500

static SemaphoreHandle_t mutex = NULL;

static const char* TAG = "EspDataStorage";

bool EspDataStorage::init() {
    if (mutex != NULL) {
        ESP_LOGW(TAG, "EspDataStorage has been initialized.");
        return false;
    }

    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        ESP_LOGE(TAG, "Failed to initialize EspDataStorage, possibly run out of memory.");
        return false;
    }
    return true;
}

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

bool EspDataStorage::createPartition(uint8_t partitionID, const char* label, size_t size) {
    std::shared_ptr<StorageDevice> device = devices[partitionID];
    if (!device) {
        ESP_LOGW(TAG, "Failed to create partition, storage device [%u] not found", partitionID);
        return false;
    }

    bool success = device->registerPartition(label, size);

    if (success) {
        ESP_LOGD(TAG, "Create partition %s (id:%u) success", label, partitionID);
    }
    return success;
}

bool EspDataStorage::mount(const char* partitionLabel, const char* basePath) {
    esp_vfs_littlefs_conf_t fsConfig = {
        .base_path = basePath,
        .partition_label = partitionLabel,
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
    ESP_LOGD(TAG, "Partition size: total: %d, used: %d", total, used);
    return false;
}

bool EspDataStorage::print(const char* filepath) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(filepath, "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    char line[100];
    ESP_LOGI(TAG, "Read from file %s:", filepath);
    while (fgets(line, sizeof(line), f)) {
        char* pos = strchr(line, '\n');
        if (pos) {
            *pos = '\0';
        }
        printf("%s\n", line);
    }

    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::mkfile(const char* filepath) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(filepath, "r");
    if (f != NULL) {
        ESP_LOGW(TAG, "File %s already exist", filepath);
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create %s", filepath);
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::read(const char* filepath, char* dest, uint32_t bufferLen) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    char line[100];
    while (fgets(line, sizeof(line), f)) {
        if (bufferLen < strlen(dest) + strlen(line)) {
            fclose(f);
            xSemaphoreGive(mutex);
            return false;
        }
        strcat(dest, line);
    }

    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::append(const char* filepath, const char* data) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(filepath, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for append");
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    fprintf(f, "%s\n", data);
    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::write(const char* filepath, const char* data) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    fprintf(f, "%s\n", data);
    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::rm(const char* filepath) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for rm file");
        return false;
    }

    if (remove(filepath) != 0) {
        ESP_LOGW(TAG, "Error deleting file");
        xSemaphoreGive(mutex);
        return false;
    }

    ESP_LOGD(TAG, "Successfully delete file %s", filepath);
    xSemaphoreGive(mutex);
    return true;
}