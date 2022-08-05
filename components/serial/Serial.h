#ifndef MAIN_SERIAL_H_
#define MAIN_SERIAL_H_

#include "Config.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"


class Serial {
public:
	Serial(unsigned int uartNo, unsigned int gpioRx, unsigned int gpioTx, int baudRate = 115200, unsigned int bufferSize = 1024);
	virtual ~Serial();
    
    // read until CRLF
    bool ReadLine(String& line, unsigned int timeoutms = 250);
    
    // read until data and CRLF
    bool ReadMultiLine(String& line, unsigned int timeoutms = 250);
    
    // attach serial
    bool Attach();

    // switch baud rate on the fly (e.g. to upgrade speed)
    bool SwitchBaudRate(int baudRate);

    // release serial
    bool Release();
    
    // Clear input buffers
    bool Flush(); 

    // write text or binary data to serial
    bool Write(const String& buffer);

    void dump();

private:
    bool ReadIntoBuffer(unsigned int timeoutms); // timeout 0 = try forever by default
    bool ReadIntoBuffer(); // timeout 0 = try forever by default
    unsigned char *mpBuffer;
    unsigned int muiBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;
    bool mbAttached = false;

    unsigned int muiUartNo;
    int miBaudRate;
    unsigned int mGpioRx;
    unsigned int mGpioTx;
};




#endif // MAIN_SERIAL_H_


