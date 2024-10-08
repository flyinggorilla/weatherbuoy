#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"
#include "Maximet.h"
#include "DataQueue.h"
#include "Serial.h"
#include "EspString.h"
#include "VelocityVector.h"
#include "RtcVariables.h"

//====================================================================================
// Maximet Notes:
// GSPEED is updated every minute, based on a) averaging 3 samples (1 per second) b) calculating max of such averages
// AVG(C)SPEED are calculated as rolling average over 10 minutes (default 10min, Race Commitee Start boat should be set to 5 minutes), and updated once per minute
// AVGCDIR only calculated when GPS available

static const char tag[] = "Maximet";

void fMaximetTask(void *pvParameter)
{
    ((Maximet *)pvParameter)->MaximetTask();
    vTaskDelete(NULL);
}

void Maximet::Start(int gpioRX, int gpioTX, bool alternateUart)
{
    mgpioRX = gpioRX;
    mgpioTX = gpioTX;
    muiUartNo = alternateUart ? UART_NUM_2 : UART_NUM_1;
    xTaskCreate(&fMaximetTask, "Maximet", 1024 * 16, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}

#define STX (0x02) // ASCII start of text
#define ETX (0x03) // ASCII end of text

float StringToFloatOrNaN(String &column)
{
    if (!column.length())
    {
        return nanf();
    }
    return column.toFloat();
}

short StringToShortOrNaN(String &column)
{
    if (!column.length())
    {
        return nans();
    }
    return column.toInt();
}

void ParseGPSLocation(String &column, Data &data)
{
    if (column.length() > 20)
    {
        int latPos = column.indexOf(':', 0);
        int lonPos = column.indexOf(':', latPos + 1);
        data.lat = column.substring(0, latPos).toFloat();
        data.lon = column.substring(latPos + 1, lonPos).toFloat();
    }
    else
    {
        data.lat = nanf();
        data.lon = nanf();
    }
    ESP_LOGD(tag, "col: '%s' lat: %0.6f lon: %0.6f ", column.c_str(), data.lat, data.lon);
}

void ParseGPSStatus(String &column, Data &data)
{
    if (column.length() == 4)
    {
        data.gpsfix = column.charAt(1) == '1';
        data.gpssat = (unsigned char)strtol(column.substring(2, 4).c_str(), nullptr, 16);
    }
}

void ParseTime(String &column, Data &data)
{
    if (column.length() >= 19)
    {
        struct tm tm;
        column.setlength(19);
        strptime(column.c_str(), "%FT%T", &tm); //"2022-01-21T22:43:11.4" no trailing Z!!!
        data.time = mktime(&tm);

        uint16_t systemDate = data.time / (60 * 60 * 24);
        double systemTime = data.time - systemDate * 60 * 60 * 24;
        ESP_LOGD(tag, "time:%ld, days:%d, seconds: %f", data.time, systemDate, systemTime);
    }
}

inline double msToKnots(double v) { return v*1.9438444924406047516198704103672L; } // 3600L/1852.0L
inline double KnotsToms(double v) { return v*0.51444444444444444444444444444444L; } // 1852L/3600.0L

void Maximet::MaximetTask()
{

    // UART0: RX: GPIO3, TX: GPIO1 --- connected to console
    // UART1: RX: GPIO9, TX: GPIO10 --- connected to flash!!!???
    // UART2: RX: GPIO16, TX: GPIO17 --- no conflicts

    // Cannot use GPIO 12, as it will prevent to boot when pulled high.
    // Change ports from default RX/TX to not conflict with Console
    mpSerial = new Serial(muiUartNo, mgpioRX, mgpioTX, SERIAL_BAUD_RATE, SERIAL_BUFFER_SIZE);

    // ESP_LOGI(tag, "Maximet task started. Waiting 30seconds for attaching to serial interface.");
    // vTaskDelay(30*1000/portTICK_PERIOD_MS);
    mpSerial->Attach();

    String line;
    /*     unsigned int uptimeMs = 0;
    unsigned int lastSendMs = 0;
    unsigned int intervalMs = 0;
    int skipped = 0; */

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
    int lastShortAvgUptime = 0;

    // esp_log_level_set(tag, ESP_LOG_DEBUG);
    // esp_log_level_set("Serial", ESP_LOG_DEBUG);
    if (!MaximetConfig()) 
    {
        ESP_LOGE(tag, "Failed to detect an attached and properly configured Maximet Weather station");

        // post continuously error code to display until Stop() is called
        while (mbRun)
        {
            mrDataQueue.PutLatestData(data);
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }
        mbRun = false;
        mbStopped = true;
        return;
    }

    VelocityVector shortAvgCSpeedVector;
    VelocityVectorMovingAverage longAvgCSpeedVector(mMaximetConfig.iAvgLong);

    #define GUST_CALCULATION_SECONDS 10 // WMO GUST default is 3 seconds, but since the buoys are moving so much lets take 10s
    VelocityVectorMovingAverage gustAvgCSpeedVector(GUST_CALCULATION_SECONDS);
    VelocityVector gustCSpeedVector;
    float gustMaxCSpeed = 0;
    float gustMaxCDir = 0;

    ESP_LOGI(tag, "Maximet task started and ready to receive data.");
    mbStopped = false;
    while (mbRun)
    {
        if (!mpSerial->ReadLine(line))
        {
            continue;
        }
        ESP_LOGD(tag, "THE LINE: %s", line.c_str());

        int cpos = 0;
        int len = line.length();
        int col = 0;
        int checksum = 0;

        int cposDataStart = 0;
        int cposDataEnd = 0;
        Model model = Model::NONE;

        float maximetGSpeed;
        float maximetAvgSpeed;
        short maximetGDir;
        short maximetAvgDir;

        ParsingStates parsingState = START;
        while (cpos < len || parsingState == VALIDDATA)
        {
            char c = line.charAt(cpos++);
            switch (parsingState)
            {
            case START:
                if (c > 0x80) {
                    // skip non-ascii char ... note that char is treated as unsigned by default (see CHAR_MIN)
                    break;
                }
                if (c == STX)
                {
                    parsingState = READCOLUMN;
                    data.init();
                    column.setlength(0);
                    cposDataStart = cpos;
                    cposDataEnd = cpos;

                    maximetGSpeed = nanf();
                    maximetAvgSpeed = nanf();
                    maximetGDir = nans();
                    maximetAvgDir = nans();
#if DEBUG_MAXIMET
line.getBytes((unsigned char*)data.line, DEBUG_MAXIMET_LINEBUFLEN-1, cposDataStart);
// remove control characters that confuses JSON
int dlpos = 0;
while(data.line[dlpos]) {
    if (data.line[dlpos] < 0x20 || data.line[dlpos] > 0x7E) {
        data.line[dlpos] = 0x2A; // asterisk
    }
    dlpos++;
}
#endif                
                }
                break;
            case READCOLUMN:
                if (c == ETX || c == ',')
                {
                    col++;
                    ESP_LOGV(tag, "Column %d: '%s'", col, column.c_str());

                    if (col == 1)
                    {
                        model = GetModel(column);
                        if (!model) 
                        {
                            parsingState = GARBLED;
                            ESP_LOGE(tag, "Garbled data, invalid Maximet model: %s", column.c_str());
                            continue;
                        }
                    }

                    if (model == Model::GMX200GPS)
                    {
                        // GMX200GPS,+48.339284:+014.309088:+0021.20,000.22,035,000.20,000.00,000.00,000.13,000.00,000.00,038,000,000,249,000,000,287,-02,-01,1,0004,0100,0104,2022-01-22T14:11:06.8,68
                        // USERINF,GPSLOCATION,GPSSPEED,GPSHEADING,CSPEED,CGSPEED,AVGCSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,CGDIR,AVGCDIR,COMPASSH,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,GPSSTATUS,TIME,CHECK
                        // -,-,MS,DEG,MS,MS,MS,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,DEG,-,-,-,-,-,-

                        switch (col)
                        {
                        case 1:
                            // no need to store with data
                            break;
                        case 2:
                            ParseGPSLocation(column, data);
                            break;
                        case 3:
                            data.gpsspeed = column.toFloat(); // leave at 0 if not available
                            break;
                        case 4:
                            data.gpsheading = column.toInt(); // leave at 0 if not available
                            break;
                        case 5:
                            data.cspeed = StringToFloatOrNaN(column); // derive from speed if not available
                            break;
                        case 6:
                            data.cgspeed = StringToFloatOrNaN(column); // derive from gspeed if not available
                            break;
                        case 7:
                            data.avgcspeed = StringToFloatOrNaN(column); // derive from avgspeed if not available
                            break;
                        case 8:
                            data.speed = column.toFloat();
                            break;
                        case 9:
                            maximetGSpeed = column.toFloat();
                            break;
                        case 10:
                            maximetAvgSpeed = column.toFloat();
                            break;
                        case 11:
                            data.dir = column.toInt();
                            break;
                        case 12:
                            maximetGDir = column.toInt();
                            break;
                        case 13:
                            maximetAvgDir = column.toInt();
                            break;
                        case 14:
                            data.cdir = StringToShortOrNaN(column);
                            break;
                        case 15:
                            data.cgdir = StringToShortOrNaN(column);
                            break;
                        case 16:
                            data.avgcdir = StringToShortOrNaN(column);
                            break;
                        case 17:
                            data.compassh = column.toInt();
                            break;
                        case 18:
                            data.xtilt = column.toInt();
                            break;
                        case 19:
                            data.ytilt = column.toInt();
                            break;
                        case 20:
                            data.zorient = column.toInt();
                            break;
                        case 21:
                            column.toCharArray(data.status, data.statuslen);
                            break;
                        case 22:
                            column.toCharArray(data.windstat, data.statuslen);
                            break;
                        case 23:
                            ParseGPSStatus(column, data);
                            break;
                        case 24:
                            ParseTime(column, data);
                            break;
                        }
                    }
                    else if (model == Model::GMX501GPS)
                    {
                        // GMX501GPS,002.15,000.81,000.23,140,160,118,290,265,148,1045.0,0987.1,067,05.02,+006.3,0006,+00,+00,+1,0004,0000,+48.336892:+014.306931:+0344.40,0106,2022-01-29T14:23:17.8,45
                        // USERINF,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,GPSLOCATION,GPSSTATUS,TIME,CHECK
                        // -,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-,-,-,-,-
                        switch (col)
                        {
                        case 1:
                            // no need to store with data
                            break;
                        case 2:
                            data.cspeed = data.speed = column.toFloat();
                            break;
                        case 3:
                            data.cgspeed = maximetGSpeed = column.toFloat();
                            break;
                        case 4:
                            data.avgcspeed = maximetAvgSpeed = column.toFloat();
                            break;
                        case 5:
                            data.dir = column.toInt();
                            break;
                        case 6:
                            maximetGDir = column.toInt();
                            data.cgdir = nans();
                            break;
                        case 7:
                            maximetAvgDir = column.toInt();
                            break;
                        case 8:
                            data.cdir = StringToShortOrNaN(column);
                            break;
                        case 9:
                            data.avgcdir = StringToShortOrNaN(column);
                            break;
                        case 10:
                            data.compassh = column.toInt();
                            break;
                        case 11:
                            data.pasl = column.toFloat();
                            break;
                        case 12:
                            data.pstn = column.toFloat();
                            break;
                        case 13:
                            data.rh = column.toFloat();
                            break;
                        case 14:
                            data.ah = column.toFloat();
                            break;
                        case 15:
                            data.temp = column.toFloat();
                            break;
                        case 16:
                            data.solarrad = muiSolarradiation = column.toInt();
                            break;
                        case 17:
                            data.xtilt = column.toInt();
                            break;
                        case 18:
                            data.ytilt = column.toInt();
                            break;
                        case 19:
                            data.zorient = column.toInt();
                            break;
                        case 20:
                            column.toCharArray(data.status, data.statuslen);
                            break;
                        case 21:
                            column.toCharArray(data.windstat, data.statuslen);
                            break;
                        case 22:
                            ParseGPSLocation(column, data);
                            break;
                        case 23:
                            ParseGPSStatus(column, data);
                            //********************************************
                            // FIXING WRONG AVGCDIR WHEN GPS is not available
                            if (!data.gpsfix) {
                                data.avgcdir = nans();
                                data.cdir = nans();
                                data.cgdir = nans();
                            }
                            break;
                        case 24:
                            ParseTime(column, data);
                            break;
                        }
                    }
                    else if (model == Model::GMX501)
                    {
                        // GMX501
                        // USERINF,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,CHECK
                        // -,MS,MS,MS,DEG,DEG,DEG,DEG,DEG,DEG,HPA,HPA,%,G/M3,C,WM2,DEG,DEG,-,-,-,-
                        switch (col)
                        {
                        case 1:
                            // no need to store with data
                            break;
                        case 2:
                            data.cspeed = data.speed = column.toFloat();
                            break;
                        case 3:
                            data.cgspeed = maximetGSpeed = column.toFloat();
                            break;
                        case 4:
                            data.avgcspeed = maximetAvgSpeed = column.toFloat();
                            break;
                        case 5:
                            data.dir = column.toInt();
                            break;
                        case 6:
                            maximetGDir = column.toInt();
                            data.cgdir = nans();
                            break;
                        case 7:
                            maximetAvgDir = column.toInt();
                            break;
                        case 8:
                            data.cdir = StringToShortOrNaN(column);
                            break;
                        case 9:
                            data.avgcdir = StringToShortOrNaN(column);
                            break;
                        case 10:
                            data.compassh = column.toInt();
                            break;
                        case 11:
                            data.pasl = column.toFloat();
                            break;
                        case 12:
                            data.pstn = column.toFloat();
                            break;
                        case 13:
                            data.rh = column.toFloat();
                            break;
                        case 14:
                            data.ah = column.toFloat();
                            break;
                        case 15:
                            data.temp = column.toFloat();
                            break;
                        case 16:
                            data.solarrad = muiSolarradiation = column.toInt();
                            break;
                        case 17:
                            data.xtilt = column.toInt();
                            break;
                        case 18:
                            data.ytilt = column.toInt();
                            break;
                        case 19:
                            data.zorient = column.toInt();
                            break;
                        case 20:
                            column.toCharArray(data.status, data.statuslen);
                            break;
                        case 21:
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
                ESP_LOGV(tag, "Checksum control area: '%s' =? %02X", column.c_str(), checksum);
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
                        ESP_LOGV(tag, "Checksum validated");
                    }
                }
                break;
            case GARBLED:
                break;
            case VALIDDATA:
                parsingState = START;
                data.uptime = esp_timer_get_time() / 1000000; // seconds since start (good enough as int can store seconds over 68 years in 31 bits)
                line[cposDataEnd] = 0;

                if (isnanf(data.cspeed))
                {
                    data.cspeed = data.speed;
                }

                if (isnanf(data.cgspeed))
                {
                    data.cgspeed = maximetGSpeed;
                }

                if (isnanf(data.avgcspeed))
                {
                    data.avgcspeed = maximetAvgSpeed;
                }

                if (isnans(data.cdir))
                {
                    data.cdir = (data.dir + data.compassh) % 360;
                }

                if (isnans(data.cgdir))
                {
                    data.cgdir = (maximetGDir + data.compassh) % 360;
                }

                if (isnans(data.avgcdir))
                {
                    // this doesnt make much sense as avgcdir is average of last 5minutes while compass iss current
                    data.avgcdir = (maximetAvgDir + data.compassh) % 360; // as avgcdir is not populated when GNSS is not available, lets do the math with compass
                }

                // maximet enters "average is building" mode when entering commandline mode,
                // and as such we need to ignore zero values while building averages
                if ( (data.cgspeed == 0.0) && (data.avgcspeed == 0.0) && (strcmp(data.windstat, "0100") == 0) )
                {
                    data.cgspeed = data.cspeed;
                    data.cgdir = data.cdir;
                    data.avgcspeed = data.cspeed;
                    data.avgcdir = data.cdir;
                }

                mrDataQueue.PutLatestData(data);
                mrDataQueue.PutAlarmData(data);

                bool is1HzOutput = (mMaximetConfig.iOutputIntervalSec <= 1);
                if (is1HzOutput)
                {
                    shortAvgCSpeedVector.add(data.cspeed, data.cdir);
                    ESP_LOGD(tag, "data.speed: %0.2f data.dir: %d, data.compassh: %d, data.cspeed: %0.2f data.cdir: %d, data.cgspeed: %0.2f data.cgdir: %d, avgspeed: %0.2f avggdir: %d, data.avgcspeed: %0.2f data.avgcdir: %d",
                             data.speed, data.dir, data.compassh, data.cspeed, data.cdir, data.cgspeed, data.cgdir, maximetAvgSpeed, maximetAvgDir, data.avgcspeed, data.avgcdir);


                    // run 10s moving average of 1s measurements; take maximum of average, which needs to be reset all "short" intervals
                    gustCSpeedVector.clear();
                    gustCSpeedVector.add(data.cspeed, data.cdir);
                    gustAvgCSpeedVector.add(gustCSpeedVector);
                    float gustCSpeed = gustAvgCSpeedVector.getSpeed();
                    if (gustCSpeed > gustMaxCSpeed) {
                        gustMaxCSpeed = gustCSpeed;
                        gustMaxCDir = gustAvgCSpeedVector.getDir();
                    }

                }



/*                if (model == Model::GMX501GPS) 
                {
                    MOVING AVERAGE OF SHORT INTERVALS!!!!!

                    longAvgCSpeedVector.add(data.cspeed, data.cdir);
                    
                    if (data.uptime >= lastLongAvgUptime)
                    {
                        //ESP_LOGW(tag, "Last records data.cspeed: %0.2f data.cdir: %d", data.cspeed, data.cdir);
                        data.cspeed = longAvgCSpeedVector.getSpeed();
                        data.cdir = longAvgCSpeedVector.getDir();
                        shortAvgCSpeedVector.clear();
                        //ESP_LOGW(tag, "Averaged data.cspeed: %0.2f data.cdir: %d", data.cspeed, data.cdir);
                    }


                    lastLongAvgUptime = data.uptime + mMaximetConfig.iAvgLong * 60; // typically 5 or 10 minutes
                }
*/

                // USERINF,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,GPSLOCATION,GPSSTATUS,TIME,CHECK
                // REPORT USERINF SPEED GSPEED AVGSPEED DIR GDIR AVGDIR CDIR AVGCDIR COMPASSH PASL PSTN RH AH TEMP SOLARRAD XTILT YTILT ZORIENT STATUS WINDSTAT GPSLOCATION GPSSTATUS TIME
                /// SOMETHING WRONG HERE ... WHEN QUEUE FULL EVERY SECOND IS PUSHED
                // Put data not more frequent than every 30 seconds into queue
                if (data.uptime >= lastShortAvgUptime || !is1HzOutput)
                {
                    if (is1HzOutput)
                    {
                        ESP_LOGW(tag, "Last records data.cspeed: %0.2f data.cdir: %d", data.cspeed, data.cdir);
                        data.cspeed = shortAvgCSpeedVector.getSpeed();
                        data.cdir = shortAvgCSpeedVector.getDir();
                        longAvgCSpeedVector.add(shortAvgCSpeedVector);
                        data.xavgcspeed = longAvgCSpeedVector.getSpeed();
                        data.xavgcdir = longAvgCSpeedVector.getDir();
                        shortAvgCSpeedVector.clear();

                        data.cgspeed = gustMaxCSpeed;
                        data.cgdir = gustMaxCDir;
                        gustMaxCSpeed = 0;
                        gustMaxCDir = 0;
                        //ESP_LOGW(tag, "Averaged data.cspeed: %0.2f data.cdir: %d", data.cspeed, data.cdir);

                        //############### TEMPORARY FIX FOR GPS ISSUE ###############################
                        #pragma message ("Temporary fix for GPS issue")
                        if ((model == Model::GMX501GPS || model == Model::GMX200GPS) && !data.gpsfix) 
                        {
                            data.avgcdir = data.xavgcdir;
                            data.avgcspeed = data.xavgcspeed;
                        }                          

                    }

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

                    lastShortAvgUptime = data.uptime + 60; // every 60 seconds!
                }

                break;
            }

            // check if garbled data detected
            if (parsingState == GARBLED) {
                muiGarbledData++;
                ESP_LOGW(tag, "GARBLED data: %i bytes, parsing stage: %i", line.length(), parsingState);
                ESP_LOG_BUFFER_HEXDUMP(tag, line.c_str(), line.length(), ESP_LOG_VERBOSE);
                break;
            }
        }
    }
    ESP_LOGI(tag, "Shut down data collection.");
    mbStopped = true;
    return;
}

const char *Maximet::GetReportString(Model model)
{
    switch (model)
    {
    case Model::GMX501:
        return MAXIMET_REPORT_GMX501;
    case Model::GMX200GPS:
        return MAXIMET_REPORT_GMX200GPS;
    case Model::GMX501GPS:
        return MAXIMET_REPORT_GMX501GPS;
    case Model::GMX501RAIN:
        return MAXIMET_REPORT_GMX501RAIN;
    case Model::NONE:
        return "NONE";
    default:
        return "";
    };
}

void Maximet::GetReportString(String &report, Model model)
{
    report.clear();
    report = GetReportString(model);
};

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
    mpSerial->Release();
}

void Maximet::Stop()
{
    if (mbStopped)
    {
        return;
    }

    mbRun = false;

    ESP_LOGI(tag, "Stopping Maximet to enter Commandline Mode");

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
            ESP_LOGE(tag, "could not stop Maximet task, rebooting");
            RtcVariables::SetExtendedResetReason(RtcVariables::EXTENDED_RESETREASON_ERROR);
            esp_restart();
        }
    }
};

