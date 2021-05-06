#ifndef MAIN_WATCHDOG_H_
#define MAIN_WATCHDOG_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


class Watchdog {
public:
	Watchdog(int seconds);
	virtual ~Watchdog();

    // clear must be called within "seconds" interval to prevent a reset
    void clear();

    // in case of power management induced changes, the watchdog can be extended accordingly.
    // the adjusted seconds will not reset already elapsed watchdog seconds!
    // void adjust(int seconds);
    
private:
    int miSeconds;
    TaskHandle_t mhTask = nullptr;
    void WatchdogTask();
    friend void fWatchdogTask(void *pvParameter);
};

#endif 
