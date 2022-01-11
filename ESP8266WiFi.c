#include "ESP8266WiFi.h"

#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define TMP_SEND_TX_BUFFER_LENGTH 20
#define TMP_CONNECT_TX_BUFFER_LENGTH 200
#define SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH (IP_ADDRESS_LENGTH + MAC_ADDRESS_LENGTH + 1)   // + 1 for line end

#define OK_STATUS            "\r\nOK\r\n"
#define SEND_OK_STATUS       "\r\nSEND OK\r\n"
#define DATA_RECEIVED_STATUS "+IPD,"
#define CLOSED_STATUS        "CLOSED\r\n"
#define ERROR_STATUS         "\r\nERROR\r\n"
#define FAIL_STATUS          "\r\nFAIL\r\n"
#define NEW_LINE             "\r\n"

static enum AccessPointParameter {
    SECURITY, SSID, SIGNAL_STRENGTH
} APParameter;

static USART_DMA *USARTDmaPointer = NULL;

static inline bool isSsidValid(char *ssid);
static inline bool isPasswordValid(char *password);

static void sendATCommand(WiFi *wifi, const char *ATCommandPattern, ...);
static bool isResponseOK(char *responseBody, bool isServerResponse);
static bool isResponseError(char *responseBody);
static void setDMATransmitBufferAddress(USART_DMA *USARTDmaInstance, char *bufferPointer, uint32_t bufferSize);
static void parseToAP(AccessPoint *accessPoint, char *buffer);


WiFi *initWifiESP8266(USART_TypeDef *USARTx,
                      DMA_TypeDef *DMAx,
                      uint32_t rxStream,
                      uint32_t txStream,
                      uint32_t rxBufferSize,
                      uint32_t txBufferSize) {
    USARTDmaPointer = initUSART_DMA(USARTx, DMAx, rxStream, txStream, rxBufferSize, txBufferSize);
    ResponseData *response = malloc(sizeof(struct ResponseData));
    RequestData *request = malloc(sizeof(struct RequestData));
    WiFi *wifiInstance = malloc(sizeof(struct WiFi));

    if (USARTDmaPointer == NULL || response == NULL || request == NULL || wifiInstance == NULL) {
        deleteESP8266(wifiInstance);
        return NULL;
    }
    wifiInstance->response = response;
    wifiInstance->request = request;

    wifiInstance->request->id = CONNECTION_ID_0;
    wifiInstance->request->dataLength = 0;
    wifiInstance->request->requestBody = USARTDmaPointer->txData->bufferPointer;
    wifiInstance->request->bufferSize = USARTDmaPointer->txData->bufferSize;

    wifiInstance->response->startTimeMillis = 0;
    wifiInstance->response->isServerResponseAwaited = false;
    wifiInstance->response->timeout = ESP8266_RESPONSE_DEFAULT_TIMEOUT_MS;
    wifiInstance->response->responseBody = USARTDmaPointer->rxData->bufferPointer;
    wifiInstance->response->bufferSize = USARTDmaPointer->rxData->bufferSize;

    wifiInstance->isNeedToSaveCredentials = false;
    wifiInstance->connectionMode = ESP8266_CONNECTION_SINGLE;
    dwtDelayInit();

    delay_ms(100); // initial delay, waiting module startup

    ResponseStatus status;
    for (uint8_t i = 0; i < ESP8266_KEEPALIVE_ATTEMPT_COUNT; i++) {
        status = healthCheckESP8266(wifiInstance);
        if (isResponseStatusSuccess(status)) {
            break;
        }
    }

    if (isResponseStatusSuccess(status)) {
        sendATCommand(wifiInstance, "ATE0");  // Disable echo (don’t send back received command)
        status = waitForResponseESP8266(wifiInstance);
    }

    if (isResponseStatusSuccess(status)) {
        status = setWifiModeESP8266(wifiInstance, ESP8266_STATION_AND_ACCESS_POINT);

        if (isResponseStatusSuccess(status)) {
            status = setConnectionModeESP8266(wifiInstance, ESP8266_CONNECTION_SINGLE);
        }

        if (isResponseStatusSuccess(status)) {
            status = setApplicationModeESP8266(wifiInstance, ESP8266_NORMAL);
        }
    }

    if (!isResponseStatusSuccess(status)) {
        deleteESP8266(wifiInstance);
    }
    return wifiInstance;
}

