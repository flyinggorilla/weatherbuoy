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
    void Start();

    //main loop run by the task
    void ReadMaximetTask();

private:
    Config &mrConfig;
    SendData &mrSendData;
};

#endif 


