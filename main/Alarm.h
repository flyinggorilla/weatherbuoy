#ifndef MAIN_Alarm_H_
#define MAIN_Alarm_H_

#include "esp_event.h"
#include "Config.h"
#include "DataQueue.h"
#include "driver/gpio.h"


class Alarm {
    public:
        Alarm(gpio_num_t canTX, gpio_num_t canRX, DataQueue &dataQueue);

        void Start();
   
    private:
        void Write();
        DataQueue &mrDataQueue;
        
        void AlarmTask();
        friend void fAlarmTask(void *pvParameter);

};


#endif 


