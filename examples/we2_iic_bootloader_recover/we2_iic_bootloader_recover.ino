#include <Arduino.h>
#include <Wire.h>

#include "firmware.h"

// Please check the I2C pins of your board, -1 for default I2C pins
#define I2C_SDA_PIN -1
#define I2C_SCL_PIN -1

bool probeDevice();

int ep2_isp_i2c_proc(uint8_t* file_buf, uint32_t file_size);

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    Serial.println();

    Serial.println("Grove Vision AI (WE2) I2C Bootloader Recovery Tool");
    Serial.println("Built on " __DATE__ " " __TIME__);
    Serial.println();

    bool ok = Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 900ul * 1000ul);
    if (!ok) {
        Serial.println("Wire initialization failed");
        while (1)
            ;
    }
    Wire.setTimeOut(100);
    Wire.setBufferSize(1024);
}

void loop() {
    int c;

ProbeDevice:
    Serial.println("Probing Grove Vision AI (WE2) on I2C bus...");
    if (probeDevice()) {
        Serial.println("Detected target device");
    } else {
        Serial.println("Cannot detect target device, please check the I2C wire or power supply");
        Serial.println();
        delay(1000);
        goto ProbeDevice;
    }
    Serial.println();

WaitKey:
    while (Serial.available()) {
        Serial.read();
    }
    Serial.println("Press 'enter' to start recover the bootloader (or 'q' to restart)");

    c = -1;
    while (c == -1) {
        c = Serial.read();
    }

    switch (c) {
    case 'Q':
    case 'q':
        Serial.println();
        delay(10);
        break;
    case '\r':
    case '\n':
        Serial.println("Recovering WE2 bootloader...");
        {
            int ret = ep2_isp_i2c_proc((uint8_t*)firmware, firmware_len);
            if (ret == 0) {
                Serial.println("Done, the bootloader has been recovered successfully!");
            } else {
                Serial.print("Failed to recover the bootloader, code: ");
                Serial.println(ret);
            }
        }
        Serial.println();
        delay(3000);
        break;
    default:
        Serial.print("Invalid key: ");
        Serial.print((char)c);
        Serial.println();
        delay(10);
        goto WaitKey;
    }
}