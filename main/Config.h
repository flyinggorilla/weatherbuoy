#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include <nvs.h>
#include "EspString.h"

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
	bool ReadDouble(nvs_handle h, const char* sKey, double& rdValue);
	bool WriteString(nvs_handle h, const char* sKey, String& rsValue);
	bool WriteBool(nvs_handle h, const char* sKey, bool bValue);
	bool WriteInt(nvs_handle h, const char* sKey, int iValue);
	bool WriteUInt(nvs_handle h, const char* sKey, unsigned int uiValue);
	bool WriteDouble(nvs_handle h, const char* sKey, double dValue);


public:
	int miSimulator;
	bool mbNmeaDisplay;
	bool mbAlarmSound;
	String msAlarmSms;
	int miAlarmRadius;
	double mdAlarmLatitude;
	double mdAlarmLongitude;
	bool mbAPMode;
	String msAPSsid;
	String msAPPass;
	String msSTASsid;
	String msSTAPass;
	String msHostname;
	String msCellularApn;
	String msCellularUser;
	String msCellularPass;
	String msCellularOperator;
	String msNtpServer;
	int miCellularNetwork;
	int miIntervalDay;
	int miIntervalNight;
	int miIntervalDiagnostics;
	int miIntervalLowbattery;
	String msBoardTempSensorId;
};

#endif /* MAIN_CONFIG_H_ */
