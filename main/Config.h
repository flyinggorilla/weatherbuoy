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
	bool WriteString(nvs_handle h, const char* sKey, String& rsValue);
	bool WriteBool(nvs_handle h, const char* sKey, bool bValue);
	bool WriteInt(nvs_handle h, const char* sKey, int iValue);
	bool WriteUInt(nvs_handle h, const char* sKey, unsigned int uiValue);


public:
	bool mbAPMode;
	String msAPSsid;
	String msAPPass;
	String msSTASsid;
	String msSTAPass;
	String msHostname;
	String msTargetUrl;
	//String msLastGoodTargetUrl;
	int miSendDataIntervalDaytime; // seconds
	int miSendDataIntervalNighttime; // seconds
	int miSendDataIntervalHealth; // seconds
};

#endif /* MAIN_CONFIG_H_ */
