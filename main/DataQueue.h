#ifndef MAIN_DATAQUEUE_H_
#define MAIN_DATAQUEUE_H_

#include "esp_event.h"
#include "EspString.h"
#include <math.h>

short nans();
unsigned char nanuc();
float nanf();
bool isnans(short n);
bool isnauc(unsigned char uc);

class Data
{

public:
    int uptime;

    float speed; // can be nanf(); required for NmeaDisplay
    // float gspeed;
    // float avgspeed;
    short dir; // can be nans(); required for NmeaDisplay
    // short gdir;
    // short avgdir;

    // wind
    float cspeed;    // only avail if GPS, or derived; can be nanf()
    float cgspeed;   // only avail if GPS, or derived; can be nanf()
    float avgcspeed; // only avail if GPS, or derived; can be nanf()
    short compassh; // can be nans()
    short cdir;    // can be nans(); corrected through compass
    short cgdir;   // can be nans() avail if GPS .... otherwise it is CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
    short avgcdir; // can be nans()

    float xavgcspeed; // TEST own implementation of WMO; can be nanf()
    short xavgcdir; // TEST own implementation of WMO; can be nanf()

    // weather
    float temp;
    float pasl;
    float pstn;
    float rh;
    float ah;
    short solarrad;

    // status
    short xtilt;
    short ytilt;
    short zorient;
    static const int statuslen = 5;
    char status[statuslen];
    char windstat[statuslen];

    // GPS data
    double lat;       // only avail if GPS
    double lon;       // only avail if GPS
    float gpsspeed;   // only avail if GPS
    short gpsheading; // only avila if GPS
    unsigned char gpsfix;
    unsigned char gpssat;
    time_t time; // only avail if GPS -- 2022-01-21T22:43:11.4

public:
    Data()
    {
        init();
    }

    void init()
    {
        uptime = esp_timer_get_time() / 1000;
        speed = nanf();
        // gspeed = nanf();
        // avgspeed = nanf();
        dir = nans();
        // gdir = 0;
        // avgdir = 0;

        // wind
        cspeed = nanf();
        cgspeed = nanf();
        avgcspeed = nanf();
        cdir = nans();
        cgdir = nans(); // CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
        avgcdir = nans();
        compassh = nans();
    
        xavgcspeed = nanf(); // TEST own implementation of WMO
        xavgcdir = nans(); // TEST own implementation of WMO

        // weather
        temp = 0;
        pasl = 0;
        pstn = 0;
        rh = 0;
        ah = 0;
        solarrad = 0;

        // status
        xtilt = 0;
        ytilt = 0;
        zorient = 0; // +1 upright, -1 upside down
        status[0] = 0;
        windstat[0] = 0;

        // GPS
        gpsfix = 0;
        gpssat = 0;
        lat = 0;
        lon = 0;
        gpsspeed = 0;
        gpsheading = 0;
        time = 0;
    }
};

class DataQueue
{
public:
    DataQueue();
    virtual ~DataQueue();

public: // data queue
    // adds a data element to the queue
    bool PutData(Data &data);

    // read data from queue;
    // returns false if no data available
    bool GetData(Data &data);

    // peeks into queue, but doesnt return pointer to not accidentally delete data
    bool WaitForData(unsigned int timeoutSeconds);

    // return the number of messages in the queue
    int GetQueueLength();

    // returns true when the queue is full
    bool IsFull();

public: // latest data
    // adds data to a 1-sized queue, contains always the latest data
    bool PutLatestData(Data &data);

    // read data from latest data queue ;
    // returns false if no data available
    bool GetLatestData(Data &data, unsigned int timeoutSeconds);

public: // alerting
    // adds data to a 1-sized queue, contains always the latest data
    bool PutAlarmData(Data &data);

    // read data from latest data queue ;
    // returns false if no data available
    bool GetAlarmData(Data &data, unsigned int timeoutSeconds);

private:
    QueueHandle_t mxDataQueue;
    QueueHandle_t mxDataLatest;
    QueueHandle_t mxDataAlarm;
};

#endif
