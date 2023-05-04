#include "NetInterface.h"

/* VERBOSE option can be uncommented for debugging purpose print statements */
#define VERBOSE



/**
 * @brief Initialize wifi
 * 
 * @param SSID network ssid
 * @param PASS network password
 * @param timeout_ms timeout in milliseconds
 */
bool NetInterface::wifi_init(const char *SSID, const char *PASS, uint32_t timeout_ms){
    
    #ifdef VERBOSE
        while(!Serial) Serial.begin(115200);
        Serial.print("Connecting to "); Serial.print(SSID);
    #endif

    unsigned long start_time = millis();
    while(WiFi.status() != WL_CONNECTED && (millis() - start_time) < timeout_ms){
        
        #ifdef VERBOSE
            Serial.write('.');
        #endif
        WiFi.begin(SSID, PASS);
        delay(1024);
    }

    if(WiFi.status() != WL_CONNECTED) return false;

    #ifdef VERBOSE
        Serial.println("Connected!");
        Serial.print("IPv4: "); Serial.println(WiFi.localIP());
    #endif

    return true;
}





/**
 * @brief initialize with specific port
 * (defaults to 3232U) and option for password
 * authorization (defaults to NULL)
 * 
 * @param port
 * @param pass
 */
void NetInterface::begin(uint16_t port, const char* pass){
    _port = port;
    _password = pass;
    _server.begin(_port);
}





/**
 * @brief called in infinite while loop to
 * continuously check for a client connection
 * and to read the upload data into flash/spiffs
 * 
 */
void NetInterface::poll(){
    
    WiFiClient _client = _server.available();

    if(!_client) return;

    #ifdef VERBOSE
        while(!Serial) Serial.begin(115200);
        Serial.print("Client has connected with IP "); Serial.println(_client.localIP());
    #endif

    /* receive request and headers, parse into sections */

    String request = _client.readStringUntil('\n'); request.trim();
    String header;
    String auth;
    bool expect = false;
    long content_length = 0;

    do{
        header = _client.readStringUntil('\n');
        header.trim();

        if(header.startsWith("Authorization: ")){
            header.remove(0, 15);
            auth = header;
        }
        else if(header.startsWith("Content-Length: ")){
            header.remove(0, 16);
            content_length = header.toInt();
        }
        else if(header.startsWith("Expect: 100-continue")){
            header.remove(0, 20);
            expect = true;
        }
    }while(header != "");
    
    #ifdef VERBOSE
        Serial.println("-----------------");
        Serial.println("Recieved headers...");
        Serial.println(request);
        Serial.print("Authorization: "); Serial.println(auth);
        Serial.print("Content-Length: "); Serial.println(content_length);
        Serial.println("-----------------");
        Serial.println();
    #endif

    /* check request */

    if(request == "GET / HTTP/1.1" || request == "GET /index HTTP/1.1" || request == "GET /index.html HTTP/1.1"){
        _sendGETResponse(_client);
        return;
    }
    else if(request == "GET /switch HTTP/1.1"){
        if(_switchApp()){
            _sendHTTPResponse(_client, 200, "OK");
            ESP.restart();
        }
        else{
            _sendHTTPResponse(_client, 500, "Internal Server Error");
            return;
        }
    }
    else if(request != "POST /sketch HTTP/1.1"){
        _sendHTTPResponse(_client, 404, "Not Found");
        return;
    }

    /* check authorization & content length */

    if(_password && (auth != _password)){
        _sendHTTPResponse(_client, 401, "Unauthorized");
        return;
    }

    if(content_length > (ESP.getFlashChipSize() / 2)){
        _sendHTTPResponse(_client, 413, "Payload Too Large");
        return;
    }

    if(content_length <= 0){
        _sendHTTPResponse(_client, 400, "Bad Request");
        return;
    }

    /* read and flash body content */
    _flashApp(_client, content_length);

    return;
}





/**
 * @brief standard response to a get request
 * 
 * @param client 
 */
