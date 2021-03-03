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
            ESP_LOGI(tag, "Message body param '%s %s'", key, value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}


int test_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len)
{
    int read_len = 0;
    while (read_len < len) {
        int data_read = esp_http_client_read(client, buffer + read_len, len - read_len);
        if (data_read <= 0) {
            return read_len;
        }
        printf("****TESTREADRESPONSE ---%d\r\n%s\r\n-------****XXX TESTREADRESPONSE-----\r\n", data_read, buffer);
        ESP_LOGI(tag, "TESTREADRESPONSE ---%d\r\n%s\r\n-------XXX TESTREADRESPONSE-----\r\n", data_read, buffer);
        ESP_LOG_BUFFER_HEX(tag, buffer, data_read);
        read_len += data_read;
    }
    return read_len;
}

void SendData::EventHandler(int32_t id, void* event_data) {
    if (id == SENDDATA_EVENT_POSTDATA) {
        ESP_LOGI(tag, "POST: %s", (const char*)event_data);
    }
    
    mEspHttpClientConfig.url = mrConfig.msTargetUrl.c_str();
    mEspHttpClientConfig.method = HTTP_METHOD_POST;
    if (!mhEspHttpClient) {
        mhEspHttpClient = esp_http_client_init(&mEspHttpClientConfig);
        ESP_LOGD(tag, "Initializing new connection (keep-alive might have failed)");
    }

    unsigned int uptime = (unsigned int) esp_timer_get_time()/1000000; // seconds since start
    mPostData = "maximet: ";
    mPostData += (char*)event_data;
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

    esp_err_t err;
    esp_http_client_set_header(mhEspHttpClient, "Content-Type", "text/plain");
    err = esp_http_client_set_post_field(mhEspHttpClient, mPostData.c_str(), mPostData.length());
    if (err != ESP_OK) {
        ESP_LOGE(tag, "cannot set POST data of size %d bytes %s", mPostData.length(), esp_err_to_name(err));
    }
    err = esp_http_client_perform(mhEspHttpClient);
    int iHttpStatusCode = 0;
    int iContentLength = 0;
    if (err == ESP_OK) {
        iHttpStatusCode = esp_http_client_get_status_code(mhEspHttpClient);
        iContentLength = esp_http_client_get_content_length(mhEspHttpClient);
        ESP_LOGI(tag, "HTTP POST Status %d, Content-Length %d", iHttpStatusCode , iContentLength);

        if (iContentLength < MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH) {
            mResponseData.receive(iContentLength);

            char sBuf[2048] = {0};
            int len = test_http_client_read_response(mhEspHttpClient, sBuf, 2048-1);
            mResponseData = sBuf;
            if ((len == iContentLength) && len) {
                ESP_LOGI(tag, "HTTP POST Response \r\n--->\r\n%s<---", sBuf);
                printf("--------A-----------\r\n");
                printf("response STRLEN = %d\r\n", strlen(sBuf));
                printf(mResponseData.c_str());
                printf("========B===========");

            /*int len = esp_http_client_read_response(mhEspHttpClient, (char*)mResponseData.c_str(), iContentLength);
            if ((len == iContentLength) && len) {
                ESP_LOGI(tag, "HTTP POST Response \r\n--->\r\n%s<---", mResponseData.c_str());
                printf("-------------------\r\n");
                printf("response STRLEN = %d\r\n", strlen(mResponseData.c_str()));
                printf(mResponseData.c_str());
                printf("===================");*/
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

                    if (command.equals("update")) {
                        value = ReadMessageValue("set-firmwarepath:");
                        const String &pem = ReadMessageValue("set-cert-pem:");
                        if (value.length()) {
                            ESP_LOGI(tag, "***** Setting up OTA update '%s' *****", value.c_str());
                            mEspHttpClientConfig.path = value.c_str();
                            mEspHttpClientConfig.skip_cert_common_name_check = true;
                            mEspHttpClientConfig.cert_pem = pem.c_str();
                            // mEspHttpClientConfig.use_global_ca_store = false;
                            err = esp_https_ota(&mEspHttpClientConfig);
                            if (err == ESP_OK) {
                                ESP_LOGI(tag, "**** Successful OTA update *****");
                            } else {
                                ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                            }
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
            
            if (len < 0) {
                ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                mResponseData.reserve(0);
            }

        }

    } else {
        ESP_LOGE(tag, "Error performing HTTP POST request %s", esp_err_to_name(err));
    }

    if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400)) {
        if (!mbOtaAppValidated) {
            if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
                ESP_LOGE(tag, "Error: Could not validate firmware app image. %s", esp_err_to_name(err));
            } 
            mbOtaAppValidated = true;
        } 
    } else {
        ESP_LOGE(tag, "HTTP POST response was not OK with status  %d", iHttpStatusCode);
    }

    //ESP_LOGI(tag, "response is chunked %s", esp_http_client_is_chunked_response(mhEspHttpClient) ? "true" : "false");

    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;

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

