idf_component_register(SRCS Esp32WeatherBuoy.cpp Watchdog.cpp Config.cpp SendData.cpp DataQueue.cpp Maximet.cpp TemperatureSensors.cpp 
                       SRCS NmeaDisplay.cpp VelocityVector.cpp Alarm.cpp
                       REQUIRES serial espstring web wifi esp_http_client app_update esp_https_ota esputil cellular max471meter
                       REQUIRES nvs_flash esp32-owb nmea2000
                       # EMBED_TXTFILES certificate.pem   
                       INCLUDE_DIRS ".")
 