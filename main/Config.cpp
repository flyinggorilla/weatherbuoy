#include <freertos/FreeRTOS.h>
#include "Config.h"
#include <nvs_flash.h>
#include "sdkconfig.h"

#define NVS_NAME "Config"

Config::Config() {
	mbAPMode = true;
	msAPSsid = CONFIG_WEATHERBUOY_HOSTNAME;
	msAPPass = CONFIG_WEATHERBUOY_WIFI_AP_PASS;
	msHostname = CONFIG_WEATHERBUOY_HOSTNAME;
	msTargetUrl = CONFIG_WEATHERBUOY_TARGET_URL;
	msSTASsid = CONFIG_WEATHERBUOY_WIFI_STA_SSID;
	msSTAPass = CONFIG_WEATHERBUOY_WIFI_STA_PASS;
	miSendDataIntervalDaytime = CONFIG_WEATHERBUOY_SENDDATA_INTERVAL_DAYTIME;
	miSendDataIntervalNighttime = CONFIG_WEATHERBUOY_SENDDATA_INTERVAL_NIGHTTIME;
	miSendDataIntervalHealth = CONFIG_WEATHERBUOY_SENDDATA_INTERVAL_HEALTH;	
}

Config::~Config() {
}

bool Config::Load(){
	nvs_handle h;

	if (nvs_flash_init() != ESP_OK) 
		return false;
	if (nvs_open(NVS_NAME, NVS_READONLY, &h) != ESP_OK)
		return false;
	ReadBool(h, "APMode", mbAPMode);
	ReadString(h, "APSsid", msAPSsid);
	ReadString(h, "APPass", msAPPass);
	ReadString(h, "STASsid", msSTASsid);
	ReadString(h, "STAPass", msSTAPass);
	ReadString(h, "Hostname", msHostname);
	ReadInt(h, "SendDataIntervalDaytime", miSendDataIntervalDaytime);
	ReadInt(h, "SendDataIntervalNighttime", miSendDataIntervalNighttime);
	ReadInt(h, "SendDataIntervalHealth", miSendDataIntervalHealth);
	ReadString(h, "TargetUrl", msTargetUrl);
	//ReadString(h, "LastGoodTargetUrl", msLastGoodTargetUrl);

	nvs_close(h);
	return true;
}


bool Config::Save()
{
	nvs_handle h;

	if (nvs_flash_init() != ESP_OK) 
		return false;

	if (nvs_open(NVS_NAME, NVS_READWRITE, &h) != ESP_OK)
		return false;
	nvs_erase_all(h); //otherwise I need double the space

	if (!WriteBool(h, "APMode", mbAPMode))
		return nvs_close(h), false;
	if (!WriteString(h, "APSsid", msAPSsid))
		return nvs_close(h), false;
	if (!WriteString(h, "APPass", msAPPass))
		return nvs_close(h), false;
	if (!WriteString(h, "STASsid", msSTASsid))
		return nvs_close(h), false;
	if (!WriteString(h, "STAPass", msSTAPass))
		return nvs_close(h), false;
	if (!WriteString(h, "Hostname", msHostname))
		return nvs_close(h), false;
	if (!WriteString(h, "TargetUrl", msTargetUrl))
		return nvs_close(h), false;
	if (!WriteInt(h, "SendDataIntervalDaytime", miSendDataIntervalDaytime))
		return nvs_close(h), false;
	if (!WriteInt(h, "SendDataIntervalNighttime", miSendDataIntervalNighttime))
		return nvs_close(h), false;
	if (!WriteInt(h, "SendDataIntervalHealth", miSendDataIntervalHealth))
		return nvs_close(h), false;

	nvs_commit(h);
	nvs_close(h);
	return true;
}

//------------------------------------------------------------------------------------

bool Config::ReadString(nvs_handle h, const char* sKey, String& rsValue){
	char* sBuf = NULL;
	__uint32_t u = 0;

	nvs_get_str(h, sKey, NULL, &u);
	if (!u)
		return false;
	sBuf = (char*)malloc(u+1);
	if (nvs_get_str(h, sKey, sBuf, &u) != ESP_OK)
		return free(sBuf), false;
	sBuf[u] = 0x00;
	rsValue = sBuf;
	free(sBuf);
	return true;
}

bool Config::ReadBool(nvs_handle h, const char* sKey, bool& rbValue){
	__uint8_t u;
	if (nvs_get_u8(h, sKey, &u) != ESP_OK)
		return false;
	rbValue = u ? true : false;
	return true;
}

bool Config::ReadInt(nvs_handle h, const char* sKey, int& riValue){
	__uint32_t u;
	if (nvs_get_u32(h, sKey, &u) != ESP_OK)
		return false;
	riValue = u;
	return true;
}

bool Config::ReadUInt(nvs_handle h, const char* sKey, unsigned int& ruiValue){
	__uint32_t u;
	if (nvs_get_u32(h, sKey, &u) != ESP_OK)
		return false;
	ruiValue = u;
	return true;
}

bool Config::ReadShortUInt(nvs_handle h, const char* sKey, unsigned short int& ruiValue){
	__uint32_t u;
	if (nvs_get_u32(h, sKey, &u) != ESP_OK)
		return false;
	ruiValue = u;
	return true;
}

bool Config:: WriteString(nvs_handle h, const char* sKey, String& rsValue){
	esp_err_t err = nvs_set_str(h, sKey, rsValue.c_str());
	if (err != ESP_OK){
		//ESP_LOGE(tag, "  <%s> -> %d", sKey, err);
		return false;
	}
	return true;
}


bool Config:: WriteBool(nvs_handle h, const char* sKey, bool bValue){
	return (nvs_set_u8(h, sKey, bValue ? 1 : 0) == ESP_OK);
}

bool Config:: WriteInt(nvs_handle h, const char* sKey, int iValue){
	return (nvs_set_u32(h, sKey, iValue) == ESP_OK);
}

bool Config:: WriteUInt(nvs_handle h, const char* sKey, unsigned int uiValue){
	return (nvs_set_u32(h, sKey, uiValue) == ESP_OK);
}

