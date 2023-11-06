#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "EspDataStorage.h"
#include "esp_log.h"

#define STORAGE_DEVICE_A_ID 1

static TaskHandle_t readTaskHandle = NULL;
static TaskHandle_t writeTaskHandle = NULL;

static EspDataStorage storage;
static Partition_t* exFS = NULL;
static Partition_t* inFS = NULL;

static bool isDone = false;

static const char* TAG = "storage";

static void readTask(void* arg) {
    while (true) {
        char buffer[10000] = {0};
        char bufferIn[10000] = {0};
        storage.read(exFS, "/data.txt", buffer, sizeof(buffer));
        storage.read(inFS, "/data.txt", bufferIn, sizeof(bufferIn));
        ESP_LOGI(TAG, "File content external:\n%s", buffer);
        ESP_LOGI(TAG, "File content internal:\n%s", bufferIn);
        vTaskDelay(pdMS_TO_TICKS(10));
        if (isDone) vTaskDelete(NULL);
    }
}

static void writeTask(void* arg) {
    while (true) {
        srand(esp_timer_get_time());
        uint32_t randInt = rand() % 10;
        char data[50];
        sprintf(data, "%d", randInt);
        storage.append(exFS, "/data.txt", data);
        sprintf(data, "%d", randInt + 5);
        storage.append(inFS, "/data.txt", data);
        vTaskDelay(pdMS_TO_TICKS(randInt));
        if (isDone) vTaskDelete(NULL);
    }
}

extern "C" void app_main(void) {
    storage.init();

    // Initialize storage partition in external flash
    storage.mkdev(STORAGE_DEVICE_A_ID, STORAGE_DEVICE_TYPE_FLASH);
    storage.mkpartition(STORAGE_DEVICE_A_ID, "exFS", 0x100000);
    exFS = storage.mount("exFS", "/exFS");

    // Mount storage partition in internal flash
    inFS = storage.mount("spiffs", "/inFS", true);

    // Create the same name file in different partition
    storage.listdir(exFS, "/");
    storage.listdir(inFS, "/");
    storage.mkfile(exFS, "/data.txt");
    storage.mkfile(inFS, "/data.txt");

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskCreate(writeTask, "Storage write internal", (1024 * 10), NULL, 1, &writeTaskHandle);
    xTaskCreate(readTask, "Storage read internal", (1024 * 100), NULL, 1, &readTaskHandle);

    // Remove file after 10 seconds
    vTaskDelay(pdMS_TO_TICKS(10000));
    isDone = true;
    vTaskDelay(pdMS_TO_TICKS(100));
    storage.listdir(exFS, "/");
    storage.listdir(inFS, "/");
    ESP_LOGI(TAG, "exFS:/data.txt size: %d", storage.fsize(exFS, "/data.txt"));
    ESP_LOGI(TAG, "inFS:/data.txt size: %d", storage.fsize(inFS, "/data.txt"));

    storage.rm(exFS, "/data.txt");
    storage.rm(inFS, "/data.txt");
    storage.unmount(exFS);
    storage.unmount(inFS);
    storage.done();
}