APConnectionStatus beginESP8266(WiFi *wifi, char *ssid, char *password) {
    if (wifi == NULL || !isSsidValid(ssid) || !isPasswordValid(password)) return ESP8266_CONNECTION_FAILED;
    ConnectionStatus connectionStatus = getConnectionStatusESP8266(wifi);
    if (connectionStatus == ESP8266_NOT_CONNECTED_TO_AP || connectionStatus == ESP8266_TRANSMISSION_DISCONNECTED) {
        connectToAccessPointESP8266(wifi, ssid, password);
        return ESP8266_WIFI_WAITING_FOR_CONNECTION;
    } else if (connectionStatus == ESP8266_CONNECTED_TO_AP) {
        return ESP8266_WIFI_CONNECTED;
    }
    return ESP8266_JOIN_UNKNOWN_ERROR;
}

ResponseStatus readResponseESP8266(WiFi *wifi) {
    if ((currentMilliSeconds() - wifi->response->startTimeMillis) >= wifi->response->timeout) {
        return ESP8266_RESPONSE_TIMEOUT;
    }

    if (isTransferCompleteUSART_DMA(USARTDmaPointer->rxData)) {
        if (isResponseOK(wifi->response->responseBody, wifi->response->isServerResponseAwaited)) {
            return ESP8266_RESPONSE_SUCCESS;
        } else if (isResponseError(wifi->response->responseBody)) {
            return ESP8266_RESPONSE_ERROR;
        } else {
            receiveRxBufferUSART_DMA(USARTDmaPointer); // idle line and no data received, start receive data
        }
    }
    return ESP8266_RESPONSE_WAITING;
}

ResponseStatus waitForResponseESP8266(WiFi *wifi) {
    ResponseStatus status;
    do {
        status = readResponseESP8266(wifi);
    } while (isResponseStatusWaiting(status));
    return status;
}

void setResponseTimeout(WiFi *wifi, uint32_t responseTimeoutMs) {
    wifi->response->timeout = responseTimeoutMs;
}

ResponseStatus healthCheckESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT");
    return waitForResponseESP8266(wifi);
}

ResponseStatus restartWifiESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+RST");
    return waitForResponseESP8266(wifi);
}

ResponseStatus resetConfigurationESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+RESTORE");
    return waitForResponseESP8266(wifi);
}

ResponseStatus setWifiModeESP8266(WiFi *wifi, WiFiMode wifiMod) {
    sendATCommand(wifi, "AT+CWMODE=%d", wifiMod);
    return waitForResponseESP8266(wifi);
}

ResponseStatus setConnectionModeESP8266(WiFi *wifi, ConnectionMode connectionMode) {
    sendATCommand(wifi, "AT+CIPMUX=%d", connectionMode);
    wifi->connectionMode = connectionMode;
    return waitForResponseESP8266(wifi);
}

ResponseStatus setApplicationModeESP8266(WiFi *wifi, TransferMode transferMode) {
    sendATCommand(wifi, "AT+CIPMODE=%d", transferMode);
    return waitForResponseESP8266(wifi);
}

ResponseStatus enableDeepSleepModeESP8266(WiFi *wifi, uint16_t timeToSleepMs) {    // Hardware has to support deep-sleep wake up (Reset pin has to be High).
    sendATCommand(wifi, "AT+GSLP=%d", timeToSleepMs);
    return waitForResponseESP8266(wifi);
}

void requestAvailableAccessPointsESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+CWLAP");
}

void connectToAccessPointESP8266(WiFi *wifi, char *ssid, char *password) {
    if (wifi->isNeedToSaveCredentials) {
        sendATCommand(wifi, "AT+CWJAP_DEF=\"%s\",\"%s\"", ssid, password);// Connect ESP8266 to access point and save connection credentials
    } else {
        sendATCommand(wifi, "AT+CWJAP_CUR=\"%s\",\"%s\"", ssid, password);// Connect ESP8266 to access point and don't save connection credentials
    }
}

