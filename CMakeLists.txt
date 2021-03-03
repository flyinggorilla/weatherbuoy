# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

string(TIMESTAMP version v%y%m%d.%H%M UTC)
set(PROJECT_VER ${version}) 
project(esp32weatherbuoy) 


########## MANDATORY MENUCONFIG OPTIONS #############

## 1) Flash 4MB
## 2) Enable Rollback
## 3) custom partitions "partitions.csv"
## 4) (??) Allow HTTP for OTA 
## 5) (?? compiler optimization 
## 6) (??) wolf ssl vs. mbettls
## 7) (??) CPU frequency 160 vs. 240Mhz ==> about 20mA more power consumption