void NetInterface::_sendGETResponse(WiFiClient& client){

    while(client.available()){
        client.read();
    }

    const char *getMessage1 = "Welcome to the Robot Remote Reprogramming interface!\n\n";
    const char *getMessage2 = "Commands:\n";
    const char *getMessage3 = "REPROGRAM: curl <IPv4>:<port>/sketch --data-binary @<path to bin file> \
                            -H 'Expect: ' -H 'Authorization: <password>' -H 'Content-Length: <binary file size (not on disk)>'\n\n";
    const char *getMessage4 = "SWITCH APP: curl <IPv4:<port>/switch";

    /* total length is len(message1) + len(IPaddr) + len(':') + len(PORT) + len(message2)*/
    size_t content_length = strlen(getMessage1) + strlen(getMessage2) + strlen(getMessage3) + strlen(getMessage4);

    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println("Content-Type: text/plain");
    client.print("Content-Length: "); client.println(content_length);

    client.print(getMessage1);
    client.print(getMessage2);
    client.print(getMessage3);
    client.print(getMessage4);
    
    #ifdef VERBOSE
        while(!Serial) Serial.begin(115200);
        Serial.println("-----------------");
        Serial.println("Sending standard GET Response");
        Serial.println("-----------------");
        Serial.println();
    #endif

    delay(500);

    client.stop();
    return;
}





/**
 * @brief 
 * 
 * @param client 
 */
void NetInterface::_sendCONTINUEReponse(WiFiClient& client){
    client.println("HTTP/1.1 100 Continue");

    #ifdef VERBOSE
        while(!Serial) Serial.begin(115200);
        Serial.println("Sent Continue");
    #endif

    return;;
}




/**
 * @brief sends a response to client based on input
 * error code and status message
 * 
 * @param client 
 * @param code 
 * @param status 
 */
void NetInterface::_sendHTTPResponse(WiFiClient& client, int code, const char* status){
    while(client.available()){
        client.read();
    }

    client.print("HTTP/1.1 ");
    client.print(code);
    client.print(" ");
    client.println(status);
    client.println("Connection: close");
    client.println("Content-Type: text/plain");
    client.print("Content-Length: "); client.println(strlen(status));
    client.println();
    client.print(status);

    #ifdef VERBOSE
        Serial.println();
        Serial.println("-----------------");
        Serial.println("Sending Response:");
        Serial.print("HTTP/1.1 "); Serial.print(code); Serial.print(" "); Serial.println(status);
        Serial.println("Connection: close");
        Serial.println("Content-Type: text/plain");
        Serial.println("Content-Length: "); client.println(strlen(status));
        Serial.println();
        Serial.println(status);
    #endif

    delay(500);

    #ifdef VERBOSE
        Serial.println("-----------------");
        Serial.println();
        Serial.println("Closing connection...");
    #endif

    client.stop();
    
    return;
}




/**
 * @brief Reads the rest of the content and discards it
 * 
 * @param client 
 * @param contentLength 
 */
void NetInterface::_flushRequestBody(WiFiClient& client, long contentLength){

    long read = 0;

    while(client.connected() && read < contentLength){
        if(client.available()){
            read++;
            client.read();
        }
    }
}





/**
 * @brief 
 * 
 * @param client 
 * @param contentLength 
 */
