#ifndef _MAX471METER_H_
#define _MAX471METER_H_

#include "esp_system.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_task.h"

    //example GPIO34 if ADC1, GPIO14 if ADC2

    // ADC1 = 32, 33, *34*, 36, 39
    // ADC2 = 4, (dont 12), 13, *14*, 15, 25, (26), (27)

    // ADC2 CANNOT BE USED WHEN WIFI IS ACTIVATED!!
    // Since the ADC2 is shared with the WIFI module, which has higher priority, 
    // reading operation of adc2_get_raw() will fail between esp_wifi_start() and esp_wifi_stop(). 
    // Use the return code to see whether the reading is successful.

// will ALWAYS USE UNIT 1!!
class ADC {
    public:
        ADC(gpio_num_t gpio);
        ~ADC();
        unsigned int Measure(unsigned int samples = 32); // returns mV

        // start with highest voltage range to 3V (DB_11) and if less than 800mV measured, use most sensitive range (DB_0)
        unsigned int AdaptiveMeasure(unsigned int samples = 16); // returns mV

    private:
        esp_adc_cal_characteristics_t *mpAdcCharsNormal = nullptr;
        esp_adc_cal_characteristics_t *mpAdcCharsSensitive = nullptr;
        adc1_channel_t mChannel;
        adc1_channel_t GpioToChannel(gpio_num_t gpio);
};

class Max471Meter {
    public:
        Max471Meter(int gpioPinVoltage, int gpioPinCurrent);
        virtual ~Max471Meter();

        unsigned int Voltage(); // [mV]
        unsigned int Current(); // [mA]
        unsigned int CurrentMin() { return muiCurrentMin; };
        unsigned int CurrentMax() { return muiCurrentMax; };


    private:
        ADC mVoltage;
        ADC mCurrent;

        void Max471MeterTask();
        friend void fMeterTask(void *pvParameter);
        unsigned int muiCurrentSum = 0;
        unsigned int muiCurrentCount = 0;
        unsigned int muiCurrentAvg = 0;
        unsigned int muiCurrentMin = 0;
        unsigned int muiCurrentMax = 0;
        portMUX_TYPE mCriticalSection = portMUX_INITIALIZER_UNLOCKED;
};

#endif