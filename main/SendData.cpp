#include "SendData.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

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
        vTaskDelay(900000 / portTICK_PERIOD_MS);
    }
}

SendData::SendData(int queueSize) {
    esp_event_loop_args_t loop_args = {
        .queue_size = queueSize,
        .task_name = "SendData",
        .task_priority = ESP_TASK_MAIN_PRIO,
        .task_stack_size = 2048,
        .task_core_id = tskNO_AFFINITY //CORE_ID_REGVAL_APP
    };
    mhLoopHandle = nullptr;
    esp_event_loop_create(&loop_args, &mhLoopHandle);
    esp_event_handler_register_with(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, eventHandler, this);
}

bool SendData::Post(String &data) {
    if (ESP_OK == esp_event_post_to(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, (void*)data.c_str(), data.length()+1, 1000 / portTICK_PERIOD_MS))
        return true;
    return false;
}


SendData::~SendData() {
    esp_event_handler_unregister_with(mhLoopHandle, SENDDATA_EVENT_BASE, SENDDATA_EVENT_POSTDATA, eventHandler);
    esp_event_loop_delete(mhLoopHandle);

}

