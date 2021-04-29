//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "ReadMaximet.h"
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

void ReadMaximet::Start(int gpioRX, int gpioTX) {
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;

    // Create a queue capable of containing 10 uint32_t values.
    mxDataQueue = xQueueCreate( 60, sizeof( Data* ) );
    if( !mxDataQueue )
    {
        ESP_LOGE(tag, "Could not create Data queue. Data collection not initialized.");
    }

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

    //ESP_LOGI(tag, "ReadMaximet task started. Waiting 30seconds for attaching to serial interface.");
    //vTaskDelay(30*1000/portTICK_PERIOD_MS);
    serial.Attach();

    String line;
/*     unsigned int uptimeMs = 0;
    unsigned int lastSendMs = 0; 
    unsigned int intervalMs = 0;
    int skipped = 0; */

    enum MaximetStates {
        STARTUP,
        COLUMNS,
        UNITS,
        ENDSTARTUP,
        SENDINGDATA
    };

    enum ParsingStates {
        SCANSTX,
        READCOLUMN,
        CHECKSUM,
        GARBLED,
        VALIDDATA
    };


    String column;
    MaximetStates maximetState = SENDINGDATA;

    ESP_LOGI(tag, "ReadMaximet task started and ready to receive data.");
    while (mbRun) {
        if (!serial.ReadLine(line)) {
            ESP_LOGE(tag, "Could not read line from serial");
            continue;
        }
        ESP_LOGD(tag, "THE LINE: %s", line.c_str());


        if (line.startsWith("STARTUP: OK")) {
            maximetState = STARTUP;
        }
        switch (maximetState) {
            case SENDINGDATA:
                break;
            case STARTUP : 
                maximetState = COLUMNS;
                continue;
            case COLUMNS : 
                maximetState = UNITS;
                mrConfig.msMaximetColumns = line;
                continue;
            case UNITS : 
                maximetState = ENDSTARTUP;
                mrConfig.msMaximetUnits = line;
                continue;
            case ENDSTARTUP:
                maximetState = SENDINGDATA;
                continue;
        }


        int cpos = 0;
        int len = line.length();
        int col = 0;
        int checksum = 0;
        Data *pData = nullptr;
        ParsingStates parsingState = SCANSTX;
        while (cpos < len) {
            char c = line.charAt(cpos++);
            switch (parsingState) {
                case SCANSTX: 
                    if (c == STX) {
                        parsingState = READCOLUMN;
                        pData = new Data();
                        column.setlength(0);
                    }
                    break;
                case READCOLUMN: 
                    if (c == ETX || c == ',') {
                        col++;    
                        ESP_LOGI(tag, "Column %d: '%s'", col, column.c_str());
                        // SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK
                        // MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-
                        switch (col) {
                            case 1: pData->speed = column.toFloat(); break;
                            case 2: pData->gspeed = column.toFloat(); break;
                            case 3: pData->avgspeed = column.toFloat(); break;
                            case 4: pData->dir = column.toInt(); break;
                            case 5: pData->gdir = column.toInt(); break;
                            case 6: pData->avgdir = column.toInt(); break;
                            case 7: pData->cdir = column.toInt(); break;
                            case 8: pData->avgcdir = column.toInt(); break;
                            case 9: pData->compassh = column.toInt(); break;
                            case 10: pData->pasl = column.toFloat(); break;
                            case 11: pData->pstn = column.toFloat(); break;
                            case 12: pData->rh = column.toFloat(); break;
                            case 13: pData->ah = column.toFloat(); break;
                            case 14: pData->temp = column.toFloat(); break;
                            case 15: pData->solarrad = column.toFloat(); break;
                            case 16: pData->xtilt = column.toFloat(); break;
                            case 17: pData->ytilt = column.toFloat(); break;
                            case 18: pData->status = column; break;
                            case 19: pData->windstat = column; break;
                        }
                        column.setlength(0);
                    } else if (c < 0x20 || c > 0x7E) {
                        ESP_LOGE(tag, "Invalid characters in maximet string: %s.", column.c_str());
                        parsingState = GARBLED;
                    } else {
                        column += c;
                        if (column.length() > 128) {
                            ESP_LOGE(tag, "Column data length exceeded maximum: %s.", column.c_str());
                            parsingState = GARBLED;
                        }
                    }

                    if (c == ETX) {
                        parsingState = CHECKSUM;
                        column.setlength(0);
                    } else {
                        checksum ^= c;
                    }
                    break;
            
                case CHECKSUM:
                    column += c;
                    ESP_LOGI(tag, "Checksum control area: '%s' =? %02X", column.c_str(), checksum);
                    if (column.length() == 2) {
                        int check = strtol(column.c_str(), 0, 16); // convert hex string like "FF" to integer
                        if (check != checksum) {
                            parsingState = GARBLED;
                            ESP_LOGW(tag, "Invalid checksum %02X != %s, trashing data.", checksum, column.c_str());
                        } else {
                            parsingState = VALIDDATA;
                            ESP_LOGI(tag, "Checksum validated");
                        }
                    }
                    break;
                case GARBLED:
                    delete pData;
                    pData = nullptr;
                    continue;
                case VALIDDATA:
                    pData->timestamp = esp_timer_get_time()/1000; // seconds since start (good enough as int can store seconds over 68 years in 31 bits)
                    ESP_LOGI(tag, "Pushing measurement data to queue: '%s', %d seconds since start", line.c_str(), pData->timestamp);
                    if (uxQueueSpacesAvailable(mxDataQueue) == 0) {
                        // queue is full, so remove an element
                        ESP_LOGW(tag, "Queue is full, dropping unsent oldest data.");
                        Data* pReceivedData = nullptr;
                        xQueueReceive(mxDataQueue, &pReceivedData, 0);
                        delete pReceivedData;
                        pReceivedData = nullptr;
                    }

                    // PUT pData to queue
                    if (xQueueSend(mxDataQueue, &pData, 0) != pdTRUE) {
                        // data could not put to queue, so make sure to delete the data
                        ESP_LOGE(tag, "Queue is full. We should never be here.");
                        delete pData;
                        pData = nullptr;
                    } 
                    continue;
            }
        }
    }
    serial.Release();
    ESP_LOGI(tag, "Shut down data collection.");
    return;
}

