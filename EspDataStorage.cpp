#include "EspDataStorage.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "SPIFlash.h"

#define MAX_OPEN_FILE 10

#define TAKE_LOCK()                                                             \
    do {                                                                        \
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) { \
            ESP_LOGE(TAG, "Failed to take mutex");                              \
            return false;                                                       \
        }                                                                       \
    } while (false)

#define TAKE_LOCK_E()                                                           \
    do {                                                                        \
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(_waitTimeout_ms)) == pdFALSE) { \
            ESP_LOGE(TAG, "Failed to take mutex");                              \
            return STORAGE_IS_BUSY;                                             \
        }                                                                       \
    } while (false)

#define GIVE_LOCK() xSemaphoreGive(mutex)

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
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1)) == pdFALSE) return true;
    GIVE_LOCK();
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
    TAKE_LOCK();
    fs->end();
    delete fs;
    fs = NULL;
    ESP_LOGI(TAG, "Unmount partition success.");
    GIVE_LOCK();
    return true;
}

bool EspDataStorage::exists(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");
    TAKE_LOCK();
    bool res = fs->exists(path);
    GIVE_LOCK();
    return res;
}

bool EspDataStorage::mkdir(Partition_t* fs, const char* dirname) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");
    TAKE_LOCK();
    bool res = fs->mkdir(dirname);
    GIVE_LOCK();
    return res;
}

bool EspDataStorage::rmdir(Partition_t* fs, const char* dirname) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    bool res = false;
    char* pathStr = strdup(dirname);
    if (pathStr) {
        char* ptr = strrchr(pathStr, '/');
        if (ptr) res = fs->rmdir(pathStr);
        free(pathStr);
    }

    if (res) {
        GIVE_LOCK();
        return true;
    }

    ESP_LOGW(TAG, "Directory \"%s\" is not empty, recursively deleting directory content", dirname);
    File root = fs->open(dirname);
    if (!root) {
        root.close();
        ESP_LOGE(TAG, "Failed to remove directory: %s", dirname);
        GIVE_LOCK();
        return false;
    }

    File f = root.openNextFile();
    while (f) {
        GIVE_LOCK();
        if (f.isDirectory()) {
            res = rmdir(fs, f.path());
        } else {
            char path[32] = {0};
            strcpy(path, f.path());
            f.close();
            res = rm(fs, path);
        }

        f = root.openNextFile();
        if (!res) {
            break;
        }
    }

    GIVE_LOCK();
    if (res) {
        res = rmdir(fs, dirname);
    }
    root.close();
    f.close();

    return res;
}

bool EspDataStorage::listdir(Partition_t* fs, const char* dirname, uint8_t level) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");
    ESP_LOGI(TAG, "Listing directory: %s", dirname);

    TAKE_LOCK();
    File root = fs->open(dirname);
    if (!root) {
        ESP_LOGW(TAG, "Failed to open directory: %s", dirname);
        root.close();
        GIVE_LOCK();
        return false;
    }
    if (!root.isDirectory()) {
        ESP_LOGW(TAG, "%s is not a directory", dirname);
        root.close();
        GIVE_LOCK();
        return false;
    }

    File f = root.openNextFile();
    while (f) {
        if (f.isDirectory()) {
            ESP_LOGI(TAG, "%*sDIR(%d)> /%s", 5 - level, "", level, f.name());
            if (level) {
                GIVE_LOCK();
                listdir(fs, f.path(), level - 1);
            }
        } else {
            ESP_LOGI(TAG, " %*sFILE(%d)> /%s, SIZE: %d", 5 - level, "", level, f.name(), f.size());
        }
        f = root.openNextFile();
    }
    root.close();
    f.close();
    GIVE_LOCK();
    return true;
}

bool EspDataStorage::mkfile(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    if (fs->exists(path)) {
        ESP_LOGD(TAG, "File %s already exist", path);
        GIVE_LOCK();
        return true;
    }

    File f = fs->open(path, FILE_WRITE);
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", path);
        f.close();
        GIVE_LOCK();
        return false;
    }

    f.close();
    GIVE_LOCK();
    return true;
}

bool EspDataStorage::rm(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    if (!fs->remove(path)) {
        ESP_LOGE(TAG, "Error deleting file: %s", path);
        GIVE_LOCK();
        return false;
    }

    ESP_LOGD(TAG, "Successfully delete file %s", path);
    GIVE_LOCK();
    return true;
}

size_t EspDataStorage::fsize(Partition_t* fs, const char* path) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    File f = fs->open(path);
    size_t sz = f.size();
    f.close();
    GIVE_LOCK();
    return sz;
}

StorageErr_t EspDataStorage::read(Partition_t* fs, const char* path, char* dest, uint32_t bufferLen, char terminator, uint32_t pos) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK_E();
    File f = fs->open(path);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        f.close();
        GIVE_LOCK();
        return STORAGE_FAIL;
    }

    if (f.isDirectory()) {
        ESP_LOGE(TAG, "Failed to read, path is directory: %s", path);
        f.close();
        GIVE_LOCK();
        return STORAGE_READ_IS_DIRECTORY;
    }

    bool isOutOfRange = !f.seek(pos);
    if (isOutOfRange) {
        ESP_LOGE(TAG, "File position (%d) out of range: %s", pos, path);
        f.close();
        GIVE_LOCK();
        return STORAGE_READ_OUT_OF_RANGE;
    }

    for (uint32_t i = 0; f.available() && bufferLen >= strlen(dest); i++) {
        char token = f.read();
        if (token == terminator) {
            f.close();
            GIVE_LOCK();
            return STORAGE_READ_FOUND_TERMINATOR;
        }

        dest[i] = token;
    }

    f.close();
    GIVE_LOCK();
    return (bufferLen >= strlen(dest)) ? STORAGE_OK : STORAGE_READ_MAX_BUFFER;
}

bool EspDataStorage::append(Partition_t* fs, const char* path, const char* data) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    File f = fs->open(path, FILE_APPEND);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for append");
        f.close();
        GIVE_LOCK();
        return false;
    }

    if (!f.print(data)) {
        ESP_LOGE(TAG, "Append failed to file: %s", path);
        f.close();
        GIVE_LOCK();
        return false;
    }

    f.close();
    GIVE_LOCK();
    return true;
}

bool EspDataStorage::write(Partition_t* fs, const char* path, const char* data) {
    assert(mutex != NULL && "EspDataStorage has not been initialized, call init() first.");
    assert(fs != NULL && "Partition object is NULL, invalid argument.");

    TAKE_LOCK();
    File f = fs->open(path, FILE_WRITE);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for write");
        f.close();
        GIVE_LOCK();
        return false;
    }

    if (!f.print(data)) {
        ESP_LOGE(TAG, "Write failed to file: %s", path);
        f.close();
        GIVE_LOCK();
        return false;
    }

    f.close();
    GIVE_LOCK();
    return true;
}