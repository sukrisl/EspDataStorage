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

EspDataStorage storage;

static const char* TAG = "storage";

static void readTask(void* arg) {
    while (true) {
        char buffer[10000] = {0};
        storage.read("/sys/data.txt", buffer, sizeof(buffer));
        ESP_LOGI(TAG, "File content:\n%s", buffer);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void writeTask(void* arg) {
    while (true) {
        srand(esp_timer_get_time());
        uint32_t randInt = rand() % 500;
        char data[50];
        sprintf(data, "%d", randInt);
        storage.append("/sys/data.txt", data);
        vTaskDelay(pdMS_TO_TICKS(randInt));
    }
}

extern "C" void app_main(void) {
    storage.init();
    storage.addDevice(STORAGE_DEVICE_A_ID, STORAGE_DEVICE_TYPE_FLASH);
    storage.createPartition(STORAGE_DEVICE_A_ID, "sys", 0x100000);
    storage.mount("sys", "/sys");
    storage.mkfile("/sys/data.txt");

    xTaskCreate(readTask, "Storage read", (1024 * 100), NULL, 1, &readTaskHandle);
    xTaskCreate(writeTask, "Storage write", (1024 * 10), NULL, 1, &writeTaskHandle);

    // Remove file after 1 minutes
    vTaskDelay(pdMS_TO_TICKS(60000));
    if (storage.rm("/sys/data.txt")) {
        ESP_LOGI(TAG, "File deleted successfully");
    } else {
        ESP_LOGE(TAG, "File deletion failed");
    }

    if (readTaskHandle != NULL) {
        vTaskDelete(readTaskHandle);
        readTaskHandle = NULL;
    }

    if (writeTaskHandle != NULL) {
        vTaskDelete(writeTaskHandle);
        writeTaskHandle = NULL;
    }
}