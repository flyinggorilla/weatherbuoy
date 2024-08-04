#ifndef MAIN_CELLULAR_H_
#define MAIN_CELLULAR_H_

#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"


static const TickType_t UART_INPUT_TIMEOUT_CMDSHORT  =      1 * 1000 / portTICK_PERIOD_MS;  // 1s only used for quickly pinging
static const TickType_t UART_INPUT_TIMEOUT_CMDNORMAL =     30 * 1000 / portTICK_PERIOD_MS;
static const TickType_t UART_INPUT_TIMEOUT_CMDLONG   = 60 * 2 * 1000 / portTICK_PERIOD_MS; // 120s is defined in spec as max response time for such long running queries
static const TickType_t UART_INPUT_TIMEOUT_PPP       = 60 * 2 * 1000 / portTICK_PERIOD_MS;

class Cellular;


typedef struct esp_cellular_netif_driver_s {
    esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
    Cellular               *pCellular;        /*!< ptr to the esp_modem objects (DTE) */
} esp_cellular_netif_driver_t;

enum PowerMode {
    POWER_OFF,
    POWER_SLEEP,
    POWER_ON
};

enum ModemModel {
    MODEMNONE,
    MODEM7600,
    MODEM7670
};

class Cellular {
public:
    Cellular();

	/// @brief  Initialize. Only needed once. Starts the worker thread.
	/// @return true if successful
	bool Init(String apn, String user, String pass, String preferredOperator, int preferredNetwork); // call before Start()

    /// @brief Power up the Simcom modem
    /// @return true if successful
    bool PowerUp();

    /// @brief  Power down the Simcom modem
    /// @return true if successful
    bool PowerDown();

	virtual ~Cellular();
    void ReadSMS();

    // send SMS message
    // rsTo is a comma separted list of phone numbers e.g. "0664123456,08611234567"
    // rsMsg is the ASCII text message
    bool SendSMS(String &rsTo, String &rsMsg);

    bool Command(const char *sCommand,const char *sSuccess, String *sResponse = nullptr, const char *sInfo = nullptr, unsigned short maxLines = 100, TickType_t timeout = UART_INPUT_TIMEOUT_CMDNORMAL);
    bool SwitchToCommandMode(); // todo, move to private
    bool SwitchToPppMode(); // can be moved to private
    bool SwitchToSleepMode();
    bool SwitchToFullPowerMode();
    void QuerySignalStatus();

    unsigned long long getDataSent() { return mullSentTotal; };
    unsigned long long getDataReceived() { return mullReceivedTotal; };

    String msPreferredOperator;
    int miPreferredNetwork;

    String msOperator;
    String msHardware;
    String msSubscriber;
    String msNetworkmode;
    int    miSignalQuality = -1;

private:

    /// @brief configures Simcom modem
    /// @return true if config worked 
    bool ModemConfigure(); // call after Init()

    /// @brief runs hardware Simcom power ON sequence
    /// @return true if Modem is powered ON
    bool ModemPowerOnSequence();

    /// @brief runs hardware Simcom power ON sequence
    /// @return true if Modem is powered ON
    bool ModemPowerOffSequence();

    /// @brief initialize network drivers to work with PPP. call only once!
    /// @return true if successful
    bool InitNetwork();


    /// @brief establish network connection on top of every successful modem CONNECT
    /// @return true if successful
    bool PppNetifStart();

    /// @brief make sure to destroy the netif after use.
    bool PppNetifStop();

    /// @brief creates a new PPP network interface; destroys the old one if exists.
    bool PppNetifCreate();

    /// @brief check if Ppp netif interface and Ppp mode is enabled, and ready for data communication
    bool PppNetifUp();


    friend esp_err_t esp_cellular_post_attach_start(esp_netif_t * esp_netif, void * args);

 	void OnEvent(esp_event_base_t base, int32_t id, void *event_data);
    friend void cellularEventHandler(void* ctx, esp_event_base_t base, int32_t id, void* event_data);


    void ReceiverTask();
    friend void fReceiverTask(void *pvParameter);

    bool ModemReadLine(String& line, TickType_t timeout);
    bool ModemReadResponse(String &sResponse, const char *expectedLastLineResponse, unsigned short maxLines, TickType_t timeout);

    bool ModemWriteLine(const char *sWrite);
    bool ModemWrite(String &command);

    
    int ModemWriteData(const char* data, int len);
    friend esp_err_t esp_cellular_transmit(void *h, void *buffer, size_t len);
    friend void esp_cellular_free_rx_buffer(void *h, void *buffer);

    bool ReadIntoBuffer(TickType_t timeout);
    void ResetSerialBuffers();
    unsigned char *mpBuffer;
    unsigned int muiBufferSize;
    unsigned int muiBufferPos;
    unsigned int muiBufferLen;

    volatile unsigned long long mullSentTotal = 0;
    volatile unsigned long long mullReceivedTotal = 0;

    unsigned int muiUartNo;
    QueueHandle_t mhUartEventQueueHandle = nullptr;
    String msApn;
    String msUser;
    String msPass;

    bool mbCommandMode = true;
    //bool mbPowerSaverActive = false;
    PowerMode mPowerMode = POWER_OFF;
    int miPppPhase = NETIF_PPP_PHASE_DEAD;

    ModemModel mModemModel = ModemModel::MODEMNONE;

    SemaphoreHandle_t mxPppConnected;
    SemaphoreHandle_t mxPppPhaseDead;
    SemaphoreHandle_t mxUartCleared;

    esp_cellular_netif_driver_t mModemNetifDriver;
    esp_netif_t *mpEspNetif = nullptr;

};




#endif // MAIN_CELLULAR_H_


