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
    void ReceiverTask();
    
    bool ReadLine(String& line);
    bool WriteLine(const char *sWrite);
    int WriteData(const char* data, int len);
    String Command(const char *sCommand, const char *sInfo, unsigned short maxLines = 5);

    //bool ReadLine();
    //String& data() { return mData; }; 
    void TurnOn();

 	void OnEvent(esp_event_base_t base, int32_t id, void *event_data);


    bool StartPPP();



private:
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

    esp_cellular_netif_driver_t mModemNetifDriver = {0};

};




#endif // MAIN_CELLULAR_H_


