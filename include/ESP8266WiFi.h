#pragma once

#include <stdarg.h>
#include "USART_DMA.h"
#include "DWT_Delay.h"
#include "IPAddress.h"
#include "MACAddress.h"
#include "Regex.h"

#define ESP8266_RESPONSE_DEFAULT_TIMEOUT_MS	 15000
#define ESP8266_KEEPALIVE_ATTEMPT_COUNT	     3
#define ESP8266_PING_PACKET_TIMEOUT_VALUE   -1
#define ESP8266_AVAILABLE_ACCESS_POINT_COUNT 20

typedef enum ESP8266ResponseStatus {
	ESP8266_RESPONSE_SUCCESS,
	ESP8266_RESPONSE_WAITING,
	ESP8266_RESPONSE_ERROR,
	ESP8266_RESPONSE_TIMEOUT
} ResponseStatus;

typedef enum ESP8266ConnectionStatus {
	ESP8266_CONNECTED_TO_AP,
	ESP8266_CREATED_TRANSMISSION,
	ESP8266_TRANSMISSION_DISCONNECTED,
	ESP8266_NOT_CONNECTED_TO_AP,
	ESP8266_CONNECT_UNKNOWN_ERROR
} ConnectionStatus;

typedef enum ESP8266AccessPointConnectionStatus {
	ESP8266_WIFI_CONNECTED,
	ESP8266_WIFI_WAITING_FOR_CONNECTION,
	ESP8266_CONNECTION_TIMEOUT,
	ESP8266_WRONG_PASSWORD,
	ESP8266_NOT_FOUND_TARGET_AP,
	ESP8266_CONNECTION_FAILED,
	ESP8266_JOIN_UNKNOWN_ERROR
} APConnectionStatus;

typedef enum ESP8266WiFiMode {
	ESP8266_STATION				     = 1,
	ESP8266_ACCESS_POINT			 = 2,
	ESP8266_STATION_AND_ACCESS_POINT = 3
} WiFiMode;

typedef enum ESP8266ConnectionMode {
	ESP8266_CONNECTION_SINGLE   = 0,
	ESP8266_CONNECTION_MULTIPLE = 1
} ConnectionMode;

typedef enum ESP8266TransferMode {
	ESP8266_NORMAL      = 0,
	ESP8266_TRANSPARENT = 1
} TransferMode;

typedef enum ESP8266ConnectionID {
	CONNECTION_ID_0 = 0,
	CONNECTION_ID_1 = 1,
	CONNECTION_ID_2 = 2,
	CONNECTION_ID_3 = 3,
	CONNECTION_ID_4 = 4,
} ConnectionID;

typedef enum WifiEncryptionType {
    ESP8266_ENCRYPTION_OPEN            = 0,
    ESP8266_ENCRYPTION_WEP             = 1,
    ESP8266_ENCRYPTION_WPA_PSK         = 2,
    ESP8266_ENCRYPTION_WPA2_PSK        = 3,
    ESP8266_ENCRYPTION_WPA_WPA2_PSK    = 4,
    ESP8266_ENCRYPTION_WPA2_ENTERPRISE = 5 // AT can NOT connect to WPA2_Enterprise AP for now
} WifiEncryptionType;

typedef enum SignalStrength {
    ESP8266_RSSI_30dBm = 30,     // Maximum signal strength
    ESP8266_RSSI_50dBm = 50,     // Excellent signal strength
    ESP8266_RSSI_60dBm = 60,     // Good, reliable signal strength
    ESP8266_RSSI_67dBm = 67,     // Reliable signal strength. The minimum for any service depending on a reliable connection
    ESP8266_RSSI_70dBm = 70,     // Not a strong signal
    ESP8266_RSSI_80dBm = 80,     // Unreliable signal strength, will not suffice for most services
    ESP8266_RSSI_90dBm = 90      // The chances of even connecting are very low at this level
} SignalStrength;


typedef struct AccessPoint {
    WifiEncryptionType encryption;
    char *ssid;
    int8_t signalStrength;
} AccessPoint;

typedef struct AccessPointList {
    AccessPoint accessPointArray[ESP8266_AVAILABLE_ACCESS_POINT_COUNT];
    uint8_t size;
} AccessPointList;

typedef struct SoftAPClient {
    IPAddress clientIP;
    MACAddress clientMac;
} SoftAPClient;

typedef struct LocalInfo {
    IPAddress accessPointIP;
    MACAddress accessPointMAC;
    IPAddress localIP;
    MACAddress localMAC;
} LocalInfo;

