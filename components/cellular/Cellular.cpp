//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "sdkconfig.h"
#include "Cellular.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"

static const char tag[] = "Cellular";

// ------------------------------------------
// change device in Menuconfig->Cellular
// ------------------------------------------
#ifdef CONFIG_LILYGO_TTGO_TCALL14_SIM800
    // LILYGO TTGO T-Call V1.4 SIM800 
    #define CELLULAR_GPIO_PWKEY GPIO_NUM_4
    #define CELLULAR_GPIO_RST GPIO_NUM_5
    #define CELLULAR_GPIO_POWER GPIO_NUM_23 // Lilygo TTGO SIM800
    #define CELLULAR_GPIO_TX GPIO_NUM_27
    #define CELLULAR_GPIO_RX GPIO_NUM_26
    #define CELLULAR_GPIO_STATUS GPIO_NUM_32
    #define CELLULAR_DEFAULT_BAUD_RATE 115200
    #define SIM800 true
    #define CELLULAR_ACCELERATED_BAUD_RATE 460800 // alt: 230400
#elif CONFIG_LILYGO_TTGO_TPCIE_SIM7600
    // LILYGO® TTGO T-PCIE SIM7600
    #define CELLULAR_GPIO_PWKEY GPIO_NUM_4
    //#define CELLULAR_GPIO_RST GPIO_NUM_5
    #define CELLULAR_GPIO_POWER GPIO_NUM_25 // Lilygo T-PCIE SIM7600
    #define CELLULAR_GPIO_TX GPIO_NUM_27
    #define CELLULAR_GPIO_RX GPIO_NUM_26
    #define CELLULAR_GPIO_STATUS GPIO_NUM_36
    #define SIM7600 true
    #define CELLULAR_ACCELERATED_BAUD_RATE 
#endif

void cellularEventHandler(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
	return ((Cellular *)ctx)->OnEvent(base, id, event_data);
}

Cellular::Cellular() {
}

