#ifndef MAIN_SENDDATA_H_
#define MAIN_SENDDATA_H_

#include "esp_event.h"
#include "EspString.h"

class SendData {
public:
	SendData(int queueSize);
	virtual ~SendData();
	void EventHandler(int32_t id, void* event_data);

    /* @brief post string data to a message queue for sending. Data is copied into queue
     * 
    */
	bool Post(String &data);


private:
    esp_event_loop_handle_t mhLoopHandle;


};




#endif // MAIN_SENDDATA_H_


