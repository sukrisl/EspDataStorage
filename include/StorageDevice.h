#pragma once

#include <cstddef>
#include <cstdint>

typedef enum {
    STORAGE_DEVICE_ONLINE = 0,
    STORAGE_DEVICE_OFFLINE,
    STORAGE_DEVICE_CORRUPT,
} StorageDeviceStatus_t;

typedef enum {
    STORAGE_DEVICE_TYPE_UNKNOWN = 0,
    STORAGE_DEVICE_TYPE_FLASH,
    STORAGE_DEVICE_TYPE_SD,
} StorageDeviceType_t;

typedef struct {
    StorageDeviceStatus_t status;
    StorageDeviceType_t type;
    uint32_t capacity;
} StorageDeviceInfo_t;

class StorageDevice {
   protected:
    StorageDeviceInfo_t info;

   public:
    virtual bool install() = 0;
    virtual bool uninstall() = 0;
    virtual bool registerPartition(const char* label, size_t size) = 0;

    void printInfo();
    StorageDeviceInfo_t getInfo();
};