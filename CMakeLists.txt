# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

string(TIMESTAMP version v%y%m%d.%H%Mtest UTC)
set(PROJECT_VER "${version} ${CMAKE_BUILD_TYPE}") 
message(${PROJECT_VER})
project(esp32weatherbuoy)  



########## MANDATORY MENUCONFIG OPTIONS #############

## 1) Flash 4MB
## 2) Enable Rollback
## 3) custom partitions "partitions.csv"
## 4) Allow HTTP for OTA 
## 5) Ensure IP reassembly activated
## 6) disable Send gratuitous ARP periodically for Modem connections (keep for wifi)