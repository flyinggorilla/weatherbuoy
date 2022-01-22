#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include <nvs.h>
#include "EspString.h"

#define WEATHERBUOY_MODE_DEFAULT 0
#define WEATHERBUOY_MODE_NMEA2000_DISPLAY 1
#define WEATHERBUOY_MODE_MAXIMET_GMX501 100
#define WEATHERBUOY_MODE_MAXIMET_GMX200GPS 101

class Config {
public:
	Config();
	virtual ~Config();

	bool Load();
	bool Save();
	void ToggleAPMode() { mbAPMode = !mbAPMode; };

private:
	bool ReadString(nvs_handle h, const char* sKey, String& rsValue);
	bool ReadBool(nvs_handle h, const char* sKey, bool& rbValue);
	bool ReadInt(nvs_handle h, const char* sKey, int& riValue);
	bool ReadUInt(nvs_handle h, const char* sKey, unsigned int& ruiValue);
	bool ReadShortUInt(nvs_handle h, const char* sKey, unsigned short int& ruiValue);
	bool WriteString(nvs_handle h, const char* sKey, String& rsValue);
	bool WriteBool(nvs_handle h, const char* sKey, bool bValue);
	bool WriteInt(nvs_handle h, const char* sKey, int iValue);
	bool WriteUInt(nvs_handle h, const char* sKey, unsigned int uiValue);


public:
	int miMode;
	bool mbAPMode;
	String msAPSsid;
	String msAPPass;
	String msSTASsid;
	String msSTAPass;
	String msHostname;
	String msTargetUrl;
	String msCellularApn;
	String msCellularUser;
	String msCellularPass;
	String msCellularOperator;
	String msNtpServer;
	int miCellularNetwork;
	String msBoardTempSensorId;
};

#endif /* MAIN_CONFIG_H_ */
