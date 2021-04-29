#ifndef MAIN_READMAXIMET_H_
#define MAIN_READMAXIMET_H_

#include "esp_event.h"
#include "EspString.h"
#include "Config.h"

class Data {
    public:
        String msMaximet; // DEBUG ONLY ############################

        int timestamp;

        float speed = 0; 
        float gspeed = 0; 
        float avgspeed = 0; 

        int dir = 0;
        int gdir = 0; 
        int avgdir = 0; 

        int compassh = 0;

        int cdir = 0; 
        int cgdir = 0; // CALCULATED!!!  (maximet["GDIR"]+maximet["COMPASSH"]) % 360;
        int avgcdir = 0; 

        float temp = 0;
        float pasl = 0; 
        float pstn = 0; 
        float rh = 0;
        float ah = 0;
        int solarrad = 0;

        float xtilt = 0;
        float ytilt = 0;

        String status;
        String windstat; 
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
    // returns nullptr if no data available
    Data* GetData();

    // peeks into queue, but doesnt return pointer to not accidentally delete data
    bool WaitForData(unsigned int timeoutSeconds);

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


