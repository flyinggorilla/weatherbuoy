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
#include "Config.h"
#include "SendData.h"
#include "ReadMaximet.h"
#include "Wifi.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "Modem.h"

static const char tag[] = "WeatherBuoy";

Config config;
Esp32WeatherBuoy esp32WeatherBuoy;
Wifi wifi;


Esp32WeatherBuoy::Esp32WeatherBuoy() {

}
Esp32WeatherBuoy::~Esp32WeatherBuoy() {

}

extern "C" {
void app_main();
}

void app_main() {
	ESP_ERROR_CHECK(esp_netif_init()); 
	esp32WeatherBuoy.Start();
}


void Esp32WeatherBuoy::Start() {

    Config config;

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!config.Load()) {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    
    Modem modem(config.msModemApn, config.msModemUser, config.msModemPass);
    modem.TurnOn();
    modem.InitNetwork();
    modem.Start();


    modem.Command("AT", "ATtention");
    modem.Command("ATI", "Display Product Identification Information");
    modem.Command("AT+CGMM", "Model Identification");
    modem.Command("AT+GMM", "Request TA Model Identification");
    modem.Command("AT+CGSN", "Product Serial Number Identification (IMEI)");
    modem.Command("AT+CREG?", "Network Registration Information States *****************************");
    modem.Command("AT+CGMR", "Request TA Revision Identification of Software Release");
    modem.Command("AT+GMR", "Request TA Revision Identification of Software Release");
    modem.Command("AT+CGMI", "Request Manufacturer Identification");
    modem.Command("AT+GMI", "Request Manufacturer Identification");
    modem.Command("AT+CIMI", "Request international mobile subscriber identity");
    modem.Command("AT+CSQ", "Signal Quality Report");
    modem.Command("AT+CNUM", "Subscriber Number");
    modem.Command("AT+CBC", "Battery Charge");
    modem.Command("AT+GSN", "Request TA Serial Number Identification (IMEI)");
    modem.Command("AT+GCAP", "Request Complete TA Capabilities List");
    modem.Command("AT&V", "Display Current Configuration", 5000);
    modem.Command("ATO", "Switch from Command Mode to Data Mode (return to Online data state)", 100);
    modem.Command("AT+CEER", "Request Extended Error Report", 1000);
     

    ESP_LOGI(tag, "Free memory: %d", esp_get_free_heap_size());

/*    ESP_LOGI(tag, "Hostname: %s", config.msHostname.c_str());
    ESP_LOGI(tag, "Target URL: %s", config.msTargetUrl.c_str());
    ESP_LOGI(tag, "App Version: %s", esp_ota_get_app_description()->version);

    //ESP_LOGI(tag, "sssi %s pass %s host %s", config.msAPSsid.c_str(), config.msAPPass.c_str(), config.msHostname.c_str());
    //wifi.StartAPMode(config.msAPSsid, config.msAPPass, config.msHostname);
    ESP_LOGI(tag, "sssi %s pass %s host %s", config.msSTASsid.c_str(), config.msSTAPass.c_str(), config.msHostname.c_str());
    wifi.StartSTAMode(config.msSTASsid, config.msSTAPass, config.msHostname);

    SendData sendData(config);

    ReadMaximet readMaximet(config, sendData);
    readMaximet.Start();

    while (true) {
        if (!sendData.PostHealth()) {
                ESP_LOGE(tag, "Could not post data, likely due to a full queue");
        }
        vTaskDelay(mConfig.miSendDataIntervalHealth*1000 / portTICK_PERIOD_MS);
    }  */
}
