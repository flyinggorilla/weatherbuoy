#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "Esp32WeatherBuoy.h"
#include "Config.h"
#include "SendData.h"
#include "Maximet.h"
#include "MaximetSimulator.h"
#include "Watchdog.h"
#include "Wifi.h"
#include "Cellular.h"
#include "Max471Meter.h"
#include "TemperatureSensors.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "NmeaDisplay.h"
#include "Alarm.h"

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

// Optionally drive an Alarm Buzzer at following GPIO (max 30mA)
#define CONFIG_ALARM_BUZZER_PIN GPIO_NUM_19

// Restart ESP32 if there is not data being successfully sent within this period.
#define CONFIG_WATCHDOG_SECONDS 60 * 65   // 65min - if hourly send including retries fail,then lets restart
#define CONFIG_SOLARRADIATIONMIN_DAYTIME 2 // >2W/m2 solar radiation to declare DAYTIME

Esp32WeatherBuoy::Esp32WeatherBuoy()
{
}
Esp32WeatherBuoy::~Esp32WeatherBuoy()
{
}

extern "C"
{
    void app_main();
}

void app_main()
{
    ESP_ERROR_CHECK(esp_netif_init());
    Esp32WeatherBuoy esp32WeatherBuoy;
    esp32WeatherBuoy.Start();
    esp_restart();
}

void TestHttp();
void TestATCommands(Cellular &cellular);

#include "VelocityVector.h"
void TestVelocityVector();

