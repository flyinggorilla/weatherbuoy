#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "SendData.h"
#include "EspString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esputil.h"
#include "Maximet.h"

static const char tag[] = "SendData";
static const int SENDDATA_QUEUE_SIZE = (3); 
static const unsigned int MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH = 16*1024;

String SendData::ReadMessageValue(const char* key) {
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0) {
        int posCrlf = mResponseData.indexOf("\r\n", posKey);
        if (posCrlf >= 0) {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posCrlf);
            ESP_LOGI(tag, "Message body key, value: '%s', '%s'", key, value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}

String SendData::ReadMessagePemValue(const char* key) {
    int posKey = mResponseData.indexOf(key);
    if (posKey >= 0) {
        static const char *ENDOFCERT = "-----END CERTIFICATE-----";
        int posEocert = mResponseData.indexOf("-----END CERTIFICATE-----\r\n", posKey);
        if (posEocert >= 0) {
            const String &value = mResponseData.substring(posKey + strlen(key) + 1, posEocert + strlen(ENDOFCERT));
            ESP_LOGI(tag, "Message body PEM value %s", value.c_str());
            return value;
        }
        ESP_LOGE(tag, "Message body param error, no value for key '%s'", key);
    }
    return String();
}


void SendData::Cleanup() {
    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;
}

bool SendData::PrepareHttpPost(unsigned int powerVoltage, unsigned int powerCurrent, float boardTemperature, float waterTemperature, bool bSendDiagnostics) {
    bSendDiagnostics = bSendDiagnostics || mbSendDiagnostics;

    // POST message
    unsigned int uptime = (unsigned int) (esp_timer_get_time()/1000000); // seconds since start

    Data maximetData;
    mPostData = "{\"maximet\":[";
    bool bComma = false;
    while (mrDataQueue.GetData(maximetData)) {
        // SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,AVGCDIR,COMPASSH,PASL,PSTN,RH,AH,TEMP,SOLARRAD,XTILT,YTILT,STATUS,WINDSTAT,CHECK
        mPostData += bComma ? ",{" : "{";
        mPostData += "\"uptime\":";
        mPostData += maximetData.uptime;
        mPostData += ",\"speed\":";
        mPostData += maximetData.speed;
        mPostData += ",\"gspeed\":";
        mPostData += maximetData.gspeed;
        mPostData += ",\"avgspeed\":";
        mPostData += maximetData.avgspeed;
        mPostData += ",\"dir\":";
        mPostData += maximetData.dir;
        mPostData += ",\"gdir\":";
        mPostData += maximetData.gdir;
        mPostData += ",\"avgdir\":";
        mPostData += maximetData.avgdir;
        mPostData += ",\"cdir\":";
        mPostData += maximetData.cdir;
        mPostData += ",\"avgcdir\":";
        mPostData += maximetData.avgcdir;
        mPostData += ",\"compassh\":";
        mPostData += maximetData.compassh;
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
        mPostData += ",\"xtilt\":";
        mPostData += maximetData.xtilt;
        mPostData += ",\"ytilt\":";
        mPostData += maximetData.ytilt;
        mPostData += ",\"status\":\"";
        mPostData += maximetData.status;
        mPostData += "\",\"windstat\":\"";
        mPostData += maximetData.windstat;
        mPostData += "\"";
        if (maximetData.time) { // check if GPS data is available
            mPostData += ",\"lon\":";
            mPostData += String(maximetData.lon, 6);
            mPostData += ",\"lat\":";
            mPostData += String(maximetData.lat, 6);
            mPostData += ",\"cspeed\":"; 
            mPostData += maximetData.cspeed;
            mPostData += ",\"cgspeed\":"; 
            mPostData += maximetData.cgspeed;
            mPostData += ",\"avgcspeed\":"; 
            mPostData += maximetData.avgcspeed;
            mPostData += ",\"gpsspeed\":"; 
            mPostData += maximetData.gpsspeed;
            mPostData += ",\"gpsheading\":"; 
            mPostData += maximetData.gpsheading;
            mPostData += ",\"gpsfix\":"; 
            mPostData += maximetData.gpsfix;
            mPostData += ",\"gpssat\":";
            mPostData += maximetData.gpssat;
        }
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
    mPostData += ",\"cputemp\":";
    mPostData += esp32_temperature();
    mPostData += "}";

    if (bSendDiagnostics) {
        mPostData += ",\"diagnostics\": {";
        mPostData += "\"resetreason\": \"";
        mPostData += esp32_getresetreasontext(esp_reset_reason());
        mPostData += "\", \"esp-idf-version\": \"";
        mPostData += esp_ota_get_app_description()->idf_ver;  
        mPostData += "\",\"targeturl\": \"";
        mPostData += mrConfig.msTargetUrl;
        mPostData += "\",\"apssid\": \"";
        mPostData += mrConfig.msAPSsid;
        mPostData += "\",\"appass\": \"";
        mPostData += mrConfig.msAPPass.length() ? "*****" : "";
        mPostData += "\",\"stassid\": \"";
        mPostData += mrConfig.msSTASsid;
        mPostData += "\",\"stapass\": \"";
        mPostData += mrConfig.msSTAPass.length() ? "*****" : "";
        mPostData += "\",\"cellular\": {\"datasent\":";
        mPostData += (unsigned long)(mrCellular.getDataSent()/1024); // convert to kB
        mPostData += ",\"datareceived\":";
        mPostData += (unsigned long)(mrCellular.getDataReceived()/1024); // convert to kB
        mPostData += ",\"network\": \"";
        mPostData += mrConfig.msCellularOperator;   
        mPostData += "\",\"operator\": \"";
        mPostData += mrCellular.msOperator;   
        mPostData += "\",\"subscriber\": \"";
        mPostData += mrCellular.msSubscriber;
        mPostData += "\",\"hardware\": \"";
        mPostData += mrCellular.msHardware;
        mPostData += "\",\"networkmode\": \"";
        mPostData += mrCellular.msNetworkmode;
        mPostData += "\",\"signalquality\": ";
        mPostData += mrCellular.miSignalQuality;
        mPostData += "},\"mode\": ";
        switch (mrConfig.miMode) {
            case WEATHERBUOY_MODE_NMEA2000_DISPLAY:
                mPostData += "\"display\"";
                break;
            case WEATHERBUOY_MODE_MAXIMET_GMX501:
                mPostData += "\"gmx501\"";
                break;
            case WEATHERBUOY_MODE_MAXIMET_GMX200GPS:
                mPostData += "\"gmx200gps\"";
                break;
            default:
                mPostData += "\"default\"";
                break;
        }
        mPostData += "}";
        mbSendDiagnostics = false;
    }
    mPostData += "}";

    ESP_LOGI(tag, "JSON: %s", mPostData.c_str());

    return true;

}

bool SendData::PerformHttpPost() {
    // Initialize URL and HTTP client
    if (!mhEspHttpClient) {
        if (!mrConfig.msTargetUrl.startsWith("http")) {
            ESP_LOGE(tag, "No proper target URL in form of'http(s)://server/' defined: url='%s'", mrConfig.msTargetUrl.c_str());
            return false;
        }
        memset(&mEspHttpClientConfig, 0, sizeof(esp_http_client_config_t));
        mEspHttpClientConfig.url = mrConfig.msTargetUrl.c_str();
        mEspHttpClientConfig.method = HTTP_METHOD_POST; 
        mhEspHttpClient = esp_http_client_init(&mEspHttpClientConfig);
    }

    
    // prepare and send HTTP headers and content length
    ESP_LOGI(tag, "Sending %d bytes to %s", mPostData.length(), mEspHttpClientConfig.url);
    //esp_http_client_set_header(mhEspHttpClient, "Content-Type", "text/plain");
    esp_http_client_set_header(mhEspHttpClient, "Content-Type", "application/json");
    esp_err_t err;
    err = esp_http_client_open(mhEspHttpClient, mPostData.length());
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Error %s in esp_http_client_open(): %s", esp_err_to_name(err), mEspHttpClientConfig.url);
        Cleanup();
        return false; 
    }

    // write POST body message
    int sent = esp_http_client_write(mhEspHttpClient, mPostData.c_str(), mPostData.length());
    if (sent == mPostData.length()) {
        ESP_LOGD(tag, "esp_http_client_write(): OK, sent: %d", sent);
    } else {
        ESP_LOGE(tag, "esp_http_client_write(): Could only send %d of %d bytes", sent, mPostData.length());
        Cleanup();
        return false;
    }

    // retreive HTTP response and headers
    int iContentLength = esp_http_client_fetch_headers(mhEspHttpClient);
    if (iContentLength == ESP_FAIL) {
        ESP_LOGE(tag, "esp_http_client_fetch_headers(): could not receive HTTP response");
        Cleanup();
        return false;
    }

    // Check HTTP status code
    int iHttpStatusCode = esp_http_client_get_status_code(mhEspHttpClient);
    if ((iHttpStatusCode >= 200) && (iHttpStatusCode < 400)) {
        ESP_LOGI(tag, "HTTP response OK. Status %d, Content-Length %d", iHttpStatusCode , iContentLength);
    } else {
        ESP_LOGE(tag, "HTTP response was not OK with status %d", iHttpStatusCode);
        Cleanup();
        return false;
    }

    // Prevent overly memory allocation
    if (iContentLength > MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH) {
        ESP_LOGE(tag, "Response body Content-length %d exceeds maximum of %d. Aborting.", iContentLength, MAX_ACCEPTABLE_RESPONSE_BODY_LENGTH);
        Cleanup();
        return false;
    }

    // Ensure enough memory is allocated
    if (!mResponseData.prepare(iContentLength)) {
        ESP_LOGE(tag, "Could not allocate %d memory for HTTP response.", iContentLength);
        Cleanup();
        return false;
    }

    // read the HTTP response body and process it
    int len = esp_http_client_read_response(mhEspHttpClient, (char*)mResponseData.c_str(), iContentLength);
    if ((len == iContentLength) && len) {
        ESP_LOGD(tag, "HTTP POST Response \r\n--->\r\n%s<---", mResponseData.c_str());

        // Interpret the Weatherbuoy messages
        String command = ReadMessageValue("command:");
        if (command.length()) {
            bool updateConfig = false;
            String value;
            value = ReadMessageValue("set-apssid:");
            if (value.length()) { mrConfig.msAPSsid = value; updateConfig = true; };
            value = ReadMessageValue("set-appass:");
            if (value.length()) { mrConfig.msAPPass = value; updateConfig = true; };
            value = ReadMessageValue("set-stassid:");
            if (value.length()) { mrConfig.msSTASsid = value; updateConfig = true; };
            value = ReadMessageValue("set-stapass:");
            if (value.length()) { mrConfig.msSTAPass = value; updateConfig = true; };
            value = ReadMessageValue("set-hostname:");
            if (value.length()) { mrConfig.msHostname = value; updateConfig = true; };
            value = ReadMessageValue("set-targeturl:");
            if (value.length()) { mrConfig.msTargetUrl = value; updateConfig = true; };
            value = ReadMessageValue("set-apssid:");
            if (value.length()) { mrConfig.msAPSsid = value; updateConfig = true; };
            value = ReadMessageValue("set-mode:");
            if (value.length()) { 
                if (value.equalsIgnoreCase("default")) {
                    mrConfig.miMode = WEATHERBUOY_MODE_DEFAULT; 
                    updateConfig = true; 
                } else if (value.equalsIgnoreCase("display")) {
                    mrConfig.miMode = WEATHERBUOY_MODE_NMEA2000_DISPLAY; 
                    updateConfig = true; 
                } else if (value.equalsIgnoreCase("gmx501")) {
                    mrConfig.miMode = WEATHERBUOY_MODE_MAXIMET_GMX501; 
                    updateConfig = true; 
                } else if (value.equalsIgnoreCase("gmx200gps")) {
                    mrConfig.miMode = WEATHERBUOY_MODE_MAXIMET_GMX200GPS; 
                    updateConfig = true; 
                } 
            }
            mbRestart = false;
            if (command.equals("restart") || command.equals("config") || command.equals("udpate")) {
                if (updateConfig) {
                    updateConfig = mrConfig.Save();
                    ESP_LOGI(tag, "New configuration received and SAVED.");
                }
                mbRestart = true;
            } else if (command.equals("diagnose")) {
                mbSendDiagnostics = true;
            }

            // Optionally Execute OTA Update command
            if (command.equals("update")) {
                mbRestart = true;
                Cleanup();
                memset(&mEspHttpClientConfig, 0, sizeof(esp_http_client_config_t));

                //*** TODO ##################################### NEEDED ?????***************************************************
                //mrMaximet.Stop();

                const String &pem = ReadMessagePemValue("set-cert-pem:");
                mEspHttpClientConfig.method = HTTP_METHOD_GET; 
                if (!mrConfig.msTargetUrl.endsWith("/")) {
                    mrConfig.msTargetUrl += "/";
                } 
                mrConfig.msTargetUrl += "firmware.bin"; 
                mEspHttpClientConfig.url = mrConfig.msTargetUrl.c_str();
                mEspHttpClientConfig.skip_cert_common_name_check = false; // dont touch!!
                if (pem.length()) {
                    mEspHttpClientConfig.cert_pem = pem.c_str();
                } 
                ESP_LOGI(tag, "OTA Url: %s", mEspHttpClientConfig.url);
                ESP_LOGI(tag, "OTA PEM %s certificate: %d bytes\r\n%s", pem.length() ? "custom" : "embedded", strlen(mEspHttpClientConfig.cert_pem), mEspHttpClientConfig.cert_pem);
                err = esp_https_ota(&mEspHttpClientConfig);
                if (err == ESP_OK) {
                    ESP_LOGI(tag, "Successful OTA update");
                } else {
                    ESP_LOGE(tag, "Error reading response %s", esp_err_to_name(err));
                }
            }

            if (mbRestart) {
                ESP_LOGI(tag, "***** RESTARTING in 1 Second *****.");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
            } 
        }
    } 
    
    if (!mbOtaAppValidated) {
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
            ESP_LOGE(tag, "Error: Could not validate firmware app image. %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(tag, "Firmware validated!");
        }
        mbOtaAppValidated = true;
    } 

    Cleanup();
    mrWatchdog.clear();
    return true;
}

SendData::SendData(Config &config, DataQueue &dataQueue, Cellular &cellular, Watchdog &watchdog) : mrConfig(config), mrDataQueue(dataQueue), mrCellular(cellular), mrWatchdog(watchdog) {
    mhEspHttpClient = nullptr; 

}

SendData::~SendData() {
    esp_http_client_cleanup(mhEspHttpClient);    
    mhEspHttpClient = nullptr;
}

