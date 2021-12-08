#ifndef _N2KSTREAM_ESP32_H_
#define _N2KSTREAM_ESP32_H_

#include "N2kStream.h"

class N2kStream_esp32 : public N2kStream
{

protected:
   // Returns first byte if incoming data, or -1 on no available data.
   int read();

   // Write data to stream.
   size_t write(const uint8_t* data, size_t size);

public:
  N2kStream_esp32();
};

#endif
