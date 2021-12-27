//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
/*
#include "SimulateMaximet.h"
#include "Serial.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"

static const char tag[] = "SimulateMaximet";
#define SERIAL_BUFFER_SIZE (2048)
#define SERIAL_BAUD_RATE (19200)

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

void SimulateMaximet::Start(int gpioRX, int gpioTX, MaximetModel model)
{
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;
    mModel = model;

    // UART0: RX: GPIO3, TX: GPIO1 --- connected to console
    // UART1: RX: GPIO9, TX: GPIO10 --- connected to flash!!!???
    // UART2: RX: GPIO16, TX: GPIO17 --- no conflicts

    // Cannot use GPIO 12, as it will prevent to boot when pulled high.
    // Change ports from default RX/TX to not conflict with Console
    mpSerial = new Serial(UART_NUM_1, mgpioRX, mgpioTX, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);

    //ESP_LOGI(tag, "SimulateMaximet task started. Waiting 30seconds for attaching to serial interface.");
    //vTaskDelay(30*1000/portTICK_PERIOD_MS);
    mpSerial->Attach();

    if (mModel == gmx200gps) {
        SendLine("MAXIMET GMX200GPS-ESP32 Simulator V2.0");
        SendLine("STARTUP: OK");
        SendLine("USERINF,GPSLOCATION,CSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,XTILT,YTILT,STATUS,WINDSTAT,CHECK");
        SendLine("-,-,MS,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,-,-,-");
        SendLine("");
        SendLine("<END OF STARTUP MESSAGE>");
    } else {
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

void SimulateMaximet::Send(float temperature, double longitude, double latitude)
{
    String gpsdata;
    if (latitude > 0)
    {
        gpsdata.printf("000.01,%+02.6f:%+02.6f:+3.10,", latitude, longitude);
    }

    String data;

    if (mModel == gmx200gps) {
        data.printf("GMX200GPS,%s,0.0,000.34,000.86,000.42,082,090,094,270,281,188,1023.1,0969.0,039,07.13,%+06.1f,0065,-01,+01,0000,0000,", gpsdata.c_str(), temperature);
    } else {
        data.printf("000.34,000.86,000.42,082,090,094,270,281,188,1023.1,0969.0,039,07.13,%+06.1f,0065,-01,+01,0000,0000,", temperature);
    }

    String line;
    unsigned char checksum = CalculateChecksum(data);

    line.printf("\x02%s\x03%02X", data.c_str(), checksum);
    //ESP_LOG_BUFFER_HEXDUMP(tag, line.c_str(), line.length(), ESP_LOG_INFO);
    SendLine(line);
}

void SimulateMaximet::SendLine(const char *text)
{
    String line(text);
    SendLine(line);
}

void SimulateMaximet::SendLine(String &line)
{
    if (!mpSerial)
        return;

    mpSerial->Write(line + "\r\n");
}

SimulateMaximet::SimulateMaximet()
{
    mpSerial = nullptr;
}

SimulateMaximet::~SimulateMaximet()
{
    if (!mpSerial)
        return;
    mpSerial->Release();
    ESP_LOGI(tag, "Shut down data collection.");
}
*/