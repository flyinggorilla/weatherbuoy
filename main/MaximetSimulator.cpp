//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"
#include "MaximetSimulator.h"
#include "esp_ota_ops.h"
#include "DataQueue.h"
#include "Serial.h"
#include "EspString.h"

static const char tag[] = "MaximetSimulator";

void fMaximetSimulatorTask(void *pvParameter)
{
    ((MaximetSimulator *)pvParameter)->MaximetSimulatorTask();
    vTaskDelete(NULL);
}

void MaximetSimulator::Start(Maximet::Model model, int gpioRX, int gpioTX, bool alternateUart)
{
    mMaximetModel = model;
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;
    muiUartNo = alternateUart ? UART_NUM_2 : UART_NUM_1;

    // UART0: RX: GPIO3, TX: GPIO1 --- connected to console
    // UART1: RX: GPIO9, TX: GPIO10 --- connected to flash!!!???
    // UART2: RX: GPIO16, TX: GPIO17 --- no conflicts

    // Cannot use GPIO 12, as it will prevent to boot when pulled high.
    // Change ports from default RX/TX to not conflict with Console
    mpSerial = new Serial(muiUartNo, mgpioRX, mgpioTX, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);
    mpSerial->Attach();

    String line;
    line.printf("MAXIMET %s-ESP32 Simulator V2.0", Maximet::GetModelName(mMaximetModel));
    SendLine(line);

    SendLine("STARTUP: OK");

    Maximet::GetReportString(line, mMaximetModel);
    SendLine(line);
    // DONT BOTHER WITH COMPLETE STARTUP INFO OF VARYING MAXIMET MODELS FOR SIMULATION
    // SendLine("MAXIMET GMX501-ESP32 Simulator V2.0");
    // SendLine("STARTUP: OK");
    // SendLine("SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK");
    // SendLine("MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-");
    // SendLine("");
    // SendLine("<END OF STARTUP MESSAGE>");
    SendLine("<END OF STARTUP MESSAGE>");
    xTaskCreate(&fMaximetSimulatorTask, "MaximetSimulator", 1024 * 16, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

bool MaximetSimulator::CommandParse(const char *command)
{
    msRequestValue = "";
    msCommand = command;
    if (msInputLine.startsWith(command))
    {
        int valuePos = msInputLine.indexOf(" ");
        if (valuePos > 2)
        {
            msRequestValue = msInputLine.substring(valuePos + 1);
            msRequestValue.trim();
        }
        if (msRequestValue.length())
        {
            ESP_LOGI(tag, "Configuring Maximet %s = %s", command, msRequestValue.c_str());
        }
        else
        {
            ESP_LOGI(tag, "Querying Maximet configuration %s", command);
        }
        return true;
    }
    return false;
}

bool MaximetSimulator::CommandResponse(const String &responseValue)
{
    if (msRequestValue.length())
    {
        SendLine("");
    }
    else
    {
        String response;
        response = msCommand;
        response += " = ";
        response += responseValue;
        SendLine(response);
    }
    return true;
};

bool MaximetSimulator::CommandResponse(float &inoutValue, const char *format)
{

    ESP_LOGD(tag, "CommandResponse RequestValue: %s", msRequestValue.c_str());
    if (msRequestValue.length())
    {
        ESP_LOGD(tag, "CommandResponse RequestValue before inoutValue=: %f", inoutValue);
        inoutValue = msRequestValue.toFloat();
        ESP_LOGD(tag, "CommandResponse RequestValue after inoutValue=: %f", inoutValue);
    }
    String sValue;
    sValue.printf(format, inoutValue);
    return CommandResponse(sValue);
};

bool MaximetSimulator::CommandResponse(int &inoutValue, const char *format)
{
    if (msRequestValue.length())
    {
        inoutValue = msRequestValue.toInt();
    }
    String sValue;
    sValue.printf(format, inoutValue);
    return CommandResponse(sValue);
};

void MaximetSimulator::MaximetSimulatorTask()
{

    ESP_LOGI(tag, "Maximet Simulator task started and ready to receive commands.");
    mbStopped = false;
    while (mbRun)
    {
        // read command
        if (!mpSerial->ReadLine(msInputLine, 1000))
        {
            if (!mbCommandline)
            {
                SendDataPoint();
            }
            continue;
        }

        msInputLine.trim();
        msInputLine.toUpperCase();
        ESP_LOGD(tag, "THE LINE: %s", msInputLine.c_str());

        if (!msInputLine.length())
        {
            SendLine("");
            continue;
        }

        // enter commandline mode
        if (msInputLine.startsWith("*"))
        {
            ESP_LOGI(tag, "Requested commandline mode with *.");
            if (mbCommandline)
            {
                SendLine("");
            }
            else
            {
                SendLine("SETUP MODE");
            }
            mbCommandline = true;
            continue;
        }

        // ignore any command when not in commandline mode
        if (!mbCommandline)
        {
            continue;
        }

        // handle command when in commandline mode
        if (msInputLine.equalsIgnoreCase("EXIT") || msInputLine.equalsIgnoreCase("QUIT") || msInputLine.equalsIgnoreCase("Q"))
        {
            SendLine("");
            mbCommandline = false;
        }
        if (msInputLine.equalsIgnoreCase("CONFIG"))
        {
            SendLine("NOT IMPLEMENTED");
        }
        else if (CommandParse("REPORT"))
        {
            CommandResponse(Maximet::GetModelReport(mMaximetModel));
        }
        else if (CommandParse("USERINF"))
        {
            CommandResponse(Maximet::GetModelName(mMaximetModel));
        }
        else if (CommandParse("ECHO"))
        {
            if (msRequestValue.startsWith("OFF") && !mbEcho)
            {
                SendLine("UNNECESSARY COMMAND");
            }
            else
            {
                CommandResponse(mbEcho ? "ON" : "OFF");
            }
        }
        else if (CommandParse("SERIAL"))
        {
            // ESP_LOGI(tag, "Command SERIAL");
            CommandResponse("SIMULATOR");
        }
        else if (CommandParse("SWVER"))
        {
            // ESP_LOGI(tag, "Command SWVER");
            CommandResponse(esp_ota_get_app_description()->version);
        }
        else if (CommandParse("SENSOR"))
        {
            ESP_LOGI(tag, "Command SENSOR");
            switch (mMaximetModel)
            {
            case Maximet::Model::GMX200GPS:
                CommandResponse("WIND,VOLT,COMPASS,GPS,TILT");
                break;
            case Maximet::Model::GMX501:
                CommandResponse("WIND,PRESS,TEMP,RH,DEWPOINT,VOLT,COMPASS,SOLAR,TILT");
                break;
            case Maximet::Model::GMX501GPS:
                CommandResponse("WIND,PRESS,TEMP,RH,DEWPOINT,VOLT,COMPASS,GPS,SOLAR,TILT");
                break;
            case Maximet::Model::GMX501RAIN:
                CommandResponse("WIND,PRESS,TEMP,RH,DEWPOINT,VOLT,COMPASS,PRECIP,SOLAR,TILT"); // !!!!! VERIFY!!
            default:
                CommandResponse("");
                break;
            }
        }
        else if (CommandParse("OUTFREQ"))
        {
            if (msRequestValue.length())
            {
                miOutfreq = msRequestValue.equalsIgnoreCase("1/MIN") ? 60 : 1;
            }
            CommandResponse(miOutfreq == 1 ? "1HZ" : "1/MIN");
        }
        else if (CommandParse("AVGLONG"))
        {
            CommandResponse(miAvgLong);
        }
        else if (CommandParse("HASL"))
        {
            CommandResponse(mfHasl);
        }
        else if (CommandParse("HASTN"))
        {
            CommandResponse(mfHastn);
        }
        else if (CommandParse("COMPASSDECL"))
        {
            CommandResponse(mfCompassdecl);
        }
        else if (CommandParse("LAT"))
        {
            CommandResponse(mfLatitude, "%10.6f");
        }
        else if (CommandParse("LONG"))
        {
            CommandResponse(mfLongitude, "%10.6f");
        }
        else
        {
            ESP_LOGW(tag, "ILLEGAL COMMAND LINE %s", msInputLine.c_str());
            SendLine("ILLEGAL COMMAND LINE");
            // SendLine("INCORRECT PARAMETERS");
            /*
            "SETUP MODE"))
            */
        }

    }
    ESP_LOGI(tag, "Shut down Maximet Simulator.");
    mbStopped = true;
    return;
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

void MaximetSimulator::SetTemperature(float temperature)
{
    mfTemp = temperature;
}

void MaximetSimulator::SendDataPoint()
{
    Data data;

    time_t now;
    time(&now);
    tm *pUtcTime = gmtime(&now);
    char isoTime[25];
    isoTime[0] = 0;
    strftime(isoTime, sizeof(isoTime) - 1, "%FT%T.0", pUtcTime);

    String line;
    data.gpsspeed = 3;
    data.temp = mfTemp;
    data.speed = 2.1;
    strcpy(data.status, "0000");
    strcpy(data.windstat, "0000");

    data.xtilt = 33;
    data.ytilt = -33;
    data.zorient = 1;

    float gspeed = 3.9;
    float avgspeed = 2.4;
    int avgdir = 44;
    int gdir = 45;
    String gpsStatus("0106");

    // 0.000001° ~= 11cm distance. --> 5 to 6 digits after comma is required
    // 0.00001° ~= 1.1m
    // 0.0001° ~= 11m
    // 0.001° ~= 110m
    // 0.01° ~= 1.1km
    // 0.1° ~= 11km

    // const double minLatitude = 47.81358410627437;
    // const double maxLatitude = 47.95123189196899;
    // const double minLongitude = 13.50722014276939;
    // const double maxLongitude = 13.5951846390785;
    const double minLatitude = 47.81358410627437;
    const double maxLatitude = 47.92123189196899;
    const double minLongitude = 13.54722014276939;
    const double maxLongitude = 13.5551846390785;

    if (data.lat < minLatitude || data.lat > maxLatitude)
    {
        data.lat = maxLatitude;
    }
    if (data.lon < minLongitude || data.lon > maxLongitude)
    {
        data.lon = minLongitude;
    }

    data.lat -= 0.000101; // move south (towards equator)
    data.lon += 0.000011; // move east
    // longitude = 0.123456;
    // latitude = 7.654321;
    // unsigned int voltage = max471Meter.Voltage();
    // unsigned int current = max471Meter.Current();

    static const char fmtFloat[] = ",%06.2f";
    static const char fmtInt[] = ",%03i";

    switch (mMaximetModel)
    {
    case Maximet::Model::GMX200GPS:
    {
        // GMX200GPS,+48.339284:+014.309088:+0021.20,000.22,035,000.20,000.00,000.00,000.13,000.00,000.00,038,000,000,249,000,000,287,-02,-01,+00,0004,0100,0104,2022-01-22T14:11:06.8,68
        // USERINF,GPSLOCATION,GPSSPEED,GPSHEADING,CSPEED,CGSPEED,AVGCSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,CGDIR,AVGCDIR,COMPASSH,XTILT,YTILT,STATUS,WINDSTAT,GPSSTATUS,TIME,CHECK
        // -,-,MS,DEG,MS,MS,MS,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,-,-,-,-,-

        // GMX200GPS,+48.339284:+014.309088:+0021.20, 000.22,035,000.20,000.00,000.00,000.13,000.00,000.00,038,000,000,249,000,000,287,-02,-01,0004,0100,0104,2022-01-22T14:11:06.8,68
        line += Maximet::GetModelName(mMaximetModel);
        line.printf(",%+010.6f:%+010.6f:+2.00", data.lat, data.lon);
        line.printf(fmtFloat, data.gpsspeed);
        line.printf(fmtFloat, data.gpsheading);
        line.printf(fmtFloat, data.cspeed);
        line.printf(fmtFloat, data.cgspeed);
        line.printf(fmtFloat, data.avgcspeed);
        line.printf(fmtFloat, data.speed);
        line.printf(fmtFloat, gspeed);
        line.printf(fmtFloat, avgspeed);
        line.printf(fmtInt, data.dir);
        line.printf(fmtInt, gdir);
        line.printf(fmtInt, avgdir);
        line.printf(fmtInt, data.cdir);
        line.printf(fmtInt, data.cgdir);
        line.printf(fmtInt, data.avgcdir);
        line.printf(fmtInt, data.compassh);
        line.printf(",%+03i", data.xtilt);
        line.printf(",%+03i", data.ytilt);
        line.printf(",%+03i", data.zorient);
        line.printf(",%s", data.status);
        line.printf(",%s", data.windstat);
        line.printf(",%s", gpsStatus.c_str());
        line.printf(",%s", isoTime);

        // data += ".,003.0,004,%0.1f,006.00,007.00,008.00,009.00,010.00,011,012,013,014,015,016,017,-018,+019,0020,0021,0022,%s",  temperature, isoTime);
        break;
    }
    case Maximet::Model::GMX501:
    {

        break;
    }
    case Maximet::Model::GMX501GPS:
    {

        break;
    }
    case Maximet::Model::GMX501RAIN:
    {
        break;
    }
    default:
    {
        ESP_LOGE(tag, "Invalid Maximet Model configured.");
    }
    }
    unsigned char checksum = CalculateChecksum(line);

    line.printf("\x02%s\x03%02X", line.c_str(), checksum);
    // ESP_LOG_BUFFER_HEXDUMP(tag, line.c_str(), line.length(), ESP_LOG_INFO);
    ESP_LOGI(tag, "%s", line.c_str());
    SendLine(line);
}

void MaximetSimulator::SendLine(const char *text)
{
    String line(text);
    SendLine(line);
}

void MaximetSimulator::SendLine(String &line)
{
    if (!mpSerial)
        return;

    ESP_LOGD(tag, "SendLine: '%s'", line.c_str());
    mpSerial->Write(line + "\r\n");
}

MaximetSimulator::MaximetSimulator()
{
    mpSerial = nullptr;
}

MaximetSimulator::~MaximetSimulator()
{
    mpSerial->Release();
}

void MaximetSimulator::Stop()
{
    if (mbStopped)
    {
        return;
    }

    mbRun = false;

    ESP_LOGI(tag, "Stopping Maximet Simulator");

    int secondsToStop = 0;
    while (!mbStopped)
    {
        mpSerial->Write("\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (mbStopped)
        {
            return;
        }

        // try for up to 3 minutes, reboot otherwise
        if (secondsToStop > 60 * 3)
        {
            ESP_LOGE(tag, "could not stop Maximet Simulator task, rebooting");
            esp_restart();
        }
    }
};
