#ifndef MAIN_Alarm_H_
#define MAIN_Alarm_H_

#include "esp_event.h"
#include "Config.h"
#include "DataQueue.h"
#include "driver/gpio.h"
#include "MovingAverage.h"

#define TILT_AVG_SECONDS 3

class Alarm
{
public:
    enum Trigger
    {
        NONE = 0,
        TILT = 1,
        ORIENT = 2,
        UNPLUGGED = 4,
        GEOFENCE = 8
    };

public:
    Alarm(DataQueue &dataQueue, Config &config, gpio_num_t buzzer);

    void Start();

    void BuzzerOff();
    void BuzzerOn();

    bool IsAlarm() { return mbAlarm && !mbAlarmConfirmed; }; // trigger only once
    void ConfirmAlarm() { msAlarmInfo.reset(); mbAlarmConfirmed = true; };
    String GetAlarmInfo();

private:
    void Write();
    DataQueue &mrDataQueue;
    String msAlarmInfo;

    void AlarmTask();
    friend void fAlarmTask(void *pvParameter);

    Config &mrConfig;
    gpio_num_t mGpioBuzzer;

    int miTiltThreshold = 80;
    SimpleMovingAverage<TILT_AVG_SECONDS> movAvgTiltXmm;
    SimpleMovingAverage<TILT_AVG_SECONDS> movAvgTiltYmm;

    bool mbAlarm = false;
    bool mbAlarmConfirmed = false;
};

#endif
