#ifndef MAIN_CELLULAR_H_
#define MAIN_CELLULAR_H_

#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"


class Cellular;


typedef struct esp_cellular_netif_driver_s {
    esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
    Cellular               *pCellular;        /*!< ptr to the esp_modem objects (DTE) */
} esp_cellular_netif_driver_t;


class Cellular {
public:
    Cellular();
	bool InitModem(); // call before Start()
	virtual ~Cellular();
    void Start(String apn, String user, String pass, String preferredOperator, int preferredNetwork); // call after Init()
    void ReadSMS();
    bool Command(const char *sCommand,const char *sSuccess, String *sResponse = nullptr, const char *sInfo = nullptr, unsigned short maxLines = 100);
    bool SwitchToCommandMode(); // todo, move to private
    bool SwitchToPppMode(); // can be moved to private
    bool SwitchToLowPowerMode();
    bool SwitchToFullPowerMode();

    unsigned long long getDataSent() { return mullSentTotal; };
    unsigned long long getDataReceived() { return mullReceivedTotal; };

    String msPreferredOperator;
    int miPreferredNetwork;

    String msOperator;
    String msHardware;
    String msSubscriber;
    String msNetworkmode;
    int    miSignalQuality = -1;

private:
    bool PowerOn();
    void InitNetwork();

    friend esp_err_t esp_cellular_post_attach_start(esp_netif_t * esp_netif, void * args);

 	void OnEvent(esp_event_base_t base, int32_t id, void *event_data);
    friend void cellularEventHandler(void* ctx, esp_event_base_t base, int32_t id, void* event_data);


    void ReceiverTask();
    friend void fReceiverTask(void *pvParameter);

    bool ModemReadLine(String& line);
    bool ModemReadResponse(String &sResponse, const char *expectedLastLineResponse, unsigned short maxLines);

    bool ModemWriteLine(const char *sWrite);
    bool ModemWrite(String &command);

    
    int ModemWriteData(const char* data, int len);
    friend esp_err_t esp_cellular_transmit(void *h, void *buffer, size_t len);

    bool ReadIntoBuffer();
    void ResetInputBuffers();
    unsigned char *mpBuffer;
    unsigned int muiBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;

    volatile unsigned long long mullSentTotal = 0;
    volatile unsigned long long mullReceivedTotal = 0;

    unsigned int muiUartNo;
    QueueHandle_t mhUartEventQueueHandle = nullptr;
    //String mData;
    //String mBuffer;
    String msApn;
    String msUser;
    String msPass;

    bool mbConnected = false;
    bool mbCommandMode = true;
    bool mbPowerSaverActive = false;

    SemaphoreHandle_t mxConnected;

    esp_cellular_netif_driver_t mModemNetifDriver;
    esp_netif_t *mpEspNetif = nullptr;

};




#endif // MAIN_CELLULAR_H_


