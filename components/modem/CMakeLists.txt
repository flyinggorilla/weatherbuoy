idf_component_register(SRCS Modem.cpp
                       REQUIRES espstring log esp_netif driver
                       INCLUDE_DIRS "."
                       )