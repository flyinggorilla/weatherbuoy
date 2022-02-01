#include "Alarm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"
#include "math.h"

static const char tag[] = "Alarm";

extern "C"
{
    // Application execution delay. Must be implemented by application.
    void delay(uint32_t ms)
    {
        vTaskDelay(ms / portTICK_PERIOD_MS);
    };
    // Current uptime in milliseconds. Must be implemented by application.
    uint32_t millis()
    {
        return esp_timer_get_time() / 1000;
    };
}

void fAlarmTask(void *pvParameter)
{
    ((Alarm *)pvParameter)->AlarmTask();
    vTaskDelete(NULL);
}

void Alarm::Start()
{
    xTaskCreate(&fAlarmTask, "Alarm", 1024 * 4, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}


void Alarm::AlarmTask()
{
    while (true)
    {
        Data data;
        if (!mrDataQueue.GetLatestData(data, 90)) {
            delay(500);
            continue;
        }


    } 
}

Alarm::Alarm(gpio_num_t canTX, gpio_num_t canRX, DataQueue &dataQueue) : mrDataQueue(dataQueue)
{
}



