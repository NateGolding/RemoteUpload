#ifndef _NET_INTERFACE_H_
#define _NET_INTERFACE_H_

#include "WiFi.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
#include "esp_ota_ops.h"
#include "string.h"


/***********************
 * Net Interface Class *
************************/

/**
 * @brief Process of use:
 * 1. call wifi_init()
 * 2. call begin()
 * 3. call poll() within an infinite loop (or
 * once if you only want one update possible)
 * 
 */
class NetInterface{

    private:

    uint16_t _port = 3232U;
    const char* _password;

    WiFiServer _server;

    const esp_partition_t* _updatePartition;

    public:

    bool wifi_init(const char *SSID, const char *PASS, uint32_t timeout_ms = 15000);
    void begin(uint16_t port = 3232U, const char* pass = NULL);
    void poll();

    void _sendGETResponse(WiFiClient& client);
    void _sendCONTINUEReponse(WiFiClient& client);
    void _sendHTTPResponse(WiFiClient& client, int code, const char* status);
    void _flushRequestBody(WiFiClient& client, long contentLength);

    void _flashApp(WiFiClient& client, long contentLength);
    void _getPartition(void);
    long _writePartition(WiFiClient& client, long contentLength, esp_ota_handle_t* handle);

    bool _switchApp(void);

};

#endif