/***
 * Seeed_Arduino_SSCMA.h
 * Description: A drive for Seeed Grove AI Family.
 * 2022 Copyright (c) Seeed Technology Inc.  All right reserved.
 * Author: Hongtai Liu(lht856@foxmail.com)
 *
 * Copyright (C) 2020  Seeed Technology Co.,Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef SEEED_ARDUINO_SSCMA_H
#define SEEED_ARDUINO_SSCMA_H

#include <stdint.h>
#include <vector>
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

#define I2C_ADDRESS (0x62)

#define HEADER_LEN 4
#ifdef ARDUINO_ARCH_AVR
#define MAX_PL_LEN 26
#else
#define MAX_PL_LEN 250
#endif
#define CHECKSUM_LEN 2

#define FEATURE_TRANSPORT 0x10
#define FEATURE_TRANSPORT_CMD_READ 0x01
#define FEATURE_TRANSPORT_CMD_WRITE 0x02
#define FEATURE_TRANSPORT_CMD_AVAILABLE 0x03
#define FEATURE_TRANSPORT_CMD_START 0x04
#define FEATURE_TRANSPORT_CMD_STOP 0x05
#define FEATURE_TRANSPORT_CMD_RESET 0x06

#define RESPONSE_PREFIX "\r{"
#define RESPONSE_SUFFIX "}\n"

#define CMD_PREFIX "AT+"
#define CMD_SUFFIX "\r\n"

#define CMD_TYPE_RESPONSE 0
#define CMD_TYPE_EVENT 1
#define CMD_TYPE_LOG 2

const char CMD_AT_ID[] = "ID?";
const char CMD_AT_NAME[] = "NAME?";
const char CMD_AT_VERSION[] = "VER?";
const char CMD_AT_STATS[] = "STAT";
const char CMD_AT_BREAK[] = "BREAK";
const char CMD_AT_RESET[] = "RST";
const char CMD_AT_WIFI[] = "WIFI";
const char CMD_AT_MQTTSERVER[] = "MQTTSERVER";
const char CMD_AT_MQTTPUBSUB[] = "MQTTPUBSUB";
const char CMD_AT_INVOKE[] = "INVOKE";
const char CMD_AT_SAMPLE[] = "SAMPLE";
const char CMD_AT_INFO[] = "INFO";
const char CMD_AT_TSCORE[] = "TSCORE";
const char CMD_AT_TIOU[] = "TIOU";
const char CMD_AT_ALGOS[] = "ALGOS";
const char CMD_AT_MODELS[] = "MODELS";
const char CMD_AT_MODEL[] = "MODEL";
const char CMD_AT_SENSORS[] = "SENSORS";
const char COMMADN_AT_ACTION[] = "ACTION";
const char CMD_AT_LED[] = "LED";

#define CMD_OK 0
#define CMD_AGAIN 1
#define CMD_ELOG 2
#define CMD_ETIMEDOUT 3
#define CMD_EIO 4
#define CMD_EINVAL 5
#define CMD_ENOMEM 6
#define CMD_EBUSY 7
#define CMD_ENOTSUP 8
#define CMD_EPERM 9
#define CMD_EUNKNOWN 10

const char EVENT_INVOKE[] = "INVOKE";
const char EVENT_SAMPLE[] = "SAMPLE";
const char EVENT_WIFI[] = "WIFI";
const char EVENT_MQTT[] = "MQTT";
const char EVENT_SUPERVISOR[] = "SUPERVISOR";

const char LOG_AT[] = "AT";
const char LOG_LOG[] = "LOG";

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t score;
    uint8_t target;
} boxes_t;

typedef struct
{
    uint8_t target;
    uint8_t score;
} classes_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint8_t score;
    uint8_t target;
} point_t;

typedef struct
{
    uint16_t prepocess;
    uint16_t inference;
    uint16_t postprocess;
} perf_t;

class SSCMA
{
private:
    TwoWire *_wire;
    uint8_t _address;
    int _wait_delay;
    perf_t _perf;
    std::vector<boxes_t> _boxes;
    std::vector<classes_t> _classes;
    std::vector<point_t> _points;

    uint32_t _ID = 0;
    char _name[32] = {0};

    char tx_buf[256] = {0};            // for cmd
    char rx_buf[2048] = {0};           // for response
    char payload[512] = {0};          // for json payload
    uint16_t offset = 0;               // for rx_buf
    StaticJsonDocument<2048> response; // for json response

public:
    SSCMA();
    ~SSCMA();

    bool begin(TwoWire *wire = &Wire, int address = I2C_ADDRESS,
               uint32_t wait_delay = 2, uint32_t clock = 400000);
    int invoke(int times = 1, bool filter = 0, bool show = 0);
    size_t available();
    size_t read(char *data, size_t length);
    size_t write(const char *data, size_t length);
    void reset();

    perf_t &perf() { return _perf; }
    std::vector<boxes_t> &boxes() { return _boxes; }
    std::vector<classes_t> &classes() { return _classes; }
    std::vector<point_t> &points() { return _points; }

    uint32_t ID(bool cache = true);
    char *name(bool cache = true);

private:
    void cmd(uint8_t feature, uint8_t cmd, uint16_t len = 0);
    int wait(int type, const char *cmd, uint32_t timeout = 500);
    void praser_event();
    void praser_log();
};

#endif