bool Maximet::MaximetConfig()
{
    mMaximetConfig.model = Model::NONE;

    // read and optionally update Maximet configuration
    if (!EnterCommandLine()) 
    {
        ESP_LOGE(tag, "No Maximet detected!");
        return false;
    }

    // LAT = +47.944191
    // LONG = +013.584622
    // WriteLat(47.944191);
    // WriteLong(013.584622);
    // WriteCompassdecl(4.0);
    ReadSerial(); // SERIAL: 22040004
    ReadSWVer();  // SWVER: 2669 V2.00.22
    ReadSensor(); // SENSOR: WIND,PRESS,TEMP,RH,DEWPOINT,VOLT,COMPASS,GPS,SOLAR,TILT
    ReadReport();
    ReadUserinf();

    ReadOutfreq();
    ReadAvgLong();
    ReadCompassdecl();
    ReadLat();
    ReadLong();


    if (mMaximetConfig.sSensor.contains("WIND") && mMaximetConfig.sSensor.contains("TILT") && mMaximetConfig.sSensor.contains("COMPASS"))
    {
        bool bSolar = false;
        if (mMaximetConfig.sSensor.contains("SOLAR"))
        {
            bSolar = true;
        }

        bool bGps = false;
        if (mMaximetConfig.sSensor.contains("GPS"))
        {
            bGps = true;
        }

        bool bRain = false;
        if (mMaximetConfig.sSensor.contains("PRECIP"))
        {
            bRain = true;
        }

        if (mMaximetConfig.sSensor.contains("PRESS"))
        {
            ReadHasl(); // not available for GMX200
            ReadHastn(); // not available for GMX200
        }

        if (bSolar)
        {
            if (bGps)
            {
                mMaximetConfig.model = Model::GMX501GPS;
            }
            else
            {
                mMaximetConfig.model = bRain ? Model::GMX501RAIN : Model::GMX501;
            }
        }
        else
        {
            mMaximetConfig.model = bGps ? Model::GMX200GPS : Model::NONE;
        }
    }
    else
    {
        ESP_LOGE(tag, "Incompatible Maximet: no Wind, Tilt or Compass sensor!");
        mMaximetConfig.model = Model::NONE;
    }

    ESP_LOGI(tag, "Detected Maximet model (SENSORS): %s", GetModelName(mMaximetConfig.model));


    // ESP_LOGI(tag, "Check config %d, %d", msReport.length(), msUserinfo.length());
    if (mMaximetConfig.sReport.length() && mMaximetConfig.model)
    {

        if (!mMaximetConfig.sReport.equals(GetModelReport(mMaximetConfig.model)))
        {
            ESP_LOGW(tag, "Configuration mismatch for %s, Columns %s, Updating to %s", GetModelName(mMaximetConfig.model), mMaximetConfig.sReport.c_str(), GetModelReport(mMaximetConfig.model));
            WriteUserinf(GetModelName(mMaximetConfig.model));
            WriteReport(GetModelReport(mMaximetConfig.model));
            WriteCompassdecl(4.3);
            WriteOutfreq(true);
            WriteHasl(469);
            WriteHastn(3);
            WriteLat(47.875249176262976);  // mid of attersee
            WriteLong(13.548413577850653); // mid of attersee
            WriteAvgLong(5); // set default AVGLONG to 5 minutes
        }

        if (!mMaximetConfig.sUserinfo.equals(GetModelName(mMaximetConfig.model)))
        {
            ESP_LOGW(tag, "Configuration mismatch for USERINFO=%s, Updating to detected model %s", mMaximetConfig.sUserinfo.c_str(), GetModelName(mMaximetConfig.model));
            WriteUserinf(GetModelName(mMaximetConfig.model));
        };

        mMaximetModel = mMaximetConfig.model;
    }
    ExitCommandLine();
    return true;
}

