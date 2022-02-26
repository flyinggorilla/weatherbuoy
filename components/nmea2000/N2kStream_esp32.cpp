#include "N2kStream_esp32.h"
#include "esp_log.h"

static const char tag[] = "NMEA2000";

// Returns first byte if incoming data, or -1 on no available data.
int N2kStream_esp32::read(){
  return -1;
};

// Write data to stream.
size_t N2kStream_esp32::write(const uint8_t *data, size_t size){
  ESP_LOG_BUFFER_HEXDUMP(tag, data, size, ESP_LOG_INFO);
  return size;
};

N2kStream_esp32::N2kStream_esp32(){

};