ConnectionStatus getConnectionStatusESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+CIPSTATUS");
    ResponseStatus status = waitForResponseESP8266(wifi);
    if (isResponseStatusSuccess(status)) {
        if (strstr(wifi->response->responseBody, "STATUS:2")) {
            return ESP8266_CONNECTED_TO_AP;
        } else if (strstr(wifi->response->responseBody, "STATUS:3")) {
            return ESP8266_CREATED_TRANSMISSION;
        } else if (strstr(wifi->response->responseBody, "STATUS:4")) {
            return ESP8266_TRANSMISSION_DISCONNECTED;
        } else if (strstr(wifi->response->responseBody, "STATUS:5")) {
            return ESP8266_NOT_CONNECTED_TO_AP;
        }
    }
    return ESP8266_CONNECT_UNKNOWN_ERROR;
}

APConnectionStatus getAccessPointConnectionStatusESP8266(WiFi *wifi) {
    ResponseStatus status = readResponseESP8266(wifi);
    if (isResponseStatusSuccess(status)) {
        return ESP8266_WIFI_CONNECTED;
    } else if (isResponseStatusWaiting(status)) {
        return ESP8266_WIFI_WAITING_FOR_CONNECTION;
    } else if (isResponseStatusTimeout(status)) {
        return ESP8266_CONNECTION_TIMEOUT;
    } else if (isResponseStatusError(status)) {
        if (strstr(wifi->response->responseBody, "+CWJAP:1")) {
            return ESP8266_CONNECTION_TIMEOUT;
        } else if (strstr(wifi->response->responseBody, "+CWJAP:2")) {
            return ESP8266_WRONG_PASSWORD;
        } else if (strstr(wifi->response->responseBody, "+CWJAP:3")) {
            return ESP8266_NOT_FOUND_TARGET_AP;
        } else if (strstr(wifi->response->responseBody, "+CWJAP:4")) {
            return ESP8266_CONNECTION_FAILED;
        }
    }
    return ESP8266_JOIN_UNKNOWN_ERROR;
}

ResponseStatus disconnectFromAccessPointESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+CWQAP");
    return waitForResponseESP8266(wifi);
}

