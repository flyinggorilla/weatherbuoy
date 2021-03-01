idf_component_register(SRCS Esp32WeatherBuoy.cpp Config.cpp SendData.cpp
                    REQUIRES serial espstring web wifi esp_http_client app_update
                    REQUIRES nvs_flash # for Config.cpp
                    INCLUDE_DIRS ".")
 