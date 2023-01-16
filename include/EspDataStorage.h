#pragma once

#include <unordered_map>
#include <memory>

#include "StorageDevice.h"

class EspDataStorage {
 private:
    std::unordered_map<uint8_t, std::shared_ptr<StorageDevice>> devices;

 public:
    bool addDevice(uint8_t id, StorageDeviceType_t type);
    bool removeDevice(uint8_t id);

    bool createPartition(uint8_t partitionID, const char* label, size_t size);
    bool mount(const char* partitionLabel, const char* basePath);

    bool print(const char* filepath);
    bool read(const char* filepath, char* dest, uint32_t bufferSize, char terminator = '\0');
    bool append(const char* filepath, const char* data);
    bool write(const char* filepath, const char* data);
    bool rm(const char* filepath);
};