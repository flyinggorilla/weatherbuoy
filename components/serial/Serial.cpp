#include "Serial.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char tag[] = "Serial";

Serial::Serial(unsigned int uartNo, unsigned int gpioNum, int baudRate, unsigned int bufferSize) {
    uart_config_t uart_config = {
        .baud_rate = baudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
        //.rx_flow_ctrl_thresh = 0,  // ignore warning
        //.use_ref_tick = 0
    };
    muiUartNo = uartNo;
    ESP_ERROR_CHECK(uart_driver_install(muiUartNo, bufferSize, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(muiUartNo, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(muiUartNo, UART_PIN_NO_CHANGE, gpioNum, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    muiBufferSize = bufferSize;
    muiBufferPos = 0;
    muiBufferLen = 0;
    mpBuffer = (uint8_t *) malloc(muiBufferSize);
}

Serial::~Serial() {
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




