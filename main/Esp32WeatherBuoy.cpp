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
#include "Cellular.h"
#include "Max471Meter.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"

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
	//esp32WeatherBuoy.Wifi();
}

void Esp32WeatherBuoy::Wifi() {
    Config config;

    ESP_LOGI(tag, "Atterwind ****WIFI MODE**** WeatherBuoy starting!");
    if (!config.Load()) {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }


    //config.msSTASsid = "";
    //config.msSTAPass = "";
    //config.Save();
    wifi.StartSTAMode(config.msSTASsid, config.msSTAPass, config.msHostname);
    TestHttp();
}


void Esp32WeatherBuoy::Start() {

    Config config;

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!config.Load()) {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    Max471Meter max471Meter;
    ESP_LOGI(tag, "Max471Meter: voltage %d mV, current %d mA??", max471Meter.Voltage(), max471Meter.Current());


    
    Cellular cellular(config.msCellularApn, config.msCellularUser, config.msCellularPass);
    cellular.TurnOn();
    cellular.InitNetwork();
    cellular.Start();

  //  cellular.SwitchToPppMode();

    String response;

    cellular.Command("AT", "OK", &response, "ATtention"); // OK
    //cellular.SwitchToCommandMode();
    cellular.Command("AT+CPIN?", "OK", &response, "Is a PIN needed?"); // +CPIN: READY




   /* //cellular.Command("ATI", "Display Product Identification Information");
    //cellular.Command("AT+CGMM", "Model Identification");
    cellular.Command("AT+GMM", "Request TA Model Identification");
    cellular.Command("AT+CGSN", "Product Serial Number Identification (IMEI)");
    cellular.Command("AT+CREG?", "Network Registration Information States *****************************");
    //cellular.Command("AT+CGMR", "Request TA Revision Identification of Software Release");
    //cellular.Command("AT+GMR", "Request TA Revision Identification of Software Release");
    //cellular.Command("AT+CGMI", "Request Manufacturer Identification");
    //cellular.Command("AT+GMI", "Request Manufacturer Identification");
    cellular.Command("AT+CIMI", "Request international mobile subscriber identity");
    cellular.Command("AT+CSQ", "Signal Quality Report");
    cellular.Command("AT+CNUM", "Subscriber Number");
    cellular.Command("AT+CBC", "Battery Charge");
    cellular.Command("AT+GSN", "Request TA Serial Number Identification (IMEI)");
    //cellular.Command("AT+GCAP", "Request Complete TA Capabilities List");
    //cellular.Command("AT&V", "Display Current Configuration", 5000);
    //cellular.Command("ATO", "Switch from Command Mode to Data Mode (return to Online data state)", 100);
    cellular.Command("AT+CEER", "Request Extended Error Report", 1000);
    String command;
    //command.printf("AT+CSTT: \"%s\",\"%s\",\"%s\"", CONFIG_WEATHERBUOY_CELLULAR_APN, CONFIG_WEATHERBUOY_CELLULAR_USER, CONFIG_WEATHERBUOY_CELLULAR_PASS);
    //command.printf("AT+CSTT: \"%s\"", CONFIG_WEATHERBUOY_CELLULAR_APN);
    //cellular.Command(command.c_str(), "Start Task and Set APN, USER NAME, PASSWORD", 1000);
    cellular.Command("AT+CGDCONT=1,\"IP\",\"webapn.at\"", "Define PDP Context");
    cellular.Command("AT+CSTT?", "Query APN and login info");
    cellular.Command("AT+COPS?", "Operator Selection");
    cellular.Command("AT+CGATT=?", "Attach or Detach from GPRS Service ");
    cellular.Command("AT+CGATT=1", "Attach or Detach from GPRS Service ");
    cellular.Command("AT+CSTT: \"webapn.at\",\"\",\"\"", "Set APN");
    cellular.Command("AT+CSTT: \"webapn.at\"", "Set APN");
    cellular.Command("AT+CROAMING", "Roaming State 0=home, 1=intl, 2=other");*/

    // +CSTT: "movistar.bluevia.es","",""

//    cellular.Command("AT+CGATT?", "Check if the MS is connected to the GPRS network. 0=disconnected"); // +CGATT: 0
//    cellular.Command("AT+CGATT=l", "Register with GPRS network."); // 
//    cellular.Command("AT+CGATT?", "Check if the MS is connected to the GPRS network. 0=disconnected"); // +CGATT: 0
//    cellular.Command("AT+CREG=?", "List of Network Registration Information States"); // +CREG: (0-2)
//    cellular.Command("AT+CREG=1", "Register on home network");  // OK
//    cellular.Command("AT+CGATT=?", "Attach/Detach to GPRS. List of supported states"); // +CGATT: (0,1)

    while (true) {
        if (cellular.Command("AT+CREG?", "OK", &response, "Network Registration Information States ")) { // +CREG: 0,2 // +CREG: 1,5
            if (response.indexOf("+CREG: ") >= 0 && response.indexOf(",5") >= 0)
                break;
        }
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }

    #define RUNCOMMAND_READ_SMS false
    if (RUNCOMMAND_READ_SMS) {
        cellular.Command("AT+CMGF=1", "OK", nullptr, "If the modem reponds with OK this SMS mode is supported"); 
        cellular.Command("AT+CMGL=\"ALL\"", "OK", &response, "dump all SMS", 1000); //
                    //     W (17440) WeatherBuoy: SMS '+CMGL: 1,"REC READ","810820","","21/03/18,14:15:32+04"
                    // Lieber yesss! Kunde, jetzt den aktuellen Stand Ihrer Freieinheiten per SMS abfragen! Einfach mit dem Text: "Info" antworten. Weitere Infos unter www.yess
                    // +CMGL: 2,"REC READ","810820","","21/03/18,14:15:32+04"
                    // s.at. Ihr yesss! Team
                    // +CMGL: 3,"REC READ","810810","","21/03/18,15:25:44+04"
                    // ruppe werden, k�nnen Sie diese SMS einfach ignorieren. Ihr yesss! Team
                    // +CMGL: 4,"REC READ","810810","","21/03/18,15:25:44+04"
                    // Lieber yesss! Kunde, die Rufnummer 436818******* m�chte Sie zu einer Gruppe hinzuf�gen. Der Aktivierungscode lautet: xxxxx
                    // Wollen Sie nicht Teil dieser G
                    // +CMGL: 5,"REC READ","+436640000000","","21/03/18,15:31:19+04"
                    // bernds test message '
        ESP_LOGW(tag, "SMS '%s'", response.c_str());
    }

    cellular.SwitchToPppMode();
  
    TestHttp();


    ESP_LOGI(tag, "Free memory: %d", esp_get_free_heap_size());

    while (true) {
        vTaskDelay(mConfig.miSendDataIntervalHealth*1000 / portTICK_PERIOD_MS);
    }  


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


void Esp32WeatherBuoy::TestHttp() {
    for (int i = 0; i < 100; i++) {
//        cellular.Command("AT+CGDCONT=1,\"IP\",\"webapn.at\"", "Define PDP Context");
        //cellular.Command("ATD*99#", "setup data connection");

        vTaskDelay(10000/portTICK_PERIOD_MS);



        esp_http_client_config_t httpConfig = {0};
        //httpConfig.url = "http://ptsv2.com/t/wb/post?testSepperl";
        //httpConfig.url = "http://216.239.32.21/t/wb/post?test1";
        httpConfig.url = "http://scooterlabs.com/echo";
        httpConfig.url = "http://66.39.74.7/echo";
        
        httpConfig.method = HTTP_METHOD_GET; 
        esp_http_client_handle_t httpClient = esp_http_client_init(&httpConfig);
        //esp_http_client_set_header(httpClient, "Content-Type", "text/plain");
        esp_err_t err;
        err = esp_http_client_open(httpClient, 0);
        if (err != ESP_OK) {
            ESP_LOGE(tag, "Error %s in esp_http_client_open(): %s", esp_err_to_name(err), httpConfig.url);
        }

        int sent = esp_http_client_write(httpClient, "", 0);
        if (sent == 0) {
            ESP_LOGD(tag, "esp_http_client_write(): OK, sent: %d", sent);
        } else {
            ESP_LOGE(tag, "esp_http_client_write(): Could only send %d of %d bytes", sent, 0);
        }


        // retreive HTTP response and headers
        int iContentLength = esp_http_client_fetch_headers(httpClient);
        if (iContentLength == ESP_FAIL) {
            ESP_LOGE(tag, "esp_http_client_fetch_headers(): could not receive HTTP response");
        }

        // Check HTTP status code
        int iHttpStatusCode = esp_http_client_get_status_code(httpClient);
        if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400)) {
            ESP_LOGI(tag, "HTTP response OK. Status %d, Content-Length %d", iHttpStatusCode , iContentLength);
        } else {
            ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        }
    }
}