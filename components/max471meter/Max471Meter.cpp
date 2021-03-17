#include "Max471Meter.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_system.h"
#include "esp_log.h"

    //example GPIO34 if ADC1, GPIO14 if ADC2

    // ADC1 = 32, 33, *34*, 36, 39
    // ADC2 = 4, (dont 12), 13, *14*, 15, 25, (26), (27)

    // GPIO32 = ADC1_CH4
    // GPIO33 = ADC1_CH5
    // GPIO34 = ADC1_CH6
    // GPIO35 = ADC1_CH7
    // GPIO36 = ADC1_CH0
    // GPIO39 = ADC1_CH3


    // ADC2 CANNOT BE USED WHEN WIFI IS ACTIVATED!!
    // Since the ADC2 is shared with the WIFI module, which has higher priority, 
    // reading operation of adc2_get_raw() will fail between esp_wifi_start() and esp_wifi_stop(). 
    // Use the return code to see whether the reading is successful.

static char tag[] = "Max471Meter";

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate

ADC::ADC(gpio_num_t gpio) {
    mpAdcChars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_12Bit, DEFAULT_VREF, mpAdcChars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(tag, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(tag, "Characterized using eFuse Vref");
    } else {
        ESP_LOGI(tag, "Characterized using Default Vref");
    }

    mChannel = GpioToChannel(gpio);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(mChannel, ADC_ATTEN_DB_11);
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

unsigned int ADC::Measure() {
    //int val = adc1_get_raw(mChannel);
    //esp_adc_cal_raw_to_voltage(val);

    uint32_t adc_reading = 0;
    //Multisampling
    static int samples = 32;
    for (int i = 0; i < samples; i++) {
        adc_reading += adc1_get_raw(mChannel);
    }
    adc_reading /= samples;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, mpAdcChars);
    ESP_LOGI(tag, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}


ADC::~ADC() {
    free(mpAdcChars);
};


Max471Meter::Max471Meter() : mVoltage{(gpio_num_t)CONFIG_MAX471METER_GPIO_VOLTAGE}, mCurrent{(gpio_num_t)CONFIG_MAX471METER_GPIO_CURRENT} {

}

Max471Meter::~Max471Meter() {

}