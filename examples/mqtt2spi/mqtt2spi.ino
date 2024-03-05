#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Seeed_Arduino_SSCMA.h>

SSCMA AI;
wifi_t wifi_config = {0};
mqtt_t mqtt_config = {0};
char topic[48] = {0};
char cmd_buf[1024] = {0};

uint32_t count = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
    Serial.begin(115200);
    // while (!Serial) {
    //   delay(1000);
    // }

    SPI.begin(SCK, MOSI, MISO, -1);
    AI.begin(&SPI, D1, D0, 15000000);

    setup_wifi();
    client.setServer(mqtt_config.server, mqtt_config.port);
    client.setCallback(mqtt_callback);
    if (!client.setBufferSize(32 * 1024)) {while(1);}

    Serial.println("Proxy start");
}

void loop()
{
    if (!client.connected()) reconnect();
    client.loop();
    AI.fetch(fetch_callback);

    // if (int s_len = Serial.available()) {
    //   AI.write(cmd_buf, Serial.readBytes(cmd_buf, s_len));
    // }
    // AI.fetch(proxy_callback);
}

void proxy_callback(const char* pl) {
  Serial.write(pl, strlen(pl));
}

void fetch_callback(const char* pl) {
  client.publish(topic, pl);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    AI.write((char *)payload, length);
    Serial.print("\n <- ");
    Serial.println(length);
}

void setup_wifi()
{
    delay(2000); // wait AT ready

    count = 0;
    Serial.println("Get WIFI config...");
    while (CMD_OK != AI.WIFI(wifi_config)) {
        delay(10);
        count++;
        if (count > 30) {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }

    count = 0;
    Serial.println("Get MQTT config...");
    while (CMD_OK != AI.MQTT(mqtt_config)) {
        delay(10);
        count++;
        if (count > 30) {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }
    Serial.println();
    Serial.print("Client ID: ");
    Serial.println(mqtt_config.client_id);
    
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(wifi_config.ssid);

    WiFi.begin(wifi_config.ssid, wifi_config.password);
    count = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        count++;
        if (count > 300) {
            Serial.println("Timeout, restart...");
            ESP.restart();
        }
    }
    count = 0;
    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");

    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());
    // Serial.println(WiFi.subnetMask());
    // Serial.println(WiFi.gatewayIP());
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Connecting MQTT...");
        if (client.connect(mqtt_config.client_id, mqtt_config.username, mqtt_config.password)) {
            count = 0;
            snprintf(topic, sizeof(topic), "sscma/v0/%s/rx", mqtt_config.client_id);
            Serial.println("connected");
            client.publish("sscma/v0/discovery", "mqtt2spi");
            client.subscribe(topic);
            snprintf(topic, sizeof(topic), "sscma/v0/%s/tx", mqtt_config.client_id);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 1 seconds");
            // Wait 1 seconds before retrying
            delay(1000);
            count++;
            if (count > 5) {
                Serial.println("Timeout, restart...");
                ESP.restart();
            }
        }
    }
}

