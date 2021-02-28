idf_component_register(SRCS "WebClient.cpp" "StringParser.cpp" "WebServer.cpp" 
                        "HttpResponseParser.cpp" "HttpResponse.cpp" "HttpRequestParser.cpp" 
                        "Url.cpp" "UrlParser.cpp" "IPAddress.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES espstring log wpa_supplicant esp-tls
                       REQUIRES mbedtls openssl ### REMOVE REMOVE PORT To ESP-TLS
                       #EMBED_TXTFILES "${PROJECT_DIR}/certificates/certkey.pem"   
                       )