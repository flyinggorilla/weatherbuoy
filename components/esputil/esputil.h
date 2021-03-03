



#ifndef MAIN_ESPUTIL_H_
#define MAIN_ESPUTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * undocumented functions http://wiki.jackslab.org/ESP32_Onchip_Sensor
 */
uint8_t temprature_sens_read();

#ifdef __cplusplus
}
#endif


/* The temperature sensor has a range of -40C to 125C.
 * The absolute sensor results vary by chip. User calibration increases precision.
 */

float esp32_temperature() {
	return (float)temprature_sens_read() * (125.0+40.0) / 255.0 - 40.0;
}


#include "esp_system.h"

const char* esp32_getresetreasontext(int reason) {
  const char* reasons[11] = { "Reset reason can not be determined", "Reset due to power-on event",  "Reset by external pin (not applicable for ESP32)", "Software reset via esp_restart", 
  "Software reset due to exception/panic", "Reset (software or hardware) due to interrupt watchdog", "Reset due to task watchdog", "Reset due to other watchdogs", 
  "Reset after exiting deep sleep mode", "Brownout reset (software or hardware)", "Reset over SDIO" };
  if ((reason < 0) || (reason >= 11)) 
    return "invalid reason";

  return reasons[reason];
}

#endif 
