#ifndef MAIN_READMAXIMET_H_
#define MAIN_READMAXIMET_H_

#include "esp_event.h"
#include "EspString.h"
#include "Config.h"
#include "Data.h"

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