void NetInterface::_flashApp(WiFiClient& client, long contentLength){

    /* get new app partition (stored in _updatePartition class variable)*/
    _getPartition();
    if(!_updatePartition){

        #ifdef VERBOSE
            Serial.println("Failed to get new app partition.");
        #endif

        _flushRequestBody(client, contentLength);
        _sendHTTPResponse(client, 500, "Internal Server Error");
        return;
    }

    /* commence (allocate memory & erase old partition)*/
    esp_ota_handle_t *ota_handle = (esp_ota_handle_t*)malloc(sizeof(esp_ota_handle_t));
    esp_err_t err = esp_ota_begin(_updatePartition, (size_t)contentLength, ota_handle);
    if(err != ESP_OK){

        #ifdef VERBOSE
            Serial.print("Error commencing flash: ");
            Serial.println(esp_err_to_name(err));
        #endif

        _flushRequestBody(client, contentLength);
        _sendHTTPResponse(client, 500, "Internal Server Error");
        return;
    }

    /* begin reading data and writing in */
    long write_length = _writePartition(client, contentLength, ota_handle);

    /* if incorrect payload size, do not boot to new app */
    if(write_length != contentLength){

        #ifdef VERBOSE
            Serial.println("-----------------");
            Serial.print("Expected "); Serial.print(contentLength);
            Serial.print(" but got "); Serial.print(write_length);
            Serial.println(" bytes");
            Serial.println("-----------------");
            Serial.println();
        #endif

        _sendHTTPResponse(client, 414, "Payload Wrong Size");
        esp_ota_abort(*ota_handle);
        return;
    }



    /* set boot to new app */
    err = esp_ota_set_boot_partition(_updatePartition);
    if(err != ESP_OK){

        #ifdef VERBOSE
            Serial.print("Error setting boot: ");
            Serial.println(esp_err_to_name(err));
        #endif

        _sendHTTPResponse(client, 500, "Internal Server Error");
        esp_ota_abort(*ota_handle);
        return;
    }

    /* free handle memory & validate flash */
    err = esp_ota_end(*ota_handle);
    if(err != ESP_OK){

        #ifdef VERBOSE
            Serial.print("Error freeing | validating flash: ");
            Serial.println(esp_err_to_name(err));
        #endif

        _sendHTTPResponse(client, 500, "Internal Server Error");
        esp_ota_abort(*ota_handle);
        return;
    }

    _sendHTTPResponse(client, 200, "OK");
    ESP.restart();
}





/**
 * @brief Gets the app to flash (0 or 1) & sets _updatePartition variable
 * 
 */
void NetInterface::_getPartition(void){

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *app0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app0");
    const esp_partition_t *app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app1");

    /* check for all partitions present */
    if(!(running && app0 && app1)){
        _updatePartition = NULL;
        return;
    }

    /* if app0, flash app 1 & vice versa */
    if(running == app0){
        _updatePartition = app1;
    }
    else{
        _updatePartition = app0;
    }

    #ifdef VERBOSE
            Serial.println("Partition (to-be-written) Info:");
            Serial.print("Label: "); Serial.println(_updatePartition->label);
            Serial.print("Address: 0x"); Serial.println(_updatePartition->address, HEX);
            Serial.print("Size: 0x"); Serial.println(_updatePartition->size, HEX);
            Serial.print("Encryption: "); _updatePartition->encrypted ? Serial.println("yes") : Serial.println("no");
            Serial.print("Flash Chip ID: "); Serial.println(_updatePartition->flash_chip->chip_id);
    #endif

    return;
}





/**
 * @brief reads from client and writes to the
 * given ota handle
 * 
 * @param client 
 * @param contentLength 
 * @param handle 
 * @return long content written in bytes
 */
long NetInterface::_writePartition(WiFiClient& client, long contentLength, esp_ota_handle_t* handle){

    long contentRead = 0;
    uint8_t buf[1];

    while(client.available()){
        int read = client.read(buf, 1);


        if(read){
            esp_err_t err = esp_ota_write(*handle, buf, 1);
            if(err != ESP_OK){

                #ifdef VERBOSE
                    Serial.print("Error writing: ");
                    Serial.println(esp_err_to_name(err));
                #endif

                return 0;
            }
            contentRead++;
        }
    }

    return contentRead;
}





/**
 * @brief switches from appx to appy
 * (where x and y are 0 or 1)
 * 
 * @return true successful boot switch
 * @return false failed boot switch
 */
bool NetInterface::_switchApp(void){

    _getPartition();
    if(_updatePartition){
       if(esp_ota_set_boot_partition(_updatePartition) != ESP_OK) return false;
       else return true;
    }
    return false;
}