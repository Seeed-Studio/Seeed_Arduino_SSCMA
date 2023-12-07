/***
 * Seeed_Arduino_GroveAI.cpp
 * Description: A drive for Seeed Grove AI Family.
 * 2022 Copyright (c) Seeed Technology Inc.  All right reserved.
 * Author: Hongtai Liu(lht856@foxmail.com)
 * 2022-4-24
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

#include "Seeed_Arduino_SSCMA.h"

SSCMA::SSCMA()
{
    _wire = &Wire;
    _address = I2C_ADDRESS;
    _wait_delay = 2;
}

SSCMA::~SSCMA() {}

void SSCMA::begin(TwoWire *wire, int address, uint32_t wait_delay,
                  uint32_t clock)
{
    _wire = wire;
    _address = address;
    _wire->begin();
    _wire->setClock(clock);
    _wait_delay = wait_delay;
    cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_RESET);
}

void SSCMA::cmd(uint8_t feature, uint8_t cmd, uint16_t len)
{
    delay(_wait_delay);
    _wire->beginTransmission(_address);
    _wire->write(feature);
    _wire->write(cmd);
    _wire->write(len >> 8);
    _wire->write(len & 0xFF);
    // TODO checksum
    _wire->write(0);
    _wire->write(0);
    _wire->endTransmission();
}

size_t SSCMA::available()
{
    uint8_t buf[2] = {0};
    delay(_wait_delay);
    _wire->beginTransmission(_address);
    _wire->write(FEATURE_TRANSPORT);
    _wire->write(FEATURE_TRANSPORT_CMD_AVAILABLE);
    _wire->write(0);
    _wire->write(0);
    // TODO checksum
    _wire->write(0);
    _wire->write(0);
    if (_wire->endTransmission() == 0)
    {
        delay(_wait_delay);
        _wire->requestFrom(_address, 2);
        _wire->readBytes(buf, 2);
    }

    return (buf[0] << 8) | buf[1];
}

size_t SSCMA::read(char *data, size_t length)
{
    uint16_t packets = length / MAX_PL_LEN;
    uint16_t remain = length % MAX_PL_LEN;
    for (uint16_t i = 0; i < packets; i++)
    {
        delay(_wait_delay);
        _wire->beginTransmission(_address);
        _wire->write(FEATURE_TRANSPORT);
        _wire->write(FEATURE_TRANSPORT_CMD_READ);
        _wire->write(MAX_PL_LEN >> 8);
        _wire->write(MAX_PL_LEN & 0xFF);
        // TODO checksum
        _wire->write(0);
        _wire->write(0);
        if (_wire->endTransmission() == 0)
        {
            delay(_wait_delay);
            _wire->requestFrom(_address, MAX_PL_LEN);
            _wire->readBytes(data + i * MAX_PL_LEN, MAX_PL_LEN);
        }
    }
    if (remain)
    {
        delay(_wait_delay);
        _wire->beginTransmission(_address);
        _wire->write(FEATURE_TRANSPORT);
        _wire->write(FEATURE_TRANSPORT_CMD_READ);
        _wire->write(remain >> 8);
        _wire->write(remain & 0xFF);
        // TODO checksum
        _wire->write(0);
        _wire->write(0);
        if (_wire->endTransmission() == 0)
        {
            delay(_wait_delay);
            _wire->requestFrom(_address, remain);
            _wire->readBytes(data + packets * MAX_PL_LEN, remain);
        }
    }
    return length;
}

size_t SSCMA::write(const char *data, size_t length)
{
    uint16_t packets = length / MAX_PL_LEN;
    uint16_t remain = length % MAX_PL_LEN;
    for (uint16_t i = 0; i < packets; i++)
    {
        delay(_wait_delay);
        _wire->beginTransmission(_address);
        _wire->write(FEATURE_TRANSPORT);
        _wire->write(FEATURE_TRANSPORT_CMD_WRITE);
        _wire->write(MAX_PL_LEN >> 8);
        _wire->write(MAX_PL_LEN & 0xFF);
        _wire->write(data + i * MAX_PL_LEN, MAX_PL_LEN);
        // TODO checksum
        _wire->write(0);
        _wire->write(0);
        _wire->endTransmission();
    }
    if (remain)
    {
        delay(_wait_delay);
        _wire->beginTransmission(_address);
        _wire->write(FEATURE_TRANSPORT);
        _wire->write(FEATURE_TRANSPORT_CMD_WRITE);
        _wire->write(remain >> 8);
        _wire->write(remain & 0xFF);
        _wire->write(data + packets * MAX_PL_LEN, remain);
        _wire->endTransmission();
    }
    return length;
}

int SSCMA::invoke(int times, bool filter, bool show)
{
    char buf[1024] = {0};
    char payload[512] = {0};
    char offset = 0;
    StaticJsonDocument<1024> response;

    snprintf(buf, sizeof(buf), CMD_PREFIX "%s=%d,%d,%d" CMD_SUFFIX,
             CMD_AT_INVOKE, times, filter, !show);
    write(buf, strlen(buf));

    unsigned long startTime = millis();
    while (millis() - startTime <= 5000)
    {
        while (int len = available())
        {
            read(buf + offset, len);
            offset += len;
            while (char *suffix = strstr(buf, RESPONSE_SUFFIX))
            {
                size_t length = suffix - buf + 1;
                memcpy(payload, buf + 1, length - 1);
                if (deserializeJson(response, payload))
                {
                    return CMD_EUNKNOWN;
                }
                if (response["code"] != CMD_OK)
                {
                    return response["code"];
                }
                if (response["type"] == CMD_TYPE_EVENT)
                {
                    if (response["data"].containsKey("perf"))
                    {
                        _perf.prepocess = response["data"]["perf"][0];
                        _perf.inference = response["data"]["perf"][1];
                        _perf.postprocess = response["data"]["perf"][2];
                    }
                    if (response["data"].containsKey("boxes"))
                    {
                        _boxes.clear();
                        JsonArray boxes = response["data"]["boxes"];
                        for (size_t i = 0; i < boxes.size(); i++)
                        {
                            JsonArray box = boxes[i];
                            boxes_t b;
                            b.x = box[0];
                            b.y = box[1];
                            b.w = box[2];
                            b.h = box[3];
                            b.score = box[4];
                            b.target = box[5];
                            _boxes.push_back(b);
                        }
                    }

                    if (response["data"].containsKey("classes"))
                    {
                        _classes.clear();
                        JsonArray classes = response["data"]["classes"];
                        for (size_t i = 0; i < classes.size(); i++)
                        {
                            JsonArray cls = classes[i];
                            classes_t c;
                            c.target = cls[0];
                            c.score = cls[1];
                            _classes.push_back(c);
                        }
                    }

                    if (response["data"].containsKey("points"))
                    {
                        _points.clear();
                        JsonArray points = response["data"]["points"];
                        for (size_t i = 0; i < points.size(); i++)
                        {
                            JsonArray point = points[i];
                            point_t p;
                            p.x = point[0];
                            p.y = point[1];
                            // p.z = point[2];
                            p.score = point[2];
                            p.target = point[3];
                            _points.push_back(p);
                        }
                    }

                    return CMD_OK;
                }
                memmove(buf, buf + length, offset - length);
                buf[offset - length] = 0; // clear the last char
                memset(payload, 0, sizeof(payload));
                offset -= length;
            }
        }
        delay(_wait_delay);
    }
    return CMD_ETIMEDOUT;
}