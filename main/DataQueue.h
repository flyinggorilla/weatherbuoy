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
    enum Event
    {
        NONE = 0,
        MAXIMET = 1,
        ALARM = 2
    };

public:
    Event event;
    int uptime;

    float speed; // required for NmeaDisplay
    // float gspeed;
    // float avgspeed;
    short dir; // required for NmeaDisplay
    // short gdir;
    // short avgdir;

    // wind
    float cspeed;    // only avail if GPS, or derived
    float cgspeed;   // only avail if GPS, or derived
    float avgcspeed; // only avail if GPS, or derived
    short compassh;
    short cdir;    // corrected through compass
    short cgdir;   // avail if GPS .... otherwise it is CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
    short avgcdir; //

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
        event = NONE;
        uptime = esp_timer_get_time() / 1000;
        speed = nanf();
        // gspeed = nanf();
        // avgspeed = nanf();
        dir = 0;
        // gdir = 0;
        // avgdir = 0;

        // wind
        cspeed = 0;
        cgspeed = 0;
        avgcspeed = 0;
        cdir = 0;
        cgdir = 0; // CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
        avgcdir = 0;
        compassh = 0;

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
    Data::Event WaitForData(unsigned int timeoutSeconds);

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

    // bool mbRun = true;
};

#endif