static const unsigned int COMMANDLINE_TIMEOUT_MS = 5000;

bool Maximet::EnterCommandLine()
{
    if (mbCommandline)
    {
        return true;
    }


    // sending % twice will print Maximet model info and serial number: "ID:2669 "MAXIMET GMX501-3B-0011" 2.00.23 [Q] PV=4" 
    // sending * will enter commandline mode: "SETUP MODE"
    String cmd("\r\nEXIT\r\n%%%%*\r\nECHO OFF\r\n");
    String line;

    // Note, Maximet boots for approx 26 seconds from cold-start, make sure to give it enough time to enter command line mode
    int attempts = 30; // 30 attempts = approx. 60 seconds due to 2 sec delays;  
                       
    mpSerial->Flush();
    while (attempts--)
    {
        mpSerial->Write(cmd);
        ESP_LOGD(tag, "writing: %s", cmd.c_str());

        if (mpSerial->ReadLine(line, COMMANDLINE_TIMEOUT_MS))
        {
            ESP_LOGD(tag, "Switch Maximet to commandline...  ReadLine: %s", line.c_str());
        } 
        else {
            ESP_LOGW(tag, "Timeout switching Maximet to commandline... ");
        }

        if (line.charAt(0) == STX) {
            ESP_LOGD(tag, "skipping data line");
            mpSerial->ReadLine(line, COMMANDLINE_TIMEOUT_MS);
        }
        
        if (line.startsWith("ID:"))
        {
            ESP_LOGI(tag, "%s", line.c_str());
            mpSerial->ReadLine(line, COMMANDLINE_TIMEOUT_MS);

        }
        if (line.startsWith("SETUP MODE"))
        {
            ESP_LOGI(tag, "Commandline mode entered.");
            return mbCommandline = true;
        }
        else if (line.startsWith("UNNECESSARY COMMAND"))
        {
            ESP_LOGI(tag, "Already in commandline mode.");
            return mbCommandline = true;
        }
        vTaskDelay(1200 / portTICK_PERIOD_MS);
        ESP_LOGD(tag, "Remaining attempts switching Maximet to commandline: %i", attempts);
    }

    if (!attempts) {
        ESP_LOGE(tag, "Could not switch Maximet to commandline");
    }

    return mbCommandline = false;
}

