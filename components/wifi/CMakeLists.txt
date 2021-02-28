idf_component_register(SRCS "Wifi.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES espstring esp_wifi wpa_supplicant
                       #REQUIRES mbedtls openssl ### REMOVE REMOVE PORT To ESP-TLS
                       #EMBED_TXTFILES "${PROJECT_DIR}/certificates/certkey.pem"   
                       )