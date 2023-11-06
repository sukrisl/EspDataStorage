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
        storage.read("/data.txt", buffer, sizeof(buffer));
        ESP_LOGI(TAG, "File content:\n%s", buffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void writeTask(void* arg) {
    while (true) {
        srand(esp_timer_get_time());
        uint32_t randInt = rand() % 500;
        char data[50];
        sprintf(data, "%d\n", randInt);
        storage.append("/data.txt", data);
        vTaskDelay(pdMS_TO_TICKS(randInt));
    }
}

extern "C" void app_main(void) {
    storage.init();
    storage.mkdev(STORAGE_DEVICE_A_ID, STORAGE_DEVICE_TYPE_FLASH);
    storage.mkpartition(STORAGE_DEVICE_A_ID, "sys", 0x100000);
    storage.mount("sys", "/sys");
    storage.mkfile("/data.txt");

    xTaskCreate(writeTask, "Storage write", (1024 * 10), NULL, 1, &writeTaskHandle);
    xTaskCreate(readTask, "Storage read", (1024 * 100), NULL, 1, &readTaskHandle);

    // Remove file after 10 seconds
    vTaskDelay(pdMS_TO_TICKS(10000));
    storage.listdir("/");
    ESP_LOGI(TAG, "/data.txt size: %d", storage.fsize("/data.txt"));
    if (storage.rm("/data.txt")) {
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