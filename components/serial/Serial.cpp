//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "Serial.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char tag[] = "Serial";




Serial::Serial(unsigned int uartNo, unsigned int gpioRx, unsigned int gpioTx, int baudRate, unsigned int bufferSize) {
    miBaudRate = baudRate;
    muiUartNo = uartNo;
    muiBufferSize = bufferSize;
    muiBufferPos = 0;
    muiBufferLen = 0;
    mGpioRx = gpioRx;
    mGpioTx = gpioTx;
    mpBuffer = (uint8_t *) malloc(muiBufferSize);
}

bool Serial::Attach() {
     
    uart_config_t uart_config = {
        .baud_rate = miBaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,  
        .source_clk = UART_SCLK_APB
        //.source_clk = UART_SCLK_REF_TICK // this should still work even when reducing power consumption with lowering clock speed (see power management docu)
    };

    // defaults are....
    // Serial0: RX0 on GPIO3, TX0 on GPIO1
    // Serial1: RX1 on GPIO9, TX1 on GPIO10 (+CTS1 and RTS1) -- UART1 connected to flash!!
    // Serial2: RX2 on GPIO16, TX2 on GPIO17 (+CTS2 and RTS2)
    if (uart_is_driver_installed(muiUartNo)) {
        ESP_LOGE(tag, "UART%d driver already installed!", muiUartNo);
    } 

    if (muiUartNo == UART_NUM_1) {
        ESP_LOGD(tag, "UART1 is used by flash memory. Be aware that you cannot use flash memory at the same time."); 
        //https://www.lucadentella.it/en/2017/11/06/esp32-26-uart/
    }
    ESP_ERROR_CHECK(uart_driver_install(muiUartNo, muiBufferSize, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(muiUartNo, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(muiUartNo, mGpioTx, mGpioRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    mbAttached = true;
    ESP_LOGI(tag, "Configured UART%d on pins RX=%d, TX=%d", muiUartNo, mGpioRx, mGpioTx);
    return true;
}

bool Serial::Release() {
    if (mbAttached) {
        mbAttached = false;
        ESP_ERROR_CHECK(uart_flush(muiUartNo));
        ESP_ERROR_CHECK(uart_driver_delete(muiUartNo));
        //############## RESTORE DEFAULT GPIO PINS ON UART1???
    }
    return true;
}


Serial::~Serial() {
    Release();
    free(mpBuffer);
}

bool Serial::Flush() {
    muiBufferPos = 0;
    muiBufferLen = 0;
    if (ESP_OK != uart_flush_input(muiUartNo)) {
        return false;
    }
    return true;
}

// try forever until 1 byte arrives
bool Serial::ReadIntoBuffer() {
    muiBufferPos = 0;
    muiBufferLen = 0;
    while (!muiBufferLen) {
        int len = uart_read_bytes(muiUartNo, mpBuffer, muiBufferSize, 250 / portTICK_RATE_MS);
        if (len < 0) {
            ESP_LOGE(tag, "Error reading from serial interface #%d", muiUartNo);
            return false;
        }
        muiBufferLen = len;
    }
    return true;
}

// timeout 0 = try forever by default
bool Serial::ReadIntoBuffer(unsigned int timeoutms) {
    muiBufferPos = 0;
    muiBufferLen = 0;
    size_t maxBytesToRead = 0;
    if (ESP_OK != uart_get_buffered_data_len(muiUartNo, &maxBytesToRead)) {
        return false;
    }

    //ESP_LOGI(tag, "Data in Buffer %d bytes", (unsigned int)maxBytesToRead);

    // if nothing is in buffer, then wait for timeout, otherwise read only whats in buffer
    if (!maxBytesToRead) {
        maxBytesToRead = 1;
    } else if (maxBytesToRead > muiBufferSize) {
        maxBytesToRead = muiBufferSize;
    }

    int len = uart_read_bytes(muiUartNo, mpBuffer, maxBytesToRead, timeoutms / portTICK_RATE_MS);
    if (len < 0) {
        ESP_LOGE(tag, "Error reading from serial interface #%d", muiUartNo);
        return false;
    }
    muiBufferLen = len;
    ESP_LOG_BUFFER_HEXDUMP(tag, mpBuffer, len, ESP_LOG_VERBOSE);
    return true;
}

bool Serial::ReadMultiLine(String& lines, unsigned int timeoutms) {
    lines = "";
    String line;
    while(ReadLine(line, timeoutms)) {
        if (!line.length()) {
            return true;
        }
        lines += line;
    }
    return false;
}

bool Serial::ReadLine(String& line, unsigned int timeoutms) {
    line = "";
    bool cr = false;
    bool crlf = true;
    int maxLineLength = muiBufferSize;
    while(maxLineLength) {
        if (muiBufferPos == muiBufferLen) {
            if (!ReadIntoBuffer(timeoutms))
                return false;
            if(!muiBufferLen) {
                return false;                
            }
        }
        if (muiBufferPos < muiBufferLen) {
            unsigned char c = mpBuffer[muiBufferPos++];
            switch (c)
            {
                case 0x0D:
                    cr = true;
                    break;
                case 0x0A:
                    crlf = cr;
                    return line;
                default:
                    cr = crlf = false;
                    line += (char)c;
            }
        } 
        maxLineLength--;
    }
    ESP_LOGE(tag, "No end of line found. Incorrect data or buffer too small.");
    return false;
}

bool Serial::Write(const String &buffer) {
    if (uart_write_bytes(muiUartNo, buffer.c_str(), buffer.length()) >= 0) {
        ESP_LOGD(tag, "Sent to UART: \"%s\"", buffer.c_str());
        //ESP_LOG_BUFFER_HEXDUMP(tag, buffer.c_str(), buffer.length(), ESP_LOG_INFO);
        return true;
    }
    ESP_LOGE(tag, "Error writing to UART %d: \"%s\"", muiUartNo, buffer.c_str());
    return false;
}