void Esp32WeatherBuoy::Start()
{

#if LOG_LOCAL_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG || CONFIG_LOG_DEFAULT_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG
    esp_log_level_set("Cellular", ESP_LOG_DEBUG);
    esp_log_level_set("Wifi", ESP_LOG_DEBUG);
    esp_log_level_set("Maximet", ESP_LOG_INFO);
    esp_log_level_set("MaximetSimulator", ESP_LOG_DEBUG);
    esp_log_level_set("Serial", ESP_LOG_DEBUG);
#endif

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!mConfig.Load())
    {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    if (mConfig.miSimulator)
    {
        ESP_LOGI(tag, "Maximet Simulation: GMX%d%s", mConfig.miSimulator / 10, mConfig.miSimulator % 10 ? "GPS" : "");
    }

    if (mConfig.mbNmeaDisplay)
    {
        ESP_LOGI(tag, "NMEA2000 Display output activated (i.e. Garmin GNX130).");
    }

#ifdef TESTVELOCITYVECTOR
    TestVelocityVector();
#endif

    int watchdogSeconds = CONFIG_WATCHDOG_SECONDS;
    Watchdog watchdog(watchdogSeconds);
    ESP_LOGI(tag, "Watchdog started and adjusted to %d seconds.", watchdogSeconds);

    TemperatureSensors tempSensors(mConfig);
    tempSensors.Init(CONFIG_TEMPERATURESENSOR_GPIO_ONEWIRE);

    ESP_LOGI(tag, "Hostname: %s", mConfig.msHostname.c_str());
    // ESP_LOGI(tag, "Target URL: %s", mConfig.msTargetUrl.c_str());
    ESP_LOGI(tag, "Target URL: %s", CONFIG_WEATHERBUOY_TARGET_URL);
    ESP_LOGI(tag, "App Version: %s", esp_ota_get_app_description()->version);

    Max471Meter max471Meter(CONFIG_MAX471METER_GPIO_VOLTAGE, CONFIG_MAX471METER_GPIO_CURRENT);
    ESP_LOGI(tag, "Max471Meter: voltage %d mV, current %d mA", max471Meter.Voltage(), max471Meter.Current());

    DataQueue dataQueue;

    // allocate NMEA2000 display output on heap as moving averages require some memory
    if (mConfig.mbNmeaDisplay)
    {
        mpDisplay = new NmeaDisplay(CONFIG_NMEA_TWAI_TX_PIN, CONFIG_NMEA_TWAI_RX_PIN, dataQueue);
        mpDisplay->Start();
    }

    // start Maximet wind/weather data reading
    int maximetRxPin = CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN;
    int maximetTxPin = CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN;
    Maximet maximet(dataQueue);
    if (!mConfig.miSimulator)
    {
        maximet.Start(maximetRxPin, maximetTxPin);
    }

    // detect available Simcom 7600E Modem, such as on Lillygo PCI board
    ESP_LOGW(tag, "lower modem speed to 460kbaud again?");
    ESP_LOGE(tag, "remove simulator check!");
    if (!mConfig.miSimulator && mCellular.InitModem())
    {
        mCellular.Start(mConfig.msCellularApn, mConfig.msCellularUser, mConfig.msCellularPass, mConfig.msCellularOperator, mConfig.miCellularNetwork);
        mOnlineMode = MODE_CELLULAR;

        // mCellular.ReadSMS(); // use only during firmware setup to receive a SIM based code
        // AT+CMGD Delete Message (with option 4 to delete all)
    }
    else // try wifi if configured
    {
        if (mConfig.msSTASsid.length())
        {
            // config.msSTASsid = "";
            // config.msSTAPass = "";
            // config.msTargetUrl = "http://10.10.14.195:3000/weatherbuoy";
            // config.Save();
            ESP_LOGI(tag, "sssi %s pass %s host %s", mConfig.msSTASsid.c_str(), mConfig.msSTAPass.c_str(), mConfig.msHostname.c_str());
            mWifi.StartSTAMode(mConfig.msSTASsid, mConfig.msSTAPass, mConfig.msHostname);
            mWifi.StartTimeSync(mConfig.msNtpServer);
            ESP_LOGI(tag, "NTP Time Syncronization enabled: %s", mConfig.msNtpServer.c_str());

            mOnlineMode = MODE_WIFISTA;
        }
        else if (mConfig.msAPSsid.length())
        {
            ESP_LOGI(tag, "sssi %s pass %s host %s", mConfig.msAPSsid.c_str(), mConfig.msAPPass.c_str(), mConfig.msHostname.c_str());
            mWifi.StartAPMode(mConfig.msAPSsid, mConfig.msAPPass, mConfig.msHostname);
            mOnlineMode = MODE_WIFIAP;
        }
        else
        {
            ESP_LOGW(tag, "Staying offline.");
            mOnlineMode = MODE_OFFLINE;
        }
    }

    SendData sendData(mConfig, dataQueue, mCellular, watchdog, maximet);

    if (mConfig.mbNmeaDisplay)
    {
        ESP_LOGI(tag, "Racing Committee Boat with Garmin GNX130 NMEA200 Display");
        RunDisplay(tempSensors, dataQueue, max471Meter, sendData, maximet);
    }
    else if (mConfig.miSimulator)
    {
        ESP_LOGI(tag, "Starting: Gill Maximet Wind Sensor Simulator GMX%d%s", mConfig.miSimulator / 10, mConfig.miSimulator % 10 ? "GPS" : "");
        RunSimulator(tempSensors, dataQueue, max471Meter, sendData, maximet, static_cast<Maximet::Model>(mConfig.miSimulator));
    }
    else
    {
        Alarm *pAlarm = nullptr;
        if (mConfig.mbAlarmSound || mConfig.msAlarmSms.length())
        {
            ESP_LOGI(tag, "Starting: Alarm system");
            pAlarm = new Alarm(dataQueue, mConfig, CONFIG_ALARM_BUZZER_PIN);
            pAlarm->Start();
        }
        ESP_LOGI(tag, "Starting: Weatherbuoy %s", maximet.GetConfig().sUserinfo.c_str());
        RunBuoy(tempSensors, dataQueue, max471Meter, sendData, maximet, pAlarm);
    }
}

