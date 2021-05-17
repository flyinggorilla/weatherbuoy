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
#define SERIAL_BUFFER_SIZE (512)
#define SERIAL_BAUD_RATE (19200)

void fReadMaximetTask(void *pvParameter) {
	((ReadMaximet*) pvParameter)->ReadMaximetTask();
	vTaskDelete(NULL);
}

void ReadMaximet::Start(int gpioRX, int gpioTX) {
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;

    // Create a queue capable of containing 10 uint32_t values.
    mxDataQueue = xQueueCreate( 60, sizeof( Data ) );
    if( !mxDataQueue )
    {
        ESP_LOGE(tag, "Could not create Data queue. Data collection not initialized.");
    }

	xTaskCreate(&fReadMaximetTask, "ReadMaximet", 1024*16, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
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
        START,
        READCOLUMN,
        CHECKSUM,
        GARBLED,
        VALIDDATA
    };


    Data data;
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
                //mrConfig.msMaximetColumns = line;
                continue;
            case UNITS : 
                maximetState = ENDSTARTUP;
                //mrConfig.msMaximetUnits = line;
                continue;
            case ENDSTARTUP:
                maximetState = SENDINGDATA;
                continue;
        }


        int cpos = 0;
        int len = line.length();
        int col = 0;
        int checksum = 0;

        int cposDataStart = 0;
        int cposDataEnd = 0;

        ParsingStates parsingState = START;
        while (cpos < len || parsingState == VALIDDATA) {
            char c = line.charAt(cpos++);
            switch (parsingState) {
                case START: 
                    if (c == STX) {
                        parsingState = READCOLUMN;
                        data.init();
                        column.setlength(0);
                        cposDataStart = cpos;
                        cposDataEnd = cpos;
                    }
                    break;
                case READCOLUMN: 
                    if (c == ETX || c == ',') {
                        col++;    
                        ESP_LOGD(tag, "Column %d: '%s'", col, column.c_str());
                        // SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK
                        // MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-
                        switch (col) {
                            case 1: data.speed = column.toFloat(); break;
                            case 2: data.gspeed = column.toFloat(); break;
                            case 3: data.avgspeed = column.toFloat(); break;
                            case 4: data.dir = column.toInt(); break;
                            case 5: data.gdir = column.toInt(); break;
                            case 6: data.avgdir = column.toInt(); break;
                            case 7: data.cdir = column.toInt(); break;
                            case 8: data.avgcdir = column.toInt(); break;
                            case 9: data.compassh = column.toInt(); break;
                            case 10: data.pasl = column.toFloat(); break;
                            case 11: data.pstn = column.toFloat(); break;
                            case 12: data.rh = column.toFloat(); break;
                            case 13: data.ah = column.toFloat(); break;
                            case 14: data.temp = column.toFloat(); break;
                            case 15: data.solarrad = muiSolarradiation = column.toInt(); break;
                            case 16: data.xtilt = column.toFloat(); break;
                            case 17: data.ytilt = column.toFloat(); break;
                            case 18: column.toCharArray(data.status, data.statuslen); break;
                            case 19: column.toCharArray(data.windstat, data.statuslen); break;
                        }
                        column.setlength(0);
                    } else if (c < 0x20 || c > 0x7E) {
                        ESP_LOGE(tag, "Invalid character %02X in maximet string: '%s'.", c, column.c_str());
                        parsingState = GARBLED;
                    } else {
                        column += c;
                        if (column.length() > 128) {
                            ESP_LOGE(tag, "Column data length exceeded maximum: '%s'.", column.c_str());
                            parsingState = GARBLED;
                        }
                    }

                    if (c == ETX) {
                        parsingState = CHECKSUM;
                        column.setlength(0);
                        cposDataEnd = cpos-1;
                    } else {
                        checksum ^= c;
                    }
                    break;
            
                case CHECKSUM:
                    column += c;
                    ESP_LOGD(tag, "Checksum control area: '%s' =? %02X", column.c_str(), checksum);
                    if (column.length() == 2) {
                        int check = strtol(column.c_str(), 0, 16); // convert hex string like "FF" to integer
                        if (check != checksum) {
                            parsingState = GARBLED;
                            ESP_LOGW(tag, "Invalid checksum %02X != %s, trashing data.", checksum, column.c_str());
                        } else {
                            parsingState = VALIDDATA;
                            ESP_LOGD(tag, "Checksum validated");
                        }
                    }
                    break;
                case GARBLED:
                    break;
                case VALIDDATA:
                    parsingState = START;
                    data.uptime = esp_timer_get_time()/1000000; // seconds since start (good enough as int can store seconds over 68 years in 31 bits)
                    line[cposDataEnd] = 0;
                    ESP_LOGI(tag, "Pushing measurement data to queue: '%s', %d seconds since start (%d..%d)", line.c_str(cposDataStart), data.uptime, cposDataStart, cposDataEnd);
                    if (uxQueueSpacesAvailable(mxDataQueue) == 0) {
                        // queue is full, so remove an element
                        ESP_LOGW(tag, "Queue is full, dropping unsent oldest data.");
                        Data receivedData;
                        xQueueReceive(mxDataQueue, &receivedData, 0);
                    }

                    // PUT pData to queue
                    if (xQueueSend(mxDataQueue, &data, 0) != pdTRUE) {
                        // data could not put to queue, so make sure to delete the data
                        ESP_LOGE(tag, "Queue is full. We should never be here.");
                    } 
                    break;
            }
        }
    }
    serial.Release();
    ESP_LOGI(tag, "Shut down data collection.");
    return;
}


bool ReadMaximet::GetData(Data &data) {
    if (xQueueReceive(mxDataQueue, &data, 0) == pdTRUE) {
        ESP_LOGD(tag, "Received data from Queue.");
        return true;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}

int ReadMaximet::GetQueueLength() {
    return uxQueueMessagesWaiting(mxDataQueue);
}

bool ReadMaximet::WaitForData(unsigned int timeoutSeconds) {
    Data receivedData;
    if (xQueuePeek(mxDataQueue, &receivedData, timeoutSeconds * 1000 / portTICK_PERIOD_MS) == pdTRUE) {
        ESP_LOGD(tag, "Peeking into Queue.");
        return true;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}


ReadMaximet::ReadMaximet(Config &config) : mrConfig(config) {
}



ReadMaximet::~ReadMaximet() {
}

