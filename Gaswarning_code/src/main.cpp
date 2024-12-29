#include <Arduino.h>
#include "wifi_connect.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"
#include "secrets/mqtt.h"
#include <PubSubClient.h>
#include <esp_sleep.h>
#include <WebServer.h>
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;

WebServer server(80);
#define BUZZER_PIN 12
#define ANALOG_PIN 33

int countTime = 0;
int awaketime = 0;

RTC_DATA_ATTR int currentwifi = 1;
RTC_DATA_ATTR char ssid1[32] = "";
RTC_DATA_ATTR char password1[32] = "";
RTC_DATA_ATTR char ssid2[32] = "";
RTC_DATA_ATTR char password2[32] = "";
RTC_DATA_ATTR char ssid3[32] = "";
RTC_DATA_ATTR char password3[32] = "";

namespace {
    const char *gas_con_topic = "gas_con";
    const char *wifi1_ssid_topic = "wifi1/ssid";
    const char *wifi1_pass_topic = "wifi1/pass";
    const char *wifi2_ssid_topic = "wifi2/ssid";
    const char *wifi2_pass_topic = "wifi2/pass";
    const char *wifi3_ssid_topic = "wifi3/ssid";
    const char *wifi3_pass_topic = "wifi3/pass";
    const char *wifi1_status_topic = "wifi1/status";
    const char *wifi2_status_topic = "wifi2/status";
    const char *wifi3_status_topic = "wifi3/status";

    uint16_t keepAlive = 60;
    uint16_t socketTimeout = 30;
}

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

bool tryConnectWiFi(const char *ssid, const char *password) {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(1000);
        retryCount++;
        Serial.print(".");
        awaketime = millis();
    }
    return WiFi.status() == WL_CONNECTED;
}

void reconnectWiFiFromRTC() {
    if (currentwifi == 1 && strlen(ssid1) > 0) {
        if (tryConnectWiFi(ssid1, password1)) {
            Serial.println("Reconnected to WiFi1 from RTC.");
        } else if (tryConnectWiFi(ssid2, password2)) {
            Serial.println("Reconnected to WiFi2 from RTC.");
            currentwifi = 2;
        } else if (tryConnectWiFi(ssid3, password3)) {
            Serial.println("Reconnected to WiFi3 from RTC.");
            currentwifi = 3;
        }
    } else if (currentwifi == 2 && strlen(ssid2) > 0) {
        if (tryConnectWiFi(ssid2, password2)) {
            Serial.println("Reconnected to WiFi2 from RTC.");
        } else if (tryConnectWiFi(ssid1, password1)) {
            Serial.println("Reconnected to WiFi1 from RTC.");
            currentwifi = 1;
        } else if (tryConnectWiFi(ssid3, password3)) {
            Serial.println("Reconnected to WiFi3 from RTC.");
            currentwifi = 3;
        }
    } else if (currentwifi == 3 && strlen(ssid3) > 0) {
        if (tryConnectWiFi(ssid3, password3)) {
            Serial.println("Reconnected to WiFi3 from RTC.");
        } else if (tryConnectWiFi(ssid2, password2)) {
            Serial.println("Reconnected to WiFi2 from RTC.");
            currentwifi = 2;
        } else if (tryConnectWiFi(ssid1, password1)) {
            Serial.println("Reconnected to WiFi1 from RTC.");
            currentwifi = 1;
        }
    }
}

void mqttReconnect() {
    while (!mqttClient.connected()) {
        Serial.println("Attempting MQTT connection...");
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        if (mqttClient.connect(client_id.c_str(), MQTT::username, MQTT::pass)) {
            Serial.println("MQTT connected");
            mqttClient.subscribe("wifi1/changessid");
            mqttClient.subscribe("wifi1/changepass");
            mqttClient.subscribe("wifi2/changessid");
            mqttClient.subscribe("wifi2/changepass");
            mqttClient.subscribe("wifi3/changessid");
            mqttClient.subscribe("wifi3/changepass");
            mqttClient.subscribe("wifi/connect");
        } else {
            reconnectWiFiFromRTC();
            Serial.print("MQTT connect failed, rc=");
            Serial.println(mqttClient.state());
            Serial.println("Retrying in 1 second...");
            delay(1000);
        }
    }
}

