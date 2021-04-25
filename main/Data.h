#ifndef MAIN_DATA_H_
#define MAIN_DATA_H_

#include "EspString.h"

class Data {
    public:
        String msMaximet;
        unsigned int muiPowerVoltage;
        unsigned int muiPowerCurrent;
        unsigned int muiPowerCurrentMax;
        unsigned int muiWaterTemperature;
        unsigned int muiBoardTemperature;
        unsigned int muiTimestamp;
};

#endif 