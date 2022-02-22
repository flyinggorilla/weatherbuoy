#ifndef MAIN_Maximet_H_
#define MAIN_Maximet_H_

#include "esp_event.h"
#include "EspString.h"
#include "DataQueue.h"
#include "Serial.h"

/*static const char *const FIELDS_GMX501GPS[] = {"USERINF", "SPEED", "GSPEED", "AVGSPEED", "DIR", "GDIR", "AVGDIR", "CDIR", "AVGCDIR", "COMPASSH",
                                               "PASL", "PSTN", "RH", "AH", "TEMP", "SOLARRAD", "XTILT", "YTILT", "ZORIENT", "STATUS", "WINDSTAT",
                                               "GPSLOCATION", "GPSSTATUS", "TIME", "CHECK"}; */

class Maximet
{
public:
    enum class Model
    {
        NONE = 0,
        GMX200GPS = 2001,
        GMX501 = 5010,
        GMX501GPS = 5011,
        GMX501RAIN = 5012,
        INVALID = -1
    };


    enum class Field
    {
        USERINF,
        TIME,
        STATUS,
        WINDSTAT,
        SPEED,
        GSPEED,
        AVGSPEED,
        CSPEED,
        CGSPEED,
        AVGCSPEED,
        DIR,
        GDIR,
        AVGDIR,
        CDIR,
        CGDIR,
        AVGCDIR,
        TEMP,
        SOLARRAD,
        PASL,
        PSTN,
        RH,
        AH,
        COMPASSH,
        XTILT,
        YTILT,
        ZORIENT,
        GPSLOCATION,
        GPSSTATUS,
        GPSSPEED,
        GPSHEADING,
        CHECK // check MUST be last
    };

    static const char *FieldNames[];
    static const Field FIELDS_GMX501GPS[];
    static const Field FIELDS_GMX200GPS[];
    static const Field FIELDS_GMX501[];
    static const Field GetFields(Model model);
    static const char *GetModelName(Model model);
    static Model GetModel(String &modelName);

    class Config
    {
    public:
        String sReport;
        String sUserinfo;
        String sSensor;
        String sSerial;
        String sSWVer;
        int iAvgLong = 0;
        int iOutputIntervalSec = 0;
        float fCompassdecl = 0;
        float fHastn = 0;
        float fHasl = 0;
        float fLat = 0;
        float fLong = 0;
    };

public:
    Maximet(DataQueue &dataQueue);
    virtual ~Maximet();

    // start the task
    void Start(int gpioRX, int gpioTX, bool alternateUart = false);

    // stop the task (e.g. before OTA update)
    void Stop() { mbRun = false; };

    // weatherbuoy electronics can be used to simulator a Gill Maximet Weatherstation via its RS232 serial interface. good for testing other weatherbuoys
    void SimulatorStart(Model maximetModel);

    unsigned int SolarRadiation() { return muiSolarradiation; };

    void SimulatorDataPoint(float temperature, double longitude, double latitude);

    Config &GetConfig() { return mMaximetConfig; };

    static const char *GetFieldName(Field field) { return FieldNames[(int)field]; };

    // generates comma separated fields list; 
    // set check=true if list should end with "CHECK"
    // note: when configuring maximet, the report string must omit the CHECK field
    void GetReportString(String &report, Model model, bool check);

private:
    // main loop run by the task
    void MaximetTask();
    friend void fMaximetTask(void *pvParameter);
    void MaximetConfig();

    void SendLine(const char *text);
    void SendLine(String &line);

    // enters Maximet commmandline mode with *\r\n and allows to send config;
    // calls "exit" automatically at the end to continue data sending
    bool EnterCommandLine();
    void ExitCommandLine();
    bool ReadConfig(String &value, const char *sConfig);
    bool WriteConfig(const char *sConfig, const String &value);
    bool ReadConfigFloat(float &val, const char *cmd);
    bool ReadConfigString(String &val, const char *cmd);
    bool ReadConfigInt(int &val, const char *cmd);

    bool ReadUserinf();
    bool ReadSensor();
    bool ReadReport();
    bool ReadOutfreq();
    bool ReadAvgLong();
    bool ReadSerial();
    bool ReadSWVer();
    bool ReadHasl();
    bool ReadHastn();
    bool ReadLat();
    bool ReadLong();
    bool ReadCompassdecl();

    // compass declination
    void WriteCompassdecl(float compassdecl);

    // height above sea level
    void WriteHasl(float hasl);

    // height above/of station
    void WriteHastn(float hastn);

    // set latitude if not GPS
    void WriteLat(float lat);
    // set longitude if not GPS
    void WriteLong(float lon);

    // configure Maximet long average interval -
    // default is 10 minutes (10 times short interval of default 60 seconds)
    void WriteAvgLong(unsigned short avglong);

    // reconfigure Maximet output frequency
    // high = true: 1 output per second
    // high = false: 1 output per minute
    void WriteOutfreq(bool high);
    // set columns maximet should send e.g. "USERINF,SPEED,GSPEED,AVGSPEED,DIR" --- this will send "REPORT USERINF SPEED .... " (no comma!) to maximet
    void WriteReport(const char *report);
    // set
    void WriteUserinf(const char *userinf);

    Model mMaximetModel;
    Serial *mpSerial;
    int mgpioRX;
    int mgpioTX;
    unsigned int muiUartNo;

    DataQueue &mrDataQueue;
    Config mMaximetConfig;

    bool mbRun = true;
    bool mbCommandline = false;

    // cached data
    unsigned int muiSolarradiation = 999;
};

#endif
