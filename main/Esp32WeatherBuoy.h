#ifndef MAIN_ESP32WEATHERBUOY_H_
#define MAIN_ESP32WEATHERBUOY_H_

#include "Config.h"
#include "Wifi.h"
#include "Cellular.h"

#define FIRMWARE_VERSION __DATE__ " " __TIME__

class Esp32WeatherBuoy {
public:
	Esp32WeatherBuoy();
	virtual ~Esp32WeatherBuoy();

	void Start();

	enum OnlineMode {
		MODE_OFFLINE,
		MODE_WIFISTA,
		MODE_WIFIAP,
		MODE_CELLULAR
	};

	void Restart(int seconds);
	Config& GetConfig() { return config; }

private:
	void WeatherBuoyTask();
    friend void fWeatherBuoyTask(void *pvParameter);

	Config config;
	Wifi wifi;
	Cellular cellular;
};




#endif // MAIN_ESP32WEATHERBUOY_H_
