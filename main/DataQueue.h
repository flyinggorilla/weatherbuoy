#ifndef MAIN_DATAQUEUE_H_
#define MAIN_DATAQUEUE_H_

#include "esp_event.h"
#include "EspString.h"

class Data {
    public:

        Data() {
            init();
        }

        void init() {
            uptime = esp_timer_get_time()/1000;
            speed = 0; 
            gspeed = 0; 
            avgspeed = 0; 
            dir = 0;
            gdir = 0; 
            avgdir = 0; 
            compassh = 0;
            cdir = 0; 
            cgdir = 0; // CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
            avgcdir = 0; 
            temp = 0;
            pasl = 0; 
            pstn = 0; 
            rh = 0;
            ah = 0;
            solarrad = 0;
            xtilt = 0;
            ytilt = 0;
            status[0] = 0;
            windstat[0] = 0; 
            gpsstatus[0] = 0;
            lat = 0;
            lon = 0;
            cspeed = 0;
            cgspeed = 0;
            avgcspeed = 0;
            gpsspeed = 0;
            gpsheading = 0;

        }

        int uptime;

        float speed; 
        float gpsspeed; // only avail if GPS
        float gspeed; 
        float avgspeed; 
        float cspeed; // only avail if GPS
        float cgspeed; // only avail if GPS
        float avgcspeed; // only avail if GPS

        int dir;
        int gdir; 
        int avgdir; 

        int compassh;
        int gpsheading; // only avila if GPS

        int cdir; 
        int cgdir; // avail if GPS .... otherwise it is CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
        int avgcdir; 

        float temp;
        float pasl; 
        float pstn; 
        float rh;
        float ah;
        int solarrad;

        float xtilt;
        float ytilt;

        double lat; // only avail if GPS
        double lon; // only avail if GPS

        static const int statuslen = 5;
        char status[statuslen];
        char windstat[statuslen]; 
        char gpsstatus[statuslen]; 
};

class DataQueue {
public:
	DataQueue();
	virtual ~DataQueue();

    // read data from queue; 
    // returns false if no data available
    bool GetData(Data &data);

    // read data from latest data queue ; 
    // returns false if no data available
    bool GetLatestData(Data &data, unsigned int timeoutSeconds);

    // peeks into queue, but doesnt return pointer to not accidentally delete data
    bool WaitForData(unsigned int timeoutSeconds);

    // return the number of messages in the queue
    int GetQueueLength();

    // returns true when the queue is full
    bool IsFull();
    
    // adds a data element to the queue
    bool PutData(Data &data);

    // adds data to a 1-sized queue, contains always the latest data
    bool PutLatestData(Data &data);


private:
   
    QueueHandle_t mxDataQueue;
    QueueHandle_t mxDataLatest;

    //bool mbRun = true;

};

#endif 


