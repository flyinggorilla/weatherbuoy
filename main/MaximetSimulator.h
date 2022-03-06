#ifndef MAIN_MaximetSimulator_H_
#define MAIN_MaximetSimulator_H_

#include "EspString.h"
#include "DataQueue.h"
#include "Serial.h"
#include "Maximet.h"


class MaximetSimulator
{

public:
    MaximetSimulator();
    virtual ~MaximetSimulator();

    // start the task
    void Start(Maximet::Model maximetModel, int gpioRX, int gpioTX, bool alternateUart = false);

    void Stop();

    void SetTemperature(float temperature);

private:
    // main loop run by the task
    void MaximetSimulatorTask();
    friend void fMaximetSimulatorTask(void *pvParameter);

    void SendDataPoint();
    void SendLine(const char *text);
    void SendLine(String &line);
    bool CommandParse(const char* command);
    bool CommandResponse(const String &responseValue);
    bool CommandResponse(float &inoutValue, const char* format = "%06.2f");
    bool CommandResponse(int &inoutValue, const char* format = "%03i");


    String msInputLine;
    String msCommand;
    String msRequestValue;
    int miAvgLong = 10;
    int miOutfreq = 1;
    bool mbEcho = false;
    float mfCompassdecl = 4.3;
    float mfLatitude = 48.337402;
    float mfLongitude = 14.307054;
    float mfHasl = 425;
    float mfHastn = 2.8;
    float mfTemp = 0;

    Maximet::Model mMaximetModel;
    Serial *mpSerial;
    int mgpioRX;
    int mgpioTX;
    unsigned int muiUartNo;

    bool mbRun = true;
    bool mbStopped = true;
    bool mbCommandline = false;

};

#endif