ResponseStatus connectESP8266(WiFi *wifi, char *host, uint16_t port) {
    char tmpBuffer[TMP_CONNECT_TX_BUFFER_LENGTH];   // create tmp buffer for command
    uint32_t savedBufferSize = USARTDmaPointer->txData->bufferSize;
    char *savedTxBuffer = USARTDmaPointer->txData->bufferPointer;
    setDMATransmitBufferAddress(USARTDmaPointer, tmpBuffer, TMP_CONNECT_TX_BUFFER_LENGTH);  // set tmp buffer as dma address

    sendATCommand(wifi, "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    ResponseStatus status = waitForResponseESP8266(wifi);
    setDMATransmitBufferAddress(USARTDmaPointer, savedTxBuffer, savedBufferSize);   // return previous buffer as dma address
    return status;
}

ResponseStatus multipleConnectESP8266(WiFi *wifi, ConnectionID id, char *host, char *port) {
    char tmpBuffer[TMP_CONNECT_TX_BUFFER_LENGTH];   // create tmp buffer for command
    uint32_t savedBufferSize = USARTDmaPointer->txData->bufferSize;
    char *savedTxBuffer = USARTDmaPointer->txData->bufferPointer;
    setDMATransmitBufferAddress(USARTDmaPointer, tmpBuffer, TMP_CONNECT_TX_BUFFER_LENGTH);  // set tmp buffer as dma address

    sendATCommand(wifi, "AT+CIPSTART=\"%d\",\"TCP\",\"%s\",%s", id, host, port);
    ResponseStatus status = waitForResponseESP8266(wifi);
    setDMATransmitBufferAddress(USARTDmaPointer, savedTxBuffer, savedBufferSize);   // return previous buffer as dma address
    return status;
}

ResponseStatus checkForConnectionESP8266(WiFi *wifi) {
    ResponseStatus status = readResponseESP8266(wifi);
    if (isResponseStatusError(status)) {
        if (strstr(wifi->response->responseBody, "ALREADY CONNECTED")) {
            return ESP8266_RESPONSE_SUCCESS;
        }
    }
    return status;
}

ResponseStatus sendESP8266(WiFi *wifi, char *data) {
    uint32_t dataLength = strlen(data) + 2;
    uint32_t savedBufferSize = USARTDmaPointer->txData->bufferSize;
    sendATCommand(wifi, "AT+CIPSEND=%d", dataLength);    // provide uriPattern length before request
    ResponseStatus status = waitForResponseESP8266(wifi);
    USARTDmaPointer->txData->bufferSize = dataLength;
    if (isResponseStatusSuccess(status)) {
        memset(USARTDmaPointer->rxData->bufferPointer, 0, USARTDmaPointer->rxData->bufferSize);
        memset(USARTDmaPointer->txData->bufferPointer, 0, USARTDmaPointer->txData->bufferSize);
        strcat(USARTDmaPointer->txData->bufferPointer, data);
        strcat(USARTDmaPointer->txData->bufferPointer, NEW_LINE);
        wifi->response->startTimeMillis = currentMilliSeconds();   // command start time
        wifi->response->isServerResponseAwaited = true;
        transmitTxBufferUSART_DMA(USARTDmaPointer);
        receiveRxBufferUSART_DMA(USARTDmaPointer);
        status = ESP8266_RESPONSE_WAITING;
    }
    USARTDmaPointer->txData->bufferSize = savedBufferSize;
    return status;
}

ResponseStatus sendRequestBodyESP8266(WiFi *wifi) {
    char tmpBuffer[TMP_SEND_TX_BUFFER_LENGTH];   // create tmp buffer for command
    uint32_t dataLength = (wifi->request->dataLength) > 0 ? (wifi->request->dataLength + 2) : (strlen(wifi->request->requestBody) + 2);
    uint32_t savedBufferSize = USARTDmaPointer->txData->bufferSize;
    char *savedTxBuffer = USARTDmaPointer->txData->bufferPointer;
    setDMATransmitBufferAddress(USARTDmaPointer, tmpBuffer, TMP_SEND_TX_BUFFER_LENGTH);  // set tmp buffer as dma address
    wifi->request->dataLength = 0;  // reset data length, preventing incorrect data transmit length for next request

    sendATCommand(wifi, "AT+CIPSEND=%d", dataLength);
    ResponseStatus status = waitForResponseESP8266(wifi);
    setDMATransmitBufferAddress(USARTDmaPointer, savedTxBuffer, dataLength);   // return previous buffer as dma address
    if (isResponseStatusSuccess(status)) {
        memset(USARTDmaPointer->rxData->bufferPointer, 0, USARTDmaPointer->rxData->bufferSize);
        strcat(USARTDmaPointer->txData->bufferPointer, NEW_LINE);
        wifi->response->startTimeMillis = currentMilliSeconds();   // command start time
        wifi->response->isServerResponseAwaited = true;
        transmitTxBufferUSART_DMA(USARTDmaPointer);
        receiveRxBufferUSART_DMA(USARTDmaPointer);
        USARTDmaPointer->txData->bufferSize = savedBufferSize;
        status = ESP8266_RESPONSE_WAITING;
    }
    USARTDmaPointer->txData->bufferSize = savedBufferSize;
    return status;
}

ResponseStatus sendRequestBodyByIdESP8266(WiFi *wifi, ConnectionID id) {
    char tmpBuffer[TMP_SEND_TX_BUFFER_LENGTH];
    uint32_t dataLength = (wifi->request->dataLength) > 0 ? (wifi->request->dataLength + 2) : (strlen(wifi->request->requestBody) + 2);
    uint32_t savedBufferSize = USARTDmaPointer->txData->bufferSize;
    char *savedTxBuffer = USARTDmaPointer->txData->bufferPointer;
    setDMATransmitBufferAddress(USARTDmaPointer, tmpBuffer, TMP_SEND_TX_BUFFER_LENGTH);
    wifi->request->dataLength = 0;  // reset data length, preventing incorrect data transmit length for next request

    sendATCommand(wifi, "AT+CIPSEND=%d,%d", id, dataLength);    // provide connection id and data length before request
    ResponseStatus status = waitForResponseESP8266(wifi);
    setDMATransmitBufferAddress(USARTDmaPointer, savedTxBuffer, savedBufferSize);
    if (isResponseStatusSuccess(status)) {
        memset(USARTDmaPointer->rxData->bufferPointer, 0, USARTDmaPointer->rxData->bufferSize);
        strcat(USARTDmaPointer->txData->bufferPointer, NEW_LINE);
        wifi->response->startTimeMillis = currentMilliSeconds();   // command start time
        wifi->response->isServerResponseAwaited = true;
        transmitTxBufferUSART_DMA(USARTDmaPointer);
        receiveRxBufferUSART_DMA(USARTDmaPointer);
        USARTDmaPointer->txData->bufferSize = savedBufferSize;
        status = ESP8266_RESPONSE_WAITING;
    }
    USARTDmaPointer->txData->bufferSize = savedBufferSize;
    return status;
}

ResponseStatus closeConnectionESP8266(WiFi *wifi) {
    if (wifi->connectionMode == ESP8266_CONNECTION_SINGLE) {
        sendATCommand(wifi, "AT+CIPCLOSE");
        return waitForResponseESP8266(wifi);
    }
    return ESP8266_RESPONSE_ERROR;
}

ResponseStatus closeConnectionByIdESP8266(WiFi *wifi, ConnectionID id) {//  ID no. of connection to close, when id=5, all connections will be closed.
    if (wifi->connectionMode == ESP8266_CONNECTION_MULTIPLE) {
        sendATCommand(wifi, "AT+CIPCLOSE=%d", id);
        return waitForResponseESP8266(wifi);
    }
    return ESP8266_RESPONSE_ERROR;
}

void getLocalInfoESP8266(WiFi *wifi, LocalInfo *localInfo) {
    sendATCommand(wifi, "AT+CIFSR");
    ResponseStatus status = waitForResponseESP8266(wifi);
    if (isResponseStatusSuccess(status)) {
        char accessPointIP[IP_ADDRESS_LENGTH + 1] = {0};
        char accessPointMAC[MAC_ADDRESS_LENGTH + 1] = {0};
        char localIP[IP_ADDRESS_LENGTH + 1] = {0};
        char localMAC[MAC_ADDRESS_LENGTH + 1] = {0};

        substringString("APIP,\"", "\"", wifi->response->responseBody, accessPointIP);
        substringString("APMAC,\"", "\"", wifi->response->responseBody, accessPointMAC);
        substringString("STAIP,\"", "\"", wifi->response->responseBody, localIP);
        substringString("STAMAC,\"", "\"", wifi->response->responseBody, localMAC);

        localInfo->accessPointIP = ipAddressFromString(accessPointIP);
        localInfo->accessPointMAC = macAddressFromString(accessPointMAC);
        localInfo->localIP = ipAddressFromString(localIP);
        localInfo->localMAC = macAddressFromString(localMAC);
    }
}

AccessPointList getAvailableAccessPointsESP8266(WiFi *wifi) {
    requestAvailableAccessPointsESP8266(wifi);
    ResponseStatus status = waitForResponseESP8266(wifi);
    AccessPointList accessPointList = {0};

    if (isResponseStatusSuccess(status)) {
        Regex regex;
        regexCompile(&regex, "(.+?)");

        char *srcPointer = wifi->response->responseBody;;
        for (int i = 0; i < ESP8266_AVAILABLE_ACCESS_POINT_COUNT; i++) {
            Matcher matcher = regexMatch(&regex, srcPointer);
            if (!matcher.isFound) break;

            char *valuePointer = &srcPointer[matcher.foundAtIndex + 1];
            valuePointer[matcher.matchLength - 2] = '\0';
            parseToAP(&accessPointList.accessPointArray[i], valuePointer);
            srcPointer += matcher.matchLength + matcher.foundAtIndex;
            accessPointList.size++;
        }
    }
    return accessPointList;
}

ResponseStatus enableSoftApESP8266(WiFi *wifi, char *ssid, char *password, uint8_t channel, WifiEncryptionType encryption) {
    sendATCommand(wifi, "AT+CWSAP?");
    ResponseStatus status = waitForResponseESP8266(wifi);
    if (!isResponseStatusError(status)) return status;

    if (isSsidValid(ssid) && isPasswordValid(password)) {
        if (wifi->isNeedToSaveCredentials) {
            sendATCommand(wifi, "AT+CWSAP_DEF=\"%s\",\"%s\",%d,%d", ssid, password, channel, encryption);
        } else {
            sendATCommand(wifi, "AT+CWSAP_CUR=\"%s\",\"%s\",%d,%d", ssid, password, channel, encryption);
        }
        return waitForResponseESP8266(wifi);
    }
    return ESP8266_RESPONSE_ERROR;
}

ResponseStatus enableOpenSoftApESP8266(WiFi *wifi, char *ssid, uint8_t channel) {
    if (isSsidValid(ssid)) {
        if (wifi->isNeedToSaveCredentials) {
            sendATCommand(wifi, "AT+CWSAP_DEF=\"%s\",\"\",%d,%d", ssid, channel, ESP8266_ENCRYPTION_OPEN);
        } else {
            sendATCommand(wifi, "AT+CWSAP_CUR=\"%s\",\"\",%d,%d", ssid, channel, ESP8266_ENCRYPTION_OPEN);
        }
        return waitForResponseESP8266(wifi);
    }
    return ESP8266_RESPONSE_ERROR;
}

uint8_t numberOfConnectedClientsESP8266(WiFi *wifi) {
    sendATCommand(wifi, "AT+CWLIF");
    ResponseStatus status = waitForResponseESP8266(wifi);
    uint8_t clientCount = 0;

    if (isResponseStatusSuccess(status)) {
        char buffer[SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH];
        memset(buffer, 0, SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH);
        char *tmpSourcePointer = wifi->response->responseBody;

        while (substringString("", "\r\n", tmpSourcePointer, buffer)) {
            tmpSourcePointer += strlen(buffer) + 2;
            clientCount++;
            memset(buffer, 0, SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH);
        }
    }
    return clientCount;
}

SoftAPClient getSoftApClientInfo(WiFi *wifi, uint8_t clientNumber) {
    char buffer[SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH] = {0};
    char *tmpSourcePointer = wifi->response->responseBody;
    SoftAPClient softApClient;

    for (uint8_t i = 0; (i <= clientNumber); i++) {
        memset(buffer, 0, SOFT_AP_CLIENT_INFO_TOKEN_MAX_LENGTH);
        substringString("", "\r\n", tmpSourcePointer, buffer);
        tmpSourcePointer += strlen(buffer) + 2;
    }
    char *ipToken = strtok(buffer, ",");
    char *macToken = strtok(NULL, ",");
    softApClient.clientIP = ipAddressFromString(ipToken);
    softApClient.clientMac = macAddressFromString(macToken);
    return softApClient;
}

void setSoftApIP(WiFi *wifi, char *ipAddress) {
    if (isIPv4AddressValid(ipAddress)) {
        sendATCommand(wifi, "AT+CIPAP=\"%s\"", ipAddress);
    }
}

void pingPacketESP8266(WiFi *wifi, char *host) {
    sendATCommand(wifi, "AT+PING=\"%s\"", host);
}

int32_t getPacketPingTimeESP8266() {
    char buffer[10] = { [0 ... 10 - 1] = 0 };
    if (substringString("+", NEW_LINE, USARTDmaPointer->rxData->bufferPointer, buffer)) {
        return strtol(USARTDmaPointer->rxData->bufferPointer, NULL, 10);
    }
    return ESP8266_PING_PACKET_TIMEOUT_VALUE;
}

void deleteESP8266(WiFi *wifi) {
    if (wifi != NULL) {// no need to free every pointer in struct, it will be deallocated at USART side
        deleteUSART_DMA(USARTDmaPointer);
        free(wifi->response);
        free(wifi->request);
        free(wifi);
    }
}

static inline bool isSsidValid(char *ssid) {
    return (ssid != NULL && strlen(ssid) < MAX_SSID_LENGTH);
}

static inline bool isPasswordValid(char *password) {
    return (password != NULL && strlen(password) < MAX_PASSWORD_LENGTH);
}

static void sendATCommand(WiFi *wifi, const char *ATCommandPattern, ...) {    // sendRequestBodyESP8266 AT command to ESP8266
    memset(USARTDmaPointer->txData->bufferPointer, 0, USARTDmaPointer->txData->bufferSize);
    va_list valist;
    va_start(valist, ATCommandPattern);
    vsprintf(USARTDmaPointer->txData->bufferPointer, ATCommandPattern, valist);
    va_end(valist);

    strcat(USARTDmaPointer->txData->bufferPointer, NEW_LINE);  // ESP8266 expects <CR><LF> or CarriageReturn and LineFeed at the end of each command
    memset(USARTDmaPointer->rxData->bufferPointer, 0, USARTDmaPointer->rxData->bufferSize);
    wifi->response->startTimeMillis = currentMilliSeconds();   // command start time
    wifi->response->isServerResponseAwaited = false;
    receiveRxBufferUSART_DMA(USARTDmaPointer);
    transmitUSART_DMA(USARTDmaPointer, USARTDmaPointer->txData->bufferPointer, strlen(USARTDmaPointer->txData->bufferPointer));
}

static bool isResponseOK(char *responseBody, bool isServerResponse) {
    if (isServerResponse) {
        return strstr(responseBody, CLOSED_STATUS) || strstr(responseBody, DATA_RECEIVED_STATUS);
    }
    return strstr(responseBody, OK_STATUS) || strstr(responseBody, SEND_OK_STATUS)  || strstr(responseBody, ">");
}

static bool isResponseError(char *responseBody) {    // find for "error" or "fail" response status
    return strstr(responseBody, ERROR_STATUS) || strstr(responseBody, FAIL_STATUS);
}

static void setDMATransmitBufferAddress(USART_DMA *USARTDmaInstance, char *bufferPointer, uint32_t bufferSize) {
    USARTDmaPointer->txData->bufferSize = bufferSize;
    USARTDmaPointer->txData->bufferPointer = bufferPointer;

    LL_DMA_DisableStream(USARTDmaInstance->DMAx, USARTDmaInstance->txData->stream);
    uint32_t dataRegisterAddress = LL_USART_DMA_GetRegAddr(USARTDmaInstance->USARTx);
    uint32_t txDataTransferDirection = LL_DMA_GetDataTransferDirection(USARTDmaInstance->DMAx, USARTDmaInstance->txData->stream);
    LL_DMA_ConfigAddresses(USARTDmaInstance->DMAx, USARTDmaInstance->txData->stream, (uint32_t) bufferPointer, dataRegisterAddress, txDataTransferDirection);
    enableDMAStream(USARTDmaInstance->DMAx, USARTDmaInstance->txData->stream);
    LL_USART_EnableDMAReq_TX(USARTDmaInstance->USARTx);
    LL_USART_EnableDMAReq_RX(USARTDmaInstance->USARTx);
}

static void parseToAP(AccessPoint *accessPoint, char *buffer) {
    char *source = buffer;
    char *parameterValue;
    APParameter = SECURITY;

    while ((APParameter < SIGNAL_STRENGTH + 1) && (parameterValue = splitStringReentrant(source, ",", &source))) {
        switch (APParameter) {
            case SECURITY:
                accessPoint->encryption = atoi(parameterValue);
                break;
            case SSID:
                accessPoint->ssid = parameterValue;
                break;
            case SIGNAL_STRENGTH:
                accessPoint->signalStrength = atoi(parameterValue);
                break;
            default:
                break;
        }
        APParameter++;
    }
}