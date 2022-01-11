# ESP8266 Wifi

DMA non-blocking WiFi interaction, using ESP8266 AT commands.

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
    APConnectionStatus connectionStatus = beginESP8266(wifi, "SSID", "WIFI_PASSWORD");
    while (connectionStatus == ESP8266_WIFI_WAITING_FOR_CONNECTION) {
        connectionStatus = getAccessPointConnectionStatusESP8266(wifi);
        delay_ms(200);
    }

    if (connectionStatus == ESP8266_WIFI_CONNECTED) {
        LocalInfo localInfo;
        getLocalInfoESP8266(wifi, &localInfo);
        printf("Ip: %s\n", localInfo.localIP);

        connectESP8266(wifi, "api.thingspeak.com", 80);
        sprintf(wifi->request->requestBody, "GET /channels/1243676/fields/1.json?results=%d", 10);
        sendRequestBodyESP8266(wifi);
        ResponseStatus status = waitForResponseESP8266(wifi);   // or check status with non-blocking readResponseESP8266()
        if (isResponseStatusSuccess(status)) {
            printf("%s", wifi->response->responseBody);
        }
    }
```
