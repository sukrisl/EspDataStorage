#include "StorageDevice.h"

#include <esp_log.h>

static const char* TAG = "StorageDevice";

static const char* storageDeviceStatusToName(StorageDeviceStatus_t stat) {
    switch (stat) {
        case STORAGE_DEVICE_ONLINE:
            return "ONLINE";
        case STORAGE_DEVICE_OFFLINE:
            return "OFFLINE";
        case STORAGE_DEVICE_CORRUPT:
            return "CORRUPTED";
    }
    return NULL;
}

static const char* storageDeviceTypeToName(StorageDeviceType_t type) {
    switch (type) {
        case STORAGE_DEVICE_TYPE_UNKNOWN:
            return "UNKNOWN";
        case STORAGE_DEVICE_TYPE_FLASH:
            return "FLASH";
        case STORAGE_DEVICE_TYPE_SD:
            return "SD";
    }
    return NULL;
}

void StorageDevice::printInfo() {
    ESP_LOGI(TAG, "status: %s", storageDeviceStatusToName(info.status));
    ESP_LOGI(TAG, "type: %s", storageDeviceTypeToName(info.type));
    ESP_LOGI(TAG, "capacity: %d bytes", info.capacity);
}

StorageDeviceInfo_t StorageDevice::getInfo() {
    return info;
}
