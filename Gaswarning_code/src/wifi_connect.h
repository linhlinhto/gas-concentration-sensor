/***
 * generic `setup_wifi` utilities
 ***/

#pragma once
#include <Arduino.h>
#include <WiFi.h>

inline void setup_wifi(const char *SSID, const char *PASS)
{
    delay(10);
    Serial.printf("Connecting to %s ", SSID);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED)
    { // ESP will hang there!!!
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected to SSID: '%s'\n", WiFi.SSID().c_str());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}