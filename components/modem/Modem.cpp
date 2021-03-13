#include "Modem.h"
#include "EspString.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"



static const char tag[] = "Modem";

// SIM800L
#define MODEM_GPIO_PWKEY GPIO_NUM_4
#define MODEM_GPIO_RST GPIO_NUM_5
#define MODEM_GPIO_POWER GPIO_NUM_23
#define MODEM_GPIO_TX GPIO_NUM_27
#define MODEM_GPIO_RX GPIO_NUM_26

void modemEventHandler(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
	return ((Modem *)ctx)->OnEvent(base, id, event_data);
}

Modem::Modem(String apn, String user, String pass) {
    msApn = apn;
    msUser = user;
    msPass = pass;

    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    uart_config_t uart_config = {
        .baud_rate = 115200,  //**********************************
        //.baud_rate = 460800,  //**********************************
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
        //.rx_flow_ctrl_thresh = 0,  // ignore warning
        //.use_ref_tick = 0
    };
    //muiUartNo = uartNo;
    muiUartNo = UART_NUM_2; /////////////////////////////////////////////***********************************
    int bufferSize = 2048;
    ESP_ERROR_CHECK(uart_driver_install(muiUartNo, bufferSize, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(muiUartNo, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(muiUartNo, MODEM_GPIO_TX, MODEM_GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    muiBufferSize = bufferSize;
    muiBufferPos = 0;
    muiBufferLen = 0;
    mpBuffer = (uint8_t *) malloc(muiBufferSize);


/*

    // Set pattern interrupt, used to detect the end of a line. 
    res = uart_enable_pattern_det_baud_intr(esp_dte->uart_port, '\n', 1, MIN_PATTERN_INTERVAL, MIN_POST_IDLE, MIN_PRE_IDLE);
    // Set pattern queue size 
    esp_dte->pattern_queue_size = config->pattern_queue_size;
    res |= uart_pattern_queue_reset(esp_dte->uart_port, config->pattern_queue_size);
    // Starting in command mode -> explicitly disable RX interrupt 
    uart_disable_rx_intr(esp_dte->uart_port);
*/

}

Modem::~Modem() {
    free(mpBuffer);
}


static esp_err_t esp_modem_transmit(void *h, void *buffer, size_t len)
{
    Modem *modem = (Modem*)h;
    if (modem->WriteData((const char *)buffer, len) > 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

/*
static esp_err_t modem_netif_receive_cb(void *buffer, size_t len, void *context)
{
    esp_modem_netif_driver_t *driver = (esp_modem_netif_driver_t *)context;
    esp_netif_receive(driver->base.netif, buffer, len, NULL);
    return ESP_OK;
}
*/

static esp_err_t esp_modem_post_attach_start(esp_netif_t * esp_netif, void * args)
{
    esp_modem_netif_driver_t *pDriver = (esp_modem_netif_driver_t*)args;
    Modem *pModem = pDriver->pModem;
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
            .handle = pModem,
            .transmit = esp_modem_transmit,
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

    //ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, dte));
    return pModem->StartPPP() ? ESP_OK : ESP_ERR_ESP_NETIF_DRIVER_ATTACH_FAILED;
}

bool Modem::StartPPP() {
/*    modem_dce_t *dce = dte->dce;
    MODEM_CHECK(dce, "DTE has not yet bind with DCE", err);
    esp_modem_dte_t *esp_dte = __containerof(dte, esp_modem_dte_t, parent);
    // Set PDP Context 
    MODEM_CHECK(dce->define_pdp_context(dce, 1, "IP", CONFIG_EXAMPLE_COMPONENT_MODEM_APN) == ESP_OK, "set MODEM APN failed", err);
    // Enter PPP mode 
    MODEM_CHECK(dte->change_mode(dte, MODEM_PPP_MODE) == ESP_OK, "enter ppp mode failed", err);

    // post PPP mode started event 
    esp_event_post_to(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, ESP_MODEM_EVENT_PPP_START, NULL, 0, 0); */
    return true;
}


void fReceiverTask(void *pvParameter) {
	((Modem*) pvParameter)->ReceiverTask();
	vTaskDelete(NULL);
}

void Modem::Start() {
	xTaskCreate(&fReceiverTask, "ModemReceiver", 8192, this, ESP_TASK_MAIN_PRIO, NULL);
} 

void Modem::ReceiverTask() {
    while(true) {
        bool read = ReadIntoBuffer();
        if (read) {
            esp_netif_receive(mModemNetifDriver.base.netif, mpBuffer, muiBufferLen, NULL);
            ESP_LOGI(tag, "ReceiverTask() received %d bytes", muiBufferLen);
        }
    }
}



void Modem::InitNetwork() {
	//ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,&modemEventHandler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &modemEventHandler, this, NULL));		


    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    mModemNetifDriver.base.post_attach = esp_modem_post_attach_start;
    mModemNetifDriver.pModem = this;
    //esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_PAP, msUser.c_str(), msPass.c_str());
    esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_NONE, msUser.c_str(), msPass.c_str());
    ESP_ERROR_CHECK(esp_netif_attach(esp_netif, &mModemNetifDriver));
}

void Modem::OnEvent(esp_event_base_t base, int32_t id, void* event_data)
{

    ESP_LOGI(tag, "Modem::OnEvent(base=%d, id=%d)", (int)base, (int)id);

    if (base == IP_EVENT) {
        ESP_LOGI(tag, "IP event! %d", id);
        if (id == IP_EVENT_PPP_GOT_IP) {
            esp_netif_dns_info_t dns_info;

            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            ESP_LOGI(tag, "Modem Connect to PPP Server");
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            ESP_LOGI(tag, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(tag, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(tag, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            ESP_LOGI(tag, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            ESP_LOGI(tag, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            //************* TODO xEventGroupSetBits(event_group, CONNECT_BIT);
            mbConnected = true;

            ESP_LOGI(tag, "GOT ip event!!!");
        } else if (id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGI(tag, "Modem Disconnect from PPP Server");
            mbConnected = false;
        } else if (id == IP_EVENT_GOT_IP6) {
            ESP_LOGI(tag, "GOT IPv6 event!");

            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(tag, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }        
    }

}




 
bool Modem::ReadIntoBuffer() {
    muiBufferPos = 0;
    muiBufferLen = 0;
    while (!muiBufferLen) {
        int len = uart_read_bytes(muiUartNo, mpBuffer, muiBufferSize, 100 / portTICK_RATE_MS); // wait 100ms to fill buffer
        if (len < 0) {
            ESP_LOGE(tag, "Error reading from serial interface #%d", muiUartNo);
            return false;
        }
        muiBufferLen = len;
        /*String buf;
        buf.reserve(len);
        memcpy((void*)buf.c_str(), mpBuffer, len);
        ESP_LOGI(tag, "ReadIntoBuffer(): %s", buf.c_str());*/
    }
    return true;
}

bool Modem::ReadLine(String& line) {
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
                    //ESP_LOGI(tag, "ReadLine(): %s", line.c_str());
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

int Modem::WriteData(const char* data, int len) {
    int iWriteLen = uart_write_bytes(muiUartNo, data, len);
    if (iWriteLen == len) {
        ESP_LOGI(tag, "WriteData(): %d bytes", len);
    } else {
        ESP_LOGE(tag, "Error WriteData(): %d", len);
    }
    return len;
}


bool Modem::WriteLine(const char *sWrite) {
    String write(sWrite);
    write += "\r";
    int iWriteLen = uart_write_bytes(muiUartNo, write.c_str(), write.length());
    if (iWriteLen == write.length()) {
        ESP_LOGI(tag, "WriteLine(): '%s'", sWrite);
        return true;
    } else {
        ESP_LOGE(tag, "Error WriteLine(): '%s'", sWrite);
        return false;
    }
}

String Modem::Command(const char* sCommand, const char *sInfo, unsigned short maxLines) {
    String response;
    String message;
    ESP_LOGI(tag, "Command(%s) %s", sCommand, sInfo);
    if (WriteLine(sCommand)) {
        while  (maxLines--) {
            if (!ReadLine(response))
                continue;
            if (response.equals("OK")) {
                ESP_LOGI(tag, "%s", message.length() ? message.c_str() : response.c_str());
                return message.length() ? message : response;
            } else if (response.startsWith("ERROR")) {
                ESP_LOGE(tag, "Error Command(%s) %s", sCommand, response.c_str());
                return response;
            }
            if (message.length()) {
                message += "\r\n";
            } 
            if (!response.length()) {
                message += "\r\n";
            }
            message += response;
            //ESP_LOGI(tag, "LINE: %s", response.c_str());
        }
    }
    return response;
}




#define SIM800_PWKEY 4
#define set_sim800_pwkey() gpio_set_level(SIM800_PWKEY,1)
#define clear_sim800_pwkey() gpio_set_level(SIM800_PWKEY,0)

#define SIM800_RST 5
#define set_sim800_rst() gpio_set_level(SIM800_RST,1)
#define clear_sim800_rst() gpio_set_level(SIM800_RST,0)

#define SIM800_POWER 23
#define set_sim800_pwrsrc() gpio_set_level(SIM800_POWER,1)
#define clear_sim800_pwrsrc() gpio_set_level(SIM800_POWER,0)


void Modem::TurnOn(void)
{
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1<<MODEM_GPIO_PWKEY)+(1<<MODEM_GPIO_RST)+(1<<MODEM_GPIO_POWER);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(& io_conf);

    ESP_LOGI(tag, "shutdown...");
    gpio_set_level(MODEM_GPIO_PWKEY, 0);
    gpio_set_level(MODEM_GPIO_RST, 0);
    gpio_set_level(MODEM_GPIO_POWER, 0);
    vTaskDelay(1000/portTICK_PERIOD_MS);

    ESP_LOGI(tag, "init...");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(MODEM_GPIO_POWER, 1);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(MODEM_GPIO_PWKEY, 1);
    gpio_set_level(MODEM_GPIO_RST, 1);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(MODEM_GPIO_RST, 0);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(MODEM_GPIO_RST, 1);
    vTaskDelay(3000/portTICK_PERIOD_MS);
    ESP_LOGI(tag, "modem turned on.");
}


