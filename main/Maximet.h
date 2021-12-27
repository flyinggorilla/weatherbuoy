#ifndef MAIN_Maximet_H_
#define MAIN_Maximet_H_

#include "esp_event.h"
#include "EspString.h"
#include "Display.h"
#include "DataQueue.h"
#include "Serial.h"

enum MaximetModel {
    gmx501,
    gmx200gps
};

class Maximet {
public:
	Maximet(DataQueue &dataQueue);
	virtual ~Maximet();

    // start the task
    void Start(int gpioRX, int gpioTX, bool alternateUart = false);

    // stop the task (e.g. before OTA update)
    void Stop() { mbRun = false; }; 

    // weatherbuoy electronics can be used to simulator a Gill Maximet Weatherstation via its RS232 serial interface. good for testing other weatherbuoys
    void SimulatorStart(MaximetModel maximetModel);

    unsigned int SolarRadiation() { return muiSolarradiation; };

    void SetDisplay(Display *pDisplay) { mpDisplay = pDisplay; };

    void SimulatorDataPoint(float temperature, double longitude, double latitude);

private:
    //main loop run by the task
    void MaximetTask();
    friend void fMaximetTask(void *pvParameter);

    void SendLine(const char* text);
    void SendLine(String &line);

    MaximetModel mMaximetModel;
    Serial *mpSerial;
    int mgpioRX;
    int mgpioTX;
    unsigned int muiUartNo;

    DataQueue &mrDataQueue;
    
    bool mbRun = true;

    unsigned int muiSolarradiation = 999;

    Display *mpDisplay = nullptr;
};

#endif 


