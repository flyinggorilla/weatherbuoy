#ifndef MAIN_MODEM_H_
#define MAIN_MODEM_H_

#include "Config.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"


class Modem;


typedef struct esp_modem_netif_driver_s {
    esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
    Modem                   *pModem;        /*!< ptr to the esp_modem objects (DTE) */
} esp_modem_netif_driver_t;


class Modem {
public:
	Modem(String apn, String user, String pass);
	virtual ~Modem();
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
    //String mData;
    //String mBuffer;
    String msApn;
    String msUser;
    String msPass;

    bool mbConnected = false;

    esp_modem_netif_driver_t mModemNetifDriver = {0};

};




#endif // MAIN_MODEM_H_


