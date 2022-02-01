#include "Alarm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
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
    if (mrConfig.mbAlarmSound) {
        gpio_reset_pin(mGpioBuzzer);
        gpio_set_direction(mGpioBuzzer, GPIO_MODE_OUTPUT);
        gpio_set_drive_capability(mGpioBuzzer, GPIO_DRIVE_CAP_MAX);
        ESP_LOGI(tag, "Buzzer enabled");
    }

    if (mrConfig.miAlarmRadius < 10) {
        //ESP_LOGE(tag, "Alarm Radius too small")
    }

    while (true)
    {
        Data data;
        if (!mrDataQueue.GetLatestData(data, 90)) {
            delay(500);
            continue;
        }


    } 

    while(1) {
        gpio_set_level(mGpioBuzzer, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);

        gpio_set_level(mGpioBuzzer, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

}

Alarm::Alarm(DataQueue &dataQueue, Config &config, gpio_num_t buzzer) : mrDataQueue(dataQueue), mrConfig(config), mGpioBuzzer(buzzer)
{
}



