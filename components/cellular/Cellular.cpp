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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char tag[] = "Cellular";

#if CONFIG_UART_ISR_IN_IRAM == 0
#error UART function must be configured into IRAM, otherwise OTA will fail. (CONFIG_UART_ISR_IN_IRAM=y)
#endif

#if CONFIG_LWIP_IP4_REASSEMBLY == 0
#error Config LWIP IP Reassembly must be enabled for proper TCP/IP communication over the Modem connection (CONFIG_LWIP_IP4_REASSEMBLY=y).
#endif

#ifdef CONFIG_LWIP_DHCP_DOES_ARP_CHECK
#error No need to perform ARP checks when connected as only device on the local network to Mobile provider (#undefined CONFIG_LWIP_DHCP_DOES_ARP_CHECK).
#endif

#ifdef CONFIG_LWIP_ESP_GRATUITOUS_ARP
#error No need to perform gratituitous ARP sends every 60 seconds for modem connection (#undefined CONFIG_LWIP_DHCP_DOES_ARP_CHECK).
#endif

#ifdef CONFIG_LWIP_PPP_ENABLE_IPV6
#error Avoid receiving an IPv6 IP address via PPP
#endif

#define CELLULAR_MAX_WAIT_NETWORK_REGISTRATION_SECONDS 120 // wait for modem to register with mobile network
#define CELLULAR_MAX_WAIT_GETIPADDRESS_SECONDS 120


// renew PPP network interface in case an error happens.
// #define RENEW_PPP_ON_NETIF_ERROR

// #if DHCP_DOES_ARP_CHECK == 1
// #warning Config LWIP IP Reassembly must be enabled for proper TCP/IP communication over the Modem connection (CONFIG_LWIP_IP4_REASSEMBLY=y).
// #endif

// #if IP_REASSEMBLY == 0
// #error Config LWIP IP Reassembly must be enabled for proper TCP/IP communication over the Modem connection (CONFIG_LWIP_IP4_REASSEMBLY=y).
// #endif

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
#define CELLULAR_ACCELERATED_BAUD_RATE 115200 * 2 // alt: 230400
#define CELLULAR_UART_EVENT_QUEUE_SIZE 8
#define CELLULAR_UART_RX_RECEIVE_BUFFER_SIZE 1024 * 4
#define CELLULAR_UART_SEND_BUFFER_SIZE 0 // 1024 * 2
#elif CONFIG_LILYGO_TTGO_TPCIE_SIM7600
// LILYGO® TTGO T-PCIE SIM7600
#define CELLULAR_GPIO_PWKEY GPIO_NUM_4
// #define CELLULAR_GPIO_RST GPIO_NUM_5
#define CELLULAR_GPIO_POWER GPIO_NUM_25 // Lilygo T-PCIE SIM7600
#define CELLULAR_GPIO_TX GPIO_NUM_27
#define CELLULAR_GPIO_RX GPIO_NUM_26
#define CELLULAR_GPIO_DTR GPIO_NUM_32 // Data Terminal Ready - used for sleep function
#define CELLULAR_GPIO_RI GPIO_NUM_33  // RING - used to wake
#define CELLULAR_GPIO_STATUS GPIO_NUM_36
#define CELLULAR_DEFAULT_BAUD_RATE 115200 * 1
#define CELLULAR_ACCELERATED_BAUD_RATE 115200 * 8 // '+IPR: (300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600,3000000,3200000,3686400)'
#define CELLULAR_UART_EVENT_QUEUE_SIZE 8
#define CELLULAR_UART_RX_RECEIVE_BUFFER_SIZE 1024 * 8
#define CELLULAR_UART_SEND_BUFFER_SIZE 0 // 0 ensures blocking writes //// was before: 1024 * 2
#endif

#ifdef UCS2SMS // to send SMS with non-ASCII characters
#include <locale>
#include <codecvt>

std::string wstringToUtf8(const std::wstring &str)

{

    std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;

    return strCnv.to_bytes(str);
}

std::wstring utf8ToWstring(const std::string &str)

{

    std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;

    return strCnv.from_bytes(str);
}
#endif

void fReceiverTask(void *pvParameter)
{
    ((Cellular *)pvParameter)->ReceiverTask();
    vTaskDelete(NULL);
}

void cellularEventHandler(void *ctx, esp_event_base_t base, int32_t id, void *event_data)
{
    return ((Cellular *)ctx)->OnEvent(base, id, event_data);
}

Cellular::Cellular()
{
    mxPppConnected = xSemaphoreCreateBinary();
    mxUartCleared = xSemaphoreCreateBinary();
    mxPppPhaseDead = xSemaphoreCreateBinary();
}

