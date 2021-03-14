#ifndef MAIN_CELLULAR_H_
#define MAIN_CELLULAR_H_

#include "Config.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"


class Cellular;


typedef struct esp_cellular_netif_driver_s {
    esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
    Cellular                   *pModem;        /*!< ptr to the esp_modem objects (DTE) */
} esp_cellular_netif_driver_t;


class Cellular {
public:
	Cellular(String apn, String user, String pass);
	virtual ~Cellular();
    void InitNetwork();
    void Start();
    String Command(const char *sCommand, const char *sInfo, unsigned short maxLines = 5);
    void TurnOn();
    bool SwitchToCommandMode(); // todo, move to private
    bool SwitchToPppMode(); // can be moved to private

private:
    friend esp_err_t esp_cellular_post_attach_start(esp_netif_t * esp_netif, void * args);

 	void OnEvent(esp_event_base_t base, int32_t id, void *event_data);
    friend void cellularEventHandler(void* ctx, esp_event_base_t base, int32_t id, void* event_data);


    void ReceiverTask();
    friend void fReceiverTask(void *pvParameter);

    bool ModemReadLine(String& line);
    String ModemReadResponse(unsigned short maxLines = 10);

    bool ModemWriteLine(const char *sWrite);
    bool ModemWrite(String &command);

    
    int ModemWriteData(const char* data, int len);
    friend esp_err_t esp_cellular_transmit(void *h, void *buffer, size_t len);

    bool ReadIntoBuffer();
    unsigned char *mpBuffer;
    unsigned int muiBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;

    unsigned int muiUartNo;
    QueueHandle_t mhUartEventQueueHandle = nullptr;
    //String mData;
    //String mBuffer;
    String msApn;
    String msUser;
    String msPass;

    bool mbConnected = false;
    bool mbCommandMode = true;

    esp_cellular_netif_driver_t mModemNetifDriver = {0};

};




#endif // MAIN_CELLULAR_H_


