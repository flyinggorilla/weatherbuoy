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

    // Create a 1-Wire bus, using the RMT timeslot driver
    mpOwb = owb_rmt_initialize(&mRmtDriverInfo, (gpio_num_t)oneWireGpioNum, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(mpOwb, true);  // enable CRC check for ROM code

    // Find all connected devices
    int num_devices = 0;
    OneWireBus_SearchState search_state = {};
    bool found = false;
    owb_search_first(mpOwb, &search_state, &found);
    OneWireBus_ROMCode device_rom_codes[CONFIG_MAX_TEMPERATURE_SENSORS] = {};
    char romCodes[CONFIG_MAX_TEMPERATURE_SENSORS][17] = {};
    while (found && (num_devices < CONFIG_MAX_TEMPERATURE_SENSORS))
    {
        owb_string_from_rom_code(search_state.rom_code, romCodes[num_devices], sizeof(romCodes[0]));
        device_rom_codes[num_devices] = search_state.rom_code;
        ESP_LOGI(tag, "Temperature sensor #%d detected: %s", num_devices, romCodes[num_devices]);
        ++num_devices;
        owb_search_next(mpOwb, &search_state, &found);
    }

    int boardTempSensorNum = -1;
    int waterTempSensorNum = -1;


    // Store the ROM code of the temperature sensore when exactly one sensor is present. This single sensor is treated as board sensor.
    // Every additional sensor found later is then optional.
    if (num_devices == 1) {
        ESP_LOGD(tag, "Config sensor ID %s ?= ROM code %s.", mrConfig.msBoardTempSensorId.c_str(), romCodes[0]);
        if (mrConfig.msBoardTempSensorId != romCodes[0]) {
            mrConfig.msBoardTempSensorId = romCodes[0];
            ESP_LOGW(tag, "Writing detected board sensor ID %s to configuration.", mrConfig.msBoardTempSensorId.c_str());
            mrConfig.Save();
        }
        boardTempSensorNum = 0;
    } else if (num_devices > 1) {
        if (mrConfig.msBoardTempSensorId.length()) {
            for (int i = 0; i < num_devices; i++) {
                if (mrConfig.msBoardTempSensorId == romCodes[i]) {
                    boardTempSensorNum = i;
                } else {
                    waterTempSensorNum = i;
                }
            }
        } else {
            ESP_LOGE(tag, "Please unplug the external water temperature sensor and restart to detect the board temperature sensor.");
        }
    } 
    

    if (boardTempSensorNum >= 0) {
        mpBoardSensor = ds18b20_malloc();  // heap allocation
        ds18b20_init(mpBoardSensor, mpOwb, device_rom_codes[boardTempSensorNum]); // associate with bus and device
        ds18b20_use_crc(mpBoardSensor, true);           // enable CRC check on all reads
        ds18b20_set_resolution(mpBoardSensor, DS18B20_RESOLUTION);
    } 
    if (waterTempSensorNum >= 0) {
        mpWaterSensor = ds18b20_malloc();  // heap allocation
        ds18b20_init(mpWaterSensor, mpOwb, device_rom_codes[waterTempSensorNum]); // associate with bus and device
        ds18b20_use_crc(mpWaterSensor, true);           // enable CRC check on all reads
        ds18b20_set_resolution(mpWaterSensor, DS18B20_RESOLUTION);
    } 


    if (!mpBoardSensor) {
        ESP_LOGE(tag, "On-board temperature sensor %s not identified.", mrConfig.msBoardTempSensorId.c_str());
    }

    if (!mpWaterSensor) {
        ESP_LOGW(tag, "External water temperature sensor not plugged in.");
    }

    Read();
    ESP_LOGI(tag, "Board temperature %.1f°C, Water temperature %.1f°C", GetBoardTemp(), GetWaterTemp());
}

void TemperatureSensors::Read() {
    if (!mpBoardSensor || !mpOwb) {
        return;
    }

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
            ESP_LOGW(tag, "Error reading water temperature sensor %d", err);
        }
    }
    ESP_LOGD(tag, "Board temperature %.1f°C, Water temperature %.1f°C", GetBoardTemp(), GetWaterTemp());
}

TemperatureSensors::~TemperatureSensors() {
    // clean up dynamically allocated data
    ds18b20_free(&mpWaterSensor);
    ds18b20_free(&mpBoardSensor);
    owb_uninitialize(mpOwb);            
}
