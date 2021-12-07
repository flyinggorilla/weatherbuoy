idf_component_register(SRCS N2kMsg.cpp N2kStream.cpp N2kMessages.cpp NMEA2000_esp32.cpp NMEA2000.cpp
                            N2kGroupFunction.cpp N2kDeviceList.cpp N2kGroupFunctionDefaultHandlers.cpp
                       REQUIRES driver
                       INCLUDE_DIRS "."
                       )

