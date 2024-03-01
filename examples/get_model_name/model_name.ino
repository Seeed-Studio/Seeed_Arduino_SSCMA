/**
 * @file model_name.ino
 * @date  01 March 2024

 * @author Spencer Yan
 *
 * @attention Required packages:
 * - ArduinoJson V7
 * - Base64 by Arturo Guadalupi
 * 
 * @copyright © 2024, Seeed Studio
 */

#include <Seeed_Arduino_SSCMA.h>

#ifdef ESP32
#include <HardwareSerial.h>

// Define two Serial devices mapped to the two internal UARTs
HardwareSerial atSerial(0);

#else
#define atSerial Serial1
#endif

SSCMA AI;

void setup()
{
    Serial.begin(9600);
    AI.begin(&atSerial);
    
    if (!AI.fetch_info())
    {
        String base64String = AI.info();
        pcSerial.printf("AI.info(): %s \n", base64String.c_str());

        int inputStringLength = base64String.length() + 1;
        char inputString[inputStringLength];
        base64String.toCharArray(inputString, inputStringLength);

        /* Decode the base64 String */
        int decodedLength = Base64.decodedLength(inputString, inputStringLength - 1); // Calculate decoding length
        char decodedString[decodedLength + 1];                                        // Allocate additional space for null terminators

        Base64.decode(decodedString, inputString, inputStringLength - 1); // Decoding Base64 strings
        decodedString[decodedLength] = '\0';                              // Manually adding null terminators to ensure correct string termination

        Serial.printf("decodedString: %s \n", decodedString);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, decodedString);
        if (error)
        { // Check if deserialization was successful
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }
        Serial.printf("model_name: %s \n", doc["model_name"]);
    }
}

void loop()
{
    ;
}