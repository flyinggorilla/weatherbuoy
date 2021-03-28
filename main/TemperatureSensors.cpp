#include "esp_log.h"
#include "TemperatureSensors.h"
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#define CONFIG_MAX_TEMPERATURE_SENSORS (2)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)

const static char tag[] = "TemperatureSensors";


TemperatureSensors::TemperatureSensors(Config &config) : mrConfig(config)  {
}

void TemperatureSensors::Init(int oneWireGpioNum) {

    bool bStoreNewConfig = false;

    // Create a 1-Wire bus, using the RMT timeslot driver
    mpOwb = owb_rmt_initialize(&mRmtDriverInfo, (gpio_num_t)oneWireGpioNum, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(mpOwb, true);  // enable CRC check for ROM code

    // Find all connected devices
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(mpOwb, &search_state, &found);
    while (found && (num_devices < CONFIG_MAX_TEMPERATURE_SENSORS))
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        ESP_LOGI(tag, "Temperature sensor #%d detected: %s", num_devices, rom_code_s);

        if ((mrConfig.msBoardTempSensorId != rom_code_s) && (mrConfig.msWaterTempSensorId != rom_code_s)) {
            if (!mrConfig.msWaterTempSensorId.length()) {
                mrConfig.msWaterTempSensorId = rom_code_s;
                ESP_LOGI(tag, "New temperature sensor %s detected and randomly assigned as water temperature sensor.", rom_code_s);
                bStoreNewConfig = true;
            } else if (!mrConfig.msBoardTempSensorId.length()) {
                mrConfig.msBoardTempSensorId = rom_code_s;
                ESP_LOGI(tag, "New temperature sensor %s detected and randomly assigned as board temperature sensor.", rom_code_s);
                bStoreNewConfig = true;
            }
        }

        if (mrConfig.msBoardTempSensorId == rom_code_s) {
            mpBoardSensor = ds18b20_malloc();  // heap allocation
            ds18b20_init(mpBoardSensor, mpOwb, search_state.rom_code); // associate with bus and device
            ds18b20_use_crc(mpBoardSensor, true);           // enable CRC check on all reads
            ds18b20_set_resolution(mpBoardSensor, DS18B20_RESOLUTION);
        } 
        if (mrConfig.msWaterTempSensorId == rom_code_s) {
            mpWaterSensor = ds18b20_malloc();  // heap allocation
            ds18b20_init(mpWaterSensor, mpOwb, search_state.rom_code); // associate with bus and device
            ds18b20_use_crc(mpWaterSensor, true);           // enable CRC check on all reads
            ds18b20_set_resolution(mpWaterSensor, DS18B20_RESOLUTION);
        } 
        ++num_devices;
        owb_search_next(mpOwb, &search_state, &found);
    }

    if (!mpBoardSensor) {
        ESP_LOGE(tag, "On-board temperature sensor %s not found.", mrConfig.msBoardTempSensorId.c_str());
    }

    if (!mpWaterSensor) {
        ESP_LOGW(tag, "Water temperature sensor %s not found.", mrConfig.msWaterTempSensorId.c_str());
    }

    if (!num_devices) {
        ESP_LOGW(tag, "No temperature sensor detected.");
    }

    if (bStoreNewConfig) {
        ESP_LOGW(tag, "Writing new sensor Ids to config: Board=%s, Water=%s", mrConfig.msBoardTempSensorId.c_str(), mrConfig.msWaterTempSensorId.c_str());
        mrConfig.Save();
    }

    Read();
    ESP_LOGI(tag, "Board temperature %.1f°C, Water temperature %.1f°C", BoardTemp(), WaterTemp());
}

void TemperatureSensors::Read() {

    ds18b20_convert_all(mpOwb);
    // In this application all devices use the same resolution,
    // so use the first device to determine the delay
    ds18b20_wait_for_conversion(mpBoardSensor);

    // Read the results immediately after conversion otherwise it may fail
    // (using printf before reading may take too long)
    DS18B20_ERROR err;
    err = ds18b20_read_temp(mpBoardSensor, &mfBoardTemp);
    if (err != DS18B20_OK) {
        mfBoardTemp = -99;
        ESP_LOGE(tag, "Error reading water temperature sensor %d", err);
    }

    if (mpWaterSensor) {
        err = ds18b20_read_temp(mpWaterSensor, &mfWaterTemp);
        if (err != DS18B20_OK) {
            mfWaterTemp = -99;
            ESP_LOGE(tag, "Error reading water temperature sensor %d", err);
        }
    }
    ESP_LOGD(tag, "Board temperature %.1f°C, Water temperature %.1f°C", BoardTemp(), WaterTemp());
}

TemperatureSensors::~TemperatureSensors() {
    // clean up dynamically allocated data
    ds18b20_free(&mpWaterSensor);
    ds18b20_free(&mpBoardSensor);
    owb_uninitialize(mpOwb);            
}
