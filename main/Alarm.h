#ifndef MAIN_Alarm_H_
#define MAIN_Alarm_H_

#include "esp_event.h"
#include "Config.h"
#include "DataQueue.h"
#include "driver/gpio.h"


class Alarm {
    public:
        Alarm(DataQueue &dataQueue, Config &config, gpio_num_t buzzer);

        void Start();
   
    private:
        void Write();
        DataQueue &mrDataQueue;
        
        void AlarmTask();
        friend void fAlarmTask(void *pvParameter);

        gpio_num_t mGpioBuzzer;
        Config &mrConfig;

};


#endif 


