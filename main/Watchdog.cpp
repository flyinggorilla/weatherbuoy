#include "Watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "RtcVariables.h"


static const char tag[] = "Watchdog";

void fWatchdogTask(void *pvParameter) {
	((Watchdog*) pvParameter)->WatchdogTask();
	vTaskDelete(NULL);
}

Watchdog::Watchdog(int seconds) {
    miSeconds = seconds;
    ESP_LOGD(tag, "Starting Watchdog Task");
	xTaskCreate(&fWatchdogTask, "Watchdog", 2048, this, ESP_TASK_TIMER_PRIO, &mhTask); 

}

void Watchdog::clear() {
    xTaskNotifyGive(mhTask);
}

void Watchdog::WatchdogTask() {
    ulTaskNotifyTake(pdTRUE, 0); // suppress the first-start "watchdog cleared"
    while(true) {
        if (ulTaskNotifyTake(pdTRUE, miSeconds*1000/portTICK_PERIOD_MS) == pdTRUE) {
            ESP_LOGW(tag, "Watchdog cleared.");
        } else {
            ESP_LOGW(tag, "Watchdog triggered. restarting.");
            RtcVariables::SetExtendedResetReason(RtcVariables::EXTENDED_RESETREASON_WATCHDOG);
            esp_restart();
        }
    }
}

Watchdog::~Watchdog() {

}
