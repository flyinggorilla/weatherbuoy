#include "Max471Meter.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_task.h"
#include "freertos/task.h"

    // ADC1 = 32, 33, *34*, 36, 39
    // GPIO32 = ADC1_CH4
    // GPIO33 = ADC1_CH5
    // GPIO34 = ADC1_CH6
    // GPIO35 = ADC1_CH7
    // GPIO36 = ADC1_CH0
    // GPIO39 = ADC1_CH3


    // ADC2 CANNOT BE USED WHEN WIFI IS ACTIVATED!!
    // ADC2 = 4, (dont 12), 13, *14*, 15, 25, (26), (27)
    // Since the ADC2 is shared with the WIFI module, which has higher priority, 
    // reading operation of adc2_get_raw() will fail between esp_wifi_start() and esp_wifi_stop(). 
    // Use the return code to see whether the reading is successful.

static char tag[] = "Max471Meter";

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate


void fMeterTask(void *pvParameter) {
	((Max471Meter*) pvParameter)->Max471MeterTask();
	vTaskDelete(NULL);
}

void Max471Meter::Max471MeterTask() {
    while(true) {
        vTaskDelay(1*1000/portTICK_PERIOD_MS);
        unsigned int measurement = mCurrent.Measure(5);
        taskENTER_CRITICAL(&mCriticalSection);
        if (muiCurrentCount > 3600) { // prevent overflow
            muiCurrentCount = 0;
            muiCurrentSum = 0;
        }
        muiCurrentSum += measurement;
        muiCurrentCount++;
        taskEXIT_CRITICAL(&mCriticalSection);
    }
}

ADC::ADC(gpio_num_t gpio, adc_atten_t attenuation) {
    mpAdcChars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_12Bit, DEFAULT_VREF, mpAdcChars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(tag, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(tag, "Characterized using eFuse Vref");
    } else {
        ESP_LOGI(tag, "Characterized using Default Vref");
    }

    gpio_set_pull_mode(gpio, GPIO_FLOATING);
    gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
    gpio_set_direction(gpio, GPIO_MODE_INPUT);

    mChannel = GpioToChannel(gpio);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(mChannel, attenuation);
};

adc1_channel_t ADC::GpioToChannel(gpio_num_t gpio) {
    // GPIO32 = ADC1_CH4
    // GPIO33 = ADC1_CH5
    // GPIO34 = ADC1_CH6
    // GPIO35 = ADC1_CH7
    // GPIO36 = ADC1_CH0
    // GPIO39 = ADC1_CH3

    switch (gpio) {
        case GPIO_NUM_32:
            return ADC1_CHANNEL_4; 
        case GPIO_NUM_33:
            return ADC1_CHANNEL_5;
        case GPIO_NUM_34:
            return ADC1_CHANNEL_6;
        case GPIO_NUM_35:
            return ADC1_CHANNEL_7;
        case GPIO_NUM_36:
            return ADC1_CHANNEL_0;
        case GPIO_NUM_39:
            return ADC1_CHANNEL_3;
        default:
            ESP_LOGE(tag, "Invalid GPIO for ADC1");
            return ADC1_CHANNEL_MAX;
    }
}

unsigned int ADC::Measure(unsigned int samples) {
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < samples; i++) {
        adc_reading += adc1_get_raw(mChannel);
    }
    adc_reading /= samples;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, mpAdcChars);
    ESP_LOGD(tag, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}


ADC::~ADC() {
    free(mpAdcChars);
};

//    ADC_ATTEN_DB_0   = No input attenumation, ADC can measure up to approx. 800 mV. 
//    ADC_ATTEN_DB_2_5 = The input voltage of ADC will be attenuated, extending the range of measurement to up to approx. 1100 mV. 
//    ADC_ATTEN_DB_6   = The input voltage of ADC will be attenuated, extending the range of measurement to up to  approx. 1350 mV. 
//    ADC_ATTEN_DB_11  = The input voltage of ADC will be attenuated, extending the range of measurement to up to  approx. 2600 mV. 
Max471Meter::Max471Meter(int gpioPinVoltage, int gpioPinCurrent) : mVoltage{(gpio_num_t)gpioPinVoltage, ADC_ATTEN_DB_11}, mCurrent{(gpio_num_t)gpioPinCurrent, ADC_ATTEN_DB_2_5} {

}

Max471Meter::~Max471Meter() {

}


unsigned int Max471Meter::Voltage() { 
    return mVoltage.Measure() * 5; 
}; // take default number of samples

unsigned int Max471Meter::Current() { 
    unsigned int avgCurrent;

    taskENTER_CRITICAL(&mCriticalSection);
    avgCurrent = muiCurrentSum / muiCurrentCount;
    muiCurrentSum = 0;
    muiCurrentCount = 0;
    taskEXIT_CRITICAL(&mCriticalSection);

    return avgCurrent; 
}; // take fewer samples per measurement, but measure more often