/*  

        // check for STX and ETX limiter
        ESP_LOGD(tag, "THE LINE: %s", line.c_str());
        int dataStart = line.lastIndexOf(STX);
        int dataEnd = line.lastIndexOf(ETX);
        int dataLen = dataEnd - dataStart;
        ESP_LOGD(tag, "datastart %d dataend %d", dataStart, dataEnd);

        // the length must be at minimum STX, ETX and 2 digit checksum
        if (dataStart >= 0 && dataLen > 3) {
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
                continue;
            }

            Data* pData = new Data();

            //unsigned int checksum = std::stoi(line.substring(dataEnd+1, dataEnd+3).c_str());
            String checksumString = line.substring(dataEnd+1, dataEnd+3);
            //unsigned int checksum = std::stoi(checksumString.c_str(), 0, 16); // use strtol instead?
            unsigned int checksum = strtoimax(checksumString.c_str(), 0, 16); // convert hex string like "FF" to integer
            pData->msMaximet = line.substring(dataStart+1, dataEnd-1);
// simulator: "NODE,DIR,SPEED ,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS ,PASL  ,PSTN  ,RH ,TEMP  ,DEWPOINT,AH   ,COMPASSH ,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK"
// simulator: "Q   ,   ,000.38,    ,      ,000.00  ,    ,000.00,       ,0100    ,0981.4,1037.3,0982.0,040,+017.6,+021.6  ,06.15,00000.000,000.000 ,N,254,0550,00.00,,,1.2,+010.5,06:47,11:49,16:51,201:+25,17:23,17:59,18:35,-06,+01,+1,,2018-10-31T13:07:24.9,+12.1,0000
//maximetline = "Q,168,000.02,213,000,000.00,053,000.05,000,0000,0991.1,1046.2,0991.4,035,+023.2,+007.1,07.42,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27,325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:31.7,+04.6,0000,";
            ESP_LOGD(tag, "checksumstring %s, valuehex %0X, value %d", checksumString.c_str(), checksum, checksum);
            ESP_LOGD(tag, "maximetline %s", pData->msMaximet.c_str());

            // parse out solar radiation
            const int solarradiationColumn = 19-1;
            int solarradiationPosStart = 0;
            int solarradiationPosEnd = 0;
            for (int i = 0; i < solarradiationColumn; i++) {
                solarradiationPosStart = pData->msMaximet.indexOf(',', solarradiationPosStart);
            }
            solarradiationPosEnd = pData->msMaximet.indexOf(',');
            muiSolarradiation = pData->msMaximet.substring(solarradiationPosStart, solarradiationPosEnd).toInt();
            ESP_LOGW(tag, "solarradiation %s <-> %d", pData->msMaximet.substring(solarradiationPosStart, solarradiationPosEnd).c_str(), muiSolarradiation);

            // Checksum, the 2 digit Hex Checksum sum figure is calculated from the Exclusive OR of the bytes between (and not including) the STX and ETX characters
            unsigned char calculatedChecksum = 0;
            for (int i = 0; i < pData->msMaximet.length(); i++) {
                calculatedChecksum ^= (unsigned char)pData->msMaximet[i];
            }
            ESP_LOGD(tag, "calculated checksum %0X", calculatedChecksum);

            ESP_LOGD(tag, "Pushing measurement data to queue: '%s' after %d ms (skipped %d)", line.c_str(), uptimeMs - lastSendMs, skipped);
            lastSendMs = uptimeMs;
            skipped = 0;

            if (uxQueueSpacesAvailable(mxDataQueue) == 0) {
                // queue is full, so remove an element
                ESP_LOGW(tag, "Queue is full, dropping unsent oldest data.");
                Data* pReceivedData = nullptr;
                xQueueReceive(mxDataQueue, &pReceivedData, 0);
                delete pReceivedData;
                pReceivedData = nullptr;
            }

            // PUT pData to queue
            if (xQueueSend(mxDataQueue, &pData, 0) != pdTRUE) {
                // data could not put to queue, so make sure to delete the data
                ESP_LOGE(tag, "Queue is full. We should never be here.");
                delete pData;
                pData = nullptr;
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
            }
        }

    }  */


Data* ReadMaximet::GetData() {
    Data* pReceivedData = nullptr;
    if (xQueueReceive(mxDataQueue, &pReceivedData, 0) == pdTRUE) {
        ESP_LOGD(tag, "Received data from Queue.");
        return pReceivedData;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return nullptr;
}

bool ReadMaximet::WaitForData(unsigned int timeoutSeconds) {
    Data* pReceivedData = nullptr;
    if (xQueuePeek(mxDataQueue, &pReceivedData, timeoutSeconds * 1000 / portTICK_PERIOD_MS) == pdTRUE) {
        ESP_LOGD(tag, "Peeking into Queue.");
        return pReceivedData;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}


ReadMaximet::ReadMaximet(Config &config) : mrConfig(config) {
}



ReadMaximet::~ReadMaximet() {
}

