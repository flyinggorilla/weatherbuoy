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
//#include <string>

static const char tag[] = "ReadMaximet";
#define SERIAL_BUFFER_SIZE (1024)
#define SERIAL_BAUD_RATE (19200)

void fReadMaximetTask(void *pvParameter) {
	((ReadMaximet*) pvParameter)->ReadMaximetTask();
	vTaskDelete(NULL);
}

void ReadMaximet::Start(int gpioRX, int gpioTX) {
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;
	xTaskCreate(&fReadMaximetTask, "ReadMaximet", 8192, this, ESP_TASK_MAIN_PRIO, NULL);
} 

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

void ReadMaximet::ReadMaximetTask() {

    // UART0: RX: GPIO3, TX: GPIO1 --- connected to console
    // UART1: RX: GPIO9, TX: GPIO10 --- connected to flash!!!???
    // UART2: RX: GPIO16, TX: GPIO17 --- no conflicts

    // Cannot use GPIO 12, as it will prevent to boot when pulled high.
    // Change ports from default RX/TX to not conflict with Console
    Serial serial(UART_NUM_1, mgpioRX, mgpioTX, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);

    ESP_LOGI(tag, "ReadMaximet task started. Waiting 30seconds for attaching to serial interface.");
    vTaskDelay(30*1000/portTICK_PERIOD_MS);
    serial.Attach();

    String line;
    unsigned int uptimeMs = 0;
    unsigned int lastSendMs = 0; 
    unsigned int intervalMs = 0;
    int skipped = 0;

    enum MaximetStates {
        UNDEFINED,
        STARTUP,
        COLUMNS,
        UNITS,
        ENDSTARTUP,
        SENDINGDATA
    };

    MaximetStates maximetState = UNDEFINED;

bool bDayTime = true; /////////////////////////////// TODO *****************************

    ESP_LOGI(tag, "ReadMaximet task started and ready to receive data.");
    while (true) {
        if (!serial.ReadLine(line)) {
            ESP_LOGE(tag, "Could not read line from serial");
        }

        if (mrSendData.isRestart()) {
            serial.Release();
            ESP_LOGI(tag, "System entered OTA, stopping Maximet Data Collection.");
            return;
        }
        
        // check for STX and ETX limiter
        ESP_LOGD(tag, "THE LINE: %s", line.c_str());
        int dataStart = line.lastIndexOf(STX);
        int dataEnd = line.lastIndexOf(ETX);
        int dataLen = dataEnd - dataStart;
        ESP_LOGD(tag, "datastart %d dataend %d", dataStart, dataEnd);

        // the length must be at minimum STX, ETX and 2 digit checksum
        if (dataStart >= 0 && dataLen > 3) {
            //unsigned int checksum = std::stoi(line.substring(dataEnd+1, dataEnd+3).c_str());
            String checksumString = line.substring(dataEnd+1, dataEnd+3);
            //unsigned int checksum = std::stoi(checksumString.c_str(), 0, 16); // use strtol instead?
            unsigned int checksum = strtoimax(checksumString.c_str(), 0, 16); // convert hex string like "FF" to integer
            String maximetline = line.substring(dataStart+1, dataEnd-1);
//maximetline = "Q,168,000.02,213,000,000.00,053,000.05,000,0000,0991.1,1046.2,0991.4,035,+023.2,+007.1,07.42,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27,325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:31.7,+04.6,0000,";
            ESP_LOGD(tag, "checksumstring %s, valuehex %0X, value %d", checksumString.c_str(), checksum, checksum);
            ESP_LOGD(tag, "maximetline %s", maximetline.c_str());

            // parse out solar radiation
            const int solarradiationColumn = 19-1;
            int solarradiationPosStart = 0;
            int solarradiationPosEnd = 0;
            for (int i = 0; i < solarradiationColumn; i++) {
                solarradiationPosStart = maximetline.indexOf(',', solarradiationPosStart);
            }
            solarradiationPosEnd = maximetline.indexOf(',');
            float solarradiation = maximetline.substring(solarradiationPosStart, solarradiationPosEnd).toFloat();
            ESP_LOGD(tag, "solarradiation %f", solarradiation);

            // Checksum, the 2 digit Hex Checksum sum figure is calculated from the Exclusive OR of the bytes between (and not including) the STX and ETX characters
            unsigned char calculatedChecksum = 0;
            for (int i = 0; i < maximetline.length(); i++) {
                calculatedChecksum ^= (unsigned char)maximetline[i];
            }
            ESP_LOGD(tag, "calculated checksum %0X", calculatedChecksum);

            maximetState = SENDINGDATA;
            if (bDayTime) {
                intervalMs = mrConfig.miSendDataIntervalDaytime * 1000; //ms;
            } else {
                intervalMs = mrConfig.miSendDataIntervalNighttime * 1000; //ms;
            }
            uptimeMs = (unsigned int)(esp_timer_get_time()/1000); // milliseconds since start
            if (intervalMs > uptimeMs - lastSendMs) {
                ESP_LOGD(tag, "Skipping measurement data'%s' as %d ms < %d ms", line.c_str(), uptimeMs - lastSendMs, intervalMs);
                skipped++;
            } else {
                ESP_LOGI(tag, "Sending measurement data'%s' after %d ms (skipped %d)", line.c_str(), uptimeMs - lastSendMs, skipped);
                lastSendMs = uptimeMs;
                skipped = 0;
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
                    maximetState = UNDEFINED;
                    ok = false;
                    break;
                }
            }

            //TODO: add configuration mode and read columns/units after every startup:
            // TX: *<CRLF>
            // RX: SETUP MODE
            // TX: REPORT<CRLF>
            // RX: REPORT = NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK
            // TX: UNITS<CRLF>
            // RX: UNITS = -,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-,DEG,-,-,-,DEG,DEG,-,-,-,V,-,-
            // TX: EXIT<CRLF>

            if (ok) {
                if (line.startsWith("STARTUP: OK")) {
                    maximetState = STARTUP;
                }
                switch (maximetState) {
                    case UNDEFINED:
                        break;
                    case STARTUP : 
                        maximetState = COLUMNS;
                        break;
                    case COLUMNS : 
                        maximetState = UNITS;
                        mrConfig.msMaximetColumns = line;
                        break;
                    case UNITS : 
                        maximetState = ENDSTARTUP;
                        mrConfig.msMaximetUnits = line;
                        if (mrConfig.msMaximetColumns && mrConfig.msMaximetUnits) {
                            mrConfig.Save();    
                            ESP_LOGI(tag, "Saving Maximet config");                        
                        }
                        break;
                    case ENDSTARTUP:
                        maximetState = SENDINGDATA;
                        break;
                    case SENDINGDATA:
                        break;
                }
                ESP_LOGI(tag, "Metadata '%s'", line.c_str());
            }
        }

    } 
}

ReadMaximet::ReadMaximet(Config &config, SendData &sendData) : mrConfig(config), mrSendData(sendData) {
}



ReadMaximet::~ReadMaximet() {
}

