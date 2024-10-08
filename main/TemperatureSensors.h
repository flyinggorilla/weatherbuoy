#ifndef INCLUDE_TEMPERATURE_SENSOR_H_
#define INCLUDE_TEMPERATURE_SENSOR_H_

#include "owb.h"
#include "ds18b20.h"
#include "Config.h"

#define CONFIG_MAX_TEMPERATURE_SENSORS (2)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)

class TemperatureSensors {
    public:
        TemperatureSensors(Config &config);
        virtual ~TemperatureSensors();

        float GetWaterTemp() { return mfWaterTemp; };
        float GetBoardTemp() { return mfBoardTemp; };
        String GetRomCodes() { return mRomCodes; };

        void Init(int oneWireGpioNum);
        void Read();

    private:
        DS18B20_Info *mpWaterSensor = nullptr;
        DS18B20_Info *mpBoardSensor = nullptr;
        Config &mrConfig;
        OneWireBus   *mpOwb = nullptr;
        owb_rmt_driver_info mRmtDriverInfo;

        float mfWaterTemp = 0;
        float mfBoardTemp = 0;
        String mRomCodes;

};

#endif