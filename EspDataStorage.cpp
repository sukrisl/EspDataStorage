#include "EspDataStorage.h"

#include <LittleFS.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "SPIFlash.h"

#define MAX_OPEN_FILE 10

static SemaphoreHandle_t mutex = NULL;

static const char* TAG = "EspDataStorage";

bool EspDataStorage::init(uint32_t waitTimeout_ms) {
    _waitTimeout_ms = waitTimeout_ms;

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

bool EspDataStorage::mount(const char* partitionLabel, const char* basePath, bool formatOnFail) {
    if (!LittleFS.begin(formatOnFail, basePath, MAX_OPEN_FILE, partitionLabel)) {
        return false;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(partitionLabel, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGD(TAG, "Partition size: total: %d, used: %d", total, used);
    return true;
}

void EspDataStorage::listDir(const char* dirname, uint8_t level) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return;
    }

    ESP_LOGI(TAG, "Listing directory: %s", dirname);

    File root = LittleFS.open(dirname);
    if (!root) {
        ESP_LOGW(TAG, "Failed to open directory: %s", dirname);
        xSemaphoreGive(mutex);
        return;
    }
    if (!root.isDirectory()) {
        ESP_LOGW(TAG, "%s is not a directory", dirname);
        xSemaphoreGive(mutex);
        return;
    }

    File f = root.openNextFile();
    while (f) {
        if (f.isDirectory()) {
            ESP_LOGI(TAG, "  DIR: %s", f.name());
            if (level) {
                listDir(f.path(), level - 1);
            }
        } else {
            ESP_LOGI(TAG, "  FILE: %s, SIZE: %d", f.name(), f.size());
        }
        f = root.openNextFile();
    }
    xSemaphoreGive(mutex);
}

bool EspDataStorage::print(const char* path) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(path, "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    char line[100];
    ESP_LOGI(TAG, "Read from file %s:", path);
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

bool EspDataStorage::mkfile(const char* path) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(path, "r");
    if (f != NULL) {
        ESP_LOGW(TAG, "File %s already exist", path);
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create %s", path);
        fclose(f);
        xSemaphoreGive(mutex);
        return false;
    }

    fclose(f);
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::read(const char* path, char* dest, uint32_t bufferLen) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(path, "r");
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

bool EspDataStorage::append(const char* path, const char* data) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(path, "a");
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

bool EspDataStorage::write(const char* path, const char* data) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    FILE* f = fopen(path, "w");
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

bool EspDataStorage::rm(const char* path) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for rm file");
        return false;
    }

    if (remove(path) != 0) {
        ESP_LOGW(TAG, "Error deleting file");
        xSemaphoreGive(mutex);
        return false;
    }

    ESP_LOGD(TAG, "Successfully delete file %s", path);
    xSemaphoreGive(mutex);
    return true;
}