void Maximet::ExitCommandLine()
{
    String cmd("\r\nexit\r\n");
    mpSerial->Write(cmd);
    mbCommandline = false;
    ESP_LOGI(tag, "Commandline mode exited.");
}

void Maximet::WriteAvgLong(unsigned short avglong)
{
    WriteConfig("AVGLONG", String(avglong));
}

bool Maximet::ReadConfig(String &value, const char *sConfig)
{
    EnterCommandLine();
    String cmd(sConfig);
    cmd += "\r\n";
    mpSerial->Write(cmd);

    cmd = sConfig;
    cmd += " = ";

    String line;
    while (mpSerial->ReadLine(line, COMMANDLINE_TIMEOUT_MS))
    {
        if (line.startsWith(cmd))
        {
            value = line.substring(cmd.length());
            // ESP_LOGI(tag, "ReadConfig ReadLine Value: %s", value.c_str());
            return true;
        }
        else if (line.startsWith("ILLEGAL ") || line.startsWith("INCORRECT "))
        {
            ESP_LOGW(tag, "%s: %s", sConfig, line.c_str());
        }
    }
    return false;
}

bool Maximet::ReadConfigInt(int &val, const char *cmd)
{
    String response;
    if (ReadConfig(response, cmd))
    {
        val = response.toInt();
        ESP_LOGI(tag, "%s: %d", cmd, val);
        return true;
    }
    return false;
};

