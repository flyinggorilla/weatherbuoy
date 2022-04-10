#ifndef MAIN_ESP32WEATHERBUOY_H_
#define MAIN_ESP32WEATHERBUOY_H_

#include "Config.h"
#include "Wifi.h"
#include "Cellular.h"
#include "DataQueue.h"
#include "Maximet.h"
#include "Max471Meter.h"
#include "SendData.h"
#include "NmeaDisplay.h"
#include "TemperatureSensors.h"
#include "Alarm.h"

#define FIRMWARE_VERSION __DATE__ " " __TIME__

class Esp32WeatherBuoy {
public:
	Esp32WeatherBuoy();
	virtual ~Esp32WeatherBuoy();

	void Start();
	//void StartMaximetSimulator(MaximetModel model);

	void Restart(int seconds);
	Config& GetConfig() { return mConfig; }

	void HandleAlarm(Alarm *pAlarm);
	void Run(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, Alarm *pAlarm);
	void RunSimulator(TemperatureSensors &tempSensors, DataQueue &dataQueue, Max471Meter &max471Meter, SendData &sendData, Maximet &maximet, Maximet::Model model);

private:
	Config mConfig;
	Wifi mWifi;
	Cellular mCellular;
	OnlineMode mOnlineMode;
	bool mbCellular;
	NmeaDisplay *mpDisplay = nullptr;
};




#endif // MAIN_ESP32WEATHERBUOY_H_
