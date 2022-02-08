#ifndef MAIN_Maximet_H_
#define MAIN_Maximet_H_

#include "esp_event.h"
#include "EspString.h"
#include "DataQueue.h"
#include "Serial.h"

class Maximet {
public:
    enum Model {
        NONE      = 0,
        GMX200GPS = 2001,
        GMX501    = 5010,
        GMX501GPS = 5011
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

    unsigned int GetOutfreq() { return muiOutputIntervalSec; };
    unsigned int GetAvgLong() { return muiAvgLong; };
    String& GetUserinf() { return msUserinfo; };
    String& GetReport() { return msReport; };

private:
    //main loop run by the task
    void MaximetTask();
    friend void fMaximetTask(void *pvParameter);
    void MaximetConfig();

    void SendLine(const char* text);
    void SendLine(String &line);


    // enters Maximet commmandline mode with *\r\n and allows to send config; 
    // calls "exit" automatically at the end to continue data sending
    bool EnterCommandLine();
    void ExitCommandLine();
    bool ReadConfig(String &value, const char *sConfig);
    bool WriteConfig(const char *sConfig, const String &value);
    bool ReadUserinf();
    bool ReadReport();
    bool ReadOutfreq();
    bool ReadAvgLong();

    // configure Maximet long average interval - 
    // default is 10 minutes (10 times short interval of default 60 seconds)
    void WriteAvgLong(unsigned short avglong);

    // reconfigure Maximet output frequency
    // high = true: 1 output per second
    // high = false: 1 output per minute
    void WriteOutfreq(bool high);  
    // set columns maximet should send e.g. "USERINF,SPEED,GSPEED,AVGSPEED,DIR" --- this will send "REPORT USERINF SPEED .... " (no comma!) to maximet
    void WriteReport(const char* report);
    // set
    void WriteUserinf(const char* userinf);

    Model mMaximetModel;
    Serial *mpSerial;
    int mgpioRX;
    int mgpioTX;
    unsigned int muiUartNo;

    DataQueue &mrDataQueue;
    
    bool mbRun = true;
    bool mbCommandline = false;
    String msReport;
    String msUserinfo;
    unsigned int muiAvgLong = 0;
    unsigned int muiOutputIntervalSec = 0;

    unsigned int muiSolarradiation = 999;

};

#endif 