bool Maximet::ReadConfigString(String &val, const char *cmd)
{
    String response;
    if (ReadConfig(response, cmd))
    {
        val = response;
        val.trim();
        ESP_LOGI(tag, "%s: %s", cmd, val.c_str());
        return true;
    }
    return false;
};

bool Maximet::ReadConfigFloat(float &val, const char *cmd)
{
    String response;
    if (ReadConfig(response, cmd))
    {
        val = response.toFloat();
        ESP_LOGI(tag, "%s: %f", cmd, val);
        return true;
    }
    return false;
};

bool Maximet::ReadUserinf()
{
    return ReadConfig(mMaximetConfig.sUserinfo, "USERINF");
};

bool Maximet::ReadSensor()
{
    return ReadConfigString(mMaximetConfig.sSensor, "SENSOR");
};

bool Maximet::ReadReport()
{
    return ReadConfigString(mMaximetConfig.sReport, "REPORT");
};

bool Maximet::ReadSerial()
{
    return ReadConfigString(mMaximetConfig.sSerial, "SERIAL");
};

bool Maximet::ReadSWVer()
{
    return ReadConfigString(mMaximetConfig.sSWVer, "SWVER");
};

bool Maximet::ReadHastn()
{
    return ReadConfigFloat(mMaximetConfig.fHastn, "HASTN");
}

