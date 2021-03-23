#include "Watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

static const char tag[] = "Watchdog";

void fWatchdogTask(void *pvParameter) {
	((Watchdog*) pvParameter)->WatchdogTask();
	vTaskDelete(NULL);
}

Watchdog::Watchdog(int seconds) {
    miSeconds = seconds;
    ESP_LOGD(tag, "Starting Watchdog Task");
	xTaskCreate(&fWatchdogTask, "Watchdog", 1024, this, ESP_TASK_TIMER_PRIO, NULL); 

}

void Watchdog::clear() {
    mbReset = false;
}

void Watchdog::WatchdogTask() {
    while(true) {
        mbReset = true;
        vTaskDelay(miSeconds*1000/portTICK_PERIOD_MS);
        if (mbReset) {
            ESP_LOGW(tag, "Watchdog triggered. restarting.");
            esp_restart();
        }
    }
}

Watchdog::~Watchdog() {

}
