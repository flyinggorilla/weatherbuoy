#ifndef MAIN_ESP32WEATHERBUOY_H_
#define MAIN_ESP32WEATHERBUOY_H_

#include "Config.h"

#define FIRMWARE_VERSION __DATE__ " " __TIME__

class Esp32WeatherBuoy {
public:
	Esp32WeatherBuoy();
	virtual ~Esp32WeatherBuoy();

	void Start();
	void Wifi();
	void TestHttp();

/*	void TaskWebServer();
	void TaskResetButton();
	void TaskDnsServer();
	void TaskTestWebClient();

	void IndicateApiCall() 	{ mbApiCallReceived = true; }; */
	void Restart(int seconds);
	Config& GetConfig() { return mConfig; }

private:
	//bool mbButtonPressed;
	//bool mbApiCallReceived;
	Config mConfig;


};




#endif // MAIN_ESP32WEATHERBUOY_H_