void Esp32WeatherBuoy::HandleAlarm(Alarm *pAlarm)
{
    if (mOnlineMode != MODE_CELLULAR || !pAlarm || !pAlarm->IsAlarm())
        return;

    ESP_LOGI(tag, "switching to full power mode next...");
    if (!mCellular.SwitchToFullPowerMode())
    {
        ESP_LOGW(tag, "Retrying switching to full power mode ...");
        if (!mCellular.SwitchToFullPowerMode())
        {
            ESP_LOGE(tag, "Switching to full power mode failed");
            return;
        }
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(tag, "SMS Numbers: %s", mConfig.msAlarmSms.c_str());
    String msg;
    msg = "ALARM ";
    msg += mConfig.msHostname;
    msg += "!\r\n";
    msg += pAlarm->GetAlarmInfo();
    msg += "\r\n";

    int startPos = 0;
    int endPos = 0;
    int sent = 0;
    while (endPos < mConfig.msAlarmSms.length())
    {
        endPos = mConfig.msAlarmSms.indexOf(',');
        if (endPos < 0)
        {
            endPos = mConfig.msAlarmSms.length();
        }
        String to = mConfig.msAlarmSms.substring(startPos, endPos);
        ESP_LOGI(tag, "Sending SMS: to: %s msg: %s", to.c_str(), msg.c_str());
        if (mCellular.SendSMS(to, msg))
        {
            sent++;
        }

        startPos = endPos + 1;
    }

    pAlarm->ConfirmAlarm();
};

void Esp32WeatherBuoy::RunBuoy(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, Alarm *pAlarm)
{
    ESP_LOGI(tag, "Starting Weatherbuoy main task.");

    bool bDiagnostics;
    bool bDiagnosticsAtStartup = true;
    unsigned int lastSendTimestamp = 0;
    unsigned int lastDiagnosticsTimestamp = 0;

    int secondsToSleep = 0;

    int logInfoSeconds = 0;

    while (true)
    {
        HandleAlarm(pAlarm); // check for alarm before and after waiting for the queue to minimize latency
        tempSensors.Read();  // note, this causes approx 700ms delay
        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
        bool isMaximetData = dataQueue.WaitForData(60);
        HandleAlarm(pAlarm); // check for alarm
        unsigned int secondsSinceLastSend;
        unsigned int secondsSinceLastDiagnostics;

        // keep modem sleeping unless time to last send elapsed
        unsigned int uptimeSeconds = (unsigned int)(esp_timer_get_time() / 1000000); // seconds since start
        secondsSinceLastSend = uptimeSeconds - lastSendTimestamp;
        if (secondsToSleep > secondsSinceLastSend)
        {
            if (logInfoSeconds == 0)
            {
                ESP_LOGI(tag, "Power management sleep: %d, Measurements in queue: %d", secondsToSleep - secondsSinceLastSend, dataQueue.GetQueueLength());
                logInfoSeconds = 60;
            }
            logInfoSeconds--;
            continue;
        }
        lastSendTimestamp = uptimeSeconds;

        // when sending, add diagnostics information after 300 seconds
        secondsSinceLastDiagnostics = uptimeSeconds - lastDiagnosticsTimestamp;
        if (secondsSinceLastDiagnostics > mConfig.miIntervalDiagnostics || bDiagnosticsAtStartup)
        {
            bDiagnostics = true;
            bDiagnosticsAtStartup = false;
            lastDiagnosticsTimestamp = uptimeSeconds;
        }
        else
        {
            bDiagnostics = false;
        }

        // check if maximeta or info data should be sent at all
        if (!isMaximetData && !bDiagnostics)
        {
            continue;
        }

        unsigned int voltage = max471Meter.Voltage();
        unsigned int current = max471Meter.Current();
        float boardtemp = tempSensors.GetBoardTemp();
        float watertemp = tempSensors.GetWaterTemp();

        if (mOnlineMode == MODE_CELLULAR)
        {
            int attempts = 3;
            bool prepared = false;
            do
            {
                ESP_LOGI(tag, "Switching to full power mode next...");
                if (!mCellular.SwitchToFullPowerMode())
                {
                    ESP_LOGE(tag, "Switching to full power mode failed. Shutting down again. Remaining attempts: %i", attempts);
                    mCellular.SwitchToLowPowerMode();
                    continue;
                }

                ESP_LOGI(tag, "Switching to PPP mode next...");
                if (!mCellular.SwitchToPppMode())
                {
                    ESP_LOGE(tag, "Failed to switch to PPP mode. Shutting down modem. Remaining attempts: %i", attempts);
                    mCellular.SwitchToLowPowerMode();
                    continue;
                };

                if (!prepared)
                {
                    // read maximet data queue and create a HTTP POST message
                    sendData.PrepareHttpPost(voltage, current, boardtemp, watertemp, bDiagnostics, mOnlineMode);
                    prepared = true;
                }

                if (sendData.PerformHttpPost())
                {
                    ESP_LOGI(tag, "Posting data succeeded. Switching to low power mode...");
                    mCellular.SwitchToLowPowerMode();
                    break;
                }
                else
                {
                    ESP_LOGE(tag, "Failed to perform HTTP Post. Shutting down modem. Remaining attempts: %i", attempts);
                    mCellular.SwitchToLowPowerMode();
                }

            } while (--attempts);
        }

        // determine nighttime by low solar radiation
        ESP_LOGI(tag, "Solarradiation: %d W/m²  Voltage: %.02fV", maximet.SolarRadiation(), voltage / 1000.0);
        if (maximet.SolarRadiation() > CONFIG_SOLARRADIATIONMIN_DAYTIME && voltage > 13100)
        {
            secondsToSleep = mConfig.miIntervalDay; // s;
            ESP_LOGI(tag, "Sending data at daytime interval every %d seconds", secondsToSleep);
        }
        else if (voltage > 12750)
        {
            secondsToSleep = mConfig.miIntervalNight; // s;
            ESP_LOGI(tag, "Sending data at nighttime interval every %d minutes", secondsToSleep / 60);
        }
        else
        { // got into low battery mode if voltage is below 12.5V
            secondsToSleep = mConfig.miIntervalLowbattery;
            ESP_LOGI(tag, "Sending data in low-battery mode every %d minutes", secondsToSleep / 60);
        }
    }
}

void Esp32WeatherBuoy::RunSimulator(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, Maximet::Model model)
{
    MaximetSimulator simulator;
    int maximetRxPin = CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN;
    int maximetTxPin = CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN;
    // since no MODEM was found, we assume this is an Adafruit ESP32 Feather board, with different serial pins
    // comment those two lines if you use a Lillygo PCIE board without Modem
    maximetRxPin = 16;
    maximetTxPin = 17;

    simulator.Start(model, maximetRxPin, maximetTxPin);

    Data data;

    unsigned int maximetDataIntervalSeconds = 60;
    unsigned int httpPostDataIntervalSeconds = 30;
    unsigned int lastSendTimestamp = 0;

    while (true)
    {
        tempSensors.Read(); // note, this causes approx 700ms delay
        vTaskDelay((maximetDataIntervalSeconds * 1000 - 700) / portTICK_PERIOD_MS);
        float boardtemp = tempSensors.GetBoardTemp();
        float watertemp = tempSensors.GetWaterTemp();

        simulator.SetTemperature(watertemp);

        unsigned int uptimeSeconds = (unsigned int)(esp_timer_get_time() / 1000000); // seconds since start
        if (uptimeSeconds - lastSendTimestamp >= httpPostDataIntervalSeconds)
        {
            lastSendTimestamp = uptimeSeconds;
            sendData.PrepareHttpPost(5000, 70, boardtemp, watertemp, true, mOnlineMode);

            int sendViaWifiTries = 10;
            while (sendViaWifiTries--)
            {
                if (mWifi.IsConnected() && sendData.PerformHttpPost())
                {
                    break;
                }
                else
                {
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    mWifi.Reconnect();
                    ESP_LOGW(tag, "Retrying sending via wifi. Remaining attempts: %i", sendViaWifiTries);
                }
            }
        }
    }
}

void Esp32WeatherBuoy::RunDisplay(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet)
{
    ESP_LOGI(tag, "Starting: Startboat NMEA 2000 Display main task.");

    bool bDiagnostics;
    bool bDiagnosticsAtStartup = true;
    unsigned int lastSendTimestamp = 0;
    unsigned int lastDiagnosticsTimestamp = 0;

    int secondsToSleep = 0;

    int logInfoSeconds = 0;

    while (true)
    {
        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
        bool isMaximetData = dataQueue.WaitForData(60);
        unsigned int secondsSinceLastSend;
        unsigned int secondsSinceLastDiagnostics;

        // keep modem sleeping unless time to last send elapsed
        unsigned int uptimeSeconds = (unsigned int)(esp_timer_get_time() / 1000000); // seconds since start
        secondsSinceLastSend = uptimeSeconds - lastSendTimestamp;
        if (secondsToSleep > secondsSinceLastSend)
        {
            if (logInfoSeconds == 0)
            {
                ESP_LOGI(tag, "Power management sleep: %d, Measurements in queue: %d", secondsToSleep - secondsSinceLastSend, dataQueue.GetQueueLength());
                logInfoSeconds = 60;
            }
            logInfoSeconds--;
            continue;
        }
        lastSendTimestamp = uptimeSeconds;

        // when sending, add diagnostics information after 300 seconds
        secondsSinceLastDiagnostics = uptimeSeconds - lastDiagnosticsTimestamp;
        if (secondsSinceLastDiagnostics > mConfig.miIntervalDiagnostics || bDiagnosticsAtStartup)
        {
            bDiagnostics = true;
            bDiagnosticsAtStartup = false;
            lastDiagnosticsTimestamp = uptimeSeconds;
        }
        else
        {
            bDiagnostics = false;
        }

        // check if maximeta or info data should be sent at all
        if (!isMaximetData && !bDiagnostics)
        {
            continue;
        }

        if (bDiagnostics)
        {
            // to retrieve updated network info we need to exit PPP mode
            mCellular.SwitchToCommandMode();

            // update network info
            mCellular.QuerySignalStatus();
        }

        tempSensors.Read(); // note, this causes approx 700ms delay
        unsigned int voltage = max471Meter.Voltage();
        unsigned int current = max471Meter.Current();
        float boardtemp = tempSensors.GetBoardTemp();
        float watertemp = tempSensors.GetWaterTemp();

        int attempts = 3;
        bool prepared = false;
        while (attempts--)
        {
            ESP_LOGI(tag, "switching to PPP mode next...");
            if (mCellular.SwitchToPppMode())
            {

                if (!prepared)
                {
                    // read maximet data queue and create a HTTP POST message
                    sendData.PrepareHttpPost(voltage, current, boardtemp, watertemp, bDiagnostics, mOnlineMode);
                    prepared = true;
                }

                if (sendData.PerformHttpPost())
                {
                    break;
                }
            }
            else
            {
                ESP_LOGW(tag, "Restarting modem, then Retrying HTTP Post. Remaining attempts: %i", attempts);
                mCellular.SwitchToLowPowerMode();
                vTaskDelay(1000 / portTICK_PERIOD_MS); // wait one second
            };
        };

        secondsToSleep = mConfig.miIntervalDay;
    }
};

void TestHttp()
{
    for (int i = 0; i < 5; i++)
    {
        //        cellular.Command("AT+CGDCONT=1,\"IP\",\"webapn.at\"", "Define PDP Context");
        // cellular.Command("ATD*99#", "setup data connection");

        vTaskDelay(10000 / portTICK_PERIOD_MS);

        esp_http_client_config_t httpConfig = {0};
        httpConfig.url = "http://ptsv2.com/t/wb/post?testWeatherbuoy";

        httpConfig.method = HTTP_METHOD_GET;
        esp_http_client_handle_t httpClient = esp_http_client_init(&httpConfig);
        // esp_http_client_set_header(httpClient, "Content-Type", "text/plain");
        esp_err_t err;
        err = esp_http_client_open(httpClient, 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(tag, "Error %s in esp_http_client_open(): %s", esp_err_to_name(err), httpConfig.url);
        }

        int sent = esp_http_client_write(httpClient, "", 0);
        if (sent == 0)
        {
            ESP_LOGD(tag, "esp_http_client_write(): OK, sent: %d", sent);
        }
        else
        {
            ESP_LOGE(tag, "esp_http_client_write(): Could only send %d of %d bytes", sent, 0);
        }

        // retreive HTTP response and headers
        int iContentLength = esp_http_client_fetch_headers(httpClient);
        if (iContentLength == ESP_FAIL)
        {
            ESP_LOGE(tag, "esp_http_client_fetch_headers(): could not receive HTTP response");
        }

        // Check HTTP status code
        int iHttpStatusCode = esp_http_client_get_status_code(httpClient);
        if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400))
        {
            ESP_LOGI(tag, "HTTP response OK. Status %d, Content-Length %d", iHttpStatusCode, iContentLength);
            return;
        }
        else
        {
            ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        }
    }
}

