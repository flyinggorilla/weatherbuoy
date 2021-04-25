#ifndef MAIN_SENDDATA_H_
#define MAIN_SENDDATA_H_

#include "esp_event.h"
#include "EspString.h"
#include "esp_http_client.h"
#include "Config.h"
#include "Cellular.h"
#include "Watchdog.h"
#include "Data.h"

class SendData {
public:
	SendData(Config &config, Cellular &cellular, Watchdog &watchdog);
	virtual ~SendData();
	//void EventHandler(int32_t id, void* event_data);
    bool PerformHttpPost(Data &data);


    // post string data to a message queue for sending. Data is copied into queue
    //bool PostData(String &data);
    //bool PostHealth(unsigned int powerVoltage, unsigned int powerCurrent, float boardTemperature, float waterTemperature);
    bool isRestart() { return mbRestart; };
private:
    //esp_event_loop_handle_t mhLoopHandle = nullptr;
    //esp_event_handler_instance_t mhEventHandlerInstance = nullptr;
    Config &mrConfig;
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

private: // health data
    unsigned int muiPowerVoltage = 0;
    unsigned int muiPowerCurrent = 0;
    float mfBoardTemperature = 0;
    float mfWaterTemperature = 0;

};




#endif // MAIN_SENDDATA_H_


