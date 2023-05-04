#include "WiFi.h"
#include "NetInterface.h"

NetInterface net;

/**
 * @brief poll the input Serial to input
 * UCB Wireless or something else (defauled to
 * my home wifi rn)
 * 
 */
void pollSSID(void){
    String input_ssid;
    while(!Serial) Serial.begin(115200);
    Serial.println(); Serial.print("Enter SSID: ");
    while(!Serial.available());
    while(Serial.available()) input_ssid = Serial.readString();
    Serial.println(); Serial.println();

    if(input_ssid == "UCB Wireless"){
        while(!net.wifi_init("UCB Wireless", NULL));
    }
    else{
        while(!net.wifi_init("Boulder Public Wifi", "K!LLy0ur53lf"));
    }   
    net.begin(3232U, "password");
}





/**
 * @brief setup
 * 
 * @details initializes wifi connection and
 * initializes a web server running on local
 * IP address
 * 
 */
void setup(void){
    pollSSID();
}





/**
 * @brief loop
 * 
 * @details polls for http clients and
 * flashes if given the proper headers
 * and data
 * 
 */
void loop(void){
    net.poll();
}