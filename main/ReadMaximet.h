#ifndef MAIN_READMAXIMET_H_
#define MAIN_READMAXIMET_H_

#include "esp_event.h"
#include "EspString.h"
#include "SendData.h"
#include "Config.h"

class ReadMaximet {
public:
	ReadMaximet(Config &config, SendData &sendData);
	virtual ~ReadMaximet();

    //start the task
    void Start(int gpioRX, int gpioTX);

private:
    //main loop run by the task
    void ReadMaximetTask();
    friend void fReadMaximetTask(void *pvParameter);

    Config &mrConfig;
    SendData &mrSendData;
    int mgpioRX;
    int mgpioTX;
};

#endif 


