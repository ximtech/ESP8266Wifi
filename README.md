# ESP8266 Wifi

***STM32*** Low-Layer(LL) library. DMA non-blocking WiFi interaction, using ESP8266 AT commands.

- Network scan
- Non-blocking and blocking response wait
- Soft AP support
- Ping support

### Add as CPM project dependency

How to add CPM to the project, check the [link](https://github.com/cpm-cmake/CPM.cmake)

```cmake
CPMAddPackage(
        NAME ESP8266Wifi
        GITHUB_REPOSITORY ximtech/ESP8266Wifi
        GIT_TAG origin/main)
```

### Project configuration

1. Start project with STM32CubeMX:
    * [USART configuration](https://github.com/ximtech/ESP8266WiFi/blob/main/example/config_base.PNG)
    * [NVIC configuration](https://github.com/ximtech/ESP8266WiFi/blob/main/example/config_nvic.PNG)
    * [DMA configuration](https://github.com/ximtech/ESP8266WiFi/blob/main/example/config_dma.PNG)
2. Select: Project Manager -> Advanced Settings -> USART -> LL
3. Generate Code
4. Add sources to project:

```cmake
add_subdirectory(${STM32_CORE_SOURCE_DIR}/USART/DMA)

include_directories(${includes}
        ${ESP8266_WIFI_DIRECTORY})   # source directories

file(GLOB_RECURSE SOURCES ${sources}
        ${ESP8266_WIFI_SOURCES})    # source files

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT}) # executable declaration should be before libraries

target_link_libraries(${PROJECT_NAME}.elf Ethernet)   # add library dependencies to project
target_link_libraries(${PROJECT_NAME}.elf Regex) 
```

3. Then Build -> Clean -> Rebuild Project

### Wiring

- <img src="https://github.com/ximtech/ESP8266WiFi/blob/main/example/pinout.PNG" alt="image" width="300"/>
- <img src="https://github.com/ximtech/ESP8266WiFi/blob/main/example/wiring.PNG" alt="image" width="300"/>
- <img src="https://github.com/ximtech/ESP8266WiFi/blob/main/example/wiring_2.PNG" alt="image" width="300"/>

## Usage

***Provide interrupt handler***

- Full example: [link](https://github.com/ximtech/ESP8266WiFi/blob/main/example/stm32f4xx_it.c)

```c
/**
  * @brief This function handles DMA2 stream2 global interrupt.
  */
void DMA2_Stream2_IRQHandler(void) {
    transferCompleteCallbackUSART_DMA(DMA2, LL_DMA_STREAM_2);    // USART1_RX
}

/**
  * @brief This function handles DMA2 stream7 global interrupt.
  */
void DMA2_Stream7_IRQHandler(void) {
    transferCompleteCallbackUSART_DMA(DMA2, LL_DMA_STREAM_7);    // USART1_TX
}
```

***The following example for base application***
```c
    WiFi *wifi = initWifiESP8266(USART1, DMA2, LL_DMA_STREAM_2, LL_DMA_STREAM_7, 2000, 1000);

    AccessPointList apList = getAvailableAccessPointsESP8266(wifi);
    for (uint8_t i = 0; i < apList.size; i++) {
        AccessPoint accessPoint = apList.accessPointArray[i];
        printf("SSID:[%s], Encryption: [%d], Signal: [%d]\n", accessPoint.ssid, accessPoint.encryption, accessPoint.signalStrength);
    }

    APConnectionStatus connectionStatus = beginESP8266(wifi, "SSID", "WIFI_PASSWORD");
    while (connectionStatus == ESP8266_WIFI_WAITING_FOR_CONNECTION) {
        connectionStatus = getAccessPointConnectionStatusESP8266(wifi);
        delay_ms(200);
    }

    if (connectionStatus == ESP8266_WIFI_CONNECTED) {
        enableOpenSoftApESP8266(wifi, "My_Test_Network_ESP8266", 1);

        int clients = numberOfConnectedClientsESP8266(wifi);
        for (int i = 0; i < clients; i++) {
            SoftAPClient client = getSoftApClientInfo(wifi, i);
            printf("Ip: %s, Mac: %s\n", client.clientIP.octetsIPv4, client.clientMac.octets);
        }


        LocalInfo localInfo;
        getLocalInfoESP8266(wifi, &localInfo);
        char buffer[25] = {0};
        ipAddressToString(&localInfo.localIP, buffer);
        printf("Ip: %s\n", buffer);

        connectESP8266(wifi, "api.thingspeak.com", 80);
        sprintf(wifi->request->requestBody, "GET /channels/1243676/fields/1.json?results=%d", 10);
        sendRequestBodyESP8266(wifi);
        ResponseStatus status = waitForResponseESP8266(wifi);   // or check status with non-blocking readResponseESP8266()
        if (isResponseStatusSuccess(status)) {
            printf("%s", wifi->response->responseBody);
        }
    }

    while (1) {
    }
```
