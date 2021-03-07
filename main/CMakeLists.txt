idf_component_register(SRCS Esp32WeatherBuoy.cpp Config.cpp SendData.cpp ReadMaximet.cpp
                    REQUIRES serial espstring web wifi esp_http_client app_update esp_https_ota esputil
                    REQUIRES nvs_flash # for Config.cpp
                    EMBED_TXTFILES certificate.pem   
                    INCLUDE_DIRS ".")
 