bool Maximet::ReadHasl()
{
    return ReadConfigFloat(mMaximetConfig.fHasl, "HASL");
}

bool Maximet::ReadCompassdecl()
{
    return ReadConfigFloat(mMaximetConfig.fCompassdecl, "COMPASSDECL");
}

bool Maximet::ReadLat()
{
    return ReadConfigFloat(mMaximetConfig.fLat, "LAT");
}

bool Maximet::ReadLong()
{
    return ReadConfigFloat(mMaximetConfig.fLong, "LONG");
}

bool Maximet::ReadAvgLong()
{
    return ReadConfigInt(mMaximetConfig.iAvgLong, "AVGLONG");
}

bool Maximet::ReadOutfreq()
{
    String val;
    if (ReadConfig(val, "OUTFREQ"))
    {
        if (val.equals("1HZ"))
        {
            mMaximetConfig.iOutputIntervalSec = 1;
        }
        else if (val.equals("1/MIN"))
        {
            mMaximetConfig.iOutputIntervalSec = 60;
        }
        else
        {
            return false;
        }
        ESP_LOGI(tag, "OUTFREQ: every %d second(s)", mMaximetConfig.iOutputIntervalSec);
        return true;
    }
    return false;
};

bool Maximet::WriteConfig(const char *sConfig, const String &value)
{
    EnterCommandLine();
    String cmd(sConfig);
    cmd += " ";
    cmd += value;
    cmd += "\r\n";
    mpSerial->Write(cmd);
    String line;
    return mpSerial->ReadMultiLine(line);
}