typedef struct ResponseData {
	uint32_t startTimeMillis;
	bool isServerResponseAwaited;
    uint32_t timeout;
	uint32_t bufferSize;
	char *responseBody;
} ResponseData;

typedef struct RequestData {
    ConnectionID id;    // used for multiple connections
    uint32_t dataLength;
    uint32_t bufferSize;
    char *requestBody;
} RequestData;

typedef struct WiFi {
    RequestData *request;
    ResponseData *response;
    bool isNeedToSaveCredentials;
    ConnectionMode connectionMode;
} WiFi;


WiFi *initWifiESP8266(USART_TypeDef *USARTx, DMA_TypeDef *DMAx, uint32_t rxStream, uint32_t txStream, uint32_t rxBufferSize, uint32_t txBufferSize);
APConnectionStatus beginESP8266(WiFi *wifi, char *ssid, char *password);    // connect to AP
ResponseStatus readResponseESP8266(WiFi *wifi);    // non-blocking response read
ResponseStatus waitForResponseESP8266(WiFi *wifi); // blocking wait
void setResponseTimeout(WiFi *wifi, uint32_t responseTimeoutMs); // set waiting timeout

// Common commands
ResponseStatus healthCheckESP8266(WiFi *wifi);
ResponseStatus restartWifiESP8266(WiFi *wifi);
ResponseStatus resetConfigurationESP8266(WiFi *wifi);   // drop all configuration to default, also reset AP auto connect
ResponseStatus setWifiModeESP8266(WiFi *wifi, WiFiMode wifiMod);
ResponseStatus setConnectionModeESP8266(WiFi *wifi, ConnectionMode connectionMode);
ResponseStatus setApplicationModeESP8266(WiFi *wifi, TransferMode transferMode);
ResponseStatus enableDeepSleepModeESP8266(WiFi *wifi, uint16_t timeToSleepMs);

ConnectionStatus getConnectionStatusESP8266(WiFi *wifi);    // get current connection status

void connectToAccessPointESP8266(WiFi *wifi, char *ssid, char *password); // Connect ESP8266 to access point
APConnectionStatus getAccessPointConnectionStatusESP8266(WiFi *wifi);
ResponseStatus disconnectFromAccessPointESP8266(WiFi *wifi);

// Connect to server
ResponseStatus connectESP8266(WiFi *wifi, char *host, uint16_t port);
ResponseStatus multipleConnectESP8266(WiFi *wifi, ConnectionID id, char *host, char *port);
ResponseStatus checkForConnectionESP8266(WiFi *wifi);
ResponseStatus closeConnectionESP8266(WiFi *wifi);
ResponseStatus closeConnectionByIdESP8266(WiFi *wifi, ConnectionID id);

ResponseStatus sendESP8266(WiFi *wifi, char *data);
ResponseStatus sendRequestBodyESP8266(WiFi *wifi);
ResponseStatus sendRequestBodyByIdESP8266(WiFi *wifi, ConnectionID id);

// Local IP and MAC
void getLocalInfoESP8266(WiFi *wifi, LocalInfo *localInfo);

// Network scan
void requestAvailableAccessPointsESP8266(WiFi *wifi);
AccessPointList getAvailableAccessPointsESP8266(WiFi *wifi);

// Soft AP
ResponseStatus enableSoftApESP8266(WiFi *wifi, char *ssid, char *password, uint8_t channel, WifiEncryptionType encryption);
ResponseStatus enableOpenSoftApESP8266(WiFi *wifi, char *ssid, uint8_t channel);
uint8_t numberOfConnectedClientsESP8266(WiFi *wifi);
SoftAPClient getSoftApClientInfo(WiFi *wifi, uint8_t clientNumber);
void setSoftApIP(WiFi *wifi, char *ipAddress);

// Ping
void pingPacketESP8266(WiFi *wifi, char *host);
int32_t getPacketPingTimeESP8266();

void deleteESP8266(WiFi *wifi);


// Helper functions
inline bool isResponseStatusWaiting(ResponseStatus status) {
    return status == ESP8266_RESPONSE_WAITING;
}

inline bool isResponseStatusSuccess(ResponseStatus status) {
	return status == ESP8266_RESPONSE_SUCCESS;
}

inline bool isResponseStatusError(ResponseStatus status) {
	return status == ESP8266_RESPONSE_ERROR;
}

inline bool isResponseStatusTimeout(ResponseStatus status) {
    return status == ESP8266_RESPONSE_TIMEOUT;
}