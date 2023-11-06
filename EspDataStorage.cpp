#include "EspDataStorage.h"

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

void EspDataStorage::done() {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    vSemaphoreDelete(mutex);
    mutex = NULL;
}

bool EspDataStorage::isBusy() {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1)) == pdFALSE) {
        return true;
    }
    xSemaphoreGive(mutex);
    return false;
}

bool EspDataStorage::mkdev(uint8_t id, StorageDeviceType_t type) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");

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

bool EspDataStorage::mkpartition(uint8_t partitionID, const char* label, size_t size) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");

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

Partition_t* EspDataStorage::mount(const char* partitionLabel, const char* basePath, bool formatOnFail) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");

    Partition_t* fs = new Partition_t();
    if (!fs->begin(formatOnFail, basePath, MAX_OPEN_FILE, partitionLabel)) {
        delete fs;
        fs = NULL;
        return fs;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(partitionLabel, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        delete fs;
        fs = NULL;
        return fs;
    }
    ESP_LOGD(TAG, "Partition size: total: %d, used: %d", total, used);
    return fs;
}

bool EspDataStorage::unmount(Partition_t* fs) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for unmount");
        return false;
    }
    fs->end();
    delete fs;
    fs = NULL;
    ESP_LOGI(TAG, "Unmount partition success.");
    xSemaphoreGive(mutex);
    return true;
}

void EspDataStorage::listdir(Partition_t* fs, const char* dirname, uint8_t level) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return;
    }

    ESP_LOGI(TAG, "Listing directory: %s", dirname);

    File root = fs->open(dirname);
    if (!root) {
        ESP_LOGW(TAG, "Failed to open directory: %s", dirname);
        root.close();
        xSemaphoreGive(mutex);
        return;
    }
    if (!root.isDirectory()) {
        ESP_LOGW(TAG, "%s is not a directory", dirname);
        root.close();
        xSemaphoreGive(mutex);
        return;
    }

    File f = root.openNextFile();
    while (f) {
        if (f.isDirectory()) {
            ESP_LOGI(TAG, "  DIR: %s", f.name());
            if (level) {
                listdir(fs, f.path(), level - 1);
            }
        } else {
            ESP_LOGI(TAG, "  FILE: %s, SIZE: %d", f.name(), f.size());
        }
        f = root.openNextFile();
    }
    root.close();
    f.close();
    xSemaphoreGive(mutex);
}

bool EspDataStorage::mkfile(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    if (fs->exists(path)) {
        ESP_LOGW(TAG, "File %s already exist", path);
        xSemaphoreGive(mutex);
        return false;
    }

    File f = fs->open(path, FILE_WRITE);
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", path);
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }

    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::rm(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for rm file");
        return false;
    }

    if (!fs->remove(path)) {
        ESP_LOGW(TAG, "Error deleting file");
        xSemaphoreGive(mutex);
        return false;
    }

    ESP_LOGD(TAG, "Successfully delete file %s", path);
    xSemaphoreGive(mutex);
    return true;
}

size_t EspDataStorage::fsize(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    File f = fs->open(path);
    size_t sz = f.size();
    f.close();
    xSemaphoreGive(mutex);
    return sz;
}

bool EspDataStorage::read(Partition_t* fs, const char* path, char* dest, uint32_t bufferLen) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    File f = fs->open(path);
    if (!f || f.isDirectory()) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }

    for (uint32_t i = 0; f.available() && bufferLen >= strlen(dest); i++) {
        dest[i] = f.read();
    }

    f.close();
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::append(Partition_t* fs, const char* path, const char* data) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    File f = fs->open(path, FILE_APPEND);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for append");
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }

    if (!f.print(data)) {
        ESP_LOGE(TAG, "Append failed to file: %s", path);
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }

    f.close();
    xSemaphoreGive(mutex);
    return true;
}

bool EspDataStorage::write(Partition_t* fs, const char* path, const char* data) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take mutex for file reading");
        return false;
    }

    File f = fs->open(path, FILE_WRITE);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for write");
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }

    if (!f.print(data)) {
        ESP_LOGE(TAG, "Write failed to file: %s", path);
        f.close();
        xSemaphoreGive(mutex);
        return false;
    }
    xSemaphoreGive(mutex);
    return true;
}