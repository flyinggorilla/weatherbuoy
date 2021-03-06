#include "ReadMaximet.h"
#include "SendData.h"
#include "Serial.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"

static const char tag[] = "ReadMaximet";
#define SERIAL_BUFFER_SIZE (1024)
#define SERIAL_BAUD_RATE (19200)

void fReadMaximetTask(void *pvParameter) {
	((ReadMaximet*) pvParameter)->ReadMaximetTask();
	vTaskDelete(NULL);
}

void ReadMaximet::Start() {
	xTaskCreate(&fReadMaximetTask, "ReadMaximet", 8192, this, ESP_TASK_MAIN_PRIO, NULL);
}

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

void ReadMaximet::ReadMaximetTask() {
    Serial serial(UART_NUM_1, GPIO_NUM_16, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);

    String line;
    unsigned int uptimeMs = 0;
    unsigned int lastSendMs = 0; 
    unsigned int intervalMs = 0;

bool bDayTime = true; /////////////////////////////// TODO *****************************

    while (true) {
        if (!serial.ReadLine(line)) {
            ESP_LOGE(tag, "Could not read line from serial");
        }
        
        int dataStart = line.indexOf(STX);
        int dataEnd = line.lastIndexOf(ETX);

        if (dataStart >= 0 && dataEnd > 0) {
            if (bDayTime) {
                intervalMs = mrConfig.miSendDataIntervalDaytime * 1000; //ms;
            } else {
                intervalMs = mrConfig.miSendDataIntervalNighttime * 1000; //ms;
            }
            uptimeMs = (unsigned int)(esp_timer_get_time()/1000); // milliseconds since start
            if (intervalMs > uptimeMs - lastSendMs) {
                ESP_LOGI(tag, "Skipping measurement data'%s' as %d ms < %d ms", line.c_str(), uptimeMs - lastSendMs, intervalMs);
            } else {
                ESP_LOGI(tag, "Sending measurement data'%s' after %d ms", line.c_str(), uptimeMs - lastSendMs);
                lastSendMs = uptimeMs;
                if (!mrSendData.PostData(line)) {
                    ESP_LOGE(tag, "Could not post data, likely due to a full queue");
                }
            }
        } else {
            bool ok = true;
            for (int i = 0; i < line.length(); i++) {
                char c = line[i];
                if (c < 0x20 || c > 0x7E) {
                    ESP_LOGI(tag, "Garbled data '%d' bytes %s", line.length(), line.c_str());
                    ok = false;
                    break;
                }
            }
            if (ok) {
                ESP_LOGI(tag, "Metadata '%s'", line.c_str());
            }
        }

    } 
}

ReadMaximet::ReadMaximet(Config &config, SendData &sendData) : mrConfig(config), mrSendData(sendData) {
}



ReadMaximet::~ReadMaximet() {
}

