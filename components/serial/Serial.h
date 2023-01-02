#ifndef MAIN_SERIAL_H_
#define MAIN_SERIAL_H_

#include "Config.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"


class Serial {
public:
	
    /// @brief Constructs a new synchroneous UART interface
    /// @param uartNo 
    /// @param gpioRx 
    /// @param gpioTx 
    /// @param baudRate 
    /// @param bufferSize buffer size in Serial object; default of 1024 bytes; 
    ///                   note that UART internal RX buffer will be initialized with 2*bufferSize
    Serial(unsigned int uartNo, unsigned int gpioRx, unsigned int gpioTx, int baudRate = 115200, unsigned int bufferSize = 1024);
	virtual ~Serial();
    
    /// @brief read until CRLF
    bool ReadLine(String& line, unsigned int timeoutms = 250);
    
    /// @brief  read until data and CRLF
    bool ReadMultiLine(String& line, unsigned int timeoutms = 250);
    
    /// @brief  attach serial
    bool Attach();

    /// @brief switch baud rate on the fly (e.g. to upgrade speed)
    bool SwitchBaudRate(int baudRate);

    /// @brief  release serial
    bool Release();
    
    /// @brief  Clear input buffers
    bool Flush(); 

    /// @brief  write text or binary data to serial
    bool Write(const String& buffer);

    void dump();

private:
    bool ReadIntoBuffer(unsigned int timeoutms); // timeout 0 = try forever by default
    bool ReadIntoBuffer(); // timeout 0 = try forever by default
    unsigned char *mpBuffer;
    unsigned int muiRxBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;
    bool mbAttached = false;

    unsigned int muiUartNo;
    int miBaudRate;
    unsigned int mGpioRx;
    unsigned int mGpioTx;
};




#endif // MAIN_SERIAL_H_


