#include "esputil.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* esp32_getresetreasontext(int reason) {
  const char* reasons[11] = { "Reset reason can not be determined", "Reset due to power-on event",  "Reset by external pin (not applicable for ESP32)", "Software reset via esp_restart", 
  "Software reset due to exception/panic", "Reset (software or hardware) due to interrupt watchdog", "Reset due to task watchdog", "Reset due to other watchdogs", 
  "Reset after exiting deep sleep mode", "Brownout reset (software or hardware)", "Reset over SDIO" };
  if ((reason < 0) || (reason >= 11)) 
    return "invalid reason";

  return reasons[reason];
}