bool Cellular::InitModem(String apn, String user, String pass) {
    msApn = apn;
    msUser = user;
    msPass = pass;

    uart_config_t uart_config = {
        .baud_rate = CELLULAR_DEFAULT_BAUD_RATE,  // default baud rate, use AT+IPR command to set higher speeds 460800 is max of SIM800
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,  
        .source_clk = UART_SCLK_APB
    };
    muiUartNo = UART_NUM_2; // UART_NUM_1 is reserved for Maximet reading
    int bufferSize = 2048;
    static int iUartEventQueueSize = 10;  // was 5; ****************************** TEST WITH HIGHER VALUE
    ESP_ERROR_CHECK(uart_driver_install(muiUartNo, bufferSize, 0, iUartEventQueueSize, &mhUartEventQueueHandle, 0));
    ESP_ERROR_CHECK(uart_param_config(muiUartNo, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(muiUartNo, CELLULAR_GPIO_TX, CELLULAR_GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    muiBufferSize = bufferSize;
    muiBufferPos = 0;
    muiBufferLen = 0;
    mpBuffer = (uint8_t *) malloc(muiBufferSize);

    TurnOn();
    InitNetwork();
    return true;
}

Cellular::~Cellular() {
    free(mpBuffer);
}


esp_err_t esp_cellular_transmit(void *h, void *buffer, size_t len)
{
    Cellular *modem = (Cellular*)h;
    if (modem->mbCommandMode) {
        ESP_LOGE(tag, "esp_cellular_transmit() cannot send because we are still in command mode");
        return ESP_FAIL;
    }
    if (modem->ModemWriteData((const char *)buffer, len) > 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t esp_cellular_post_attach_start(esp_netif_t * esp_netif, void * args)
{
    esp_cellular_netif_driver_t *pDriver = (esp_cellular_netif_driver_t*)args;
    Cellular *pModem = pDriver->pCellular;
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
            .handle = pModem,
            .transmit = esp_cellular_transmit,
            .transmit_wrap = NULL,
            .driver_free_rx_buffer = NULL
    };
    pDriver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    // enable both events, so we could notify the modem layer if an error occurred/state changed
    esp_netif_ppp_config_t ppp_config = {
            .ppp_phase_event_enabled = true,
            .ppp_error_event_enabled = true
    };
    esp_netif_ppp_set_params(esp_netif, &ppp_config);
    ESP_LOGI(tag, "Netif Post-Attach called.");

    //ESP_LOGW(tag, "esp_cellular_post_attach_start() - register too many event handlers? NETIF_PPP_STATUS should have been already registered!!");
    //ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, cellularEventHandler, pModem));
    return ESP_OK;
    
//**************************************************    
    //return pModem->SwitchToPppMode() ? ESP_OK : ESP_ERR_ESP_NETIF_DRIVER_ATTACH_FAILED;
}



void fReceiverTask(void *pvParameter) {
	((Cellular*) pvParameter)->ReceiverTask();
	vTaskDelete(NULL);
}

void Cellular::Start() {
    ESP_LOGD(tag, "Starting receiver task....");
	xTaskCreate(&fReceiverTask, "ModemReceiver", 8192, this, ESP_TASK_MAIN_PRIO, NULL); //********************** HIGHER PRIO!! changed 
    ESP_LOGD(tag, "xTraskCreate done receiver task....");

    String response;
    if (!Command("AT", "OK", nullptr, "ATtention")) {
        ESP_LOGE(tag, "Severe problem, no connection to Modem");
    };

    #ifdef SIM7600
        if (!Command("ATE0", "OK", nullptr, "Echo off")) { // SIM7600
            ESP_LOGE(tag, "Could not turn off echo.");
        };
    #endif

    Command("ATI", "OK", &msHardware, "Display Product Identification Information"); // SIM800 R14.18
                                                                                    // SIM7600M21-A_V1.1
    Command("AT+CPIN?", "OK", &response, "Is a PIN needed?"); // +CPIN: READY
    if(!response.startsWith("+CPIN: READY")) {
        ESP_LOGE(tag, "Error, PIN required: %s", response.c_str());
    }


//    Command("AT+IPR=?", "OK", &response,  "Operator Selection"); // AT+IPR=?
                                                                // +IPR: (0,300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600,3000000,3200000,3686400)
//    ESP_LOGD(tag, "Baud Rates: %s", response.c_str());

    #ifdef SIM800
        if (Command("AT+IPR=460800", "OK", &response,  "Set 460800 baud rate. Default = 115200.")) {
            if (ESP_OK == uart_set_baudrate(muiUartNo, 460800)) {
                ESP_LOGI(tag, "Switched to 460800 baud");
            } else {
                ESP_LOGE(tag, "Error switching to 460800 baud");
            }
        } 
    #elif SIM7600
        if (response.lastIndexOf("3686400")) {
            if (Command("AT+IPR=3686400", "OK")) {
                if (ESP_OK == uart_set_baudrate(muiUartNo, 3686400)) {
                    ESP_LOGI(tag, "Switched to 3686400 baud");
                } else {
                    ESP_LOGE(tag, "Error switching to 3686400 baud");
                }
            }
        } 
        ESP_LOGD(tag, "Baud Rates: %s", response.c_str());
    #endif


    #ifdef SIM800
        int maxWaitForNetworkRegistration = 120;
        while (maxWaitForNetworkRegistration--) {
            if (Command("AT+CREG?", "OK", &response, "Network Registration Information States ")) { // +CREG: 0,2 // +CREG: 1,5
                if (response.indexOf("+CREG: ") >= 0 && response.indexOf(",5") >= 0)
                    break;
            }
            vTaskDelay(1000/portTICK_PERIOD_MS);
        } 
    #endif

    #ifdef SIM7600

        Command("AT+CNMP=?", "OK", nullptr, "List of preferred modes"); // +CNMP: (2,9,10,13,14,19,22,38,39,48,51,54,59,60,63,67)
        Command("AT+CNMP?", "OK", nullptr, "Preferred mode"); // +CNMP: 2 // 2 = automatic
        Command("AT+CNSMOD=?", "OK", nullptr, "List of network system modes"); // +CNSMOD: (0-1)
        Command("AT+CNSMOD?", "OK", nullptr, "Current network system mode"); // +CNSMOD: 0,4
                                                                        // 0 – no service
                                                                        // 1 – GSM
                                                                        // 2 – GPRS
                                                                        // 3 – EGPRS (EDGE)
                                                                        // 4 – WCDMA
                                                                        // 5 – HSDPA only(WCDMA)
                                                                        // 6 – HSUPA only(WCDMA)
                                                                        // 7 – HSPA (HSDPA and HSUPA, WCDMA)
                                                                        // 8 – LTE
                                                                        // 9 – TDS-CDMA
                                                                        // 10 – TDS-HSDPA only
                                                                        // 11 – TDS- HSUPA onl
        //Command("AT+CNMP=13", "OK", nullptr, "Set GSM mode"); 

        Command("AT+CEREG=?", "OK", nullptr, "List of EPS registration stati");
        Command("AT+CEREG?", "OK", nullptr, "EPS registration status"); // +CEREG: 0,4
                                                // 4 – unknown (e.g. out of E-UTRAN coverage)
                                                // 5 - roaming


        Command("AT+NETMODE?", "OK", nullptr, "EPS registration status"); // 2 – WCDMA mode(default)
        //Command("AT+CPOL?", "OK", nullptr, "Preferred operator list"); // 
        //Command("AT+COPN", "OK", nullptr, "Read operator names"); // ... +COPN: "23201","A1" ... // list of thousands!!!!!

        Command("AT+CGDATA=?", "OK", nullptr, "Read operator names"); // 


        int maxWaitForNetworkRegistration = 120;
        while (maxWaitForNetworkRegistration--) {
            if (Command("AT+CEREG?", "OK", &response, "Network Registration Information States ")) { // +CREG: 0,2 // +CREG: 1,5
                if (response.indexOf("+CEREG: ") >= 0 && response.indexOf(",5") >= 0)
                    break;
            }
            vTaskDelay(1000/portTICK_PERIOD_MS);
        } 
    #endif
    //vTaskDelay(10000/portTICK_PERIOD_MS);


    Command("AT+COPS?", "OK", &response,  "Operator Selection"); // +COPS: 0,0,"A1" // SIM800
                                                                // +COPS: 0,0,"yesss!",2 // SIM7600
    msOperator = "";
    if (response.startsWith("+COPS: ")) {
        msOperator = response.substring(response.indexOf(",\"")+2, response.lastIndexOf("\""));
    }
    ESP_LOGI(tag, "Operator: %s", msOperator.c_str());

    #ifdef SIM800    
        Command("AT+CROAMING", "OK", &response,  "Roaming State 0=home, 1=intl, 2=other"); // +CROAMING: 2
        if (response.startsWith("+CROAMING: 0")) {
            ESP_LOGI(tag, "No roaming.");
        } else if (response.startsWith("+CROAMING: 1")) {
            ESP_LOGW(tag, "International roaming.");
        } else if (response.startsWith("+CROAMING: 2")) {
            ESP_LOGI(tag, "Roaming into network %s", msOperator.c_str());
        } 
    #endif 

    Command("AT+CSQ", "OK", &response,  "Signal Quality Report"); // +CSQ: 13,0
    ESP_LOGI(tag, "Signal Quality: %s", response.substring(6).c_str());
    Command("AT+CNUM", "OK", &response,  "Subscriber Number"); // +CNUM: "","+43681207*****",145,0,4
    msSubscriber = "";
    if (response.startsWith("+CNUM: ")) {
        msSubscriber = response.substring(response.indexOf(",\"")+2, response.lastIndexOf("\","));
    }
    ESP_LOGI(tag, "Subscriber: %s", msSubscriber.c_str());

    

} 

void Cellular::ReceiverTask() {
    ESP_LOGD(tag, "Started Modem Receiver task");
    while(true) {
        // do nothing in command mode
        if (mbCommandMode) {
            vTaskDelay(1000/portTICK_PERIOD_MS);
            //ESP_LOGD(tag, "ReceiverTask() idle in command mode");
            continue;
        }

        // ppp mode
        bool read = ReadIntoBuffer();
        if (read) {
            esp_netif_receive(mModemNetifDriver.base.netif, mpBuffer, muiBufferLen, NULL);
            //ESP_LOGD(tag, "ReceiverTask() received %d bytes", muiBufferLen);
        } else {
            ESP_LOGE(tag, "ReceiverTask() ReadIntoBuffer return false");
        }
    }
}

void Cellular::InitNetwork() {
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, cellularEventHandler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, cellularEventHandler, this, nullptr));		

    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    mpEspNetif = esp_netif_new(&cfg);
    assert(mpEspNetif);

    mModemNetifDriver.base.post_attach = esp_cellular_post_attach_start;
    mModemNetifDriver.pCellular = this;
    //esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_PAP, msUser.c_str(), msPass.c_str());
    //esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_NONE, msUser.c_str(), msPass.c_str());
    if (ESP_OK == esp_netif_attach(mpEspNetif, &mModemNetifDriver)) {
        ESP_LOGI(tag, "Installed cellular network driver.");
    } else {
        ESP_LOGW(tag, "Failed to install cellular network driver. Retry later??");
    }
}

void Cellular::OnEvent(esp_event_base_t base, int32_t id, void* event_data)
{

    //ESP_LOGI(tag, "Cellular::OnEvent(base=%d, id=%d)", (int)base, (int)id);

    if (base == IP_EVENT) {
        ESP_LOGI(tag, "IP event! %d", id);
        if (id == IP_EVENT_PPP_GOT_IP) {
            esp_netif_dns_info_t dns_info;

            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            ESP_LOGI(tag, "Cellular Connect to PPP Server");
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            ESP_LOGI(tag, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(tag, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(tag, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            ESP_LOGI(tag, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            ESP_LOGI(tag, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            mbConnected = true;

            ESP_LOGI(tag, "GOT ip event!!!");
            esp_netif_action_connected(mpEspNetif, 0, 0, nullptr);
        } else if (id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGI(tag, "Cellular Disconnect from PPP Server");
            mbConnected = false;
        } else if (id == IP_EVENT_GOT_IP6) {
            ESP_LOGI(tag, "GOT IPv6 event!");

            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(tag, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        } else {
            ESP_LOGW(tag, "Unknown IP_EVENT! id=%d", id);
        }
    } else if (base == NETIF_PPP_STATUS) {
        const char  *status;
        switch (id) {
            case NETIF_PPP_ERRORNONE: 
                status = "No error.";
                break;
            case NETIF_PPP_ERRORPARAM: 
                status = "Invalid parameter.";
                break;
            case NETIF_PPP_ERROROPEN: 
                status = "Unable to open PPP session.";
                break;
            case NETIF_PPP_ERRORDEVICE: 
                status = "Invalid I/O device for PPP.";
                break;
            case NETIF_PPP_ERRORALLOC: 
                status = "Unable to allocate resources.";
                break;
            case NETIF_PPP_ERRORUSER: 
                status = "User interrupt.";
                break;
            case NETIF_PPP_ERRORCONNECT: 
                status = "Connection lost.";
                break;
            case NETIF_PPP_ERRORAUTHFAIL: 
                status = "Failed authentication challenge.";
                break;
            case NETIF_PPP_ERRORPROTOCOL: 
                status = "Failed to meet protocol.";
                break;
            case NETIF_PPP_ERRORPEERDEAD: 
                status = "Connection timeout";
                break;
            case NETIF_PPP_ERRORIDLETIMEOUT: 
                status = "Idle Timeout";
                break;
            case NETIF_PPP_ERRORCONNECTTIME: 
                status = "Max connect time reached";
                break;
            case NETIF_PPP_ERRORLOOPBACK: 
                status = "Loopback detected";
                break;
            case NETIF_PPP_PHASE_DEAD: 
                status = "NETIF_PPP_PHASE_DEAD";
                break;
            case NETIF_PPP_PHASE_MASTER: 
                status = "NETIF_PPP_PHASE_MASTER";
                break;
            case NETIF_PPP_PHASE_HOLDOFF: 
                status = "NETIF_PPP_PHASE_HOLDOFF";
                break;
            case NETIF_PPP_PHASE_INITIALIZE: 
                status = "NETIF_PPP_PHASE_INITIALIZE";
                break;
            case NETIF_PPP_PHASE_SERIALCONN: 
                status = "NETIF_PPP_PHASE_SERIALCONN";
                break;
            case NETIF_PPP_PHASE_DORMANT: 
                status = "NETIF_PPP_PHASE_DORMANT";
                break;
            case NETIF_PPP_PHASE_ESTABLISH: 
                status = "NETIF_PPP_PHASE_ESTABLISH";
                break;
            case NETIF_PPP_PHASE_AUTHENTICATE: 
                status = "NETIF_PPP_PHASE_AUTHENTICATE";
                break;
            case NETIF_PPP_PHASE_CALLBACK: 
                status = "NETIF_PPP_PHASE_CALLBACK";
                break;
            case NETIF_PPP_PHASE_NETWORK: 
                status = "NETIF_PPP_PHASE_NETWORK";
                break;
            case NETIF_PPP_PHASE_RUNNING: 
                status = "NETIF_PPP_PHASE_RUNNING";
                break;
            case NETIF_PPP_PHASE_TERMINATE: 
                status = "NETIF_PPP_PHASE_TERMINATE";
                break;
            case NETIF_PPP_PHASE_DISCONNECT: 
                status = "NETIF_PPP_PHASE_DISCONNECT";
                break;
            default:
                ESP_LOGW(tag, "Unknown Netif PPP event! id=%d", id);
                status = "UNKOWN";
         }

        if (id > NETIF_PPP_ERRORNONE && id < NETIF_PP_PHASE_OFFSET) {
            ESP_LOGE(tag, "Netif PPP Status Error: %s", status);
        } else {
            ESP_LOGD(tag, "Netif PPP Status: %s", status);
        }
    } else {
        ESP_LOGW(tag, "Unknown event! base=%d, id=%d", (int)base, id);
    }

}

bool Cellular::ReadIntoBuffer() {
    muiBufferPos = 0;
    muiBufferLen = 0;
    uart_event_t event;
    //Waiting for UART event.
    if(xQueueReceive(mhUartEventQueueHandle, (void * )&event, (portTickType)portMAX_DELAY)) {
        switch(event.type) {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA: {
                    int length = 0;
                    ESP_ERROR_CHECK(uart_get_buffered_data_len(muiUartNo, (size_t*)&length));        
                    int len = uart_read_bytes(muiUartNo, mpBuffer, muiBufferSize, 0); //100 / portTICK_RATE_MS); // wait 100ms to fill buffer
                    if (len < 0) {
                        ESP_LOGE(tag, "Error reading from serial interface #%d", muiUartNo);
                        return false;
                    }
                    muiBufferLen = len;

                    if(!mbCommandMode) {
                        muiReceivedTotal += len;
                        ESP_LOGD(tag, "<<<<<<< RECEIVED %d bytes <<<<<<<", len);
                        ESP_LOG_BUFFER_HEXDUMP(tag, mpBuffer, len, ESP_LOG_DEBUG);
                    }
                    return true; 
                }
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGW(tag, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                // uart_flush_input(muiUartNo);
                // xQueueReset(mhUartEventQueueHandle);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGE(tag, "ring buffer full");
                // If buffer full happened, you should consider encreasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(muiUartNo);
                xQueueReset(mhUartEventQueueHandle);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                if (event.timeout_flag && event.size == 0) {
                    mbCommandMode = true;
                } else {
                    uart_flush_input(muiUartNo);
                    ESP_LOGD(tag, "uart rx break");
                }
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGE(tag, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGE(tag, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                break;
            //Others
            default:
                ESP_LOGI(tag, "uart event type: %d", event.type);
                break;
        }
        return true;
    }   
    return false; 
}

bool Cellular::SwitchToCommandMode() {
    uart_event_t uartSwitchToPppEvent = {
        .type = UART_BREAK,
        .size = 0,
        .timeout_flag = true
    };

    //Waiting for UART event.
    // before xending the new message, run xQueueReset(????????????????)
    if(xQueueSend(mhUartEventQueueHandle, (void * )&uartSwitchToPppEvent, (portTickType)portMAX_DELAY)) {

        uart_flush(muiUartNo);

        //esp_netif_action_stop(mpEspNetif, 0, 0, nullptr);
        // esp_netif_action_disconnected

        // The +++ character sequence causes the TA to cancel the data flow over the 
        // AT interface and switch to Command mode. This allows you to enter AT 
        // Command while maintaining the data connection to the remote server. 
        // To prevent the +++ escape sequence from being misinterpreted as data, it 
        // should comply to following sequence: 
        // No characters entered for T1 time (1 second) 
        // "+++" characters entered with no characters in between (1 second) 
        // No characters entered for T1 timer (1 second) 
        // Switch to Command mode, otherwise go to step 1. 
        String write("+++");
        vTaskDelay(1000/portTICK_PERIOD_MS); // spec: 1s delay for the modem to recognize the escape sequence
        int iWriteLen = ModemWriteData(write.c_str(), write.length());
        vTaskDelay(2000/portTICK_PERIOD_MS);
        if (iWriteLen == write.length()) {
            ESP_LOGD(tag, "SwitchToCommandMode(%s):", write.c_str());
            //uart_flush(muiUartNo);
        } else {
            ESP_LOGE(tag, "Error SwitchToCommandMode(%s): ", write.c_str());
            return false;
        }
        String response;
        if (ModemReadResponse(response, "OK", 5)) {
            ESP_LOGI(tag, "Switched to command mode.");
            return true;
        } 

// 

    }

    ESP_LOGE(tag, "Error switching to command mode.");
    return false;
}

bool Cellular::SwitchToPppMode() {
    // reading on PPP handshake and LCP start frame https://lateblt.tripod.com/bit60.txt

    if (!mbCommandMode) {
        ESP_LOGW(tag, "SwitchToPppMode() already in PPP mode");
        return true;
    }

    ESP_LOGI(tag, "SwitchToPppMode()");
//********************************************************    String command = "AT+CGDCONT=1,\"IP\",\"";
    
    #ifdef SIM800
        String command = "AT+CGDCONT=1,\"IP\",\"";   // it did work with IP, but also with PPP?? 
        command += msApn;
        command += "\"";
        String response;
        if (!Command(command.c_str(), "OK", &response, "Define PDP Context")) {
            ESP_LOGE(tag, "SwitchToPppMode(PDP Context) FAILED");
            mbCommandMode = true;
            return false;
        }

        if (Command("ATD*99#", "CONNECT", &response, "Connect for data connection.")) {
            ESP_LOGI(tag, "SwitchToPppMode(NEW) CONNECTED");
            mbCommandMode = false;
            esp_netif_action_start(mpEspNetif, 0, 0, nullptr);
            return true;
        } 
    #endif

    #ifdef SIM7600
        String command = "AT+CGDCONT=1,\"PPP\",\"";   //#################### PPP or IP ?????
        command += msApn;
        command += "\"";
        String response;
        if (!Command(command.c_str(), "OK", &response, "Define PDP Context")) {
            ESP_LOGE(tag, "SwitchToPppMode(PDP Context) FAILED");
            mbCommandMode = true;
            return false;
        }

        if (Command("AT+CGDATA=\"PPP\",1", "CONNECT", &response, "Connect for data connection.")) {
            ESP_LOGI(tag, "SwitchToPppMode(NEW) CONNECTED");
            mbCommandMode = false;
            esp_netif_action_start(mpEspNetif, 0, 0, nullptr);
            return true;
        } else {
            ESP_LOGW(tag, "Not yet connected!! Later??"); //******************************************************
            mbCommandMode = false;
            esp_netif_action_start(mpEspNetif, 0, 0, nullptr);
            return true;
        }
    #endif

    Command("ATO", "CONNECT", &response, "resumes the connection and switches back from Command mode to data mode.");
    if (response.equals("CONNECT")) {
        ESP_LOGI(tag, "SwitchToPppMode(RESUME) CONNECTED");
        mbCommandMode = false;
        return true;
    }
    return false;
}

bool Cellular::ModemReadLine(String& line) {
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
                if (line.length()) {
                    //ESP_LOGI(tag, "ModemReadLine(): %s", line.c_str());
                    return true;
                }
            } else {
                line += (char)c;
            }
        } 
        maxLineLength--;
    }
    ESP_LOGE(tag, "No end of line found. Incorrect data or buffer too small.");
    return false;
}

int Cellular::ModemWriteData(const char* data, int len) {
    int iWriteLen = uart_write_bytes(muiUartNo, data, len);
    if (iWriteLen == len) {
        ESP_LOGD(tag, ">>>>>>> SENT %d bytes >>>>>>>", len);
        ESP_LOG_BUFFER_HEXDUMP(tag, data, len, ESP_LOG_DEBUG);
        //ESP_LOGI(tag, "ModemWriteData(): %d bytes", len);
        muiSentTotal += len;        
    } else {
        ESP_LOGE(tag, "Error ModemWriteData(): %d", len);
    }
    return len;
}

bool Cellular::ModemWrite(String &command) {
    int iWriteLen = uart_write_bytes(muiUartNo, command.c_str(), command.length());
    if (iWriteLen == command.length()) {
//        ESP_LOGD(tag, "ModemWrite(): '%s'", command.c_str());
        return true;
    } 
    
    ESP_LOGE(tag, "Error ModemWrite(): '%s'", command.c_str());
    return false;
}


bool Cellular::ModemWriteLine(const char *sWrite) {
    String write(sWrite);
    write += "\r";
    return ModemWrite(write);
}


bool Cellular::ModemReadResponse(String &sResponse, const char *expectedLastLineResponse, unsigned short maxLines) {
    String sLine;
    sResponse = "";
    while  (maxLines--) {
        if (!ModemReadLine(sLine))
            continue;
        //ESP_LOGI(tag, "LINE: {{%s}}", sLine.c_str());
        
        sResponse += sLine;
        if (sLine.equals(expectedLastLineResponse)) {
            //ESP_LOGI(tag, "Success: '%s', '%s'", sLine.c_str(), sResponse.c_str());
            return true;
        } else if (sLine.startsWith("ERROR") || sLine.startsWith("NO CARRIER")) {
            ESP_LOGE(tag, "Error expected %s: '%s', '%s'", expectedLastLineResponse, sLine.c_str(), sResponse.c_str());
            return false;
        } 
        if (sResponse.length()) {
            sResponse += "\r\n";
        } 
        if (!sLine.length()) {
            sResponse += "\r\n";
        }
    }
    ESP_LOGE(tag, "Error more lines than %d, expected %s: '%s', '%s'", maxLines, expectedLastLineResponse, sLine.c_str(), sResponse.c_str());
    return false;
}


bool Cellular::Command(const char* sCommand, const char *sSuccess, String *spResponse, const char *sInfo, unsigned short maxLines) {
    ESP_LOGD(tag, "Command(%s) %s", sCommand, sInfo);
    String response; 
    if (!spResponse) {
        spResponse = &response;
    }
    if (ModemWriteLine(sCommand)) {
        if (ModemReadResponse(*spResponse, sSuccess, maxLines)) {
            ESP_LOGD(tag, "%s --> Command(%s):\r\n%s", sInfo ? sInfo : "", sCommand, spResponse->c_str());
            return true;
        }
    }
    ESP_LOGE(tag, "%s --> Command(%s)=?%s:\r\n%s", sInfo ? sInfo : "", sCommand, sSuccess, spResponse->c_str());
    return false;
}




bool Cellular::TurnOn(void)
{
    #ifdef CONFIG_LILYGO_TTGO_TCALL14_SIM800
        gpio_set_direction(CELLULAR_GPIO_PWKEY, GPIO_MODE_OUTPUT);
        gpio_set_direction(CELLULAR_GPIO_POWER, GPIO_MODE_OUTPUT);

        ESP_LOGI(tag, "initializing modem...");
        gpio_set_level(CELLULAR_GPIO_PWKEY, 1);
        //gpio_set_level(CELLULAR_GPIO_RST, 0);
        gpio_set_level(CELLULAR_GPIO_POWER, 0);
        vTaskDelay(500/portTICK_PERIOD_MS);
        gpio_set_level(CELLULAR_GPIO_POWER, 1);
        vTaskDelay(500/portTICK_PERIOD_MS);
        gpio_set_level(CELLULAR_GPIO_PWKEY, 0);
        vTaskDelay(1000/portTICK_PERIOD_MS); // Power-Key must be down for at least 1 second
        gpio_set_level(CELLULAR_GPIO_PWKEY, 1);

        // wait at least 5 seconds for SIM800 to get ready
        vTaskDelay(5000/portTICK_PERIOD_MS); 
        ESP_LOGI(tag, "modem turned on.");
        return true;
    #endif

    #ifdef CONFIG_LILYGO_TTGO_TPCIE_SIM7600
        gpio_set_direction(CELLULAR_GPIO_PWKEY, GPIO_MODE_OUTPUT);
        gpio_set_direction(CELLULAR_GPIO_POWER, GPIO_MODE_OUTPUT);
        gpio_set_direction(GPIO_NUM_36, GPIO_MODE_INPUT);  ///######### TODO GPIO CONSTANT

        ESP_LOGI(tag, "initializing LILYGO T-PCIE SIM7600 modem...");
        // POWER_PIN : This pin controls the power supply of the SIM7600
        gpio_set_level(CELLULAR_GPIO_POWER, 1);

        // PWR_PIN ： This Pin is the PWR-KEY of the SIM7600
        gpio_set_level(CELLULAR_GPIO_PWKEY, 0);
        vTaskDelay(500/portTICK_PERIOD_MS); // for SIM7600 it is minimum 100ms, typical 500ms LOW
        gpio_set_level(CELLULAR_GPIO_PWKEY, 1);

        // wait at least 12 seconds for SIM7600 bit....
        int maxModemUartReadyTime = 30; // seconds
        while (maxModemUartReadyTime--) {
            if (gpio_get_level(CELLULAR_GPIO_STATUS)) {
                ESP_LOGI(tag, "Modem turned on.");
                return true;
            }
            vTaskDelay(1000/portTICK_PERIOD_MS);
            ESP_LOGD(tag, "still booting modem.... %d", maxModemUartReadyTime);
        }

    #endif

    ESP_LOGE(tag, "Could not turn on modem.");
    return false;
}