void Maximet::WriteOutfreq(bool high)
{
    if (high)
    {
        WriteConfig("OUTFREQ", "1HZ");
    }
    else
    {
        WriteConfig("OUTFREQ", "1/MIN");
    }
    ReadOutfreq();
}

void Maximet::WriteReport(const char *report)
{
    String columns(report);
    columns.replace(',', ' ');
    int checkPos = columns.indexOf(" CHECK");
    if (checkPos >= 0)
    {
        columns.setlength(checkPos);
    }
    WriteConfig("REPORT", columns);
    ReadReport();
}

void Maximet::WriteUserinf(const char *userinf)
{
    WriteConfig("USERINF", userinf);
    ReadUserinf();
}

// compass declination
void Maximet::WriteCompassdecl(float compassdecl)
{
    WriteConfig("COMPASSDECL", String(compassdecl, 1));
    ReadCompassdecl();
};

// height above sea level
void Maximet::WriteHasl(float hasl)
{
    WriteConfig("HASL", String(hasl, 2));
    ReadHasl();
};

// height above/of station
void Maximet::WriteHastn(float hastn)
{
    WriteConfig("HASTN", String(hastn, 2));
    ReadHastn();
};

// latitude
void Maximet::WriteLat(float lat)
{
    WriteConfig("LAT", String(lat, 6));
    ReadLat();
};

// longitude
void Maximet::WriteLong(float lon)
{
    WriteConfig("LONG", String(lon, 6));
    ReadLong();
};

// setdef
void Maximet::WriteSetDef()
{
    EnterCommandLine();
    String cmd("SETDEF\r\n");
    mpSerial->Write(cmd);
    String line;
    mpSerial->ReadMultiLine(line);
    ESP_LOGW(tag, "Maximet set to defaults");
};

void Maximet::WriteFinish()
{
    ExitCommandLine();
};

