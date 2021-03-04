#include "SendData.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esputil.h"

static const char tag[] = "SendData";
static const char SENDDATA_QUEUE_SIZE = 16; 
static const unsigned int MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH = 16*1024;

//ESP_EVENT_DECLARE_BASE(SENDDATA_EVENT_BASE);
ESP_EVENT_DEFINE_BASE(SENDDATA_EVENT_BASE);

enum {
    SENDDATA_EVENT_POSTDATA
};


// 1. Define the event handler
void eventHandler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    //ESP_LOGI(tag, "EVENT: %d", id);
    if (base == SENDDATA_EVENT_BASE) {
        ((SendData*)handler_arg)->EventHandler(id, event_data);
    }
}

String SendData::ReadMessageValue(const char* key) {
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0) {
        int posCrlf = mResponseData.indexOf("\r\n", posKey);
        if (posCrlf >= 0) {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posCrlf);
            ESP_LOGI(tag, "Message body key, value: '%s', '%s'", key, value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}

String SendData::ReadMessagePemValue(const char* key) {
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0) {
        static const char *ENDOFCERT = "-----END CERTIFICATE-----";
        int posEocert = mResponseData.indexOf("-----END CERTIFICATE-----\r\n", posKey);
        if (posEocert >= 0) {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posEocert + strlen(ENDOFCERT));
            ESP_LOGI(tag, "Message body PEM value %s", value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}


void SendData::Cleanup() {
    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;
}

void SendData::PerformHttpPost(const char *postData) {
    // Initialize URL and HTTP client
    if (!mhEspHttpClient) {
        memset(&mEspHttpClientConfig, 0, sizeof(mEspHttpClientConfig));
        mEspHttpClientConfig.url = mrConfig.msTargetUrl.c_str();
        mEspHttpClientConfig.method = HTTP_METHOD_POST; 
        mhEspHttpClient = esp_http_client_init(&mEspHttpClientConfig);
        //ESP_LOGD(tag, "Initializing new connection (keep-alive might have failed)");
    }

    // POST message
    unsigned int uptime = (unsigned int) esp_timer_get_time()/1000000; // seconds since start
    mPostData = "maximet: ";
    mPostData += postData;
    mPostData += "\r\nsystem: ";
    mPostData += esp_ota_get_app_description()->version;
    mPostData += ",";
    mPostData += mrConfig.msHostname; 
    mPostData += ",";
    mPostData += uptime;
    mPostData += ",";
    mPostData += esp_get_free_heap_size();
    mPostData += ",";
    mPostData += esp_get_minimum_free_heap_size();
    mPostData += "\r\n";
    if (mbSendDiagnostics) {
        mPostData += "esp-idf-version: ";
        mPostData += esp_ota_get_app_description()->idf_ver;  
        mPostData += "\r\nreset-reason: ";
        mPostData += esp32_getresetreasontext(esp_reset_reason());
        mPostData += "\r\n";
        mbSendDiagnostics = false;
    }

    // prepare and send HTTP headers and content length
    ESP_LOGI(tag, "Sending %d bytes to %s", mPostData.length(), mEspHttpClientConfig.url);
    esp_http_client_set_header(mhEspHttpClient, "Content-Type", "text/plain");
    esp_err_t err;
    err = esp_http_client_open(mhEspHttpClient, mPostData.length());
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Error %s in esp_http_client_open(): %s", esp_err_to_name(err), mEspHttpClientConfig.url);
        Cleanup();
        return;
    }

    // write POST body message
    int sent = esp_http_client_write(mhEspHttpClient, mPostData.c_str(), mPostData.length());
    if (sent == mPostData.length()) {
        ESP_LOGD(tag, "esp_http_client_write(): OK, sent: %d", sent);
    } else {
        ESP_LOGE(tag, "esp_http_client_write(): Could only send %d of %d bytes", sent, mPostData.length());
        Cleanup();
        return;
    }

    // retreive HTTP response and headers
    int iContentLength = esp_http_client_fetch_headers(mhEspHttpClient);
    if (iContentLength == ESP_FAIL) {
        ESP_LOGE(tag, "esp_http_client_fetch_headers(): could not receive HTTP response");
        Cleanup();
        return;
    }

    // Check HTTP status code
    int iHttpStatusCode = esp_http_client_get_status_code(mhEspHttpClient);
    if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400)) {
        ESP_LOGI(tag, "HTTP response OK. Status %d, Content-Length %d", iHttpStatusCode , iContentLength);
    } else {
        ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        Cleanup();
        return;
    }

    // Prevent overly memory allocation
    if (iContentLength > MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH) {
        ESP_LOGE(tag, "Response body Content-length %d exceeds maximum of %d. Aborting.", iContentLength, MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH);
        Cleanup();
        return;
    }

    // Ensure enough memory is allocated
    if (!mResponseData.prepare(iContentLength)) {
        ESP_LOGE(tag, "Could not allocate %d memory for HTTP response.", iContentLength);
        Cleanup();
        return;
    }

    // read the HTTP response body and process it
    int len = esp_http_client_read_response(mhEspHttpClient, (char*)mResponseData.c_str(), iContentLength);
    if ((len == iContentLength) && len) {
        ESP_LOGI(tag, "HTTP POST Response \r\n--->\r\n%s<---", mResponseData.c_str());

        // Interpret the Weatherbuoy messages
        String command = ReadMessageValue("command:");
        if (command.length()) {
            bool updateConfig = false;
            String value;
            value = ReadMessageValue("set-apssid:");
            if (value.length()) { mrConfig.msAPSsid = value; updateConfig = true; };
            value = ReadMessageValue("set-appass:");
            if (value.length()) { mrConfig.msAPPass = value; updateConfig = true; };
            value = ReadMessageValue("set-stassid:");
            if (value.length()) { mrConfig.msSTASsid = value; updateConfig = true; };
            value = ReadMessageValue("set-stapass:");
            if (value.length()) { mrConfig.msSTAPass = value; updateConfig = true; };
            value = ReadMessageValue("set-hostname:");
            if (value.length()) { mrConfig.msHostname = value; updateConfig = true; };
            value = ReadMessageValue("set-targeturl:");
            if (value.length()) { mrConfig.msTargetUrl = value; updateConfig = true; };
            value = ReadMessageValue("set-apssid:");
            if (value.length()) { mrConfig.msAPSsid = value; updateConfig = true; };

            bool restart = false;
            if (command.equals("restart") || command.equals("config") || command.equals("udpate")) {
                if (updateConfig) {
                    updateConfig = mrConfig.Save();
                    ESP_LOGI(tag, "New configuration received and SAVED.");
                }
                restart = true;
            } else if (command.equals("diagnose")) {
                mbSendDiagnostics = true;
            }

            // Optionally Execute OTA Update command
            if (command.equals("update")) {
                value = ReadMessageValue("set-firmwarepath:");
                const String &pem = ReadMessagePemValue("set-cert-pem:");
                if (value.length()) {
                    ESP_LOGI(tag, "***** Setting up OTA update '%s'\r\n%s\r\n *****", value.c_str(), pem.c_str());
                    //mEspHttpClientConfig.path = value.c_str();
                    mEspHttpClientConfig.method = HTTP_METHOD_GET; 
                    mEspHttpClientConfig.url = "https://10.10.29.104:9100/weatherbuoy/firmware.bin";
                    mEspHttpClientConfig.skip_cert_common_name_check = true;
                    mEspHttpClientConfig.cert_pem = pem.c_str();
                    err = esp_https_ota(&mEspHttpClientConfig);
                    if (err == ESP_OK) {
                        ESP_LOGI(tag, "**** Successful OTA update *****");
                    } else {
                        ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                    }
                    restart = true;
                } else {
                    ESP_LOGE(tag, "OTA update path missing!");
                }
            }

            if (restart) {
                ESP_LOGI(tag, "***** RESTARTING in 1 Second *****.");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
            } 
        }
    } 
    
    if (!mbOtaAppValidated) {
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
            ESP_LOGE(tag, "Error: Could not validate firmware app image. %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(tag, "Firmware validated!");
        }
        mbOtaAppValidated = true;
    } 

    //ESP_LOGI(tag, "response is chunked %s", esp_http_client_is_chunked_response(mhEspHttpClient) ? "true" : "false");

    Cleanup();
}


void SendData::EventHandler(int32_t id, void* event_data) {
    if (id == SENDDATA_EVENT_POSTDATA) {
        ESP_LOGI(tag, "POST: %s", (const char*)event_data);
    }

    PerformHttpPost((const char*)event_data);

}

SendData::SendData(Config &config) : mrConfig(config) {
    esp_event_loop_args_t loop_args = {
        .queue_size = SENDDATA_QUEUE_SIZE,
        .task_name = "SendData",
        .task_priority = ESP_TASK_MAIN_PRIO,
        .task_stack_size = 8192,
        .task_core_id = tskNO_AFFINITY //CORE_ID_REGVAL_APP
    };
    mhLoopHandle = nullptr;
    esp_event_loop_create(&loop_args, &mhLoopHandle);
    esp_event_handler_register_with(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, eventHandler, this);

    // esp http client
    mhEspHttpClient = nullptr; 

}

bool SendData::Post(String &data) {
    if (ESP_OK == esp_event_post_to(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, (void*)data.c_str(), data.length()+1, 1000 / portTICK_PERIOD_MS))
        return true;
  
    return false;
}


SendData::~SendData() {
    esp_event_handler_unregister_with(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, eventHandler);
    esp_event_loop_delete(mhLoopHandle);

    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;
}

