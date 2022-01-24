//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "DataQueue.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"

static const char tag[] = "DataQueue";

bool DataQueue::GetData(Data &data)
{
    if (xQueueReceive(mxDataQueue, &data, 0) == pdTRUE)
    {
        ESP_LOGD(tag, "Received data from Queue.");
        return true;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}

int DataQueue::GetQueueLength()
{
    return uxQueueMessagesWaiting(mxDataQueue);
}

bool DataQueue::WaitForData(unsigned int timeoutSeconds)
{
    Data receivedData;
    if (xQueuePeek(mxDataQueue, &receivedData, timeoutSeconds * 1000 / portTICK_PERIOD_MS) == pdTRUE)
    {
        ESP_LOGD(tag, "Peeking into Queue.");
        return true;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}

bool DataQueue::GetLatestData(Data &data, unsigned int timeoutSeconds)
{
    if (xQueueReceive(mxDataLatest, &data, timeoutSeconds * 1000 / portTICK_PERIOD_MS) == pdTRUE)
    {
        ESP_LOGD(tag, "Retreiving item from Queue.");
        return true;
    }
    ESP_LOGD(tag, "No data in Queue.");
    return false;
}


bool DataQueue::IsFull()
{
    return (uxQueueSpacesAvailable(mxDataQueue) == 0);
}

bool DataQueue::PutData(Data &data)
{
    return (xQueueSend(mxDataQueue, &data, 0) == pdTRUE);
}

bool DataQueue::PutLatestData(Data &data)
{
    return (xQueueOverwrite(mxDataLatest, &data) == pdTRUE);
}

DataQueue::DataQueue()
{
    // Create a queue capable of containing 10 uint32_t values.
    mxDataQueue = xQueueCreate(60, sizeof(Data));
    mxDataLatest = xQueueCreate(1, sizeof(Data));
    if (!mxDataQueue)
    {
        ESP_LOGE(tag, "Could not create Data queue. Data collection not initialized.");
    }
}

DataQueue::~DataQueue()
{
}


short nans() {
    return SHRT_MIN;
}

unsigned char nanuc() {
    return UCHAR_MAX;
}

float nanf() {
    return nanf("");
}

bool isnans(short n) {
    return n == nans();
}

bool isnauc(unsigned char uc) {
    return uc == nanuc();
}
