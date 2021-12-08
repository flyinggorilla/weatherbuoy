#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "Esp32WeatherBuoy.h"
#include "Config.h"
#include "SendData.h"
#include "ReadMaximet.h"
#include "Watchdog.h"
#include "Wifi.h"
#include "Cellular.h"
#include "Max471Meter.h"
#include "TemperatureSensors.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"

///////////////////// BEGIN NMEA SPECIFIC  ////////////////////////////////


#include "NMEA2000_esp32.h"
#include "N2kStream_esp32.h"
#include "N2kMessages.h"
extern "C" {
// Application execution delay. Must be implemented by application.
     void delay(uint32_t ms) {
        vTaskDelay(ms / portTICK_PERIOD_MS);
     };
// Current uptime in milliseconds. Must be implemented by application.
     uint32_t millis() {
         return esp_timer_get_time()/1000;
     };
}

///////////////////// END NMEA SPECIFIC ////////////////////////////////



static const char tag[] = "WeatherBuoy";

// ------------------------------------------
// change device in Menuconfig->Cellular
// ------------------------------------------
#ifdef CONFIG_LILYGO_TTGO_TCALL14_SIM800
    // LILYGO TTGO T-Call V1.4 SIM800 
    // ADC ----------------------------
    // Using ADC1 with possible GPIO: 32, 34, 35, 36, 39 
    #define CONFIG_MAX471METER_GPIO_VOLTAGE 35 
    #define CONFIG_MAX471METER_GPIO_CURRENT 34

    // MAXIMET Serial
    // dont use default RX/TX GPIO as it is needed for console output.
    // dont use conflicting internal flash and boot-blocking GPIO 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 
    #define CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN 13 
    #define CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN 14 
#elif CONFIG_LILYGO_TTGO_TPCIE_SIM7600
    // LILYGO® TTGO T-PCIE SIM7600
    // ADC ----------------------------
    // Using ADC1 with possible GPIO: 32, 34, 35, 39 (note GPIO 33 did not work)
    #define CONFIG_MAX471METER_GPIO_VOLTAGE 39 
    #define CONFIG_MAX471METER_GPIO_CURRENT 34

    // MAXIMET Serial
    // dont use default RX/TX GPIO as it is needed for console output.
    // dont use conflicting internal flash and boot-blocking GPIO 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 
    #define CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN 13 
    #define CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN 14 

    // OneWire protocol for temperature sensors DS18B20
    #define CONFIG_TEMPERATURESENSOR_GPIO_ONEWIRE 15 
    // Test setup DS18B20:
    // Breadboard TOS ROM code: "303c01e076e5f528" 
    // Waterproof sensor ROM code: "220120639e26f028"
#endif

#define NMEA 
#ifdef NMEA
    // I2C Pins 
    #define CONFIG_I2C_SCL_PIN 22
    #define CONFIG_I2C_SDA_PIN 21 
    // I2C Pins used for TWAI (NMEA over CAN bus)
    #define CONFIG_NMEA_TWAI_RX_PIN GPIO_NUM_22
    #define CONFIG_NMEA_TWAI_TX_PIN GPIO_NUM_21 
#endif

// Restart ESP32 if there is not data being successfully sent within this period.
#define CONFIG_WATCHDOG_SECONDS 60*125 // 125min - if two hourly sends fail, thats the latest to restart
#define CONFIG_SENDDATA_INTERVAL_DAYTIME 60 // seconds
#define CONFIG_SENDDATA_INTERVAL_NIGHTTIME 60*5 // 5 minutes
#define CONFIG_SENDDATA_INTERVAL_DIAGNOSTICS 60*15 // every 15min
#define CONFIG_SENDDATA_INTERVAL_LOWBATTERY 60*60 // hourly


Esp32WeatherBuoy::Esp32WeatherBuoy() {

}
Esp32WeatherBuoy::~Esp32WeatherBuoy() {

}

extern "C" {
void app_main();
}

void app_main() {
	ESP_ERROR_CHECK(esp_netif_init()); 
    Esp32WeatherBuoy esp32WeatherBuoy;
	esp32WeatherBuoy.Start();
    esp_restart();
}


void TestHttp();
void TestATCommands(Cellular &cellular);

