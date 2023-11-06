#pragma once

#include <memory>
#include <unordered_map>

#include "StorageDevice.h"

class EspDataStorage {
   private:
    std::unordered_map<uint8_t, std::shared_ptr<StorageDevice>> devices;
    uint32_t _waitTimeout_ms;

   public:
    bool init(uint32_t waitTimeout_ms = 500);

    bool addDevice(uint8_t id, StorageDeviceType_t type);
    bool removeDevice(uint8_t id);

    bool createPartition(uint8_t partitionID, const char* label, size_t size);
    bool mount(const char* partitionLabel, const char* basePath, bool formatOnFail = false);

    void listDir(const char* dirname, uint8_t level = 1);

    bool print(const char* path);
    bool mkfile(const char* path);
    bool read(const char* path, char* dest, uint32_t bufferLen);
    bool append(const char* path, const char* data);
    bool write(const char* path, const char* data);
    bool rm(const char* path);
};