static constexpr const char *FieldNames[] = {
    "USERINF", "TIME", "STATUS", "WINDSTAT",
    "SPEED", "GSPEED", "AVGSPEED", "CSPEED", "CGSPEED", "AVGCSPEED",
    "DIR", "GDIR", "AVGDIR", "CDIR", "CGDIR", "AVGCDIR",
    "TEMP", "SOLARRAD", "PASL", "PSTN", "RH", "AH",
    "COMPASSH", "XTILT", "YTILT", "ZORIENT",
    "GPSLOCATION", "GPSSTATUS", "GPSSPEED", "GPSHEADING",
    "CHECK"};

/*
static const Maximet::Field FIELDS_GMX501GPS[] = {Maximet::Field::USERINF, Maximet::Field::SPEED, Maximet::Field::GSPEED, Maximet::Field::AVGSPEED, Maximet::Field::DIR, Maximet::Field::GDIR, Maximet::Field::AVGDIR, Maximet::Field::CDIR, Maximet::Field::AVGCDIR,
                                                  Maximet::Field::COMPASSH, Maximet::Field::PASL, Maximet::Field::PSTN, Maximet::Field::RH, Maximet::Field::AH, Maximet::Field::TEMP, Maximet::Field::SOLARRAD, Maximet::Field::XTILT, Maximet::Field::YTILT, Maximet::Field::ZORIENT,
                                                  Maximet::Field::STATUS, Maximet::Field::WINDSTAT, Maximet::Field::GPSLOCATION, Maximet::Field::GPSSTATUS, Maximet::Field::TIME, Maximet::Field::CHECK}; // Field list MUST end with CHECK

static const Maximet::Field FIELDS_GMX200GPS[] = {Maximet::Field::USERINF, Maximet::Field::GPSLOCATION, Maximet::Field::GPSSPEED, Maximet::Field::GPSHEADING, Maximet::Field::CSPEED, Maximet::Field::CGSPEED, Maximet::Field::AVGCSPEED,
                                                  Maximet::Field::SPEED, Maximet::Field::GSPEED, Maximet::Field::AVGSPEED, Maximet::Field::DIR, Maximet::Field::GDIR, Maximet::Field::AVGDIR, Maximet::Field::CDIR, Maximet::Field::CGDIR, Maximet::Field::AVGCDIR, Maximet::Field::COMPASSH,
                                                  Maximet::Field::XTILT, Maximet::Field::YTILT, Maximet::Field::ZORIENT, Maximet::Field::STATUS, Maximet::Field::WINDSTAT, Maximet::Field::GPSSTATUS, Maximet::Field::TIME, Maximet::Field::CHECK}; // Field list MUST end with CHECK

static const Maximet::Field FIELDS_GMX501[] = {Maximet::Field::USERINF, Maximet::Field::SPEED, Maximet::Field::GSPEED, Maximet::Field::AVGSPEED, Maximet::Field::DIR, Maximet::Field::GDIR, Maximet::Field::AVGDIR, Maximet::Field::CDIR, Maximet::Field::AVGCDIR,
                                               Maximet::Field::COMPASSH, Maximet::Field::PASL, Maximet::Field::PSTN, Maximet::Field::RH, Maximet::Field::AH, Maximet::Field::TEMP, Maximet::Field::SOLARRAD, Maximet::Field::XTILT, Maximet::Field::YTILT, Maximet::Field::ZORIENT,
                                               Maximet::Field::STATUS, Maximet::Field::WINDSTAT, Maximet::Field::CHECK}; // Field list MUST end with CHECK
*/

const char *Maximet::GetModelReport(Model model)
{
    switch (model)
    {
    case Model::GMX501:
        return MAXIMET_REPORT_GMX501;
    case Model::GMX501GPS:
        return MAXIMET_REPORT_GMX501GPS;
    case Model::GMX501RAIN:
        return MAXIMET_REPORT_GMX501RAIN;
    case Model::GMX200GPS:
        return MAXIMET_REPORT_GMX200GPS;
    default:
        return MAXIMET_REPORT_GMX501;
    }
}

const char *Maximet::GetModelName(Model model)
{
    switch (model)
    {
    case Model::GMX200GPS:
        return "GMX200GPS";
    case Model::GMX501:
        return "GMX501";
    case Model::GMX501GPS:
        return "GMX501GPS";
    case Model::GMX501RAIN:
        return "GMX501RAIN";
    default:
        return "NONE";
    }
};

Maximet::Model Maximet::GetModel(String &modelName)
{
    if (modelName.equals(GetModelName(Model::GMX200GPS)))
        return Model::GMX200GPS;
    if (modelName.equals(GetModelName(Model::GMX501)))
        return Model::GMX501;
    if (modelName.equals(GetModelName(Model::GMX501GPS)))
        return Model::GMX501GPS;
    if (modelName.equals(GetModelName(Model::GMX501RAIN)))
        return Model::GMX501RAIN;
    return Model::NONE;
}
