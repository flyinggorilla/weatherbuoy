// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
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
#include "RtcVariables.h"

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
#define CONFIG_NMEA_TWAI_POWER_PIN GPIO_NUM_23 // power the chip through GPIO, so CAN BUS chip stays powered down during boot
#endif

// Optionally drive an Alarm Buzzer at following GPIO (max 30mA for standard GPIO, but this one can with proper config drive up to 70mA)
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
    RtcVariables::Init();

#if LOG_LOCAL_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG || CONFIG_LOG_DEFAULT_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG
    esp_log_level_set("Cellular", ESP_LOG_DEBUG);
    esp_log_level_set("Wifi", ESP_LOG_DEBUG);
    esp_log_level_set("Alarm", ESP_LOG_DEBUG);
    esp_log_level_set("Maximet", ESP_LOG_INFO);
    esp_log_level_set("MaximetSimulator", ESP_LOG_DEBUG);
    esp_log_level_set("Serial", ESP_LOG_DEBUG);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-netif_lwip-ppp", ESP_LOG_WARN);
#endif

    ESP_LOGI(tag, "Atterwind WeatherBuoy starting!");
    if (!mConfig.Load())
    {
        ESP_LOGE(tag, "Error, could not load configuration.");
    }

    if (!mConfig.msCellularApn.length() && strlen(CONFIG_WEATHERBUOY_CELLULAR_APN))
    {
        mConfig.msCellularApn = CONFIG_WEATHERBUOY_CELLULAR_APN;
        mConfig.Save();
        ESP_LOGW(tag, "Saved firmware default APN configuration! %s", mConfig.msCellularApn.c_str());
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
        mpDisplay = new NmeaDisplay(CONFIG_NMEA_TWAI_TX_PIN, CONFIG_NMEA_TWAI_RX_PIN, CONFIG_NMEA_TWAI_POWER_PIN ,dataQueue);
        mpDisplay->Start();

        tempSensors.Read();  // note, this causes approx 700ms delay
        unsigned int voltage = max471Meter.Voltage();
        unsigned int current = max471Meter.Current();
        float boardtemp = tempSensors.GetBoardTemp();
        mpDisplay->SetSystemInfo(voltage, current, boardtemp);
    }

    // start Maximet wind/weather data reading
    int maximetRxPin = CONFIG_WEATHERBUOY_READMAXIMET_RX_PIN;
    int maximetTxPin = CONFIG_WEATHERBUOY_READMAXIMET_TX_PIN;
    Maximet maximet(dataQueue);
    if (!mConfig.miSimulator)
    {
        maximet.Start(maximetRxPin, maximetTxPin);
    }

    
    //ESP_LOGE(tag, "remove http client logging!");

    //ESP_LOGE(tag, "remove simulator check!");
    //if (!mConfig.miSimulator && mCellular.Init())

    // detect available Simcom 7600E Modem, such as on Lillygo PCI board
    if (mCellular.Init(mConfig.msCellularApn, mConfig.msCellularUser, mConfig.msCellularPass, mConfig.msCellularOperator, mConfig.miCellularNetwork) && mCellular.PowerUp())
    {
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
            ESP_LOGI(tag, "sssi %s, pass %d characters, host %s", mConfig.msSTASsid.c_str(), mConfig.msSTAPass.length(), mConfig.msHostname.c_str());
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

    if (mConfig.miSimulator)
    {
        // run Simulator
        ESP_LOGI(tag, "Starting: Gill Maximet Wind Sensor Simulator GMX%d%s", mConfig.miSimulator / 10, mConfig.miSimulator % 10 ? "GPS" : "");
        RunSimulator(tempSensors, dataQueue, max471Meter, sendData, maximet, static_cast<Maximet::Model>(mConfig.miSimulator));
    }
    else
    {
        // run Weatherbuoy and Startboat
        Alarm *pAlarm = nullptr;
        if (mConfig.mbAlarmSound || mConfig.msAlarmSms.length())
        {
            ESP_LOGI(tag, "Starting: Alarm system");
            pAlarm = new Alarm(dataQueue, mConfig, CONFIG_ALARM_BUZZER_PIN);
            pAlarm->Start();
        }

        Run(tempSensors, dataQueue, max471Meter, sendData, maximet, pAlarm);
    }
}

void Esp32WeatherBuoy::HandleAlarm(Alarm *pAlarm)
{
    if (mOnlineMode != MODE_CELLULAR || !pAlarm || !pAlarm->IsAlarm())
        return;

    ESP_LOGI(tag, "switching to full power mode next...");
    if (!mCellular.SwitchToFullPowerMode())
    {
        ESP_LOGE(tag, "SEVERE, Switching to full power mode failed. Restarting.");
        RtcVariables::SetExtendedResetReason(RtcVariables::EXTENDED_RESETREASON_MODEMFULLPOWERFAILED);
        esp_restart();
        return;
    }

    //vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(tag, "SMS Numbers: %s", mConfig.msAlarmSms.c_str());
    String msg;
    //String numbers(mConfig.msAlarmSms); // copy phone numbers, so to split later
    msg = "ALARM ";
    msg += mConfig.msHostname;
    msg += "!\r\n";
    msg += pAlarm->GetAlarmInfo();
    msg += "\r\n";

    int startPos = 0; // start of phone number
    int endPos = 0; // end of phone number, before position of delimiter or end of string
    const int length = mConfig.msAlarmSms.length();
    int sent = 0;
    while (startPos < length)
    {
        endPos = mConfig.msAlarmSms.indexOf(',', startPos);
        if (endPos < 0)
        {
            endPos = length;
        } 
        String to = mConfig.msAlarmSms.substring(startPos, endPos);
        to.trim();

        if (to.length())
        {
            //ESP_LOGI(tag, "Sending SMS: to: %s msg: %s", to.c_str(), msg.c_str());
            if (mCellular.SendSMS(to, msg))
            {
                sent++;
            }
        } else  {
            break;
        }
        startPos = endPos + 1;
    }

    pAlarm->ConfirmAlarm();
};

