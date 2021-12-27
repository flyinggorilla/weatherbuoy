//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "Maximet.h"
#include "DataQueue.h"
#include "Serial.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"

static const char tag[] = "Maximet";
#define SERIAL_BUFFER_SIZE (2048)
#define SERIAL_BAUD_RATE (19200)

void fMaximetTask(void *pvParameter)
{
    ((Maximet *)pvParameter)->MaximetTask();
    vTaskDelete(NULL);
}

void Maximet::Start(int gpioRX, int gpioTX)
{
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;
    xTaskCreate(&fMaximetTask, "Maximet", 1024 * 16, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

void Maximet::MaximetTask()
{

    // UART0: RX: GPIO3, TX: GPIO1 --- connected to console
    // UART1: RX: GPIO9, TX: GPIO10 --- connected to flash!!!???
    // UART2: RX: GPIO16, TX: GPIO17 --- no conflicts

    // Cannot use GPIO 12, as it will prevent to boot when pulled high.
    // Change ports from default RX/TX to not conflict with Console
    mpSerial = new Serial(UART_NUM_1, mgpioRX, mgpioTX, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);

    //ESP_LOGI(tag, "Maximet task started. Waiting 30seconds for attaching to serial interface.");
    //vTaskDelay(30*1000/portTICK_PERIOD_MS);
    mpSerial->Attach();

    String line;
    /*     unsigned int uptimeMs = 0;
    unsigned int lastSendMs = 0; 
    unsigned int intervalMs = 0;
    int skipped = 0; */

    enum MaximetStates
    {
        STARTUP,
        COLUMNS,
        UNITS,
        ENDSTARTUP,
        SENDINGDATA
    };

    enum ParsingStates
    {
        START,
        READCOLUMN,
        CHECKSUM,
        GARBLED,
        VALIDDATA
    };

    Data data;
    String column;
    MaximetStates maximetState = SENDINGDATA;
    int lastUptime = 0;

    ESP_LOGI(tag, "Maximet task started and ready to receive data.");
    while (mbRun)
    {
        if (!mpSerial->ReadLine(line))
        {
            ESP_LOGE(tag, "Could not read line from serial");
            continue;
        }
        ESP_LOGD(tag, "THE LINE: %s", line.c_str());

        if (line.startsWith("STARTUP: OK"))
        {
            maximetState = STARTUP;
        }
        switch (maximetState)
        {
        case SENDINGDATA:
            break;
        case STARTUP:
            maximetState = COLUMNS;
            continue;
        case COLUMNS:
            maximetState = UNITS;
            //mrConfig.msMaximetColumns = line;
            continue;
        case UNITS:
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
        MaximetModel model = gmx501;

        ParsingStates parsingState = START;
        while (cpos < len || parsingState == VALIDDATA)
        {
            char c = line.charAt(cpos++);
            switch (parsingState)
            {
            case START:
                if (c == STX)
                {
                    parsingState = READCOLUMN;
                    data.init();
                    column.setlength(0);
                    cposDataStart = cpos;
                    cposDataEnd = cpos;
                }
                break;
            case READCOLUMN:
                if (c == ETX || c == ',')
                {
                    col++;
                    ESP_LOGD(tag, "Column %d: '%s'", col, column.c_str());

                    if ((col == 1) && column.equalsIgnoreCase("gmx200gps"))
                    {
                        model = gmx200gps;
                    }

                    if (model == gmx200gps)
                    {
                        // GMX200GPS,+47.260818:+13.925746:+3.10,,0.0,000.34,000.86,000.42,082,090,094,270,281,188,1023.1,0969.0,039,07.13,+023.2,0065,-01,+01,0000,0000,28
                        switch (col)
                        {
                        case 1:
                            // no need to store with data
                            break;
                        case 2: {
                                String x;
                                int latPos = column.indexOf(':', 0);
                                int lonPos = column.indexOf(':', latPos+1);
                                data.lat = column.substring(0, latPos).toFloat();
                                data.lon = column.substring(latPos+1, lonPos).toFloat();
                                ESP_LOGW(tag, "lat: %0.6f lon: %0.6f ", data.lat, data.lon);
                            }
                            break;
                        case 3:
                            data.cspeed = column.toFloat();
                            break;
                        case 4:
                            data.gspeed = column.toFloat();
                            break;
                        case 5:
                            data.avgspeed = column.toFloat();
                            break;
                        case 6:
                            data.dir = column.toInt();
                            break;
                        case 7:
                            data.gdir = column.toInt();
                            break;
                        case 8:
                            data.avgdir = column.toInt();
                            break;
                        case 9:
                            data.cdir = column.toInt();
                            break;
                        case 10:
                            data.avgcdir = column.toInt();
                            break;
                        case 11:
                            data.compassh = column.toInt();
                            break;
                        case 12:
                            data.xtilt = column.toFloat();
                            break;
                        case 13:
                            data.ytilt = column.toFloat();
                            break;
                        case 14:
                            column.toCharArray(data.status, data.statuslen);
                            break;
                        case 15:
                            column.toCharArray(data.windstat, data.statuslen);
                            break;
                        }
                    }
                    else
                    {
                        // GMX501
                        // SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK
                        // MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-
                        switch (col)
                        {
                        case 1:
                            data.speed = column.toFloat();
                            break;
                        case 2:
                            data.gspeed = column.toFloat();
                            break;
                        case 3:
                            data.avgspeed = column.toFloat();
                            break;
                        case 4:
                            data.dir = column.toInt();
                            break;
                        case 5:
                            data.gdir = column.toInt();
                            break;
                        case 6:
                            data.avgdir = column.toInt();
                            break;
                        case 7:
                            data.cdir = column.toInt();
                            break;
                        case 8:
                            data.avgcdir = column.toInt();
                            break;
                        case 9:
                            data.compassh = column.toInt();
                            break;
                        case 10:
                            data.pasl = column.toFloat();
                            break;
                        case 11:
                            data.pstn = column.toFloat();
                            break;
                        case 12:
                            data.rh = column.toFloat();
                            break;
                        case 13:
                            data.ah = column.toFloat();
                            break;
                        case 14:
                            data.temp = column.toFloat();
                            break;
                        case 15:
                            data.solarrad = muiSolarradiation = column.toInt();
                            break;
                        case 16:
                            data.xtilt = column.toFloat();
                            break;
                        case 17:
                            data.ytilt = column.toFloat();
                            break;
                        case 18:
                            column.toCharArray(data.status, data.statuslen);
                            break;
                        case 19:
                            column.toCharArray(data.windstat, data.statuslen);
                            break;
                        }
                    }

                    column.setlength(0);
                }
                else if (c < 0x20 || c > 0x7E)
                {
                    ESP_LOGE(tag, "Invalid character %02X in maximet string: '%s'.", c, column.c_str());
                    parsingState = GARBLED;
                }
                else
                {
                    column += c;
                    if (column.length() > 128)
                    {
                        ESP_LOGE(tag, "Column data length exceeded maximum: '%s'.", column.c_str());
                        parsingState = GARBLED;
                    }
                }

                if (c == ETX)
                {
                    parsingState = CHECKSUM;
                    column.setlength(0);
                    cposDataEnd = cpos - 1;
                }
                else
                {
                    checksum ^= c;
                }
                break;

            case CHECKSUM:
                column += c;
                ESP_LOGD(tag, "Checksum control area: '%s' =? %02X", column.c_str(), checksum);
                if (column.length() == 2)
                {
                    int check = strtol(column.c_str(), 0, 16); // convert hex string like "FF" to integer
                    if (check != checksum)
                    {
                        parsingState = GARBLED;
                        ESP_LOGW(tag, "Invalid checksum %02X != %s, trashing data.", checksum, column.c_str());
                    }
                    else
                    {
                        parsingState = VALIDDATA;
                        ESP_LOGD(tag, "Checksum validated");
                    }
                }
                break;
            case GARBLED:
                break;
            case VALIDDATA:
                parsingState = START;
                data.uptime = esp_timer_get_time() / 1000000; // seconds since start (good enough as int can store seconds over 68 years in 31 bits)
                line[cposDataEnd] = 0;

                // Send data to display, if present
                if (mpDisplay)
                {
                    mpDisplay->Send(data.temp);
                    ESP_LOGI(tag, "Sent data to display: %.1f", data.temp);
                }

                // Put data not more frequent than every 30 seconds into queue
                if (data.uptime >= (lastUptime + 30))
                {
                    ESP_LOGI(tag, "Pushing measurement data to queue: '%s', %d seconds since start (%d..%d)", line.c_str(cposDataStart), data.uptime, cposDataStart, cposDataEnd);

                    if (mrDataQueue.IsFull())
                    {
                        ESP_LOGW(tag, "Queue is full, dropping unsent oldest data.");
                        Data droppedData;
                        mrDataQueue.GetData(droppedData);
                    }

                    if (!mrDataQueue.PutData(data))
                    {
                        ESP_LOGE(tag, "Queue is full. We should never be here.");
                    }

                    /*
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
                        }*/

                    lastUptime = data.uptime;
                }

                break;
            }
        }
    }
    mpSerial->Release();
    ESP_LOGI(tag, "Shut down data collection.");
    return;
}

void Maximet::SimulatorStart(MaximetModel maximetModel)
{
    mMaximetModel = maximetModel;
    if (mMaximetModel == gmx200gps)
    {
        SendLine("MAXIMET GMX200GPS-ESP32 Simulator V2.0");
        SendLine("STARTUP: OK");
        SendLine("USERINF,GPSLOCATION,CSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,XTILT,YTILT,STATUS,WINDSTAT,CHECK");
        SendLine("-,-,MS,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,-,-,-");
        SendLine("");
        SendLine("<END OF STARTUP MESSAGE>");
    }
    else
    {
        SendLine("MAXIMET GMX501-ESP32 Simulator V2.0");
        SendLine("STARTUP: OK");
        SendLine("SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK");
        SendLine("MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-");
        SendLine("");
        SendLine("<END OF STARTUP MESSAGE>");
    }
}

unsigned char CalculateChecksum(String &msg)
{
    unsigned char cs = 0;
    for (int i = 0; i < msg.length(); i++)
    {
        cs ^= (unsigned char)(msg.charAt(i));
    }
    return cs;
}

void Maximet::SimulatorDataPoint(float temperature, double longitude, double latitude)
{
    String gpsdata;
    if (latitude > 0)
    {
        gpsdata.printf("%+02.6f:%+02.6f:+3.10", latitude, longitude);
    }

    String data;

    if (mMaximetModel == gmx200gps)
    {
        data.printf("GMX200GPS,%s,0.0,000.34,000.86,000.42,082,090,094,270,281,188,1023.1,0969.0,039,07.13,%+06.1f,0065,-01,+01,0000,0000,", gpsdata.c_str(), temperature);
    }
    else
    {
        data.printf("000.34,000.86,000.42,082,090,094,270,281,188,1023.1,0969.0,039,07.13,%+06.1f,0065,-01,+01,0000,0000,", temperature);
    }

    String line;
    unsigned char checksum = CalculateChecksum(data);

    line.printf("\x02%s\x03%02X", data.c_str(), checksum);
    //ESP_LOG_BUFFER_HEXDUMP(tag, line.c_str(), line.length(), ESP_LOG_INFO);
    SendLine(line);
}

void Maximet::SendLine(const char *text)
{
    String line(text);
    SendLine(line);
}

void Maximet::SendLine(String &line)
{
    if (!mpSerial)
        return;

    mpSerial->Write(line + "\r\n");
}

Maximet::Maximet(DataQueue &dataQueue) : mrDataQueue(dataQueue)
{
    mpSerial = nullptr;
}

Maximet::~Maximet()
{
}
