#ifndef MAIN_ESP32WEATHERBUOY_H_
#define MAIN_ESP32WEATHERBUOY_H_

#include "Config.h"
#include "Wifi.h"
#include "Cellular.h"
#include "SimulateMaximet.h"
#include "DataQueue.h"
#include "Maximet.h"
#include "Max471Meter.h"
#include "SendData.h"
#include "TemperatureSensors.h"

#define FIRMWARE_VERSION __DATE__ " " __TIME__

class Esp32WeatherBuoy {
public:
	Esp32WeatherBuoy();
	virtual ~Esp32WeatherBuoy();

	void Start();
	//void StartMaximetSimulator(MaximetModel model);
	enum OnlineMode {
		MODE_OFFLINE,
		MODE_WIFISTA,
		MODE_WIFIAP,
		MODE_CELLULAR
	};

	void Restart(int seconds);
	Config& GetConfig() { return config; }

	void RunBuoy(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet);
	void RunDisplay(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData);
	void RunSimulator(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, MaximetModel model);

private:
	Config config;
	Wifi wifi;
	Cellular cellular;
	OnlineMode mOnlineMode;
	bool mbCellular;
};




#endif // MAIN_ESP32WEATHERBUOY_H_
