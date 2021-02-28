#include "SendData.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"

static const char tag[] = "SendData";

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

void SendData::EventHandler(int32_t id, void* event_data) {
    if (id == SENDDATA_EVENT_POSTDATA) {
        ESP_LOGI(tag, "POST: %s", (const char*)event_data);
    }
    
    mEspHttpClientConfig.url = "https://10.10.29.104:9100/weatherbuoy";
    mEspHttpClientConfig.host = "10.10.29.104";
    mEspHttpClientConfig.path = "/weatherbuoy";
    mEspHttpClientConfig.port = 9100;
    mEspHttpClientConfig.transport_type = HTTP_TRANSPORT_OVER_SSL;
    mEspHttpClientConfig.method = HTTP_METHOD_POST;
    //mEspHttpClientConfig.user_data = csResponseBuffer;
    //mEspHttpClientConfig.buffer_size = HTTPRESPBUFSIZE-1;
    if (!mhEspHttpClient) {
        mhEspHttpClient = esp_http_client_init(&mEspHttpClientConfig);
        ESP_LOGI("tag", "esp_http_client_init()");
    }

    mPostData = "maximet: \"";
    mPostData += (char*)event_data;
    mPostData += "\"\r\n";

    mPostData += "weatherbuoy: ";
    mPostData += (unsigned int) esp_timer_get_time()/1000000; // seconds since start
    mPostData += ",";
    mPostData += esp_get_free_heap_size();
    mPostData += ",";
    mPostData += esp_get_minimum_free_heap_size();
    mPostData += "\r\n";

    esp_err_t err;

    //esp_http_client_set_method(mhEspHttpClient, HTTP_METHOD_GET);
    //esp_http_client_set_url(mhEspHttpClient, "https://10.10.29.104:9100");
    //esp_http_client_set_header(mhEspHttpClient, "Content-Type", "application/yaml");
    esp_http_client_set_header(mhEspHttpClient, "Content-Type", "text/plain");
    //esp_http_client_set_header(mhEspHttpClient, "Accept", "text/plain");
    //esp_http_client (mhEspHttpClient, "Content-Length", "application/yaml'");
    //esp_http_client_set_header(mhEspHttpClient, "Content-Length", "application/yaml'");
    //esp_http_client_set_header(mhEspHttpClient, "Content-Type", "application/octet-stream'");
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

        if (iContentLength < 8192) {
            mResponseData.receive(iContentLength);
            int len = esp_http_client_read_response(mhEspHttpClient, (char*)mResponseData.c_str(), iContentLength);
            if (len == iContentLength) {
                ESP_LOGI(tag, "HTTP POST Response %s", mResponseData.c_str());
            } else {
                ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                mResponseData.reserve(0);
            }

        }

        //ESP_LOGI(tag, "HTTP POST Response %s strlen(%d)", csResponseBuffer, strlen(csResponseBuffer));
    } else {
        ESP_LOGE(tag, "Error performing HTTP POST request %s", esp_err_to_name(err));
    }

    //ESP_LOGI(tag, "response is chunked %s", esp_http_client_is_chunked_response(mhEspHttpClient) ? "true" : "false");


    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;

}

SendData::SendData(int queueSize) {
    esp_event_loop_args_t loop_args = {
        .queue_size = queueSize,
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

