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
        //unsigned int measurement = mCurrent.Measure(5);
        unsigned int measurement = mCurrent.AdaptiveMeasure(5);
        taskENTER_CRITICAL(&mCriticalSection);
        if (muiCurrentCount > 60*15) { // reset every 15min
            muiCurrentCount = 0;
            muiCurrentSum = 0;
            muiCurrentMin = 0;
            muiCurrentMax = 0;
        }

        muiCurrentSum += measurement;
        if (!muiCurrentMin || muiCurrentMin > measurement) {
            muiCurrentMin = measurement;
        }
        if (muiCurrentMax < measurement) {
            muiCurrentMax = measurement;
        }
        muiCurrentCount++;
        taskEXIT_CRITICAL(&mCriticalSection);
        ESP_LOGD(tag, "Current %d mA", measurement);
    }
}

ADC::ADC(gpio_num_t gpio) {
    mpAdcCharsNormal = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    mpAdcCharsSensitive = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    adc_bits_width_t bits = ADC_WIDTH_BIT_12;
    adc_atten_t attenuation = ADC_ATTEN_DB_11;

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, attenuation, bits, DEFAULT_VREF, mpAdcCharsNormal);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(tag, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        //mpAdcChars->coeff_b = 0; // overriding coeff-B from 142mV or 75mV (depending on attenuation) to 0 for correct reading.
        ESP_LOGI(tag, "Characterized using eFuse Vref %d mV, low %s, high %s, coeffA %d, coeffB %d", mpAdcCharsNormal->vref, mpAdcCharsNormal->low_curve ? "yes" : "-", mpAdcCharsNormal->high_curve ? "yes" : "-", mpAdcCharsNormal->coeff_a, mpAdcCharsNormal->coeff_b);
    } else {
        ESP_LOGI(tag, "Characterized using Default Vref");
    }
    val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, bits, DEFAULT_VREF, mpAdcCharsSensitive);
    ESP_LOGI(tag, "Sensitive mode: Characterized using eFuse Vref %d mV, low %s, high %s, coeffA %d, coeffB %d", mpAdcCharsSensitive->vref, mpAdcCharsSensitive->low_curve ? "yes" : "-", mpAdcCharsSensitive->high_curve ? "yes" : "-", mpAdcCharsSensitive->coeff_a, mpAdcCharsSensitive->coeff_b);

    gpio_set_pull_mode(gpio, GPIO_FLOATING);
    gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
    gpio_set_direction(gpio, GPIO_MODE_INPUT);

    mChannel = GpioToChannel(gpio);
    adc1_config_width(bits);
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
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, mpAdcCharsNormal);
    ESP_LOGD(tag, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}

unsigned int ADC::AdaptiveMeasure(unsigned int samples) {

    bool sensitiveMode = false;
    if (adc1_get_raw(mChannel) < 800) {
        sensitiveMode = true;
    }

    if (sensitiveMode) {
        adc1_config_channel_atten(mChannel, ADC_ATTEN_DB_11);
    }

    uint32_t adc_reading = 0;
    for (int i = 0; i < samples; i++) {
        adc_reading += adc1_get_raw(mChannel);
    }
    //Multisampling
    adc_reading /= samples;

    uint32_t voltage;

    //Convert adc_reading to voltage in mV
    if (sensitiveMode) {
        adc1_config_channel_atten(mChannel, ADC_ATTEN_DB_11);
        voltage = esp_adc_cal_raw_to_voltage(adc_reading, mpAdcCharsSensitive);
        ESP_LOGD(tag, "Sensitive mode - Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    } else {
        voltage = esp_adc_cal_raw_to_voltage(adc_reading, mpAdcCharsNormal);
        ESP_LOGD(tag, "Normal mode - Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    }

    return voltage;
}



ADC::~ADC() {
    free(mpAdcCharsNormal);
};

//    ADC_ATTEN_DB_0   = No input attenumation, ADC can measure up to approx. 800 mV. 
//    ADC_ATTEN_DB_2_5 = The input voltage of ADC will be attenuated, extending the range of measurement to up to approx. 1100 mV. 
//    ADC_ATTEN_DB_6   = The input voltage of ADC will be attenuated, extending the range of measurement to up to  approx. 1350 mV. 
//    ADC_ATTEN_DB_11  = The input voltage of ADC will be attenuated, extending the range of measurement to up to  approx. 2600 mV. 
Max471Meter::Max471Meter(int gpioPinVoltage, int gpioPinCurrent) : mVoltage{(gpio_num_t)gpioPinVoltage}, mCurrent{(gpio_num_t)gpioPinCurrent} {
	xTaskCreate(&fMeterTask, "Max471Meter", 2048, this, ESP_TASK_PRIO_MIN, NULL); 
}

Max471Meter::~Max471Meter() {

}


unsigned int Max471Meter::Voltage() { 
    return mVoltage.Measure() * 5; 
}; // take default number of samples

unsigned int Max471Meter::Current() { 
    taskENTER_CRITICAL(&mCriticalSection);
    if (muiCurrentCount) {
        muiCurrentAvg = muiCurrentSum / muiCurrentCount;
    }
    taskEXIT_CRITICAL(&mCriticalSection);

    return muiCurrentAvg; 
}; // take fewer samples per measurement, but measure more often
