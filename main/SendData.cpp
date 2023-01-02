//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "SendData.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_netif.h"
#include "esputil.h"
#include "Maximet.h"
#include "string.h"
#include "assert.h"

static const char tag[] = "SendData";
static const int SENDDATA_QUEUE_SIZE = (3);
static const unsigned int MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH = 16 * 1024;

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

bool DnsLookup(const char *hostname)
{

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = nullptr;
    struct in_addr addr;

    int err = getaddrinfo(hostname, NULL, &hints, &res);

    if (err != 0 || res == NULL)
    {
        ESP_LOGE(tag, "DNS lookup failed err=%d res=%p %s", err, res, esp_err_to_name(err));
    }
    else
    {
        /* Code to print the resolved IP.
            Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(tag, "DNS lookup succeeded. IP=%s", inet_ntoa(addr));
    }

    if (res)
    {
        freeaddrinfo(res);
    }
    return true;
}

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(tag, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(tag, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(tag, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(tag, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);

        // retreive UTC timestamp in milliseconds from HTTP response header and update system time if needed
        if (strcmp("Timestamp", evt->header_key) == 0 && evt->header_value)
        {
            char *endptr = nullptr;
            long long timestamp_ms = strtoll(evt->header_value, &endptr, 10);

            struct timeval now;
            gettimeofday(&now, NULL);
            time_t delta_s = abs(timestamp_ms / 1000 - now.tv_sec);

            ESP_LOGI(tag, "HTTP response header Timestamp: %s, %lli Delta: %lis", evt->header_value, timestamp_ms, delta_s);

            // adjust system time if more than 2 seconds off
            if (delta_s > 2)
            {
                //struct timeval delta;
                //delta.tv_sec = timestamp_ms / 1000 - now.tv_sec;
                //delta.tv_usec = (timestamp_ms % 1000) * 1000 - now.tv_usec;
                //if (adjtime(&delta, NULL))
                //{
                    // delta too large, setting time absolutely
                    now.tv_sec = timestamp_ms / 1000;
                    now.tv_usec = (timestamp_ms % 1000) * 1000;
                    settimeofday(&now, NULL);
                    ESP_LOGW(tag, "Adjusted system time by %li seconds.", delta_s);
                //}
                //else
                //{
                //    ESP_LOGW(tag, "Smooth system time adjustment by %li seconds.", delta_s);
                //}
            }
        }

        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(tag, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(tag, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(tag, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)(evt->data), &mbedtls_err, NULL);
        if (err != ESP_OK)
        {
            ESP_LOGE(tag, "TLS: esp error code: 0x%x %s, mbedtls: 0x%x. Error cleared.", err, esp_err_to_name(err), mbedtls_err);
            //*****************DnsLookup("atterwind.info");
        }
        break;
        // case HTTP_EVENT_REDIRECT:
        //     ESP_LOGD(tag, "HTTP_EVENT_REDIRECT");
        //     esp_http_client_set_header(evt->client, "From", "user@example.com");
        //     esp_http_client_set_header(evt->client, "Accept", "text/html");
        //     break;
    }
    return ESP_OK;
}

String SendData::ReadMessageValue(const char *key)
{
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0)
    {
        int posCrlf = mResponseData.indexOf("\r\n", posKey);
        if (posCrlf >= 0)
        {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posCrlf);
            ESP_LOGI(tag, "Message body key, value: '%s', '%s'", key, value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}

String SendData::ReadMessagePemValue(const char *key)
{
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0)
    {
        static const char *ENDOFCERT = "-----END CERTIFICATE-----";
        int posEocert = mResponseData.indexOf("-----END CERTIFICATE-----\r\n", posKey);
        if (posEocert >= 0)
        {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posEocert + strlen(ENDOFCERT));
            ESP_LOGI(tag, "Message body PEM value %s", value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}

void SendData::Cleanup()
{
    esp_http_client_cleanup(mhEspHttpClient);
    mhEspHttpClient = nullptr;
}

void PostDataAddFloat(String &rPostData, const char *key, float val, bool comma = true)
{
    if (comma)
        rPostData += ",";
    rPostData += "\"";
    rPostData += key;
    rPostData += "\":";
    if (isnanf(val))
    {
        rPostData += "null";
    }
    else
    {
        rPostData += val;
    }
}

void PostDataAddShort(String &rPostData, const char *key, short val, bool comma = true)
{
    if (comma)
        rPostData += ",";
    rPostData += "\"";
    rPostData += key;
    rPostData += "\":";
    if (isnans(val))
    {
        rPostData += "null";
    }
    else
    {
        rPostData += val;
    }
}

void PostDataAddInt(String &rPostData, const char *key, int val, bool comma = true)
{
    if (comma)
        rPostData += ",";
    rPostData += "\"";
    rPostData += key;
    rPostData += "\":";
    rPostData += val;
}

void PostDataAddString(String &rPostData, const char *key, String &val, bool comma = true)
{
    if (comma)
        rPostData += ",";
    rPostData += "\"";
    rPostData += key;
    rPostData += "\":";
    rPostData += val;
}

bool SendData::PrepareHttpPost(unsigned int powerVoltage, unsigned int powerCurrent, float boardTemperature, float waterTemperature, bool bSendDiagnostics, OnlineMode onlineMode)
{
    bSendDiagnostics = bSendDiagnostics || mbSendDiagnostics;

    // POST message
    unsigned int uptime = (unsigned int)(esp_timer_get_time() / 1000000); // seconds since start

    Data maximetData;
    mPostData = "{\"maximet\":[";
    bool bComma = false;
    while (mrDataQueue.GetData(maximetData))
    {
        mPostData += bComma ? ",{" : "{";
        mPostData += "\"uptime\":";
        mPostData += maximetData.uptime;
        PostDataAddFloat(mPostData, "cspeed", maximetData.cspeed);
        PostDataAddFloat(mPostData, "cgspeed", maximetData.cgspeed);
        PostDataAddFloat(mPostData, "avgcspeed", maximetData.avgcspeed);
        PostDataAddShort(mPostData, "cdir", maximetData.cdir);
        PostDataAddShort(mPostData, "cgdir", maximetData.cgdir);
        PostDataAddShort(mPostData, "avgcdir", maximetData.avgcdir);
        PostDataAddShort(mPostData, "dir", maximetData.dir);
        PostDataAddShort(mPostData, "compassh", maximetData.compassh);
        PostDataAddFloat(mPostData, "xavgcspeed", maximetData.xavgcspeed);
        PostDataAddShort(mPostData, "xavgcdir", maximetData.xavgcdir);

        if (mrMaximetConfig.model == Maximet::Model::GMX501 || mrMaximetConfig.model == Maximet::Model::GMX501GPS)
        {
            mPostData += ",\"pasl\":";
            mPostData += maximetData.pasl;
            mPostData += ",\"pstn\":";
            mPostData += maximetData.pstn;
            mPostData += ",\"rh\":";
            mPostData += maximetData.rh;
            mPostData += ",\"ah\":";
            mPostData += maximetData.ah;
            mPostData += ",\"temp\":";
            mPostData += maximetData.temp;
            mPostData += ",\"solarrad\":";
            mPostData += maximetData.solarrad;
        }
        mPostData += ",\"xtilt\":";
        mPostData += maximetData.xtilt;
        mPostData += ",\"ytilt\":";
        mPostData += maximetData.ytilt;
        mPostData += ",\"zorient\":";
        mPostData += maximetData.zorient;
        mPostData += ",\"status\":\"";
        mPostData += maximetData.status;
        mPostData += "\",\"windstat\":\"";
        mPostData += maximetData.windstat;
        mPostData += "\"";
        if (maximetData.time)
        { // check if GPS data is available
            mPostData += ", \"gps\": {";
            mPostData += "\"time\":";
            mPostData += maximetData.time;
            if (!isnan(maximetData.lon) && !isnan(maximetData.lat))
            {
                mPostData += ",\"lat\":";
                mPostData += String(maximetData.lat, 6);
                mPostData += ",\"lon\":";
                mPostData += String(maximetData.lon, 6);
                // mPostData += ",\"sog\":";
                // mPostData += maximetData.gpsspeed;
                PostDataAddFloat(mPostData, "sog", maximetData.gpsspeed);
                // mPostData += ",\"cog\":";
                // mPostData += maximetData.gpsheading;
                PostDataAddShort(mPostData, "cog", maximetData.gpsheading);
            }
            mPostData += ",\"fix\":";
            mPostData += maximetData.gpsfix;
            mPostData += ",\"sat\":";
            mPostData += maximetData.gpssat;
            mPostData += "}";
        }
#if DEBUG_MAXIMET
#pragma message "RAW MAXIMET DATA CONSUMES EXTRA MEMORY - USE ONLY FOR DEBUGGING"
        mPostData += ",\"raw\":\"";
        mPostData += maximetData.line;
        mPostData += "\"";
#endif
        mPostData += "}";
        bComma = true;
    }
    mPostData += "]";

    mPostData += ",\"system\": {\"version\":\"";
    mPostData += esp_ota_get_app_description()->version;
    mPostData += "\",\"hostname\": \"";
    mPostData += mrConfig.msHostname;
    mPostData += "\",\"uptime\":";
    mPostData += uptime;
    mPostData += ",\"heapfree\":";
    mPostData += esp_get_free_heap_size();
    mPostData += ",\"minheapfree\":";
    mPostData += esp_get_minimum_free_heap_size();
    mPostData += ",\"voltage\":";
    mPostData += powerVoltage;
    mPostData += ",\"current\":";
    mPostData += powerCurrent;
    mPostData += ",\"boardtemp\":";
    mPostData += boardTemperature;
    mPostData += ",\"watertemp\":";
    mPostData += waterTemperature;
    mPostData += "}";

    if (bSendDiagnostics)
    {
        mPostData += ",\"diagnostics\": {";
        mPostData += "\"resetreason\": \"";
        mPostData += esp32_getresetreasontext(esp_reset_reason());
        mPostData += "\", \"esp-idf-version\": \"";
        mPostData += esp_ota_get_app_description()->idf_ver;
        mPostData += "\",\"targeturl\": \"";
        mPostData += CONFIG_WEATHERBUOY_TARGET_URL; // mrConfig.msTargetUrl;
        mPostData += "\",\"apssid\": \"";
        mPostData += mrConfig.msAPSsid;
        mPostData += "\",\"appass\": \"";
        mPostData += mrConfig.msAPPass.length() ? "*****" : "";
        mPostData += "\",\"stassid\": \"";
        mPostData += mrConfig.msSTASsid;
        mPostData += "\",\"stapass\": \"";
        mPostData += mrConfig.msSTAPass.length() ? "*****" : "";
        mPostData += "\", \"intervalday\": ";
        mPostData += mrConfig.miIntervalDay;
        mPostData += ", \"intervalnight\": ";
        mPostData += mrConfig.miIntervalNight;
        mPostData += ", \"intervallowbat\": ";
        mPostData += mrConfig.miIntervalLowbattery;
        mPostData += ", \"intervaldiag\": ";
        mPostData += mrConfig.miIntervalDiagnostics;
        if (onlineMode == MODE_CELLULAR)
        {
            mPostData += ",\"cellular\": {\"datasent\":";
            mPostData += (unsigned long)(mrCellular.getDataSent() / 1024); // convert to kB
            mPostData += ",\"datareceived\":";
            mPostData += (unsigned long)(mrCellular.getDataReceived() / 1024); // convert to kB
            mPostData += ",\"network\": \"";
            mPostData += mrConfig.msCellularOperator;
            mPostData += "\",\"operator\": \"";
            mPostData += mrCellular.msOperator;
            mPostData += "\",\"subscriber\": \"";
            mPostData += mrCellular.msSubscriber;
            mPostData += "\",\"apn\": \"";
            mPostData += mrConfig.msCellularApn;
            mPostData += "\",\"hardware\": \"";
            mPostData += mrCellular.msHardware;
            mPostData += "\",\"networkmode\": \"";
            mPostData += mrCellular.msNetworkmode;
            mPostData += "\",\"signalquality\": ";
            mPostData += mrCellular.miSignalQuality;
            mPostData += ", \"prefnetwork\": ";
            mPostData += mrConfig.miCellularNetwork;
            mPostData += ", \"prefoperator\": \"";
            mPostData += mrConfig.msCellularOperator;
            mPostData += "\"}";
        }
        if (mrConfig.mbNmeaDisplay)
        {
            mPostData += ",\"display\": \"NMEA2000\"";
        }
        if (mrConfig.mbAlarmSound || mrConfig.msAlarmSms.length())
        {
            mPostData += ",\"alarm\": ";
            mPostData += "{\"sound\": ";
            mPostData += mrConfig.mbAlarmSound ? "true" : "false";
            mPostData += ",\"sms\": \"";
            mPostData += mrConfig.msAlarmSms;
            mPostData += "\", \"radius\": ";
            mPostData += mrConfig.miAlarmRadius;
            mPostData += ",\"lat\":";
            mPostData += String(mrConfig.mdAlarmLatitude, 6);
            mPostData += ",\"lon\":";
            mPostData += String(mrConfig.mdAlarmLongitude, 6);
            mPostData += "}";
        }
        if (mrConfig.miSimulator)
        {
            mPostData += ", \"simulator\": \"GMX";
            mPostData += mrConfig.miSimulator / 10;
            mPostData += mrConfig.miSimulator % 10 ? "GPS\"" : "\"";
        }
        if (mrMaximetConfig.model)
        {
            mPostData += ", \"maximet\": {";
            mPostData += "\"avglong\": ";
            mPostData += mrMaximetConfig.iAvgLong;
            mPostData += ",\"outfreq\": ";
            mPostData += mrMaximetConfig.iOutputIntervalSec;
            mPostData += ",\"userinf\": \"";
            mPostData += mrMaximetConfig.sUserinfo;
            mPostData += "\",\"report\": \"";
            mPostData += mrMaximetConfig.sReport;
            mPostData += "\",\"sensor\": \"";
            mPostData += mrMaximetConfig.sSensor;
            mPostData += "\",\"serial\": \"";
            mPostData += mrMaximetConfig.sSerial;
            mPostData += "\"";
            if (mrMaximetConfig.model != Maximet::Model::GMX200GPS)
            {
                mPostData += ",\"hasl\": ";
                mPostData += mrMaximetConfig.fHasl;
                mPostData += ",\"hastn\": ";
                mPostData += mrMaximetConfig.fHastn;
            }
            mPostData += ",\"compassdecl\": ";
            mPostData += mrMaximetConfig.fCompassdecl;
            mPostData += ",\"lat\": ";
            mPostData += String(mrMaximetConfig.fLat, 6);
            mPostData += ",\"lon\": ";
            mPostData += String(mrMaximetConfig.fLong, 6);
            mPostData += ",\"garbled\": ";
            mPostData += mrMaximet.GarbledDataStat(); // NOTE: this is not threadsafe implemented
            mPostData += "}";
        }
        mPostData += "}";
        mbSendDiagnostics = false;
    }
    mPostData += "}";

    mdCurrentLocationLatitude = maximetData.lat;
    mdCurrentLocationLongitude = maximetData.lon;

    ESP_LOGI(tag, "JSON: %s", mPostData.c_str());

    return true;
}

bool SendData::PerformHttpPost()
{

    //*******************ESP_LOGW(tag, "TESTING DNS LOOKUP.");
    //*******************DnsLookup("atterwind.info");

    // Initialize URL and HTTP client
    if (!mhEspHttpClient)
    {
        /*if (!mrConfig.msTargetUrl.startsWith("http"))
        {
            //ESP_LOGE(tag, "No proper target URL in form of'http(s)://server/' defined: url='%s'", mrConfig.msTargetUrl.c_str());
            ESP_LOGE(tag, "No proper target URL in form of'http(s)://server/' defined: url='%s'", CONFIG_WEATHERBUOY_TARGET_URL);
            return false;
        }*/

        static_assert(sizeof(CONFIG_WEATHERBUOY_TARGET_URL) > 4, "http(s) URL to weatherbuoy server must be define in ESP-IDF config.");

        mEspHttpClientConfig = {0};                               // memset(&mEspHttpClientConfig, 0, sizeof(esp_http_client_config_t));
        mEspHttpClientConfig.url = CONFIG_WEATHERBUOY_TARGET_URL; // mrConfig.msTargetUrl.c_str();
        mEspHttpClientConfig.method = HTTP_METHOD_POST;
        mEspHttpClientConfig.timeout_ms = 60 * 1000; // default of 5000ms (5s) is too short
        mEspHttpClientConfig.event_handler = http_event_handler;
        mhEspHttpClient = esp_http_client_init(&mEspHttpClientConfig);
        ESP_LOGD(tag, "Http timeout set to: %is", mEspHttpClientConfig.timeout_ms / 1000);
    }

    // prepare and send HTTP headers and content length
    ESP_LOGI(tag, "Sending %d bytes to '%s'", mPostData.length(), mEspHttpClientConfig.url);

    esp_http_client_set_header(mhEspHttpClient, "Content-Type", "application/json");

    esp_err_t err;
    err = esp_http_client_open(mhEspHttpClient, mPostData.length());
    if (err != ESP_OK)
    {
        ESP_LOGE(tag, "Error %s in esp_http_client_open(): %s", esp_err_to_name(err), mEspHttpClientConfig.url);
        Cleanup();
        return false;
    }

    // write POST body message
    int sent = esp_http_client_write(mhEspHttpClient, mPostData.c_str(), mPostData.length());
    if (sent == mPostData.length())
    {
        ESP_LOGD(tag, "esp_http_client_write(): OK, sent: %d", sent);
    }
    else
    {
        ESP_LOGE(tag, "esp_http_client_write(): Could only send %d of %d bytes", sent, mPostData.length());
        Cleanup();
        return false;
    }

    // retreive HTTP response and headers
    int iContentLength = esp_http_client_fetch_headers(mhEspHttpClient);
    if (iContentLength == ESP_FAIL)
    {
        ESP_LOGE(tag, "esp_http_client_fetch_headers(): could not receive HTTP response");
        Cleanup();
        return false;
    }

    // Check HTTP status code
    int iHttpStatusCode = esp_http_client_get_status_code(mhEspHttpClient);
    if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400))
    {
        ESP_LOGI(tag, "HTTP response OK. Status %d, Content-Length %d", iHttpStatusCode, iContentLength);
    }
    else
    {
        ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        Cleanup();
        return false;
    }

    // Prevent overly memory allocation
    if (iContentLength > MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH)
    {
        ESP_LOGE(tag, "Response body Content-length %d exceeds maximum of %d. Aborting.", iContentLength, MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH);
        Cleanup();
        return false;
    }

    // Ensure enough memory is allocated
    if (!mResponseData.prepare(iContentLength))
    {
        ESP_LOGE(tag, "Could not allocate %d memory for HTTP response.", iContentLength);
        Cleanup();
        return false;
    }

    // read the HTTP response body and process it
    int len = esp_http_client_read_response(mhEspHttpClient, (char *)mResponseData.c_str(), iContentLength);
    if ((len == iContentLength) && len)
    {
        ESP_LOGD(tag, "HTTP POST Response \r\n--->\r\n%s<---", mResponseData.c_str());

        // Interpret the Weatherbuoy messages
        String command = ReadMessageValue("command:");
        if (command.length())
        {
            bool updateConfig = false;
            String value;

            value = ReadMessageValue("set-apssid:");
            if (value.length())
            {
                mrConfig.msAPSsid = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-appass:");
            if (value.length())
            {
                mrConfig.msAPPass = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-stassid:");
            if (value.length())
            {
                mrConfig.msSTASsid = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-stapass:");
            if (value.length())
            {
                mrConfig.msSTAPass = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-hostname:");
            if (value.length())
            {
                mrConfig.msHostname = value;
                updateConfig = true;
            };

            // value = ReadMessageValue("set-targeturl:");
            // if (value.length())
            //{
            //     mrConfig.msTargetUrl = value;
            //     updateConfig = true;
            // };

            value = ReadMessageValue("set-apssid:");
            if (value.length())
            {
                mrConfig.msAPSsid = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-simulator:");
            if (value.length())
            {
                if (value.equalsIgnoreCase("off") || value.equalsIgnoreCase("false"))
                {
                    mrConfig.miSimulator = Maximet::Model::NONE; //   WEATHERBUOY_SIMULATOR_OFF;
                    updateConfig = true;
                }
                else if (value.equalsIgnoreCase("gmx501gps"))
                {
                    mrConfig.miSimulator = Maximet::Model::GMX501GPS; // WEATHERBUOY_SIMULATOR_MAXIMET_GMX501GPS;
                    updateConfig = true;
                }
                else if (value.equalsIgnoreCase("gmx501"))
                {
                    mrConfig.miSimulator = Maximet::Model::GMX501; // WEATHERBUOY_SIMULATOR_MAXIMET_GMX501;
                    updateConfig = true;
                }
                else if (value.equalsIgnoreCase("gmx200gps"))
                {
                    mrConfig.miSimulator = Maximet::Model::GMX200GPS; // WEATHERBUOY_SIMULATOR_MAXIMET_GMX200GPS;
                    updateConfig = true;
                }
            }

            value = ReadMessageValue("set-display:");
            if (value.length())
            {
                mrConfig.mbNmeaDisplay = value.equalsIgnoreCase("NMEA2000") || value.equalsIgnoreCase("on") || value.equalsIgnoreCase("true");
                updateConfig = true;
            };

            value = ReadMessageValue("set-alarmgps:");
            if (value.length())
            {
                if (!isnan(mdCurrentLocationLongitude) && !isnan(mdCurrentLocationLatitude) && !(mdCurrentLocationLatitude == 0) && !(mdCurrentLocationLongitude == 0))
                {
                    mrConfig.mdAlarmLatitude = mdCurrentLocationLatitude;
                    mrConfig.mdAlarmLongitude = mdCurrentLocationLongitude;
                    updateConfig = true;
                }
                else if (value.indexOf(':') > 0)
                {
                    int latPos = value.indexOf(':', 0);
                    double lat = value.substring(0, latPos).toFloat();
                    double lon = value.substring(latPos + 1).toFloat();
                    if (!isnan(lat) && !isnan(lon))
                    {
                        mrConfig.mdAlarmLatitude = lat;
                        mrConfig.mdAlarmLongitude = lon;
                        updateConfig = true;
                    }
                }
            }

            value = ReadMessageValue("set-alarmsound:");
            if (value.length())
            {
                mrConfig.mbAlarmSound = value.equalsIgnoreCase("true") || value.equalsIgnoreCase("on");
                updateConfig = true;
            };

            value = ReadMessageValue("set-alarmsms:");
            if (value.length())
            {
                mrConfig.msAlarmSms = value;
                updateConfig = true;
            };

            value = ReadMessageValue("set-alarmradius:");
            if (value.length())
            {
                mrConfig.miAlarmRadius = value.toInt();
                updateConfig = true;
            };

            value = ReadMessageValue("set-intervalday:");
            if (value.length())
            {
                mrConfig.miIntervalDay = value.toInt();
                updateConfig = true;
            };

            value = ReadMessageValue("set-intervalnight:");
            if (value.length())
            {
                mrConfig.miIntervalNight = value.toInt();
                updateConfig = true;
            };

            value = ReadMessageValue("set-intervaldiag:");
            if (value.length())
            {
                mrConfig.miIntervalDiagnostics = value.toInt();
                updateConfig = true;
            };

            value = ReadMessageValue("set-intervallowbat:");
            if (value.length())
            {
                mrConfig.miIntervalLowbattery = value.toInt();
                updateConfig = true;
            };

            // Maximet configurations
            value = ReadMessageValue("set-hasl:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteHasl(value.toFloat());
                updateConfig = true;
            };

            value = ReadMessageValue("set-hastn:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteHastn(value.toFloat());
                updateConfig = true;
            };

            value = ReadMessageValue("set-position:");
            if (value.indexOf(':') > 0)
            {
                mrMaximet.Stop();
                int latPos = value.indexOf(':', 0);
                double lat = value.substring(0, latPos).toFloat();
                double lon = value.substring(latPos + 1).toFloat();
                if (!isnan(lat) && !isnan(lon))
                {
                    mrMaximet.WriteLat(lat);
                    mrMaximet.WriteLong(lon);
                    updateConfig = true;
                }
            }

            value = ReadMessageValue("set-compassdecl:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteCompassdecl(value.toFloat());
                updateConfig = true;
            };

            value = ReadMessageValue("set-outfreq:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteOutfreq(value.toInt() == 1);
                updateConfig = true;
            };

            value = ReadMessageValue("set-userinf:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteUserinf(value.c_str());
                updateConfig = true;
            };

            value = ReadMessageValue("set-avglong:");
            if (value.length())
            {
                mrMaximet.Stop();
                mrMaximet.WriteAvgLong(value.toInt());
                updateConfig = true;
            };

            mbRestart = false;
            if (command.equals("restart") || command.equals("config") || command.equals("udpate"))
            {
                if (updateConfig)
                {
                    updateConfig = mrConfig.Save();
                    ESP_LOGI(tag, "New configuration received and SAVED.");
                }
                mbRestart = true;
            }
            else if (command.equals("diagnose"))
            {
                mbSendDiagnostics = true;
            }

            // Optionally Execute OTA Update command
            if (command.equals("update"))
            {
                mbRestart = true;
                Cleanup();
                memset(&mEspHttpClientConfig, 0, sizeof(esp_http_client_config_t));

                const String &pem = ReadMessagePemValue("set-cert-pem:");
                String targetUrl(CONFIG_WEATHERBUOY_TARGET_URL);
                mEspHttpClientConfig.method = HTTP_METHOD_GET;
                if (!targetUrl.endsWith("/"))
                {
                    targetUrl += "/";
                }
                targetUrl += "firmware.bin";
                mEspHttpClientConfig.url = targetUrl.c_str();
                mEspHttpClientConfig.skip_cert_common_name_check = false; // dont touch!!
                if (pem.length())
                {
                    mEspHttpClientConfig.cert_pem = pem.c_str();
                }
                ESP_LOGI(tag, "OTA Url: %s", mEspHttpClientConfig.url);
                ESP_LOGD(tag, "OTA PEM %s certificate: %d bytes\r\n%s", pem.length() ? "custom" : "embedded", strlen(mEspHttpClientConfig.cert_pem), mEspHttpClientConfig.cert_pem);
                err = esp_https_ota(&mEspHttpClientConfig);
                if (err == ESP_OK)
                {
                    ESP_LOGI(tag, "Successful OTA update");
                }
                else
                {
                    ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                }
            }

            if (mbRestart)
            {
                ESP_LOGI(tag, "***** RESTARTING in 1 Second *****.");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
            }
        }
    }

    if (!mbOtaAppValidated)
    {
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)
        {
            ESP_LOGE(tag, "Error: Could not validate firmware app image. %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(tag, "Firmware validated!");
        }
        mbOtaAppValidated = true;
    }

    Cleanup();
    mrWatchdog.clear();
    return true;
}

SendData::SendData(Config &config, DataQueue &dataQueue, Cellular &cellular, Watchdog &watchdog, Maximet &maximet) : mrConfig(config), mrDataQueue(dataQueue), mrCellular(cellular), mrWatchdog(watchdog), mrMaximet(maximet), mrMaximetConfig(maximet.GetConfig())
{
    mhEspHttpClient = nullptr;
}

SendData::~SendData()
{
    esp_http_client_cleanup(mhEspHttpClient);
    mhEspHttpClient = nullptr;
}
