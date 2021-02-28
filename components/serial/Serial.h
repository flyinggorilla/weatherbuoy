#ifndef MAIN_SERIAL_H_
#define MAIN_SERIAL_H_

#include "Config.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"


class Serial {
public:
	Serial(unsigned int uartNo, unsigned int gpioNum, int baudRate, unsigned int bufferSize);
	virtual ~Serial();
    
    bool ReadLine(String& line);
    //bool ReadLine();
    //String& data() { return mData; }; 

private:
    bool ReadIntoBuffer();
    unsigned char *mpBuffer;
    unsigned int muiBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;

    unsigned int muiUartNo;
    //String mData;
    //String mBuffer;

};




#endif // MAIN_SERIAL_H_


