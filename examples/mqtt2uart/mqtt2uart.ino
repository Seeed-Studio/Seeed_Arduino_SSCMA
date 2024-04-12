#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Seeed_Arduino_SSCMA.h>

#ifdef ESP32
#include <HardwareSerial.h>

// Define two Serial devices mapped to the two internal UARTs
HardwareSerial atSerial(0);

#else
#define atSerial Serial1
#endif

#ifdef ARDUINO_XIAO_ESP32C3
#define WIFI_VERSION "xiao_esp32c3:1.0.0"
#elif defined(ARDUINO_XIAO_ESP32S3)
#define WIFI_VERSION "xiao_esp32s3:1.0.0"
#endif

SSCMA AI;
wifi_t wifi_config = {0};
mqtt_t mqtt_config = {0};
wifi_status_t wifi_status = {0};
mqtt_status_t mqtt_status = {0};
char topic[48] = {0};
char cmd_buf[1024] = {0};

uint32_t count = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
    Serial.begin(115200);
    // while (!Serial)
    // {
    //     delay(1000);
    // }

    // you should increase the buffer size if you have more than 32K for Serial
#ifdef ESP32
    atSerial.setRxBufferSize(32 * 1024);
#endif

    AI.begin(&atSerial, D3);

    setup_wifi();
    if (!client.setBufferSize(32 * 1024))
    {
        while (1)
            ;
    }

    Serial.println("Proxy start");
}

void loop()
{
    is_changed();
    if (!client.connected())
        reconnect();
    client.loop();
    AI.fetch(fetch_callback);
}

void proxy_callback(const char *pl)
{
    Serial.write(pl, strlen(pl));
}

void fetch_callback(const char *pl, size_t len)
{
    // Serial.print("-> ");
    // Serial.println(len);
    client.publish(topic, pl);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    AI.write((char *)payload, length);
    // Serial.print("<- ");
    // Serial.println(length);
}

void setup_wifi()
{
    count = 0;
#ifdef WIFI_VERSION
    Serial.println("Set WIFI version...");
    while (CMD_OK != AI.WIFIVER(WIFI_VERSION))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }
#endif
    Serial.println("Set WIFI status...");
    while (CMD_OK != AI.WIFISTA(wifi_status))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }
    count = 0;
    Serial.println("Set MQTT status...");
    while (CMD_OK != AI.MQTTSTA(mqtt_status))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }

    count = 0;
    Serial.println("Get WIFI config...");
    while (CMD_OK != AI.WIFI(wifi_config))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }
    count = 0;
    Serial.println("Get MQTT config...");
    while (CMD_OK != AI.MQTT(mqtt_config))
    {
        delay(10);
        count++;
        if (count > 10)
        {
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
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        count++;
        if (count > 100)
        {
            Serial.println("Timeout, restart...");
            ESP.restart();
        }
    }
    count = 0;
    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");

    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.subnetMask());
    Serial.println(WiFi.gatewayIP());
    wifi_config.status = 2;
    wifi_status.status = 2;
    strcpy(wifi_status.ipv4, WiFi.localIP().toString().c_str());
    strcpy(wifi_status.netmask, WiFi.subnetMask().toString().c_str());
    strcpy(wifi_status.gateway, WiFi.gatewayIP().toString().c_str());
    count = 0;
    Serial.println("Set WIFI status...");
    while (CMD_OK != AI.WIFISTA(wifi_status))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }

    client.setServer(mqtt_config.server, mqtt_config.port);
    client.setCallback(mqtt_callback);
}

void is_changed()
{
    static uint32_t time_count = millis();
    if (millis() - time_count < 3000) // check every 5s
    {
        return;
    }
    else
    {
        time_count = millis();
    }
    count = 0;
    wifi_t _wifi_config;
    while (CMD_OK != AI.WIFI(_wifi_config))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }

    if (_wifi_config.status != wifi_config.status || _wifi_config.security != wifi_config.security || strcmp(_wifi_config.ssid, wifi_config.ssid) != 0 || strcmp(_wifi_config.password, wifi_config.password) != 0)
    {
        Serial.println("WIFI config changed, restart...");
        ESP.restart();
    }

    count = 0;
    mqtt_t _mqtt_config;
    while (CMD_OK != AI.MQTT(_mqtt_config))
    {
        delay(10);
        count++;
        if (count > 10)
        {
            Serial.println("Failed, restart...");
            ESP.restart();
        }
    }

    if (_mqtt_config.status != mqtt_config.status || _mqtt_config.port != mqtt_config.port || _mqtt_config.use_ssl != mqtt_config.use_ssl || strcmp(_mqtt_config.server, mqtt_config.server) != 0 || strcmp(_mqtt_config.username, mqtt_config.username) != 0 || strcmp(_mqtt_config.password, mqtt_config.password) != 0 || strcmp(_mqtt_config.client_id, mqtt_config.client_id) != 0)
    {
        Serial.println("MQTT config changed, restart...");
        ESP.restart();
    }
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Connecting MQTT...");
        if (client.connect(mqtt_config.client_id, mqtt_config.username, mqtt_config.password))
        {
            count = 0;
            snprintf(topic, sizeof(topic), "sscma/v0/%s/rx", mqtt_config.client_id);
            Serial.println("connected");
            snprintf(cmd_buf, sizeof(cmd_buf), "{\"client_id\":\"%s\"}", mqtt_config.client_id);
            client.publish("sscma/v0/discovery", cmd_buf);
            client.subscribe(topic);
            snprintf(topic, sizeof(topic), "sscma/v0/%s/tx", mqtt_config.client_id);
            mqtt_config.status = 2;
            mqtt_status.status = 2;
            AI.MQTTSTA(mqtt_status);
        }
        else
        {
            mqtt_config.status = 0;
            mqtt_status.status = 0;
            AI.MQTTSTA(mqtt_status);
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 1 seconds");
            // Wait 1 seconds before retrying
            delay(1000);
            count++;
            if (count > 5)
            {
                Serial.println("Timeout, restart...");
                ESP.restart();
            }
        }
    }
}
