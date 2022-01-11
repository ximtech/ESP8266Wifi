cmake_minimum_required(VERSION 3.20)
project(ESP8266WiFi VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME DWTDelay
        GITHUB_REPOSITORY ximtech/DWTDelay
        GIT_TAG origin/main)

CPMAddPackage(
        NAME Ethernet
        GITHUB_REPOSITORY ximtech/Ethernet
        GIT_TAG origin/main)

CPMAddPackage(
        NAME STM32Core
        GITHUB_REPOSITORY ximtech/STM32Core
        GIT_TAG origin/main)

set(ESP8266_WIFI_DIRECTORY
        ${DWT_DELAY_DIRECTORY}
        ${USART_DMA_DIRECTORY}
        ${ESP8266Wifi_SOURCE_DIR}
        ${ESP8266Wifi_SOURCE_DIR}/include
        CACHE STRING "ESP8266 wifi directories include to the main project" FORCE)

set(ESP8266_WIFI_SOURCES
        ${DWT_DELAY_SOURCES}
        ${USART_DMA_SOURCES}
        ${ESP8266Wifi_SOURCE_DIR}/include/ESP8266WiFi.h
        ${ESP8266Wifi_SOURCE_DIR}/ESP8266WiFi.c
        CACHE STRING "ESP8266 wifi source files include to the main project" FORCE)