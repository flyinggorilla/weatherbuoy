idf_component_register(SRCS Esp32WeatherBuoy.cpp Config.cpp SendData.cpp
                    REQUIRES serial espstring nvs_flash
                    INCLUDE_DIRS ".")