void TestATCommands(Cellular &cellular)
{
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());
    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());

    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());

    ESP_LOGI(tag, "Free memory: %d", esp_get_free_heap_size());

    cellular.SwitchToCommandMode();

    ESP_LOGI(tag, "Cellular Data usage: sent=%lu, received=%lu", (unsigned long)cellular.getDataSent(), (unsigned long)cellular.getDataReceived());

    String response;
    cellular.Command("ATI", "OK", nullptr, "Display Product Identification Information");                 // SIM800 R14.18
    cellular.Command("AT+CGMM", "OK", nullptr, "Model Identification");                                   // SIMCOM_SIM800L
    cellular.Command("AT+GMM", "OK", nullptr, "Request TA Model Identification");                         // SIMCOM_SIM800L
    cellular.Command("AT+CGSN", "OK", nullptr, "Product Serial Number Identification (IMEI)");            // 8673720588*****
    cellular.Command("AT+CREG?", "OK", nullptr, "Network Registration Information States");               // +CREG: 0,5
    cellular.Command("AT+CGMR", "OK", nullptr, "Request TA Revision Identification of Software Release"); // Revision:1418B05SIM800L24
    cellular.Command("AT+GMR", "OK", nullptr, "Request TA Revision Identification of Software Release");  // Revision:1418B05SIM800L24
    cellular.Command("AT+CGMI", "OK", nullptr, "Request Manufacturer Identification");                    // SIMCOM_Ltd
    cellular.Command("AT+GMI", "OK", nullptr, "Request Manufacturer Identification");                     // SIMCOM_Ltd
    cellular.Command("AT+CIMI", "OK", nullptr, "Request international mobile subscriber identity");       // 23212200*******
    cellular.Command("AT+CROAMING", "OK", nullptr, "Roaming State 0=home, 1=intl, 2=other");              // +CROAMING: 2
    cellular.Command("AT+CSQ", "OK", nullptr, "Signal Quality Report");                                   // +CSQ: 13,0
    cellular.Command("AT+CNUM", "OK", nullptr, "Subscriber Number");                                      // +CNUM: "","+43681207*****",145,0,4
    cellular.Command("AT+CBC", "OK", nullptr, "Battery Charge");                                          // +CBC: 0,80,4043
    cellular.Command("AT+GSN", "OK", nullptr, "Request TA Serial Number Identification (IMEI)");          // 8673720588*****
    cellular.Command("AT+GCAP", "OK", nullptr, "Request Complete TA Capabilities List");                  // +GCAP: +CGSM
    cellular.Command("AT+CSTT?", "OK", nullptr, "Query APN and login info");                              // +CSTT: "CMNET","",""
    cellular.Command("AT+COPS?", "OK", nullptr, "Operator Selection");                                    // +COPS: 0,0,"A1"
    cellular.Command("AT+CGATT=?", "OK", nullptr, "Attach or Detach from GPRS Service ");                 // +CGATT: (0,1)