bool Cellular::Init(String apn, String user, String pass, String preferredOperator, int preferredNetwork)
{

    msApn = apn;
    msUser = user;
    msPass = pass;
    msPreferredOperator = preferredOperator;
    miPreferredNetwork = preferredNetwork;

    gpio_reset_pin(CELLULAR_GPIO_PWKEY);
    gpio_reset_pin(CELLULAR_GPIO_POWER);
    gpio_reset_pin(CELLULAR_GPIO_STATUS);
    gpio_reset_pin(CELLULAR_GPIO_DTR);
    gpio_set_direction(CELLULAR_GPIO_PWKEY, GPIO_MODE_OUTPUT);
    gpio_set_direction(CELLULAR_GPIO_POWER, GPIO_MODE_OUTPUT);
    gpio_set_direction(CELLULAR_GPIO_STATUS, GPIO_MODE_INPUT);
    gpio_set_direction(CELLULAR_GPIO_DTR, GPIO_MODE_OUTPUT);

    uart_config_t uart_config = {
        .baud_rate = CELLULAR_DEFAULT_BAUD_RATE, // default baud rate, use AT+IPR command to set higher speeds 460800 is max of CONFIG_LILYGO_TTGO_TCALL14_SIM800
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        //.source_clk = UART_SCLK_REF_TICK
        .source_clk = UART_SCLK_APB};
    muiUartNo = UART_NUM_2; // UART_NUM_1 is reserved for Maximet reading
    int bufferSize = CELLULAR_UART_RX_RECEIVE_BUFFER_SIZE;
    static int iUartEventQueueSize = CELLULAR_UART_EVENT_QUEUE_SIZE;
    ESP_ERROR_CHECK(uart_driver_install(muiUartNo, bufferSize, CELLULAR_UART_SEND_BUFFER_SIZE, iUartEventQueueSize, &mhUartEventQueueHandle, 0));
    ESP_ERROR_CHECK(uart_param_config(muiUartNo, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(muiUartNo, CELLULAR_GPIO_TX, CELLULAR_GPIO_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    muiBufferSize = bufferSize / 2;
    muiBufferPos = 0;
    muiBufferLen = 0;
    mpBuffer = (uint8_t *)malloc(muiBufferSize + 16);

    ESP_LOGD(tag, "Initializing network");
    InitNetwork();

    ESP_LOGD(tag, "Starting receiver task");
    xTaskCreate(&fReceiverTask, "ModemReceiver", 8192, this, ESP_TASKD_EVENT_PRIO, NULL);
    // #define ESP_TASK_TIMER_PRIO           (ESP_TASK_PRIO_MAX - 3)
    // #define ESP_TASKD_EVENT_PRIO          (ESP_TASK_PRIO_MAX - 5)
    // #define ESP_TASK_TCPIP_PRIO           (ESP_TASK_PRIO_MAX - 7)
    // #define ESP_TASK_MAIN_PRIO            (ESP_TASK_PRIO_MIN + 1)

    return true;
}

bool Cellular::PowerUp()
{
    if (!ModemPowerOnSequence())
        return false;

    if (!ModemConfigure())
        return false;

    return true;
}

bool Cellular::PowerDown()
{
    return ModemPowerOffSequence();
}

Cellular::~Cellular()
{
    free(mpBuffer);
}

esp_err_t esp_cellular_transmit(void *h, void *buffer, size_t len)
{
    Cellular *modem = (Cellular *)h;
    if (!modem)
    {
        ESP_LOGE(tag, "SEVERE, esp_cellular_transmit() called, but handle is invalid");
        return ESP_FAIL;
    }

    if (modem->mbCommandMode)
    {
        ESP_LOGE(tag, "esp_cellular_transmit() cannot send because we are still in command mode");
        return ESP_FAIL;
    }
    if (modem->ModemWriteData((const char *)buffer, len) > 0)
    {
        return ESP_OK;
    }
    return ESP_FAIL;
}

void esp_cellular_free_rx_buffer(void *h, void *buffer)
{
    Cellular *modem = (Cellular *)h;
    if (!modem)
    {
        ESP_LOGE(tag, "SEVERE, esp_cellular_free_rx_buffer() called, but handle is invalid");
        return;
    }

    if (modem->mbCommandMode)
    {
        ESP_LOGE(tag, "esp_cellular_free_rx_buffer(), cannot free because we are still in command mode");
    }

    ESP_LOGW(tag, "Netif driver. Free RX buffer.");
    void esp_netif_free_rx_buffer(void *h, void *buffer);
}

esp_err_t esp_cellular_post_attach_start(esp_netif_t *esp_netif, void *args)
{
    esp_cellular_netif_driver_t *pDriver = (esp_cellular_netif_driver_t *)args;
    Cellular *pModem = pDriver->pCellular;
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = pModem,
        .transmit = esp_cellular_transmit,
        .transmit_wrap = NULL,
        .driver_free_rx_buffer = esp_cellular_free_rx_buffer};
    pDriver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    // enable both events, so we could notify the modem layer if an error occurred/state changed
    esp_netif_ppp_config_t ppp_config = {
        .ppp_phase_event_enabled = true,
        .ppp_error_event_enabled = true};
    esp_netif_ppp_set_params(esp_netif, &ppp_config);
    ESP_LOGD(tag, "Netif Post-Attach called.");
    return ESP_OK;
}

const char *PppPhaseText(int pppPhase)
{
    switch (pppPhase)
    {
    case NETIF_PPP_PHASE_DEAD:
        return "NETIF_PPP_PHASE_DEAD";
    case NETIF_PPP_PHASE_MASTER:
        return "NETIF_PPP_PHASE_MASTER";
    case NETIF_PPP_PHASE_HOLDOFF:
        return "NETIF_PPP_PHASE_HOLDOFF";
    case NETIF_PPP_PHASE_INITIALIZE:
        return "NETIF_PPP_PHASE_INITIALIZE";
    case NETIF_PPP_PHASE_SERIALCONN:
        return "NETIF_PPP_PHASE_SERIALCONN";
    case NETIF_PPP_PHASE_DORMANT:
        return "NETIF_PPP_PHASE_DORMANT";
    case NETIF_PPP_PHASE_ESTABLISH:
        return "NETIF_PPP_PHASE_ESTABLISH";
    case NETIF_PPP_PHASE_AUTHENTICATE:
        return "NETIF_PPP_PHASE_AUTHENTICATE";
    case NETIF_PPP_PHASE_CALLBACK:
        return "NETIF_PPP_PHASE_CALLBACK";
    case NETIF_PPP_PHASE_NETWORK:
        return "NETIF_PPP_PHASE_NETWORK";
    case NETIF_PPP_PHASE_RUNNING:
        return "NETIF_PPP_PHASE_RUNNING";
    case NETIF_PPP_PHASE_TERMINATE:
        return "NETIF_PPP_PHASE_TERMINATE";
    case NETIF_PPP_PHASE_DISCONNECT:
        return "NETIF_PPP_PHASE_DISCONNECT";
    }
    return "unknown";
}

bool Cellular::ModemPowerOnSequence(void)
{
    // switch to default baud rate
    if (ESP_OK == uart_set_baudrate(muiUartNo, CELLULAR_DEFAULT_BAUD_RATE))
    {
        ESP_LOGI(tag, "Switched to %d baud", CELLULAR_DEFAULT_BAUD_RATE);
    }
    else
    {
        ESP_LOGE(tag, "Error switching to %d baud", CELLULAR_DEFAULT_BAUD_RATE);
    }

    ESP_LOGI(tag, "initializing LILYGO T-PCIE SIM7600 modem...");

    // DTR : set high, to enter sleep mode
    gpio_set_level(CELLULAR_GPIO_DTR, 0);

    // POWER_PIN : This pin controls the power supply of the SIM7600
    gpio_set_level(CELLULAR_GPIO_POWER, 0);
    gpio_set_level(CELLULAR_GPIO_PWKEY, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS); // for SIM7600 it is minimum 100ms, typical 500ms LOW
    gpio_set_level(CELLULAR_GPIO_POWER, 1);

    // PWR_PIN ： This Pin is the PWR-KEY of the SIM7600
    gpio_set_level(CELLULAR_GPIO_PWKEY, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS); // for SIM7600 it is minimum 100ms, typical 500ms LOW
    gpio_set_level(CELLULAR_GPIO_PWKEY, 1);

    // wait at least 12 seconds for SIM7600 bit....
    int maxModemUartReadyTime = 30; // seconds
    while (maxModemUartReadyTime--)
    {
        if (gpio_get_level(CELLULAR_GPIO_STATUS))
        {
            ESP_LOGI(tag, "Modem turned on.");
            mPowerMode = POWER_ON;
            break;
        }
        if (!maxModemUartReadyTime)
        {
            ESP_LOGE(tag, "Could not turn on modem!");
            return false;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGD(tag, "still booting modem.... %d", maxModemUartReadyTime);
    }

    // Only enter AT Command through serial port after SIM7500&SIM7600 Series is powered on and
    // Unsolicited Result Code "RDY" is received from serial port. If auto-bauding is enabled, the Unsolicited
    // Result Codes "RDY" and so on are not indicated when you start up the ME, and the "AT" prefix, or "at" prefix must be set at the beginning

    int maxModemReadyTime = 30; // seconds
    String line;
    while (maxModemReadyTime--)
    {
        if (ModemReadLine(line, UART_INPUT_TIMEOUT_CMDNORMAL))
            ESP_LOGD(tag, "ModemReadLine: '%s'", line.c_str());

        if (line.contains("RDY"))
        {
            ESP_LOGI(tag, "Modem ready.");
            mbCommandMode = true;
            return true;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGD(tag, "still waiting for modem to get ready.... %d", maxModemReadyTime);
    }

    ESP_LOGE(tag, "Could not turn on modem.");
    return false;
}

bool Cellular::ModemPowerOffSequence(void)
{
    ESP_LOGI(tag, "Powering down SIM7600 modem...");
    PppNetifStop();

    // DTR : set high, to enter sleep mode
    gpio_set_level(CELLULAR_GPIO_DTR, 0);

    // POWER_PIN : This pin controls the power supply of the SIM7600
    gpio_set_level(CELLULAR_GPIO_POWER, 0);
    gpio_set_level(CELLULAR_GPIO_PWKEY, 0); // must be minimum 2.5s low to power down

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // wait at least 26 seconds for SIM7600 bit....
    int maxModemPowerDownTime = 30; // seconds
    while (maxModemPowerDownTime--)
    {
        if (!gpio_get_level(CELLULAR_GPIO_STATUS))
        {
            ESP_LOGI(tag, "Modem turned off.");
            mPowerMode = POWER_OFF;
            return true;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGD(tag, "still powering down modem.... %d", maxModemPowerDownTime);
    }

    ESP_LOGE(tag, "Could not turn of modem!");
    return false;
}

bool Cellular::ModemConfigure()
{
    String response;
    String command;

    if (!Command("AT", "OK", nullptr, "ATtention"))
    {
        ESP_LOGE(tag, "Severe problem, no connection to Modem");
    };

    if (!Command("ATE0", "OK", nullptr, "Echo off"))
    { // CONFIG_LILYGO_TTGO_TPCIE_SIM7600
        ESP_LOGE(tag, "Could not turn off echo.");
    };

    Command("AT+CGMR", "OK", &response, "Display Firmware info"); // +CGMR: LE11B04SIM7600M21-A\r\nOK"
    if (response.startsWith("+CGMR: "))
    {
        int end = response.indexOf("\r\n");
        msHardware = response.substring(7, end);
        ESP_LOGI(tag, "Modem firmware: %s", msHardware.c_str());
    }
    else
    {
        ESP_LOGE(tag, "Error, checking modem firmware : %s", response.c_str());
    }

    Command("AT+CPIN?", "OK", &response, "Is a PIN needed?"); // +CPIN: READY
    if (!response.startsWith("+CPIN: READY"))
    {
        ESP_LOGE(tag, "Error, PIN required: %s", response.c_str());
    }

#if LOG_LOCAL_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG || LOG_DEFAULT_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG
    Command("AT+IPR=?", "OK", &response, "What serial speeds are supported?", UART_INPUT_TIMEOUT_CMDLONG);
    ESP_LOGD(tag, "Baud Rates: %s", response.c_str());
#endif

    command = "AT+IPR=";
    command += CELLULAR_ACCELERATED_BAUD_RATE;
    if (Command(command.c_str(), "OK", &response, "Set accelerated baud rate. Default = 115200."))
    {
        if (ESP_OK == uart_set_baudrate(muiUartNo, CELLULAR_ACCELERATED_BAUD_RATE))
        {
            ESP_LOGI(tag, "Switched to %d baud", CELLULAR_ACCELERATED_BAUD_RATE);
        }
        else
        {
            ESP_LOGE(tag, "Error switching to %d baud", CELLULAR_ACCELERATED_BAUD_RATE);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS); // one second delay seems to be mandatory after changing the baud rate; otherwise next AT command hangs
    }

    // ensure modem connection works properly after switching to high-speed by performing ATtention tests
    int attempts = 10;
    while (attempts--)
    {
        if (Command("AT", "OK", &response, "Attention."))
        {
            break;
        }
        if (attempts)
        {
            ESP_LOGE(tag, "SEVERE, could not enter commandline mode. %s. restarting.", response.c_str());
        }
        else
        {
            ESP_LOGD(tag, "Retrying testing command mode due to %s. Remaining attempts %i", response.c_str(), attempts);
        }
    }

    // this should enable NITZ network time and timezone update.
    // if (Command("AT+CTZU=1", "OK", &response, "Enable Automatic time and time zone update"))
    //{
    //    ESP_LOGW(tag, "Disable automatic time and time zone update. %s", response.c_str());
    //}

    // only query the list of operators in debug mode or when preferred operator is NOT set
    if (!msPreferredOperator.length() || LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
    {
        int listOperatorsTries = 30;
        while (listOperatorsTries--)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            // this command may run longer to find all the operators
            if (Command("AT+COPS=?", "OK", &response, "List operators", 200, UART_INPUT_TIMEOUT_CMDLONG))
            {
                ESP_LOGI(tag, "Operators: %s", response.c_str());
                break;
            }
            if (listOperatorsTries)
            {
                ESP_LOGI(tag, "still waiting for list of operators.... %d", listOperatorsTries);
            }
            else
            {
                ESP_LOGE(tag, "could not retrieve list of operators");
            }
        }
    }

    // Setting the preferred Operator is super important. e.g. for Yesss.at use "A1" as operator.
    command = "AT+COPS=0,0,\"";
    command += msPreferredOperator;
    command += "\"";
    if (Command(command.c_str(), "OK", &response, "Set preferred operator"))
    {
        ESP_LOGI(tag, "Set preferred operator to '%s'", msPreferredOperator.c_str());
    }
    else
    {
        ESP_LOGW(tag, "Could not set preferred operator to '%s': %s", msPreferredOperator.c_str(), response.c_str());
    }

    command = "AT+CNMP=";
    command += miPreferredNetwork;
    if (Command(command.c_str(), "OK", &response, "Set preferred network"))
    {
        ESP_LOGI(tag, "Set preferred network to '%d'", miPreferredNetwork);
    }
    else
    {
        ESP_LOGW(tag, "Could not set preferred network to %i: %s", miPreferredNetwork, response.c_str());
    }

    
    int startTimeSeconds = esp_timer_get_time() / 1000000; 
    while (true)
    {
        if (Command("AT+CREG?", "OK", &response, "Network Registration Information States "))
        { // +CREG: 0,2 // +CREG: 1,5
            if (response.indexOf("+CREG: ") >= 0 && ((response.indexOf(",5") >= 0) || (response.indexOf(",1") >= 0)))
                break;
        }
        // 0 – not registered, ME is not currently searching a new operator to register to
        // 1 – registered, home network
        // 2 – not registered, but ME is currently searching a new operator to register to
        // 3 – registration denied
        // 4 – unknown
        // 5 – registered, roaming

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        int timeCurrentSeconds = esp_timer_get_time() / 1000000; // seconds
        int waitTimeSeconds = timeCurrentSeconds - startTimeSeconds;
        if (waitTimeSeconds > 10) {
            ESP_LOGW(tag, "Waiting for network already for %d seconds", waitTimeSeconds);
        }

        if (waitTimeSeconds > CELLULAR_MAX_WAIT_NETWORK_REGISTRATION_SECONDS) {
            ESP_LOGE(tag, "Registering with network failed after waiting for %d seconds", waitTimeSeconds);
            return false;
        }
    }

    msNetworkmode = "";
    if (Command("AT+CNSMOD?", "OK", &response, "Current network system mode"))
    { // +CNSMOD: 0,4
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
        // 12 – TDS- HSPA (HSDPA and HSUPA)
        // 13 – CDMA
        // 14 – EVDO
        // 15 – HYBRID (CDMA and EVDO)
        // 16 – 1XLTE(CDMA and LTE)
        // 23 – eHRPD
        // 24 – HYBRID(CDMA and eHRPD)
        msNetworkmode = response.substring(response.indexOf(",") + 1, response.lastIndexOf("\""));
        int iMode = msNetworkmode.toInt();
        const char *modes[25] = {"no service", "GSM", "GPRS", "EGPRS (EDGE)", "WCDMA", "HSDPA only(WCDMA)",
                                 "HSUPA only(WCDMA)", "HSPA (HSDPA and HSUPA, WCDMA)", "LTE",
                                 "TDS-CDMA", "TDS-HSDPA only", "TDS- HSUPA only", "TDS- HSPA (HSDPA and HSUPA)",
                                 "CDMA", "EVDO", "HYBRID (CDMA and EVDO)", "1XLTE(CDMA and LTE)",
                                 "eHRPD", "HYBRID(CDMA and eHRPD)"};

        if (iMode >= 0 && iMode < (sizeof(modes) / sizeof(modes[0])))
        {
            msNetworkmode = modes[iMode];
        }
    }
    ESP_LOGI(tag, "Network mode: %s", msNetworkmode.c_str());

    Command("AT+COPS?", "OK", &response, "Operator Selection"); // +COPS: 0,0,"A1" // CONFIG_LILYGO_TTGO_TCALL14_SIM800
                                                                // +COPS: 0,0,"yesss!",2 // SIM7600
    msOperator = "";
    if (response.startsWith("+COPS: "))
    {
        msOperator = response.substring(response.indexOf(",\"") + 2, response.lastIndexOf("\""));
    }
    ESP_LOGI(tag, "Operator: %s", msOperator.c_str());

    // set PDP context 1 to providers APN
    command = "AT+CGDCONT=1,\"IP\",\"";
    command += msApn;
    command += "\"";
    if (Command(command.c_str(), "OK", &response, "Define PDP Context"))
    {
        ESP_LOGI(tag, "PDP context APN: %s", msApn.c_str());
    }
    else
    {
        ESP_LOGE(tag, "Could not set PDP context for APN: %s, %s", msApn.c_str(), response.c_str());
    }

#if LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE
    if (Command("AT+CGDCONT?", "OK", &response, "Read PDP Context"))
    { // +CGDCONT: 1,"IP","webapn.at","0.0.0.0",0,0,0,0
        ESP_LOGD(tag, "Configured PDP context: %s", response.c_str());
    }
#endif

    // retrieve Signal quality only when it hasnt been set yet. 
    // In other cases it is better to retrieve signal quality at end of data transfer
    if (miSignalQuality < 0) {
        if (Command("AT+CSQ", "OK", &response, "Signal Quality Report"))
        { // +CSQ: 13,0
            String sq;
            sq = response.substring(6, response.indexOf(","));
            miSignalQuality = sq.toInt();
            ESP_LOGI(tag, "Signal Quality: %i, (%s)", miSignalQuality, sq.c_str());
        }
        else
        {
            miSignalQuality = -1;
        }        
    }


    Command("AT+CNUM", "OK", &response, "Subscriber Number"); // +CNUM: "","+43681207*****",145,0,4
    msSubscriber = "";
    if (response.startsWith("+CNUM: "))
    {
        msSubscriber = response.substring(response.indexOf(",\"") + 2, response.lastIndexOf("\","));
    }
    ESP_LOGI(tag, "Subscriber: %s", msSubscriber.c_str());

    Command("AT+CGNSSMODE=0,1", "OK", &response, "Disable GPS mode");
    ESP_LOGI(tag, "Disable GPS mode: %s", response.c_str());

    if (Command("AT+CSCLK=1", "OK", &response, "Enable sleep via UART DTR."))
    { // mode 4 would shut down RF entirely to "flight-mode"; mode 0 still keeps SMS receiption intact
        ESP_LOGI(tag, "Enabled modem to sleep via UART DTR.");
    }

    /*
    if (Command("AT+CCLK?", "OK", &response, "Query modem system time/date"))
    {
        // String type value; format is “yy/MM/dd,hh:mm:ss±zz”, where
        // characters indicate year (two last digits), month, day, hour, minutes, seconds and time zone (indicates the difference, expressed in
        // quarters of an hour, between the local time and GMT; three last digits
        // are mandatory, range -47…+48). E.g. 6th of May 2008, 14:28:10 GMT+8 equals to “08/05/06,14:28:10+32”.
        // NOTE:   1. Time zone is nonvolatile, and the factory value is invalid time zone.
        //        2. Command +CCLK? will return time zone when time zone is valid, and if time zone is 00, command +CCLK? will return “+00”, but not “-00”
        ESP_LOGW(tag, "Network time/date: %s", response.c_str());
    }
    */

    return true;
}

void Cellular::ReadSMS()
{
    // READ SMS
    String response;
    Command("AT+CMGF=1", "OK", nullptr, "If the modem reponds with OK this SMS mode is supported");
    Command("AT+CMGL=\"ALL\"", "OK", &response, "dump unread SMS", 1000); //
    ESP_LOGI(tag, "SMS '%s'", response.c_str());
}

bool Cellular::SendSMS(String &rsTo, String &rsMsg)
{
    // SEND SMS
    ESP_LOGI(tag, "enabling text mode");
    String response;
    bool ret = Command("AT+CMGF=1", "OK", &response, "Switch modem to sending SMS in text.");
    ESP_LOGI(tag, "enabling text mode %s. response %s", ret ? "succeeded" : "failed", response.c_str());

    ret = Command("AT+CMGS=?", "OK", &response, "Query SMS sending capability SMS.");
    ESP_LOGI(tag, "query SMS sending %s. response %s", ret ? "succeeded" : "failed", response.c_str());

#ifdef UCS2SMS
    ret = Command("AT+CSCS=?", "OK", &response, "Query SMS character sets.");
    ESP_LOGI(tag, "query SMS character sets %s. response %s", ret ? "succeeded" : "failed", response.c_str());
    String characterSets;
    if (response.startsWith("+CSCS: "))
    {
        characterSets = response.substring(7);
        ESP_LOGI(tag, "Possible SMS character sets %s", characterSets.c_str());
    }
    ret = Command("AT+CSCS?", "OK", &response, "Query SMS character set.");
    ESP_LOGI(tag, "query SMS character sets %s. response %s", ret ? "succeeded" : "failed", response.c_str());
    if (response.startsWith("+CSCS: "))
    {
        characterSets = response.substring(7);
        ESP_LOGI(tag, "Configured SMS character sets %s", characterSets.c_str());
    }
    ret = Command("AT+CSCS=\"UCS2\"", "OK", &response, "Query SMS character set.");
    ESP_LOGI(tag, "query SMS character sets %s. response %s", ret ? "succeeded" : "failed", response.c_str());
#endif

    String command;
    static const char CTRLZ = 0x1A;
    // static const char ESC = 0x1B;
    ESP_LOGI(tag, "Sending SMS to:'%s', message:'%s'", rsTo.c_str(), rsMsg.c_str());
    command.printf("AT+CMGS=\"%s\"\r\n", rsTo.c_str());
    if (!Command(command.c_str(), ">", &response, "prepare to send SMS text"))
    {
        ESP_LOGE(tag, "ERROR preparing SMS to: %s, response: %s", rsMsg.c_str(), response.c_str());
        return false;
    }
#ifdef UCS2SMS
    String hexUcs2Msg;
    // convert UTF-8 to UCS-2
    std::wstring ucs2Msg = utf8ToWstring("testmsg😁");
    // convert bytes to hex
    // limit to 70 UCS-2 chars
    command = hexUcs2Msg;
#endif
    String smsMsg = rsMsg;
    smsMsg.replace("\r\n", "\r");
    command = smsMsg;
    command += CTRLZ;
    if (!Command(command.c_str(), "OK", &response, "Sending SMS message."))
    {
        ESP_LOGI(tag, "ERROR sending SMS to: %s, msg: %s, response: %s", rsTo.c_str(), rsMsg.c_str(), response.c_str());
        return false;
    }
    ESP_LOGI(tag, "Successfully sent SMS to: %s, msg: %s", rsTo.c_str(), rsMsg.c_str());
    return true;
}
// "+491575123456"
// > Testing message... [Ctrl+Z]
// +CMGS: 1

void Cellular::ReceiverTask()
{
    ESP_LOGD(tag, "Started Modem Receiver task");
    while (true)
    {
        // do nothing in command mode
        if (mbCommandMode)
        {
            vTaskDelay(250 / portTICK_PERIOD_MS);
            continue;
        }

        // ppp mode
        bool read = ReadIntoBuffer(UART_INPUT_TIMEOUT_PPP);
        if (read)
        {
            if (muiBufferLen)
            {
                if (mbCommandMode)
                {
                    ESP_LOGW(tag, "ReceiverTask(CMD) dropping remaining PPP data from UART");
                }
                else if (mpEspNetif)
                {
                    esp_netif_receive(mpEspNetif, mpBuffer, muiBufferLen, NULL);
                }
            }
        }
        else
        {
            PppNetifStop();
            ESP_LOGD(tag, "ReceiverTask(%s) %is timeout ReadIntoBuffer returned false. Stopped PPP network interface.",
                     mbCommandMode ? "CMD" : "DATA", (int)(UART_INPUT_TIMEOUT_PPP * portTICK_PERIOD_MS / 1000));
        }
    }
}

bool Cellular::InitNetwork()
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, cellularEventHandler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, cellularEventHandler, this, nullptr));

    return PppNetifCreate();
    // return true;
}

bool Cellular::PppNetifCreate()
{

    if (mpEspNetif)
    {
        esp_netif_destroy(mpEspNetif);
        mModemNetifDriver.base.netif = nullptr;
        mpEspNetif = nullptr;
        ESP_LOGW(tag, "Netif destroyed. Ready to renew.");
    }

    // Init netif object
    esp_netif_config_t espNetifConfig = ESP_NETIF_DEFAULT_PPP();
    mpEspNetif = esp_netif_new(&espNetifConfig);
    assert(mpEspNetif);

    mModemNetifDriver.base.post_attach = esp_cellular_post_attach_start;
    mModemNetifDriver.pCellular = this;
    // esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_PAP, msUser.c_str(), msPass.c_str());
    // esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_NONE, msUser.c_str(), msPass.c_str());
    if (ESP_OK == esp_netif_attach(mpEspNetif, &mModemNetifDriver))
    {
        ESP_LOGI(tag, "Installed cellular network driver.");
        return true;
    }

    ESP_LOGE(tag, "SEVERE, Failed to install cellular network driver.");
    return false;
}

bool Cellular::PppNetifUp()
{
    if (!mbCommandMode && mpEspNetif && esp_netif_is_netif_up(mpEspNetif))
    {
        return true;
    }
    return false;
}

bool Cellular::PppNetifStop()
{
    if (mbCommandMode)
    {
        return true;
    }

    int attempts = 3;
    while (attempts--)
    {
        // stop Ppp activity and clear UART
        esp_netif_action_stop(mpEspNetif, 0, 0, nullptr);
        if (xSemaphoreTake(mxPppPhaseDead, 30 * 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            mbCommandMode = true;
            ESP_LOGI(tag, "Netif action stopped");
            ResetSerialBuffers();
            return true;
        }
        ESP_LOGW(tag, "Netif not DEAD after 30 seconds, which was left in phase %i %s, remaining attempts: %i", miPppPhase, PppPhaseText(miPppPhase), attempts);
    }

    ESP_LOGE(tag, "Netif not DEAD, recreating Netif, which was left in phase %i, %s", miPppPhase, PppPhaseText(miPppPhase));
    return false;
}

bool Cellular::PppNetifStart()
{
    // create the network interface if not yet exists
    if (!mpEspNetif)
    {
        if (!PppNetifCreate())
        {
            return false;
        }
    }

    mbCommandMode = false;
    esp_netif_action_start(mpEspNetif, 0, 0, nullptr);
    ESP_LOGI(tag, "PPP network interface started.");

    return true;
}

void Cellular::ResetSerialBuffers()
{
    ESP_LOGD(tag, "ResetInputbuffers(Mode=%s)", mbCommandMode ? "CMD" : "DATA");
    uart_flush_input(muiUartNo);
    uart_wait_tx_done(muiUartNo, 1000 / portTICK_PERIOD_MS);
    muiBufferLen = 0;
    muiBufferPos = 0;
}

bool Cellular::ReadIntoBuffer(TickType_t timeout)
{
    muiBufferPos = 0;
    muiBufferLen = 0;
    uart_event_t event;
    event.type = UART_EVENT_MAX;
    event.size = 0;
    event.timeout_flag = 0;
    // Waiting for UART event.
    if (pdTRUE != xQueueReceive(mhUartEventQueueHandle, (void *)&event, timeout))
    {
        ESP_LOGW(tag, "ReadIntoBuffer timeout at xQueueReceive. Mode=%s", mbCommandMode ? "CMD" : "DATA");
        return false;
    }

    switch (event.type)
    {
    // Event of UART receving data
    // We'd better handler data event fast, there would be much more data events than
    // other types of events. If we take too much time on data event, the queue might
    // be full.
    case UART_DATA:
    {
        int len = uart_read_bytes(muiUartNo, mpBuffer, muiBufferSize, 0);
        if (len < 0)
        {
            ESP_LOGE(tag, "Error reading uart.");
            muiBufferLen = 0;
            return false;
        }
        muiBufferLen = len;

        if (len)
        {
            mullReceivedTotal += len;
            ESP_LOGV(tag, "UART %s received: %d", mbCommandMode ? "RESPONSE" : "DATA", len);
            ESP_LOG_BUFFER_HEXDUMP(tag, mpBuffer, len, ESP_LOG_VERBOSE);
        }
        return true;
    }
    // Event of HW FIFO overflow detected
    case UART_FIFO_OVF:
        // If fifo overflow happened, you should consider adding flow control for your application.
        // The ISR has already reset the rx FIFO,
        // As an example, we directly flush the rx buffer here in order to read more data.
        ResetSerialBuffers();
        ESP_LOGE(tag, "uart hw fifo overflow");
        return false;
    // Event of UART ring buffer full
    case UART_BUFFER_FULL:
        // If buffer full happened, you should consider increasing your buffer size
        // As an example, we directly flush the rx buffer here in order to read more data.
        ESP_LOGE(tag, "uart ring buffer full");
        ResetSerialBuffers();
        return false;
    case UART_BREAK:
        ESP_LOGD(tag, "uart break event.");
        return true;
    case UART_DATA_BREAK:
        // stopping data mode, to change to command mode
        ESP_LOGD(tag, "uart data break event.");
        return true; // important to return TRUE, as this event only indicates end of PPP, but no data corruption!!
    case UART_PARITY_ERR:
        ESP_LOGE(tag, "uart parity error");
        ResetSerialBuffers();
        return false;
    case UART_FRAME_ERR:
        ESP_LOGE(tag, "uart frame error");
        ResetSerialBuffers();
        return false;
    case UART_PATTERN_DET:
        break;
    default:
        ESP_LOGW(tag, "uart event type: %d", event.type);
        break;
    }
    return true;
}

bool Cellular::SwitchToSleepMode()
{
    if (!SwitchToCommandMode())
    {
        return false;
    }

    QuerySignalStatus();

    String response;
    if (Command("AT+CFUN=0", "OK", &response, "Set modem to minimum functionality."))
    { // mode 4 would shut down RF entirely to "flight-mode"; mode 0 still keeps SMS receiption intact
        ESP_LOGI(tag, "Set modem to minimum functionality.");
    }
    else
    {
        ESP_LOGE(tag, "Setting modem to minimum functionality failed.");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ESP_LOGI(tag, "Switched to power saving mode via DTR.");
    gpio_set_level(CELLULAR_GPIO_DTR, 1);
    // mbPowerSaverActive = true;
    mPowerMode = POWER_SLEEP;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return true;

    // SIM7100/SIM7500/SIM7600/SIM7800 module must in idle mode (no data transmission, no audio playing, no other at command running and so on) in order to let SIM7100/SIM7500/SIM7600/SIM7800 module enter
    // into sleep mode
}

bool Cellular::SwitchToFullPowerMode()
{
    if (mPowerMode == POWER_OFF)
    {
        if (!PowerUp())
            return false;
    }
    else if (mPowerMode == POWER_SLEEP)
    {
        gpio_set_level(CELLULAR_GPIO_DTR, 0);
        // simcom documentation: "Anytime host want send data to module, it must be pull down DTR then wait minimum 20ms" --> Command() waits 100ms anyway
    }
    else
    {
        // power is already on
        return true;
    }

    // if (!mbPowerSaverActive)

    // .Wakeup Module
    // SIM7100/SIM7500/SIM7600/SIM7800 module can exit from sleep mode automatically when the following
    // events are satisfied:
    // UART event, DTR is pulled down if wants to wakeup module.
    // wake through sleeping UART: continuously send AT+CSCLK=1\r\n  until  OK comes back

    String response;
    String command;

    int timeLimit = esp_timer_get_time() / 1000000 + 60; // wait max 60 seconds
    int timeLast = 0;
    while (true)
    {
        if (Command("AT+CSCLK=1", "OK", &response, "Ensure UART is back.", 10, UART_INPUT_TIMEOUT_CMDSHORT))
        {
            break;
        }

        int timeCurrent = esp_timer_get_time() / 1000000; // seconds

        if (timeCurrent != timeLast)
        {
            ESP_LOGI(tag, "Waiting for modem to wake up, up to %is", timeLimit - timeCurrent);
            timeLast = timeCurrent;
        }

        if (timeCurrent >= timeLimit)
        {
            ESP_LOGE(tag, "Could not communicate with modem after waking from sleep.");
            gpio_set_level(CELLULAR_GPIO_DTR, 1);
            return false;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    if (Command("AT+CFUN=1", "OK", &response, "Set modem to full power mode."))
    { // mode 4 would shut down RF entirely to "flight-mode"; mode 0 still keeps SMS receiption intact
        ESP_LOGI(tag, "Switched to full power mode.");
        // mbPowerSaverActive = false;
        mPowerMode = POWER_ON;
    }
    else
    {
        ESP_LOGE(tag, "Could not switch to full power mode.");
        return false;
    }

    ESP_LOGD(tag, "Switching back to full power....");

    timeLimit = esp_timer_get_time() / 1000000 + CELLULAR_MAX_WAIT_NETWORK_REGISTRATION_SECONDS ; 
    timeLast = 0;
    while (true)
    {
        if (Command("AT+CREG?", "OK", &response, "Network Registration Information States "))
        { // +CREG: 0,2 // +CREG: 1,5
            if (response.indexOf("+CREG: ") >= 0 && ((response.indexOf(",5") >= 0) || (response.indexOf(",1") >= 0)))
                break;
        }
        else
        {
            ESP_LOGE(tag, "Could not register on on network. Put modem into Sleep mode.");
            // mbPowerSaverActive = true;
            mPowerMode = POWER_SLEEP;
            gpio_set_level(CELLULAR_GPIO_DTR, 1);
            return false;
        }
        // 0 – not registered, ME is not currently searching a new operator to register to
        // 1 – registered, home network
        // 2 – not registered, but ME is currently searching a new operator to register to
        // 3 – registration denied
        // 4 – unknown
        // 5 – registered, roaming

        int timeCurrent = esp_timer_get_time() / 1000000; // seconds

        if (timeCurrent != timeLast)
        {
            ESP_LOGI(tag, "Waiting for network up to %is", timeLimit - timeCurrent);
            timeLast = timeCurrent;
        }

        if (timeCurrent >= timeLimit)
        {
            ESP_LOGE(tag, "Could not communicate with modem after waking from sleep.");
            return false;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(tag, "Switching to full power mode completed.");

    return true;
}

void Cellular::QuerySignalStatus()
{
    String response;
    msNetworkmode = "";
    if (Command("AT+CNSMOD?", "OK", &response, "Current network system mode"))
    { // +CNSMOD: 0,4
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
        // 12 – TDS- HSPA (HSDPA and HSUPA)
        // 13 – CDMA
        // 14 – EVDO
        // 15 – HYBRID (CDMA and EVDO)
        // 16 – 1XLTE(CDMA and LTE)
        // 23 – eHRPD
        // 24 – HYBRID(CDMA and eHRPD)
        msNetworkmode = response.substring(response.indexOf(",") + 1, response.lastIndexOf("\""));
        int iMode = msNetworkmode.toInt();
        const char *modes[25] = {"no service", "GSM", "GPRS", "EGPRS (EDGE)", "WCDMA", "HSDPA only(WCDMA)",
                                 "HSUPA only(WCDMA)", "HSPA (HSDPA and HSUPA, WCDMA)", "LTE",
                                 "TDS-CDMA", "TDS-HSDPA only", "TDS- HSUPA only", "TDS- HSPA (HSDPA and HSUPA)",
                                 "CDMA", "EVDO", "HYBRID (CDMA and EVDO)", "1XLTE(CDMA and LTE)",
                                 "eHRPD", "HYBRID(CDMA and eHRPD)"};

        if (iMode >= 0 && iMode < (sizeof(modes) / sizeof(modes[0])))
        {
            msNetworkmode = modes[iMode];
        }
    }
    ESP_LOGI(tag, "Network mode: %s", msNetworkmode.c_str());

    if (Command("AT+CSQ", "OK", &response, "Signal Quality Report"))
    { // +CSQ: 13,0
        String sq;
        sq = response.substring(6, response.indexOf(","));
        miSignalQuality = sq.toInt();
        ESP_LOGI(tag, "Signal Quality: %i (%s)", miSignalQuality, sq.c_str());
    }
    else
    {
        miSignalQuality = -1;
    }
}

bool Cellular::SwitchToCommandMode()
{
    ESP_LOGI(tag, "Switching to command mode...");
    if (!PppNetifStop())
    {
        PppNetifCreate();
    };

    ESP_LOGV(tag, "sending break event.");
    uart_event_t uartBreakEvent = {
        //.type = UART_BREAK,
        .type = UART_DATA_BREAK,
        .size = 0,
        .timeout_flag = true};

    if (pdTRUE == xQueueSend(mhUartEventQueueHandle, (void *)&uartBreakEvent, 1000 / portTICK_PERIOD_MS))
    {
        /*        ESP_LOGD(tag, "Waiting for UART clearance up to 30 seonds.");
                if (xSemaphoreTake(mxUartCleared, 30*1000 / portTICK_PERIOD_MS) == pdTRUE)
                {
                    ESP_LOGI(tag, "Switched to command mode, UART cleared.");
                } else {
                    ESP_LOGE(tag, "UART clearance timed out.");
                } */
    }
    else
    {
        ESP_LOGE(tag, "SwitchToCommandMode() could not send send break event.");
    }

    String response;
    int attempts = 3;
    while (attempts--)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // v25.ter specification requires 100ms wait period before reissuing another call

        if (ModemWriteLine("AT"))
        {
            if (ModemReadResponse(response, "OK", 10, UART_INPUT_TIMEOUT_CMDNORMAL))
            {
                return true;
            }
            ESP_LOGD(tag, "Retrying testing command mode due to %s. Remaining attempts %i", response.c_str(), attempts);
        }
    }

    ESP_LOGE(tag, "SEVERE, could not enter commandline mode. %s. restarting.", response.c_str());
    return false;
}

bool Cellular::SwitchToPppMode()
{
    if (PppNetifUp())
    {
        ESP_LOGI(tag, "SwitchToPppMode() already in PPP mode");
        return true;
    }

    // ensure netif is stopped and command mode active
    PppNetifStop();
    ESP_LOGI(tag, "SwitchToPppMode() establishing modem PPP connection.");

    String response;
    if (Command("AT+CGDATA=\"PPP\",1", "CONNECT", &response, "Connect for data connection."))
    {
        ESP_LOGI(tag, "Modem CONNECTED. Ready for PPP.");
        if (!PppNetifStart())
        {
            ESP_LOGE(tag, "SEVERE, could not start network interface.");
            PppNetifStop();
#ifdef RENEW_PPP_ON_NETIF_ERROR
            PppNetifCreate();
#else
            ESP_LOGW(tag, "IGNORING RENEW PPP");
#endif
            return false;
        };

        // WAIT FOR IP ADDRESS
        // (IP Lost timer defaults to 120 seconds, so wait 60 additional seconds??)
        ESP_LOGI(tag, "Waiting up to %i seconds for getting IP address", CELLULAR_MAX_WAIT_GETIPADDRESS_SECONDS);
        if (xSemaphoreTake(mxPppConnected, CELLULAR_MAX_WAIT_GETIPADDRESS_SECONDS * 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            // note that esp_netif_action_ calls should all happen in THIS thread, and not in the event handler
            esp_netif_action_connected(mpEspNetif, 0, 0, nullptr);
            return true;
        }

        ESP_LOGE(tag, "Stopped Netif because IP address could not be obtained");
        PppNetifStop();
#ifdef RENEW_PPP_ON_NETIF_ERROR
        PppNetifCreate();
#else
        ESP_LOGW(tag, "IGNORING RENEW PPP");
#endif
        return false;
    }

    ESP_LOGW(tag, "SEVERE, Not yet connected!! Staying in command mode.");
    mbCommandMode = true;
    return false;
}

bool Cellular::ModemReadLine(String &line, TickType_t timeout)
{
    line = "";

    int maxLineLength = muiBufferSize;
    bool cr = false;
    bool crlf = false;
    while (maxLineLength)
    {
        if (muiBufferPos == muiBufferLen)
        {
            if (!ReadIntoBuffer(timeout))
                return false;
        }
        if (muiBufferPos < muiBufferLen)
        {
            unsigned char c = mpBuffer[muiBufferPos++];

            switch (c)
            {
            case 0x0D:
                cr = true;
                break;
            case 0x0A:
                crlf = cr;
                if (line.length())
                {
                    // ESP_LOGD(tag, "ModemReadLine: '%s'", line.c_str());
                    return true;
                }
                break;
            case '>': // modem waits for input!
                if (!line.length() && crlf)
                {
                    line += (char)c;
                    return true;
                }
                // intentionally fall through!
            default:
                line += (char)c;
                cr = crlf = false;
            }
        }
        maxLineLength--;
    }
    ESP_LOGE(tag, "No end of line found. Incorrect data or buffer too small.");
    return false;
}

int Cellular::ModemWriteData(const char *data, int len)
{
    int iWriteLen = uart_write_bytes(muiUartNo, data, len);
    if (iWriteLen == len)
    {
        ESP_LOGV(tag, "UART DATA bytes sent %d", len);
        ESP_LOG_BUFFER_HEXDUMP(tag, data, len, ESP_LOG_VERBOSE);
        // ESP_LOGI(tag, "ModemWriteData(): %d bytes", len);
        mullSentTotal += len;
    }
    else
    {
        ESP_LOGE(tag, "Error ModemWriteData(): %d", len);
    }
    return len;
}

bool Cellular::ModemWrite(String &command)
{
    int iWriteLen = uart_write_bytes(muiUartNo, command.c_str(), command.length());
    if (iWriteLen == command.length())
    {
        //        ESP_LOGD(tag, "ModemWrite(): '%s'", command.c_str());
        return true;
    }

    ESP_LOGE(tag, "Error ModemWrite(): '%s'", command.c_str());
    return false;
}

bool Cellular::ModemWriteLine(const char *sWrite)
{
    String write(sWrite);
    write += "\r";
    ESP_LOGD(tag, "Modem command: %s", write.c_str());
    return ModemWrite(write);
}

bool Cellular::ModemReadResponse(String &sResponse, const char *expectedLastLineResponse, unsigned short maxLines, TickType_t timeout)
{
    String sLine;
    sResponse = "";
    while (maxLines--)
    {

        if (ModemReadLine(sLine, timeout))
        {
            ESP_LOGD(tag, "ModemReadLine: '%s'", sLine.c_str());
        }
        else
        {
            ESP_LOGE(tag, "ModemReadLine timed out");
            return false;
        }

        sResponse += sLine;
        // ESP_LOGD(tag, "LINE: '%s', '%s'", sLine.c_str(), sResponse.c_str());
        // ESP_LOGD(tag, "ModemReadLine: '%s'", sLine.c_str());
        if (sLine.startsWith(expectedLastLineResponse))
        {
            // ESP_LOGI(tag, "Success: '%s', '%s'", sLine.c_str(), sResponse.c_str());
            return true;
        }
        else if (sLine.startsWith("+PPPD: DISCONNECTED")) // Sent by ESP-IDF PPP Layer, without OK
        {
            // unsolicited response, ensure to read next line
            ESP_LOGI(tag, "PPP Daemon disconnected.");
            continue;
        }
        else if (sLine.startsWith("ERROR") || sLine.startsWith("NO CARRIER") || sLine.startsWith("+CME ERROR") || sLine.startsWith("+CMS ERROR"))
        {
            ESP_LOGD(tag, "Unexpected '%s' instead of '%s', response: '%s'", sLine.c_str(), expectedLastLineResponse, sResponse.c_str());
            return false;
        }
        if (sResponse.length())
        {
            sResponse += "\r\n";
        }
        if (!sLine.length())
        {
            sResponse += "\r\n";
        }
    }
    ESP_LOGE(tag, "Error more lines than %i, expected %s: '%s', '%s'", maxLines, expectedLastLineResponse, sLine.c_str(), sResponse.c_str());
    return false;
}

bool Cellular::Command(const char *sCommand, const char *sSuccess, String *spResponse, const char *sInfo, unsigned short maxLines, TickType_t timeout)
{

    // V.250 specification:
    // The DTE shall not begin issuing a subsequent command line until at least one-tenth of a second has
    // elapsed after receipt of the entire result code issued by the DCE in response to the preceding
    // command line.
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGD(tag, "Command(%s) %s", sCommand, sInfo);
    String response;
    if (!spResponse)
    {
        spResponse = &response;
    }
    if (ModemWriteLine(sCommand))
    {
        if (ModemReadResponse(*spResponse, sSuccess, maxLines, timeout))
        {
            ESP_LOGD(tag, "%s %s:\r\n%s", sCommand, sInfo ? sInfo : "", spResponse->c_str());
            return true;
        }
    }

    ESP_LOGE(tag, "Unexpected response: '%s', expected: '%s' for command: %s %s", spResponse->c_str(), sSuccess ? sSuccess : "OK", sInfo ? sInfo : "", sCommand);
    return false;
}

void Cellular::OnEvent(esp_event_base_t base, int32_t id, void *event_data)
{

    // ESP_LOGI(tag, "Cellular::OnEvent(base=%d, id=%d)", (int)base, (int)id);

    if (base == IP_EVENT)
    {
        if (id == IP_EVENT_PPP_GOT_IP)
        {

            esp_netif_dns_info_t dns_info;

            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            if (event->esp_netif != mpEspNetif)
            {
                ESP_LOGE(tag, "Unexpected!: event->esp_netif != mpEspNetif");
            }

            ESP_LOGI(tag, "PPP Connection established.");
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            ESP_LOGI(tag, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(tag, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(tag, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            ESP_LOGI(tag, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            ESP_LOGI(tag, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI(tag, "~~~~~~~~~~~~~~");
            xSemaphoreGive(mxPppConnected);
        }
        else if (id == IP_EVENT_PPP_LOST_IP)
        {
            ESP_LOGW(tag, "Cellular Disconnect from PPP Server");
        }
        else if (id == IP_EVENT_GOT_IP6)
        {
            ESP_LOGI(tag, "GOT IPv6 event!");

            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(tag, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }
        else
        {
            ESP_LOGW(tag, "Unknown IP_EVENT! id=%d", id);
        }
    }
    else if (base == NETIF_PPP_STATUS)
    {

        if (id >= NETIF_PP_PHASE_OFFSET)
        {
            miPppPhase = id;
        }

        const char *status;
        switch (id)
        {
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
            xSemaphoreGive(mxPppPhaseDead);
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
        case NETIF_PPP_CONNECT_FAILED:
            status = "NETIF_PPP_CONNECT_FAILED";
            break;
        default:
            ESP_LOGW(tag, "Unknown Netif PPP event! id=%d", id);
            status = "UNKNOWN";
        }

        if (id > NETIF_PPP_ERRORNONE && id < NETIF_PP_PHASE_OFFSET)
        {
            if (id == NETIF_PPP_ERRORUSER)
            {
                ESP_LOGD(tag, "Netif PPP user interrupted.");
            }
            else
            {
                ESP_LOGE(tag, "Netif PPP Status Error: %s", status);
            }
        }
        else
        {
            ESP_LOGD(tag, "Netif PPP Status: %s", status);
        }
    }
    else
    {
        ESP_LOGW(tag, "Unknown event! base=%d, id=%d", (int)base, id);
    }
}
