#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#include "esp_event.h"
#include "Config.h"
#include "NMEA2000_esp32.h"
//#include "N2kStream_esp32.h"
#include "DataQueue.h"


// Weatherbuoy electronics can be used on a boat too, for instance for a Sail Race Committee boat.
// For that purpose, we use the ESP32 CAN Bus capability, and an additional CAN tranceiver (e.g. SN65HVD230)
// to send data to an NMEA2000 capable display such as Garmin GNX130

class NmeaDisplay {
    public:
        NmeaDisplay(gpio_num_t canTX, gpio_num_t canRX, gpio_num_t canPower, DataQueue &dataQueue);
        void SetSystemInfo(float voltage, float current, float boardtemp); // thread safe!!
        // starts the thread to refresh display data in 1s interval. 
        void Start();
   
    private:
        void Write();
        DataQueue &mrDataQueue;
        tNMEA2000_esp32 mNmea;
        //N2kStream_esp32 mNmeaLogStream;
        gpio_num_t mGpioPower; // the sn65hvd230 can bus chip takes about 11mA and can be powered from GPIO pin
        //SimpleMovingAverage<300> mWindspeed5minAvg; // 5 minutes if 1sec samples
        //SimpleMovingAverage<300> mWinddirection5minAvg; // 5 minutes if 1sec samples
        
        void DisplayTask();
        friend void fDisplayTask(void *pvParameter);

        // system info
        float mfVoltage = 0.0;
        float mfCurrent = 0.0;
        float mfBoardTemp = 0.0;
        portMUX_TYPE mCriticalSection = portMUX_INITIALIZER_UNLOCKED;
        int mNmeaErrors = 0;

};


#endif 


