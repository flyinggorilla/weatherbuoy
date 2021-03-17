#ifndef _MAX471METER_H_
#define _MAX471METER_H_

#include "esp_system.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

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
        unsigned int Measure(); // returns mV

    private:
        esp_adc_cal_characteristics_t *mpAdcChars = nullptr;
        adc1_channel_t mChannel;
        adc1_channel_t GpioToChannel(gpio_num_t gpio);
};

class Max471Meter {
    public:
        Max471Meter();
        virtual ~Max471Meter();

        unsigned int Voltage() { return mVoltage.Measure(); };
        unsigned int Current() { return mCurrent.Measure(); };

    private:
        ADC mVoltage;
        ADC mCurrent;
};

#endif