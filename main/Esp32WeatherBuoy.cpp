/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
//#include <cstdio>
//#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "Esp32WeatherBuoy.h"
#include "Serial.h"
#include "Config.h"
#include "SendData.h"

#define SERIAL_BUFFER_SIZE (1024)
#define SERIAL_BAUD_RATE (19200)

static const char tag[] = "WeatherBuoy";

Config config;
Esp32WeatherBuoy esp32WeatherBuoy;

Esp32WeatherBuoy::Esp32WeatherBuoy() {

}
Esp32WeatherBuoy::~Esp32WeatherBuoy() {

}

extern "C" {
void app_main();
}

void app_main() {
	//ESP_ERROR_CHECK(nvs_flash_init()); this should be done by config!!
	//ESP_ERROR_CHECK(esp_netif_init());
	esp32WeatherBuoy.Start();
}

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text




void Esp32WeatherBuoy::Start() {

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");


    Serial serial(UART_NUM_1, GPIO_NUM_16, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);
    SendData sendData(2);

    String line;
    while (true) {
        serial.ReadLine(line);
        
        int dataStart = line.indexOf(STX);
        int dataEnd = line.lastIndexOf(ETX);

        if (dataStart >= 0 && dataEnd > 0) {
            ESP_LOGI(tag, "Measurement data'%s'", line.c_str());
            sendData.Post(line);
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




    /*for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();*/