void Esp32WeatherBuoy::Start() {

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!config.Load()) {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    int watchdogSeconds = CONFIG_WATCHDOG_SECONDS;
    Watchdog watchdog(watchdogSeconds);
    ESP_LOGI(tag, "Watchdog started and adjusted to %d seconds.", watchdogSeconds);

    TemperatureSensors tempSensors(config);
    tempSensors.Init(CONFIG_TEMPERATURESENSOR_GPIO_ONEWIRE);


    ESP_LOGI(tag, "Hostname: %s", config.msHostname.c_str());
    ESP_LOGI(tag, "Target URL: %s", config.msTargetUrl.c_str());
    ESP_LOGI(tag, "App Version: %s", esp_ota_get_app_description()->version);

    Max471Meter max471Meter(CONFIG_MAX471METER_GPIO_VOLTAGE, CONFIG_MAX471METER_GPIO_CURRENT);
    ESP_LOGI(tag, "Max471Meter: voltage %d mV, current %d mA", max471Meter.Voltage(), max471Meter.Current());

    ReadMaximet readMaximet(config);
    readMaximet.Start(CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN, CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN);

    tNMEA2000_esp32 nmea(CONFIG_NMEA_TWAI_TX_PIN, CONFIG_NMEA_TWAI_RX_PIN);

    //const short NMEA_DEVICE_ENVIRONMENTAL = 0;
    /////////////// HOW TO USE MULTIPLE DEVICES
    ////////////// https://github.com/ttlappalainen/NMEA2000/blob/master/Examples/MultiDevice/MultiDevice.ino
    /////////////////////////////////////////////////////////////////////
    nmea.SetDeviceCount(1); // Enable multi device support for 2 devices
    nmea.SetProductInformation("00000002", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "weatherbuoy",  // Manufacturer's Model ID
                                 "1.1.0.22 (2021-12-08)",  // Manufacturer's Software version code
                                 "1.1.0.0 (2021-12-08)" // Manufacturer's Model version
                                 );


    nmea.SetDeviceInformation(1, // Unique number. Use e.g. Serial number.
                                130, // Device function=Atmospheric. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                85, // Device class=External Environment. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf                               
                               );

    nmea.SetMode(tNMEA2000::N2km_NodeOnly,23);

    N2kStream_esp32 nmeaLogStream;
    nmea.SetForwardStream(&nmeaLogStream);
    //nmea.SetForwardOwnMessages(true);
    nmea.EnableForward(true);
    // List here messages your device will transmit.
    const unsigned long TransmitMessages[]={130306L,0};    // 130306L PGN: Wind 
    nmea.ExtendTransmitMessages(TransmitMessages);
    nmea.Open();


    OnlineMode onlineMode = MODE_CELLULAR;

    //ESP_LOGW(tag, "OFFLINE MODE");
    //onlineMode = MODE_OFFLINE;

    switch(onlineMode) {
        case MODE_CELLULAR: {
            cellular.InitModem();
            cellular.Start(config.msCellularApn, config.msCellularUser, config.msCellularPass, config.msCellularOperator, config.miCellularNetwork);
            // cellular.ReadSMS(); use only during firmware setup to receive a SIM based code 
            break; }
        case MODE_WIFISTA:
            //config.msSTASsid = "";
            //config.msSTAPass = "";
            //config.msTargetUrl = "http://10.10.14.195:3000/weatherbuoy";
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

    //TestATCommands();
    //TestHttp();

    SendData sendData(config, readMaximet, cellular, watchdog);

    ESP_LOGI(tag, "Starting Weatherbuoy main task.");

    bool bDiagnostics;
    bool bDiagnosticsAtStartup = true;
    unsigned int lastSendTimestamp = 0;
    unsigned int lastDiagnosticsTimestamp = 0;
    
    int secondsToSleep = 0;

    int logInfoSeconds = 0;

    while (true) {
        tempSensors.Read(); // note, this causes approx 700ms delay
        vTaskDelay(1*1000 / portTICK_PERIOD_MS);
        bool isMaximetData = readMaximet.WaitForData(60);
        unsigned int secondsSinceLastSend;
        unsigned int secondsSinceLastDiagnostics;

        //////////////////////////////////
        //////// NMEA TEST CODE
        tN2kMsg n2kMsg;
        SetN2kWindSpeed(n2kMsg, 1, 65, 66, N2kWind_Apparent);
        ESP_LOGI(tag, "NMEA sending...");
        if(nmea.SendMsg(n2kMsg)) {
            ESP_LOGI(tag, "NMEA SendMsg succeeded");
        } else {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        } 



        nmea.ParseMessages();

        // keep modem sleeping unless time to last send elapsed
        unsigned int uptimeSeconds = (unsigned int)(esp_timer_get_time()/1000000); // seconds since start
        secondsSinceLastSend = uptimeSeconds - lastSendTimestamp;
        if (secondsToSleep > secondsSinceLastSend) {
            if (logInfoSeconds == 0) {
                ESP_LOGI(tag, "Power management sleep: %d, Measurements in queue: %d", secondsToSleep - secondsSinceLastSend, readMaximet.GetQueueLength());
                logInfoSeconds = 60;
            }
            logInfoSeconds--;
            continue;
        }
        lastSendTimestamp = uptimeSeconds;

        // when sending, add diagnostics information after 300 seconds
        secondsSinceLastDiagnostics = uptimeSeconds - lastDiagnosticsTimestamp;
        if (secondsSinceLastDiagnostics > CONFIG_SENDDATA_INTERVAL_DIAGNOSTICS || bDiagnosticsAtStartup) {
            bDiagnostics = true;
            bDiagnosticsAtStartup = false;
            lastDiagnosticsTimestamp = uptimeSeconds;
        } else {
            bDiagnostics = false;
        }
        
        // check if maximeta or info data should be sent at all
        if (!isMaximetData && !bDiagnostics) {
            continue;
        }

        unsigned int voltage = max471Meter.Voltage();
        unsigned int current = max471Meter.Current();
        float boardtemp = tempSensors.GetBoardTemp();
        float watertemp = tempSensors.GetWaterTemp();

        if (onlineMode == MODE_CELLULAR) {
            ESP_LOGI(tag, "switching to full power mode next...");
            if (!cellular.SwitchToFullPowerMode()) {
                ESP_LOGW(tag, "Retrying switching to full power mode ...");
                if (!cellular.SwitchToFullPowerMode()) {
                    ESP_LOGE(tag, "Switching to full power mode failed");
                }
            }         

            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGI(tag, "switching to PPP mode next...");
            if (!cellular.SwitchToPppMode()) {
                ESP_LOGW(tag, "Retrying switching to PPP mode next...");
                if (!cellular.SwitchToPppMode()) {
                    ESP_LOGE(tag, "Failed to switch to PPP mode.");
                }
            }; 
        }

        // read maximet data queue and create a HTTP POST message
        sendData.PrepareHttpPost(voltage, current, boardtemp, watertemp, bDiagnostics);

        // try sending, max 3 times
        if (onlineMode != MODE_OFFLINE) {
            int tries = 3;
            while(tries--) {
                if(sendData.PerformHttpPost()) {
                    break;
                }
                if (tries) {
                    ESP_LOGW(tag, "Retrying HTTP Post...");
                    vTaskDelay(1000/portTICK_PERIOD_MS); // wait one second
                }
            }
        }

        if (onlineMode == MODE_CELLULAR) {
            ESP_LOGI(tag, "switching to low power mode...");
            cellular.SwitchToLowPowerMode();            
        } 

        // determine nighttime by low solar radiation
        ESP_LOGI(tag, "Solarradiation: %d W/m²  Voltage: %.02fV", readMaximet.SolarRadiation(), voltage/1000.0);
        if (readMaximet.SolarRadiation() > 2 && voltage > 13100) {
            secondsToSleep = CONFIG_SENDDATA_INTERVAL_DAYTIME; //s;
            ESP_LOGI(tag, "Sending data at daytime interval every %d seconds", secondsToSleep);
        } else if (voltage > 12750) {
            secondsToSleep = CONFIG_SENDDATA_INTERVAL_NIGHTTIME; //s;
            ESP_LOGI(tag, "Sending data at nighttime interval every %d minutes", secondsToSleep/60);
        } else {   // got into low battery mode if voltage is below 12.5V 
            secondsToSleep = CONFIG_SENDDATA_INTERVAL_LOWBATTERY;
            ESP_LOGI(tag, "Sending data in low-battery mode every %d minutes", secondsToSleep/60);
        }
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
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());
  
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());

    ESP_LOGI(tag, "Free memory: %d", esp_get_free_heap_size());

    cellular.SwitchToCommandMode();

    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());



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


        //Command("AT+CNMP=?", "OK", nullptr, "List of preferred modes"); // +CNMP: (2,9,10,13,14,19,22,38,39,48,51,54,59,60,63,67)
        //Command("AT+CNMP?", "OK", nullptr, "Preferred mode"); 
            //Command("AT+CNMP=13", "OK", &response, "Preferred mode"); // +CNMP: 2 // 2 = automatic
            //  Command("AT+CNMP=38", "OK", &response, "Preferred mode"); // +CNMP: 2 // 2 = automatic
            //        Command("AT+CNSMOD=2", "OK", nullptr, "Set GPRS mode"); 

        //Command("AT+CEREG=?", "OK", nullptr, "List of EPS registration stati");
        //Command("AT+CEREG?", "OK", nullptr, "EPS registration status"); // +CEREG: 0,4
                                                // 4 – unknown (e.g. out of E-UTRAN coverage)
                                                // 5 - roaming


        //Command("AT+NETMODE?", "OK", nullptr, "EPS registration status"); // 2 – WCDMA mode(default)
        //Command("AT+CPOL?", "OK", nullptr, "Preferred operator list"); // 
        //Command("AT+COPN", "OK", nullptr, "Read operator names"); // ... +COPN: "23201","A1" ... // list of thousands!!!!!

        //Command("AT+CGDATA=?", "OK", nullptr, "Read operator names"); // 
//    Command("AT+IPR=?", "OK", &response,  "Operator Selection"); // AT+IPR=?
                                                                // +IPR: (0,300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600,3000000,3200000,3686400)


}
