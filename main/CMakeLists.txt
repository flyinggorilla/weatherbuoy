idf_component_register(SRCS Esp32WeatherBuoy.cpp Watchdog.cpp Config.cpp SendData.cpp ReadMaximet.cpp TemperatureSensors.cpp
                    REQUIRES serial espstring web wifi esp_http_client app_update esp_https_ota esputil cellular max471meter
                    REQUIRES nvs_flash esp32-owb 
                    # EMBED_TXTFILES certificate.pem   
                    INCLUDE_DIRS ".")
 