void Esp32WeatherBuoy::Run(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, Alarm *pAlarm)
{
    ESP_LOGI(tag, "Running: %s, Maximet: %s", 
            mpDisplay ? "Racing Committee Boat with Garmin GNX130 NMEA200 Display" : "Weatherbuoy", 
            maximet.GetConfig().sUserinfo.c_str());

    String tempSensorRomCodes = tempSensors.GetRomCodes();

    bool bDiagnostics;
    bool bDiagnosticsAtStartup = true;
    unsigned int lastSendTimestamp = 0;
    unsigned int lastDiagnosticsTimestamp = 0;

    int secondsToSleep = 0;

    int logInfoSeconds = 0;

    while (true)
    {
        HandleAlarm(pAlarm); // check for alarm before and after waiting for the queue to minimize latency
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

        tempSensors.Read();  // note, this causes approx 700ms delay
        unsigned int voltage = max471Meter.Voltage();
        unsigned int current = max471Meter.Current();
        float boardtemp = tempSensors.GetBoardTemp();
        float watertemp = tempSensors.GetWaterTemp();

        if (mpDisplay) {
            mpDisplay->SetSystemInfo(voltage, current, boardtemp);
        }

        if (mOnlineMode == MODE_CELLULAR)
        {
            const int MAX_ATTEMPTS = 5;
            int attempts = MAX_ATTEMPTS;
            bool prepared = false;
            do
            {
                ESP_LOGI(tag, "Switching to full power mode next...");
                if (!mCellular.SwitchToFullPowerMode())
                {
                    ESP_LOGE(tag, "SEVERE, Switching to full power mode failed. Remaining attempts: %i", attempts);
                    if (attempts < MAX_ATTEMPTS) {
                        RtcVariables::IncModemRestarts();
                        mCellular.PowerDown();
                    }
                    continue;
                }

                ESP_LOGI(tag, "Switching to PPP mode next...");
                if (!mCellular.SwitchToPppMode())
                {
                    ESP_LOGE(tag, "SEVERE, Failed to switch to PPP mode. Remaining attempts: %i", attempts);
                    if (attempts < MAX_ATTEMPTS) {
                        RtcVariables::IncModemRestarts();
                        mCellular.PowerDown();
                    }
                    continue;
                };

                if (!prepared)
                {
                    // read maximet data queue and create a HTTP POST message
                    sendData.PrepareHttpPost(voltage, current, boardtemp, watertemp, bDiagnostics, mOnlineMode, tempSensorRomCodes);
                    prepared = true;
                }

                bool httpPostSucceeded = false;
                int httpAttempts = 3;
                do {
                    if(sendData.PerformHttpPost()) {
                        httpPostSucceeded = true;
                        break;
                    } else {
                        ESP_LOGE(tag, "HTTP Post request failed. Retrying HTTP request. Remaining attempts: %i", httpAttempts);
                        vTaskDelay(1000/portTICK_PERIOD_MS); // wait one second
                    }
                } while (--httpAttempts);

                if (httpPostSucceeded)
                {
                    ESP_LOGI(tag, "Posting data succeeded. Switching to low power mode...");
                    mCellular.SwitchToSleepMode();

                    int duration = (unsigned int)(esp_timer_get_time() / 1000000) - uptimeSeconds;
                    ESP_LOGI(tag, "Total data send duration %is.", duration);
                    break;
                }
                else
                {
                    ESP_LOGE(tag, "Failed to perform HTTP Post. Power cycling modem. Remaining attempts: %i", attempts);
                    RtcVariables::IncModemRestarts();
                    mCellular.PowerDown();
                }

            } while (--attempts);

            if (attempts == 0) {
                RtcVariables::SetExtendedResetReason(RtcVariables::EXTENDED_RESETREASON_CONNECTIONRETRIES);
                ESP_LOGE(tag, "SEVERE, Too many attempts to connect to server failed. Restarting.");
                esp_restart();
            }
        }

        // display runs on power, so no worries about power management
        if (mpDisplay) 
        {
            secondsToSleep = mConfig.miIntervalDay;
            continue;
        }

        // Power Managment
        // -----------------------------------------------------------
        // determine nighttime by low solar radiation
        ESP_LOGI(tag, "Solarradiation: %d W/m2  Voltage: %.02fV", maximet.SolarRadiation(), voltage / 1000.0);
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
        { // got into low battery mode if voltage is below 12.75V
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
    String tempSensorRomCodes = tempSensors.GetRomCodes();

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
            sendData.PrepareHttpPost(5000, 70, boardtemp, watertemp, true, mOnlineMode, tempSensorRomCodes);

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