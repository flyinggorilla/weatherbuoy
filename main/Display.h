#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#include "esp_event.h"
#include "Config.h"
#include "NMEA2000_esp32.h"
#include "N2kStream_esp32.h"
#include "MovingAverage.h"


// Weatherbuoy electronics can be used on a boat too, for instance for a Sail Race Committee boat.
// For that purpose, we use the ESP32 CAN Bus capability, and an additional CAN tranceiver (e.g. SN65HVD230)
// to send data to an NMEA2000 capable display such as Garmin GNX130

class Display {
    public:

        Display(gpio_num_t canTX, gpio_num_t canRX);

        void Send(float temp);
    
    private:
        tNMEA2000_esp32 mNmea;
        N2kStream_esp32 mNmeaLogStream;
        MovingAverage<300> mMovingWindspeedAvg; // 5 minutes if 1sec samples
        MovingAverage<300> mMovingWindangleAvg; // 5 minutes if 1sec samples

};


#endif 


