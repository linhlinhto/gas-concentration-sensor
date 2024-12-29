#include <Arduino.h>
#include "secrets/wifi.h"
#include "wifi_connect.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"
#include "secrets/mqtt.h"
#include <PubSubClient.h>
#include <esp_sleep.h>
#include <vector>
#include <string>

#define BUZZER_PIN 12
#define ANALOG_PIN 33  // Chân đọc giá trị analog

int countTime = 0;
int awaketime = 0;

RTC_DATA_ATTR int currentwifi = 1;  // Lưu trữ WiFi đang kết nối
RTC_DATA_ATTR String ssid1 = "Huong Giang";
RTC_DATA_ATTR String password1 = "0974011612";
RTC_DATA_ATTR String ssid2 = "linhlinhto";
RTC_DATA_ATTR String password2 = "hailinh11";
RTC_DATA_ATTR String ssid3 = "";
RTC_DATA_ATTR String password3 = "";
namespace {
    const char *gas_con_topic = "gas_con";  // Chủ đề MQTT để gửi dữ liệu analog
    const char *wifi1_ssid_topic = "wifi1/ssid";
    const char *wifi1_pass_topic = "wifi1/pass";
    const char *wifi2_ssid_topic = "wifi2/ssid";
    const char *wifi2_pass_topic = "wifi2/pass";
    const char *wifi3_ssid_topic = "wifi3/ssid";
    const char *wifi3_pass_topic = "wifi3/pass";
    const char *wifi1_status_topic = "wifi1/status";
    const char *wifi2_status_topic = "wifi2/status";
    const char *wifi3_status_topic = "wifi3/status";

    uint16_t keepAlive = 60;    // seconds (default is 15)
    uint16_t socketTimeout = 30; // seconds (default is 15)
}

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

int Wifichange = 0;  // Biến theo dõi WiFi nào đang được kiểm tra và chuyển đổi

bool tryConnectWiFi(const char *ssid, const char *password)
{
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {  // Retry for 45 seconds
        delay(1000);
        retryCount++;
        Serial.print(".");
        awaketime = millis();
    }
    return WiFi.status() == WL_CONNECTED;
}

void reconnectWiFiFromRTC()
{
    if (currentwifi == 1 && ssid1 != "") {
        if (tryConnectWiFi(ssid1.c_str(), password1.c_str())) {
            Serial.println("Reconnected to WiFi1 from RTC.");
        }
        else if (tryConnectWiFi(ssid2.c_str(), password2.c_str()))
        {
            Serial.println("Reconnected to WiFi2 from RTC.");
            currentwifi =  2; 
        }
        else if (tryConnectWiFi(ssid3.c_str(), password3.c_str()))
        {
            Serial.println("Reconnected to WiFi3 from RTC.");
            currentwifi =  3; 
        }
    } else if (currentwifi == 2 && ssid2 != "") {
        if (tryConnectWiFi(ssid2.c_str(), password2.c_str())) {
            Serial.println("Reconnected to WiFi2 from RTC.");
        }
        else if (tryConnectWiFi(ssid1.c_str(), password1.c_str()))
        {
            Serial.println("Reconnected to WiFi1 from RTC.");
            currentwifi =  1; 
        }
        else if (tryConnectWiFi(ssid3.c_str(), password3.c_str()))
        {
            Serial.println("Reconnected to WiFi3 from RTC.");
            currentwifi =  3; 
        }
    } else if (currentwifi == 3 && ssid3 != "") {
        if (tryConnectWiFi(ssid3.c_str(), password3.c_str())) {
            Serial.println("Reconnected to WiFi3 from RTC.");
        }
        else if (tryConnectWiFi(ssid2.c_str(), password2.c_str()))
        {
            Serial.println("Reconnected to WiFi2 from RTC.");
            currentwifi =  2; 
        }
        else if (tryConnectWiFi(ssid1.c_str(), password1.c_str()))
        {
            Serial.println("Reconnected to WiFi1 from RTC.");
            currentwifi =  1; 
        }
    }
    
}