void sendWiFiStatus() {
    switch (currentwifi) {
        case 1:
            mqttClient.publish(wifi1_status_topic, "ON", false);
            mqttClient.publish(wifi2_status_topic, "OFF", false);
            mqttClient.publish(wifi3_status_topic, "OFF", false);
            break;
        case 2:
            mqttClient.publish(wifi1_status_topic, "OFF", false);
            mqttClient.publish(wifi2_status_topic, "ON", false);
            mqttClient.publish(wifi3_status_topic, "OFF", false);
            break;
        case 3:
            mqttClient.publish(wifi1_status_topic, "OFF", false);
            mqttClient.publish(wifi2_status_topic, "OFF", false);
            mqttClient.publish(wifi3_status_topic, "ON", false);
            break;
    }
}

void sendWiFiConfig() {
    mqttClient.publish(wifi1_ssid_topic, ssid1, false);
    mqttClient.publish(wifi1_pass_topic, password1, false);
    mqttClient.publish(wifi2_ssid_topic, ssid2, false);
    mqttClient.publish(wifi2_pass_topic, password2, false);
    mqttClient.publish(wifi3_ssid_topic, ssid3, false);
    mqttClient.publish(wifi3_pass_topic, password3, false);
}

void checkAndSwitchWiFi(int wifiIndex, const char *newSSID, const char *newPass) {
    if (wifiIndex == currentwifi) {
        bool connectToNew = false;
        switch (wifiIndex) {
            case 1:
                if (strcmp(password1, newPass) != 0 || strcmp(ssid1, newSSID) != 0) connectToNew = true;
                break;
            case 2:
                if (strcmp(password2, newPass) != 0 || strcmp(ssid2, newSSID) != 0) connectToNew = true;
                break;
            case 3:
                if (strcmp(password3, newPass) != 0 || strcmp(ssid3, newSSID) != 0) connectToNew = true;
                break;
        }
        if (connectToNew) {
            if (tryConnectWiFi(newSSID, newPass)) {
                switch (wifiIndex) {
                    case 1:
                        strncpy(ssid1, newSSID, sizeof(ssid1));
                        strncpy(password1, newPass, sizeof(password1));
                        break;
                    case 2:
                        strncpy(ssid2, newSSID, sizeof(ssid2));
                        strncpy(password2, newPass, sizeof(password2));
                        break;
                    case 3:
                        strncpy(ssid3, newSSID, sizeof(ssid3));
                        strncpy(password3, newPass, sizeof(password3));
                        break;
                }
                Serial.println("Connected to new WiFi and updated configuration.");
            } else {
                Serial.println("Failed to connect to new WiFi, configuration remains unchanged.");
                reconnectWiFiFromRTC();
                if (!mqttClient.connected()) {
                    mqttReconnect();
                }
                mqttClient.loop();
                switch (wifiIndex) {
                case 1:
                    mqttClient.publish("wifi1/changessid", String(ssid1).c_str(), true);
                    mqttClient.publish("wifi1/changepass", String(password1).c_str(), true);
                    break;
                case 2:
                    mqttClient.publish("wifi2/changessid", String(ssid2).c_str(), true);
                    mqttClient.publish("wifi2/changepass", String(password2).c_str(), true);
                    break;
                case 3:
                    mqttClient.publish("wifi3/changessid", String(ssid3).c_str(), true);
                    mqttClient.publish("wifi3/changepass", String(password3).c_str(), true);
                    break;
                }
                delay(1000);
            }
        }
    } else {
        switch (wifiIndex) {
            case 1:
                strncpy(ssid1, newSSID, sizeof(ssid1));
                strncpy(password1, newPass, sizeof(password1));
                break;
            case 2:
                strncpy(ssid2, newSSID, sizeof(ssid2));
                strncpy(password2, newPass, sizeof(password2));
                break;
            case 3:
                strncpy(ssid3, newSSID, sizeof(ssid3));
                strncpy(password3, newPass, sizeof(password3));
                break;
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    if (strcmp(topic, "wifi1/changessid") == 0) {
        checkAndSwitchWiFi(1, message, password1);
    } else if (strcmp(topic, "wifi1/changepass") == 0) {
        checkAndSwitchWiFi(1, ssid1, message);
    } else if (strcmp(topic, "wifi2/changessid") == 0) {
        checkAndSwitchWiFi(2, message, password2);
    } else if (strcmp(topic, "wifi2/changepass") == 0) {
        checkAndSwitchWiFi(2, ssid2, message);
    } else if (strcmp(topic, "wifi3/changessid") == 0) {
        checkAndSwitchWiFi(3, message, password3);
    } else if (strcmp(topic, "wifi3/changepass") == 0) {
        checkAndSwitchWiFi(3, ssid3, message);
    } else if (strcmp(topic, "wifi/connect") == 0) {
        int wifiIndex = atoi(message);
        if (wifiIndex >= 1 && wifiIndex <= 3 && wifiIndex != currentwifi) {
            const char *targetSSID;
            const char *targetPassword;
            switch (wifiIndex) {
                case 1:
                    targetSSID = ssid1;
                    targetPassword = password1;
                    break;
                case 2:
                    targetSSID = ssid2;
                    targetPassword = password2;
                    break;
                case 3:
                    targetSSID = ssid3;
                    targetPassword = password3;
                    break;
            }
            if (tryConnectWiFi(targetSSID, targetPassword)) {
                currentwifi = wifiIndex;
                sendWiFiStatus();
            } else {
                reconnectWiFiFromRTC();
                if (!mqttClient.connected()) {
                    mqttReconnect();
                }           
                mqttClient.loop();
                mqttClient.publish("wifi/connect", String(currentwifi).c_str(), true);
                delay(1000);
            }
        }
    }
}

void checkAnalogData() {
    int analogValue = analogRead(ANALOG_PIN);
    float concentration = (analogValue / 4095.0) * 100;
    mqttClient.publish(gas_con_topic, String(concentration).c_str(), false);
    if (concentration > 40) {
        digitalWrite(BUZZER_PIN, HIGH);
        while (concentration > 40) {
            analogValue = analogRead(ANALOG_PIN);
            concentration = (analogValue / 4095.0) * 100;
            mqttClient.publish(gas_con_topic, String(concentration).c_str(), false);
        }
        digitalWrite(BUZZER_PIN, LOW);
    }
}
void handleNotFound() {
    String message = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=/' /></head><body>";
    message += "<p>If you are not redirected, click <a href='/'>here</a>.</p>";
    message += "</body></html>";
    server.send(200, "text/html", message);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>WiFi Setup</title></head><body>";
    html += "<h1>WiFi Configuration</h1>";
    html += "<form method='POST' action='/submit'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

void handleSubmit() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (!ssid.isEmpty() && !password.isEmpty()) {
        strncpy(ssid1, ssid.c_str(), sizeof(ssid1));
        strncpy(password1, password.c_str(), sizeof(password1));
        currentwifi = 1;

        String response = "<!DOCTYPE html><html><head><title>WiFi Setup</title></head><body>";
        response += "<h1>Configuration Saved</h1>";
        response += "<p>ESP will restart and connect to the new WiFi settings.</p>";
        response += "</body></html>";
        server.send(200, "text/html", response);

        delay(3000);
        esp_sleep_enable_timer_wakeup(7 * 1000000);
        esp_deep_sleep_start();
    } else {
        server.send(400, "text/html", "<h1>Error: SSID and Password cannot be empty!</h1>");
    }
}

void startCaptivePortal() {
    Serial.println("Starting Captive Portal...");
    WiFi.softAP("ESP32_Config", "12345678");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Chuyển hướng tất cả các yêu cầu DNS về địa chỉ AP
    dnsServer.start(DNS_PORT, "*", IP);

    server.on("/", handleRoot);
    server.on("/submit", HTTP_POST, handleSubmit);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("HTTP server started");
}



void setup() {
    Serial.begin(115200);
    delay(10);
    pinMode(ANALOG_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    if (strlen(ssid1) == 0) {
        startCaptivePortal();
    } else {
        reconnectWiFiFromRTC();
    }

    if (WiFi.isConnected()) {
        tlsClient.setCACert(ca_cert);
        mqttClient.setServer(MQTT::broker, MQTT::port);
        mqttClient.setCallback(mqttCallback);
        mqttClient.setKeepAlive(keepAlive);
        mqttClient.setSocketTimeout(socketTimeout);
        if (!mqttClient.connected()) {
            mqttReconnect();
        }
        mqttClient.loop();
        awaketime = millis();
    }
    else {
        startCaptivePortal();
    }
}

void loop() {
    if (WiFi.getMode() == WIFI_AP) {
        dnsServer.processNextRequest(); // Xử lý các yêu cầu DNS
        server.handleClient(); // Xử lý các yêu cầu HTTP
        return;
    }
    else{
    if (millis() - countTime >= 1000) {
        countTime = millis();
        checkAnalogData();
        sendWiFiStatus();
        sendWiFiConfig();
    }

    if (millis() - awaketime >= 10000) {
        Serial.println("Entering deep sleep for 5 minutes...");
        esp_sleep_enable_timer_wakeup(5 * 60 * 1000000);
        esp_deep_sleep_start();
    }

    if (!mqttClient.connected()) {
        mqttReconnect();
    }
    mqttClient.loop();
    }
}