#define RUNCOMMAND_READ_SMS true
    if (RUNCOMMAND_READ_SMS)
    {
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

    while (true)
    {
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

    // Command("AT+CNMP=?", "OK", nullptr, "List of preferred modes"); // +CNMP: (2,9,10,13,14,19,22,38,39,48,51,54,59,60,63,67)
    // Command("AT+CNMP?", "OK", nullptr, "Preferred mode");
    // Command("AT+CNMP=13", "OK", &response, "Preferred mode"); // +CNMP: 2 // 2 = automatic
    //   Command("AT+CNMP=38", "OK", &response, "Preferred mode"); // +CNMP: 2 // 2 = automatic
    //         Command("AT+CNSMOD=2", "OK", nullptr, "Set GPRS mode");

    // Command("AT+CEREG=?", "OK", nullptr, "List of EPS registration stati");
    // Command("AT+CEREG?", "OK", nullptr, "EPS registration status"); // +CEREG: 0,4
    //  4 – unknown (e.g. out of E-UTRAN coverage)
    //  5 - roaming

    // Command("AT+NETMODE?", "OK", nullptr, "EPS registration status"); // 2 – WCDMA mode(default)
    // Command("AT+CPOL?", "OK", nullptr, "Preferred operator list"); //
    // Command("AT+COPN", "OK", nullptr, "Read operator names"); // ... +COPN: "23201","A1" ... // list of thousands!!!!!

    // Command("AT+CGDATA=?", "OK", nullptr, "Read operator names"); //
    //     Command("AT+IPR=?", "OK", &response,  "Operator Selection"); // AT+IPR=?
    //  +IPR: (0,300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600,3000000,3200000,3686400)
}

#ifdef TESTVELOCITYVECTOR
void TestVelocityVector()
{
    VelocityVector v;
    ESP_LOGI("TESTVV", "New: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 45);
    ESP_LOGI("TESTVV", "Add [10, 45°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 360 - 45);
    ESP_LOGI("TESTVV", "Add [10, -45°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 90 + 45);
    v.add(10, 180 + 45);
    ESP_LOGI("TESTVV", "Add [10, 135°], Add [10, 225°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(5, 180);
    ESP_LOGI("TESTVV", "Add [5, 180°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.clear();
    ESP_LOGI("TESTVV", "Clear: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 0);
    v.add(10, 90);
    v.add(10, 180);
    ESP_LOGI("TESTVV", "Add [10, 0°], Add [10, 90°], Add [10, 180°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.clear();
    ESP_LOGI("TESTVV", "Clear: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 90);
    ESP_LOGI("TESTVV", "Add [10, 90°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.clear();
    ESP_LOGI("TESTVV", "Clear: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 180);
    ESP_LOGI("TESTVV", "Add [10, 180°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.clear();
    ESP_LOGI("TESTVV", "Clear: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 270);
    ESP_LOGI("TESTVV", "Add [10, 270°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.clear();
    ESP_LOGI("TESTVV", "Clear: [%0.6f, %d]", v.getSpeed(), v.getDir());
    v.add(10, 360);
    ESP_LOGI("TESTVV", "Add [10, 360°]: [%0.6f, %d]", v.getSpeed(), v.getDir());
}
#endif