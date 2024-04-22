#include <Seeed_Arduino_SSCMA.h>
#include <WiFi.h>

#include "app_httpd.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid     = "";
const char* password = "";

void initSharedBuffer();
void initStatInfo();

void startRemoteProxy(Proto);
void startCameraServer();

void loopRemoteProxy();

void setup() {
    initSharedBuffer();
    initStatInfo();

    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();

    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    startRemoteProxy(PROTO_UART);
    startCameraServer();

    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
}

void loop() {
    loopRemoteProxy();
    delay(5);
}
