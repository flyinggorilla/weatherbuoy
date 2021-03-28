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

bool Serial::ReadIntoBuffer() {
    muiBufferPos = 0;
    muiBufferLen = 0;
    while (!muiBufferLen) {
        int len = uart_read_bytes(muiUartNo, mpBuffer, muiBufferSize, 1000 / portTICK_RATE_MS);
        if (len < 0) {
            ESP_LOGE(tag, "Error reading from serial interface #%d", muiUartNo);
            return false;
        }
        muiBufferLen = len;
    }
    return true;
}

bool Serial::ReadLine(String& line) {
    line = "";

    int maxLineLength = muiBufferSize;
    while(maxLineLength) {
        if (muiBufferPos == muiBufferLen) {
            if (!ReadIntoBuffer())
                return false;
        }
        if (muiBufferPos < muiBufferLen) {
            unsigned char c = mpBuffer[muiBufferPos++];
            if (c == 0x0D || c ==0x0A) {  // skip trailing line feeds \r\n
                if (line.length())
                    return true;
            } else {
                line += (char)c;
            }
        } 
        maxLineLength--;
    }
    ESP_LOGE(tag, "No end of line found. Incorrect data or buffer too small.");
    return false;
}




