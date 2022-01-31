#ifndef MAIN_SENDDATA_H_
#define MAIN_SENDDATA_H_

#include "esp_event.h"
#include "EspString.h"
#include "esp_http_client.h"
#include "Config.h"
#include "Cellular.h"
#include "Watchdog.h"
#include "DataQueue.h"

class SendData {
public:
	SendData(Config &config, DataQueue &dataQueue, Cellular &cellular, Watchdog &watchdog);
	virtual ~SendData();
    void SetMaximetDiagnostics(String &report, unsigned int avglong, unsigned int outfreq);
    bool PrepareHttpPost(unsigned int powerVoltage, unsigned int powerCurrent, float boardTemperature, float waterTemperature, bool bSendDiagnostics);
    bool PerformHttpPost();
    bool isRestart() { return mbRestart; };
private:
    Config &mrConfig;
    DataQueue &mrDataQueue;
    Cellular &mrCellular;
    Watchdog &mrWatchdog;

private: // esp http client
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_http_client_config_t mEspHttpClientConfig = {0};
    esp_http_client_handle_t mhEspHttpClient = nullptr;
    String mPostData;
    String mResponseData;
    String ReadMessageValue(const char* key);
    String ReadMessagePemValue(const char* key);
    void Cleanup();
    bool mbSendDiagnostics = false;
    bool mbOtaAppValidated = false;
    bool mbRestart = false;

private:
    unsigned int muiMaximetOutfreqSeconds = 0;
    unsigned int muiMaximetAvgLong = 0;
    String msMaximetReport;
};




#endif // MAIN_SENDDATA_H_


