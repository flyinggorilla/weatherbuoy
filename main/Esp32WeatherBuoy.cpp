#include "sdkconfig.h"
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

Esp32WeatherBuoy esp32WeatherBuoy;

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

void TestHttp();
void TestATCommands(Cellular &cellular);

void Esp32WeatherBuoy::Start() {

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!config.Load()) {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    ESP_LOGI(tag, "Hostname: %s", config.msHostname.c_str());
    ESP_LOGI(tag, "Target URL: %s", config.msTargetUrl.c_str());
    ESP_LOGI(tag, "App Version: %s", esp_ota_get_app_description()->version);

    Max471Meter max471Meter(CONFIG_MAX471METER_GPIO_VOLTAGE, CONFIG_MAX471METER_GPIO_CURRENT);
    ESP_LOGI(tag, "Max471Meter: voltage %d mV, current %d mA??", max471Meter.Voltage(), max471Meter.Current());

    OnlineMode onlineMode = MODE_CELLULAR;
    switch(onlineMode) {
        case MODE_CELLULAR: 
            cellular.InitModem(config.msCellularApn, config.msCellularUser, config.msCellularPass);
            cellular.Start();
            cellular.SwitchToPppMode();
            break;
        case MODE_WIFISTA:
            //config.msSTASsid = "";
            //config.msSTAPass = "";
            //config.Save();
            ESP_LOGI(tag, "sssi %s pass %s host %s", config.msSTASsid.c_str(), config.msSTAPass.c_str(), config.msHostname.c_str());
            wifi.StartSTAMode(config.msSTASsid, config.msSTAPass, config.msHostname);
            break;
        case MODE_WIFIAP:
            ESP_LOGI(tag, "sssi %s pass %s host %s", config.msAPSsid.c_str(), config.msAPPass.c_str(), config.msHostname.c_str());
            wifi.StartAPMode(config.msAPSsid, config.msAPPass, config.msHostname);
            break;
        case MODE_OFFLINE:
            ESP_LOGW(tag, "Staying offline.");
    }

    ESP_LOGI(tag, "Starting maximet stuff");
    //TestATCommands();
    //TestHttp();

    SendData sendData(config, cellular);

    ReadMaximet readMaximet(config, sendData);
    readMaximet.Start();

    while (true) {
        if (!sendData.PostHealth(max471Meter.Voltage(), max471Meter.Current())) {
                ESP_LOGE(tag, "Could not post data, likely due to a full queue");
        }
        vTaskDelay(config.miSendDataIntervalHealth*1000 / portTICK_PERIOD_MS);
    }  
}


void TestHttp() {
    for (int i = 0; i < 5; i++) {
//        cellular.Command("AT+CGDCONT=1,\"IP\",\"webapn.at\"", "Define PDP Context");
        //cellular.Command("ATD*99#", "setup data connection");

        vTaskDelay(10000/portTICK_PERIOD_MS);



        esp_http_client_config_t httpConfig = {0};
        httpConfig.url = "http://ptsv2.com/t/wb/post?testWeatherbuoy";
        
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
            return;
        } else {
            ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        }
    }
}

