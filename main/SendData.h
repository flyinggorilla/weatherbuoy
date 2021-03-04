#ifndef MAIN_SENDDATA_H_
#define MAIN_SENDDATA_H_

#include "esp_event.h"
#include "EspString.h"
#include "esp_http_client.h"
#include "Config.h"

class SendData {
public:
	SendData(Config &config);
	virtual ~SendData();
	void EventHandler(int32_t id, void* event_data);

    // post string data to a message queue for sending. Data is copied into queue
    bool Post(String &data);
private:
    esp_event_loop_handle_t mhLoopHandle;
    Config &mrConfig;

private: // esp http client
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_http_client_config_t mEspHttpClientConfig = {0};
    esp_http_client_handle_t mhEspHttpClient = nullptr;
    String mPostData;
    String mResponseData;
    String ReadMessageValue(const char* key);
    String ReadMessagePemValue(const char* key);
    void PerformHttpPost(const char *postData);
    void Cleanup();
    bool mbSendDiagnostics = false;
    bool mbOtaAppValidated = false;
};




#endif // MAIN_SENDDATA_H_


