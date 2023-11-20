#pragma once

#include <LittleFS.h>

#include <memory>
#include <unordered_map>

#include "StorageDevice.h"

typedef fs::LittleFSFS Partition_t;

class EspDataStorage {
   private:
    std::unordered_map<uint8_t, std::shared_ptr<StorageDevice>> devices;
    uint32_t _waitTimeout_ms;

   public:
    bool init(uint32_t waitTimeout_ms = 500);
    void done();
    bool isBusy();

    bool mkdev(uint8_t id, StorageDeviceType_t type);
    bool rmdev(uint8_t id);

    bool mkpartition(uint8_t partitionID, const char* label, size_t size);
    Partition_t* mount(const char* partitionLabel, const char* basePath, bool formatOnFail = false);
    bool unmount(Partition_t* fs);

    bool exists(Partition_t* fs, const char* path);
    bool mkdir(Partition_t* fs, const char* dirname);
    bool rmdir(Partition_t* fs, const char* dirname);
    bool listdir(Partition_t* fs, const char* dirname, uint8_t level = 0);

    bool mkfile(Partition_t* fs, const char* path);
    bool rm(Partition_t* fs, const char* path);
    size_t fsize(Partition_t* fs, const char* path);
    bool read(Partition_t* fs, const char* path, char* dest, uint32_t bufferLen, char terminator = 0, uint32_t pos = 0);
    bool append(Partition_t* fs, const char* path, const char* data);
    bool write(Partition_t* fs, const char* path, const char* data);
};