void TestATCommands(Cellular &cellular) {
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", cellular.getDataSent(), cellular.getDataReceived());
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", cellular.getDataSent(), cellular.getDataReceived());
  
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", cellular.getDataSent(), cellular.getDataReceived());

    ESP_LOGI(tag, "Free memory: %d", esp_get_free_heap_size());

    cellular.SwitchToCommandMode();

    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", cellular.getDataSent(), cellular.getDataReceived());



    String response;
    cellular.Command("ATI", "OK", nullptr, "Display Product Identification Information"); // SIM800 R14.18
    cellular.Command("AT+CGMM", "OK", nullptr,  "Model Identification"); // SIMCOM_SIM800L
    cellular.Command("AT+GMM", "OK", nullptr,  "Request TA Model Identification"); // SIMCOM_SIM800L
    cellular.Command("AT+CGSN", "OK", nullptr,  "Product Serial Number Identification (IMEI)"); // 8673720588*****
    cellular.Command("AT+CREG?", "OK", nullptr,  "Network Registration Information States"); // +CREG: 0,5
    cellular.Command("AT+CGMR", "OK", nullptr,  "Request TA Revision Identification of Software Release"); // Revision:1418B05SIM800L24
    cellular.Command("AT+GMR", "OK", nullptr,  "Request TA Revision Identification of Software Release"); // Revision:1418B05SIM800L24
    cellular.Command("AT+CGMI", "OK", nullptr,  "Request Manufacturer Identification"); // SIMCOM_Ltd
    cellular.Command("AT+GMI", "OK", nullptr,  "Request Manufacturer Identification"); // SIMCOM_Ltd
    cellular.Command("AT+CIMI", "OK", nullptr,  "Request international mobile subscriber identity"); // 23212200*******
    cellular.Command("AT+CROAMING", "OK", nullptr,  "Roaming State 0=home, 1=intl, 2=other"); // +CROAMING: 2
    cellular.Command("AT+CSQ", "OK", nullptr,  "Signal Quality Report"); // +CSQ: 13,0
    cellular.Command("AT+CNUM", "OK", nullptr,  "Subscriber Number"); // +CNUM: "","+43681207*****",145,0,4
    cellular.Command("AT+CBC", "OK", nullptr,  "Battery Charge"); // +CBC: 0,80,4043
    cellular.Command("AT+GSN", "OK", nullptr,  "Request TA Serial Number Identification (IMEI)"); // 8673720588*****
    cellular.Command("AT+GCAP", "OK", nullptr,  "Request Complete TA Capabilities List"); // +GCAP: +CGSM
    cellular.Command("AT+CSTT?", "OK", nullptr,  "Query APN and login info"); // +CSTT: "CMNET","",""
    cellular.Command("AT+COPS?", "OK", nullptr,  "Operator Selection"); // +COPS: 0,0,"A1"
    cellular.Command("AT+CGATT=?", "OK", nullptr,  "Attach or Detach from GPRS Service "); // +CGATT: (0,1)

    #define RUNCOMMAND_READ_SMS true
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
        ESP_LOGI(tag, "SMS '%s'", response.c_str());
    }

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }  

    // cellular.Command("AT&V", "OK", nullptr,  "Display Current Configuration", 5000); // DEFAULT PROFILE ..... lots of stuff
    // cellular.Command("ATO", "OK", nullptr,  "Switch from Command Mode to Data Mode (return to Online data state)", 100);
    // cellular.Command("AT+CEER", "OK", nullptr,  "Request Extended Error Report", 1000); // +CEER: No Cause
    // cellular.Command("AT+CGDCONT=1,\"IP\",\"webapn.at\"", "OK", nullptr,  "Define PDP Context"); // OK
    // cellular.Command("AT+CGATT=1", "OK", nullptr,  "Attach or Detach from GPRS Service "); 
    // cellular.Command("AT+CSTT: \"webapn.at\",\"\",\"\"", "OK", nullptr,  "Set APN");
    // cellular.Command("AT+CSTT: \"webapn.at\"", "OK", nullptr,  "Set APN");
    // cellular.Command("AT+CGATT?", "OK", nullptr,  "Check if the MS is connected to the GPRS network. 0=disconnected"); // +CGATT: 0
    // cellular.Command("AT+CGATT=l", "OK", nullptr,  "Register with GPRS network."); // 
    // cellular.Command("AT+CGATT?", "OK", nullptr,  "Check if the MS is connected to the GPRS network. 0=disconnected"); // +CGATT: 0
    // cellular.Command("AT+CREG=?", "OK", nullptr,  "List of Network Registration Information States"); // +CREG: (0-2)
    // cellular.Command("AT+CREG=1", "OK", nullptr,  "Register on home network");  // OK
    // cellular.Command("AT+CGATT=?", "OK", nullptr,  "Attach/Detach to GPRS. List of supported states"); // +CGATT: (0,1)

}