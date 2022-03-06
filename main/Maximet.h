#ifndef MAIN_Maximet_H_
#define MAIN_Maximet_H_

#include "esp_event.h"
#include "EspString.h"
#include "DataQueue.h"
#include "Serial.h"

/*static const char *const FIELDS_GMX501GPS[] = {"USERINF", "SPEED", "GSPEED", "AVGSPEED", "DIR", "GDIR", "AVGDIR", "CDIR", "AVGCDIR", "COMPASSH",
                                               "PASL", "PSTN", "RH", "AH", "TEMP", "SOLARRAD", "XTILT", "YTILT", "ZORIENT", "STATUS", "WINDSTAT",
                                               "GPSLOCATION", "GPSSTATUS", "TIME", "CHECK"}; */

#define SERIAL_BUFFER_SIZE (2048)
#define SERIAL_BAUD_RATE (19200)

static const char MAXIMET_REPORT_GMX200GPS[] = "USERINF,GPSLOCATION,GPSSPEED,GPSHEADING,CSPEED,CGSPEED,AVGCSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,CGDIR,AVGCDIR,COMPASSH,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,GPSSTATUS,TIME,CHECK";
static const char MAXIMET_REPORT_GMX501GPS[] = "USERINF,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,GPSLOCATION,GPSSTATUS,TIME,CHECK";
static const char MAXIMET_REPORT_GMX501[] = "USERINF,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,ZORIENT,STATUS,WINDSTAT,CHECK";

// Note: MAXIMET_REPORT_GMX501RAIN REFLECTS STATUS-QUO AND NOT PREFERRED NEW CONFIG
static const char MAXIMET_REPORT_GMX501RAIN[] = "NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,PRECIPT,PRECIPI,PRECIPS,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK";


class Maximet
{
public:
    enum Model
    {
        NONE = 0,
        GMX200GPS = 2001,
        GMX501 = 5010,
        GMX501GPS = 5011,
        GMX501RAIN = 5012,
    };

    static const char *GetModelReport(Model model);
    static const char *GetModelName(Model model);
    static Model GetModel(String &modelName);
    // provides comma separated fields list of targeted configuration 
    static void GetReportString(String &report, Model model);
    static const char* GetReportString(Model model);


    class Config
    {
    public:
        Model model;
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

    // stop the task (e.g. before OTA update, or before writing Maximet configuration)
    void Stop();

    // weatherbuoy electronics can be used to simulator a Gill Maximet Weatherstation via its RS232 serial interface. good for testing other weatherbuoys
    void StartSimulator(Model maximetModel, int gpioRX, int gpioTX, bool alternateUart = false);

    unsigned int SolarRadiation() { return muiSolarradiation; };

    Config &GetConfig() { return mMaximetConfig; };


public:    
    // maximet configuration writes; 
    // Write* methods automatically invoke Maximet commandline mode

    // checks whether Maximet task is ready to write
    bool IsReadyToWrite();

    // compass declination
    void WriteCompassdecl(float compassdecl);

    // height above sea level
    void WriteHasl(float hasl);

    // set height above/of station
    void WriteHastn(float hastn);

    // set stationary latitude
    // automatically provided if GPS is available
    void WriteLat(float lat);

    // set stationary longitude
    // automatically provided if GPS is available
    void WriteLong(float lon);

    // configure Maximet long average interval -
    // default is 10 minutes (10 times short interval of default 60 seconds)
    void WriteAvgLong(unsigned short avglong);

    // reconfigure Maximet output frequency
    // high = true: 1 output per second
    // high = false: 1 output per minute
    void WriteOutfreq(bool high);

    // set fields maximet should send e.g. "USERINF,SPEED,GSPEED,AVGSPEED,DIR" 
    // sends "REPORT USERINF SPEED .... " (no comma!) to maximet
    // automatically removes the last field ",CHECK" if present
    void WriteReport(const char *report);
    
    // set user info string,  e.g. GMX501GPS
    void WriteUserinf(const char *userinf);

    // call after finished with Write* commands
    // exits configuration mode
    void WriteFinish();


private:
    // main loop run by the task
    void MaximetTask();
    friend void fMaximetTask(void *pvParameter);
    bool MaximetConfig();

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

    // read configured user info alias USERINF: contains maximet model. e.g. "GMX501" when properly configured for weatherbuoy
    bool ReadUserinf();

    // read list of available sensors 
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


    Model mMaximetModel;
    Serial *mpSerial;
    int mgpioRX;
    int mgpioTX;
    unsigned int muiUartNo;

    DataQueue &mrDataQueue;
    Config mMaximetConfig;

    bool mbRun = true;
    bool mbStopped = true;
    bool mbCommandline = false;

    // cached data
    unsigned int muiSolarradiation = 999;
};

#endif
