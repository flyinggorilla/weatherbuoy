#ifndef MAIN_WATCHDOG_H_
#define MAIN_WATCHDOG_H_


class Watchdog {
public:
	Watchdog(int seconds);
	virtual ~Watchdog();
    void clear();
    
private:
    int miSeconds;
    bool mbReset;
    void WatchdogTask();
    friend void fWatchdogTask(void *pvParameter);
};

#endif 
