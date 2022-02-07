#ifndef MAIN_Alarm_H_
#define MAIN_Alarm_H_

#include "esp_event.h"
#include "Config.h"
#include "DataQueue.h"
#include "driver/gpio.h"

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

    void GetAlarmInfo(String &info);

private:
    void Write();
    DataQueue &mrDataQueue;
    String msAlarmInfo;

    void AlarmTask();
    friend void fAlarmTask(void *pvParameter);

    Config &mrConfig;
    gpio_num_t mGpioBuzzer;

    int miTiltThreshold = 80;
};

#endif