void mqttReconnect()
{
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




void sendWiFiStatus()
{   
    switch (currentwifi) {
            case 1:
            mqttClient.publish(wifi1_status_topic,"ON", false);
            mqttClient.publish(wifi2_status_topic,"OFF", false);
            mqttClient.publish(wifi3_status_topic,"OFF", false);
                break;
            case 2:
                mqttClient.publish(wifi1_status_topic,"OFF", false);
                mqttClient.publish(wifi2_status_topic,"ON", false);
                mqttClient.publish(wifi3_status_topic,"OFF", false);
                break;
            case 3:
                mqttClient.publish(wifi1_status_topic,"OFF", false);
                mqttClient.publish(wifi2_status_topic,"OFF", false);
                mqttClient.publish(wifi3_status_topic,"ON", false);
                break;
        }
    
}

void sendWiFiConfig()
{
    mqttClient.publish(wifi1_ssid_topic, ssid1.c_str(), false);
    mqttClient.publish(wifi1_pass_topic, password1.c_str(), false);
    mqttClient.publish(wifi2_ssid_topic, ssid2.c_str(), false);
    mqttClient.publish(wifi2_pass_topic, password2.c_str(), false);
    mqttClient.publish(wifi3_ssid_topic, ssid3.c_str(), false);
    mqttClient.publish(wifi3_pass_topic, password3.c_str(), false);
}



void checkAndSwitchWiFi(int wifiIndex, const char* newSSID, const char* newPass)
{
    if(wifiIndex == currentwifi){
    boolean conecttonew = false;
    switch (wifiIndex) {
            case 1:
                if(password1 != String(newPass) || ssid1 != String(newSSID)) conecttonew = true;
                break;
            case 2:
                if(password2 != String(newPass) || ssid2 != String(newSSID)) conecttonew = true;
                break;
            case 3:
                if(password3 != String(newPass) || ssid3 != String(newSSID)) conecttonew = true;
                break;
        }
    if(conecttonew){
    if (tryConnectWiFi(newSSID, newPass)) {
        // Nếu kết nối thành công, cập nhật cấu hình
        switch (wifiIndex) {
            case 1:
                ssid1 = String(newSSID);
                password1 = String(newPass);
                break;
            case 2:
                ssid2 = String(newSSID);
                password2 = String(newPass);
                break;
            case 3:
                ssid3 = String(newSSID);
                password3 = String(newPass);
                break;
        }
        Serial.println("Connected to new WiFi and updated configuration.");
        tlsClient.setCACert(ca_cert);
        mqttClient.setKeepAlive(keepAlive); // To see how long mqttClient detects the TCP connection is lost
        mqttClient.setSocketTimeout(socketTimeout); // To see how long mqttClient detects the TCP connection is lost
        mqttClient.setServer(MQTT::broker, MQTT::port);

        if (!mqttClient.connected()) {
            mqttReconnect();
        }
        mqttClient.loop();
    } else {
        Serial.println("Failed to connect to new WiFi, configuration remains unchanged.");
       reconnectWiFiFromRTC();
            if (!mqttClient.connected()) {
                    mqttReconnect();
            }
            mqttClient.loop();
             switch (wifiIndex) {
            case 1:
                mqttClient.publish("wifi1/changessid", ssid1.c_str(), true);
                mqttClient.publish("wifi1/changepass", password1.c_str(), true);
                break;
            case 2:
                mqttClient.publish("wifi2/changessid", ssid2.c_str(), true);
                mqttClient.publish("wifi2/changepass", password2.c_str(), true);
                break;
            case 3:
                mqttClient.publish("wifi3/changessid", ssid3.c_str(), true);
                mqttClient.publish("wifi3/changepass", password3.c_str(), true);
                break;
            }
            delay(1000);
           
        }
    }
    }
    else {
        switch (wifiIndex) {
            case 1:
                ssid1 = String(newSSID);
                password1 = String(newPass);
                break;
            case 2:
                ssid2 = String(newSSID);
                password2 = String(newPass);
                break;
            case 3:
                ssid3 = String(newSSID);
                password3 = String(newPass);
                break;
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == "wifi1/changessid") {
        Serial.print("Received new SSID for WiFi1: ");
        Serial.println(message);
        checkAndSwitchWiFi(1, message.c_str(), password1.c_str());  // Kiểm tra và chuyển đổi WiFi1
    }
    if (String(topic) == "wifi1/changepass") {
        Serial.print("Received new Password for WiFi1: ");
        Serial.println(message);
        checkAndSwitchWiFi(1, ssid1.c_str(), message.c_str());  // Kiểm tra và chuyển đổi WiFi1
    }
    if (String(topic) == "wifi2/changessid") {
        Serial.print("Received new SSID for WiFi2: ");
        Serial.println(message);
        checkAndSwitchWiFi(2, message.c_str(), password2.c_str());  // Kiểm tra và chuyển đổi WiFi2
    }
    if (String(topic) == "wifi2/changepass") {
        Serial.print("Received new Password for WiFi2: ");
        Serial.println(message);
        checkAndSwitchWiFi(2, ssid2.c_str(), message.c_str());  // Kiểm tra và chuyển đổi WiFi2
    }
    if (String(topic) == "wifi3/changessid") {
        Serial.print("Received new SSID for WiFi3: ");
        Serial.println(message);
        checkAndSwitchWiFi(3, message.c_str(), password3.c_str());  // Kiểm tra và chuyển đổi WiFi3
    }
    if (String(topic) == "wifi3/changepass") {
        Serial.print("Received new Password for WiFi3: ");
        Serial.println(message);
        checkAndSwitchWiFi(3, ssid3.c_str(), message.c_str());  // Kiểm tra và chuyển đổi WiFi3
    }

    // Xử lý kết nối WiFi theo yêu cầu từ topic "wifi/connect"
    if (String(topic) == "wifi/connect") {
        int wifiIndex = message.toInt();
        Serial.print("Received request to connect to WiFi: ");
        Serial.println(wifiIndex);

        if (wifiIndex >= 1 && wifiIndex <= 3 && wifiIndex != currentwifi) {
            const char *targetSSID;
            const char *targetPassword;

            switch (wifiIndex) {
                case 1:
                    targetSSID = ssid1.c_str();
                    targetPassword = password1.c_str();
                    break;
                case 2:
                    targetSSID = ssid2.c_str();
                    targetPassword = password2.c_str();
                    break;
                case 3:
                    targetSSID = ssid3.c_str();
                    targetPassword = password3.c_str();
                    break;
            }

            if (tryConnectWiFi(targetSSID, targetPassword)) {
                currentwifi = wifiIndex;
                Serial.print("Successfully connected to WiFi ");
                Serial.println(wifiIndex);
                sendWiFiStatus();
            } else {
                Serial.print("Failed to connect to WiFi ");
                Serial.println(wifiIndex);
                // Quay lại WiFi hiện tại
                reconnectWiFiFromRTC();
                if (!mqttClient.connected()) {
                    mqttReconnect();
                }
                mqttClient.loop();
                mqttClient.publish("wifi/connect", String(currentwifi).c_str(), true);
                delay(1000);
            }
        } else {
            Serial.println("Invalid WiFi index received.");
        }
    }
}


void checkAnalogData()
{
    int analogValue = analogRead(ANALOG_PIN); // Đọc giá trị analog từ chân 33
    Serial.print("Analog Value: ");
    Serial.println(analogValue);
    float concen = (analogValue / 4095.0) * 100;
    mqttClient.publish(gas_con_topic, String(concen).c_str(), false);
    if (concen > 40) {
        digitalWrite(BUZZER_PIN, HIGH);
        while (concen > 40) {
            int analogValue = analogRead(ANALOG_PIN); // Đọc giá trị analog từ chân 33
            Serial.print("Analog Value: ");
            Serial.println(analogValue);
            float concen = (analogValue / 4095.0) * 100;
            mqttClient.publish(gas_con_topic, String(concen).c_str(), false);
        }
        digitalWrite(BUZZER_PIN, LOW);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(10);
    pinMode(ANALOG_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    reconnectWiFiFromRTC();
    tlsClient.setCACert(ca_cert);
    mqttClient.setServer(MQTT::broker, MQTT::port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(keepAlive); // To see how long mqttClient detects the TCP connection is lost
    mqttClient.setSocketTimeout(socketTimeout); // To see how long mqttClient detects the TCP connection is lost
    if (!mqttClient.connected()) {
        mqttReconnect();
    }
    mqttClient.loop();
    awaketime = millis();
}

void loop()
{
    if(millis()-countTime >= 1000){
        countTime = millis();
        // Kiểm tra và gửi dữ liệu
        checkAnalogData();
        sendWiFiStatus();
        sendWiFiConfig();
    }
    if (millis()-awaketime >= 10000){
        Serial.println("Entering deep sleep for 5 minute...");
        esp_sleep_enable_timer_wakeup(1 * 30 * 1000000); // 5 phút
        esp_deep_sleep_start();
    }
        
     if (!mqttClient.connected()) {
        mqttReconnect();
    }
    mqttClient.loop();
}