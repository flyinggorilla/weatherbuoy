#ifndef MAIN_READMAXIMET_H_
#define MAIN_READMAXIMET_H_

#include "esp_event.h"
#include "EspString.h"
#include "Config.h"

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
        }

        int uptime;

        float speed; 
        float gspeed; 
        float avgspeed; 

        int dir;
        int gdir; 
        int avgdir; 

        int compassh;

        int cdir; 
        int cgdir; // CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
        int avgcdir; 

        float temp;
        float pasl; 
        float pstn; 
        float rh;
        float ah;
        int solarrad;

        float xtilt;
        float ytilt;

        static const int statuslen = 5;
        char status[statuslen];
        char windstat[statuslen]; 
};

class ReadMaximet {
public:
	ReadMaximet(Config &config);
	virtual ~ReadMaximet();

    // start the task
    void Start(int gpioRX, int gpioTX);

    // stop the task (e.g. before OTA update)
    void Stop() { mbRun = false; }; 

    // read data from queue; 
    // returns false if no data available
    bool GetData(Data &data);

    // peeks into queue, but doesnt return pointer to not accidentally delete data
    bool WaitForData(unsigned int timeoutSeconds);

    // return the number of messages in the queue
    int GetQueueLength();

    unsigned int SolarRadiation() { return muiSolarradiation; };


private:
    //main loop run by the task
    void ReadMaximetTask();
    friend void fReadMaximetTask(void *pvParameter);

    Config &mrConfig;
    //SendData &mrSendData;
    int mgpioRX;
    int mgpioTX;
    
    QueueHandle_t mxDataQueue;
    bool mbRun = true;

    unsigned int muiSolarradiation = 999;
};

#endif 


