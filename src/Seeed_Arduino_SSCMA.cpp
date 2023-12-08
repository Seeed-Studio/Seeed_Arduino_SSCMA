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

bool SSCMA::begin(TwoWire *wire, int address, uint32_t wait_delay,
                  uint32_t clock)
{
    _wire = wire;
    _address = address;
    _wire->begin();
    _wire->setClock(clock);
    _wait_delay = wait_delay;
    cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_RESET);

    offset = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(payload, 0, sizeof(payload));
    response.clear();

    return ID(false) && name(false);
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
    Serial.print("write: ");
    Serial.write(data, length);
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

void SSCMA::praser_event()
{
    if (strstr(response["name"], CMD_AT_INVOKE))
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
                c.target = cls[1];
                c.score = cls[0];
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
    }
}
void SSCMA::praser_log()
{
}

int SSCMA::wait(int type, const char *cmd, uint32_t timeout)
{
    int ret = CMD_OK;
    unsigned long startTime = millis();
    while (millis() - startTime <= timeout)
    {

        if (size_t len = available())
        {
            offset += read(rx_buf + offset, len);
            rx_buf[offset] = '\0';
        }
        // seek response suffix
        while (char *suffix = strnstr(rx_buf, RESPONSE_SUFFIX, offset))
        {
            // parse response
            if (char *prefix = strnstr(rx_buf, RESPONSE_PREFIX, suffix - rx_buf))
            {
                // parse json payload
                size_t len = suffix - prefix;
                memcpy(payload, prefix + 1, len);
                payload[len] = '\0';
                // delete this pyload in rx_buf
                // Serial.print("payload: ");
                // Serial.println(payload);
                memmove(rx_buf, suffix + strlen(RESPONSE_SUFFIX),
                        offset - len - strlen(RESPONSE_PREFIX));
                offset -= suffix - rx_buf + strlen(RESPONSE_SUFFIX);
                rx_buf[offset] = '\0';
                response.clear();
                DeserializationError error = deserializeJson(response, payload);
                if (error)
                {
                    // Serial.print(F("deserializeJson() failed: "));
                    // Serial.println(error.c_str());
                    continue;
                }

                if (response["type"] == CMD_TYPE_EVENT)
                {
                    praser_event();
                }

                if (response["type"] == CMD_TYPE_LOG)
                {
                    praser_log();
                }

                ret = response["code"];

                if (response["type"] == type && strcmp(response["name"], cmd) == 0)
                {
                    return ret;
                }
            }
            else
            {
                // discard buffer befor suffix
                memmove(rx_buf, suffix + strlen(RESPONSE_SUFFIX),
                        offset - (suffix - rx_buf) - strlen(RESPONSE_PREFIX));
                offset -= suffix - rx_buf + strlen(RESPONSE_SUFFIX);
                rx_buf[offset] = '\0';
            }
        }
        delay(_wait_delay);
    }

    return CMD_ETIMEDOUT;
}

int SSCMA::invoke(int times, bool filter, bool show)
{
    snprintf(tx_buf, sizeof(tx_buf), CMD_PREFIX "%s=%d,%d,%d" CMD_SUFFIX,
             CMD_AT_INVOKE, times, filter, !show);

    write(tx_buf, strlen(tx_buf));

    if (wait(CMD_TYPE_RESPONSE, CMD_AT_INVOKE) == CMD_OK)
    {
        if (wait(CMD_TYPE_EVENT, CMD_AT_INVOKE) == CMD_OK)
        {
            return CMD_OK;
        }
    }

    return CMD_ETIMEDOUT;
}

uint32_t SSCMA::ID(bool cache)
{
    if (cache && _ID)
    {
        return _ID;
    }

    snprintf(tx_buf, sizeof(tx_buf), CMD_PREFIX "%s" CMD_SUFFIX, CMD_AT_ID);

    write(tx_buf, strlen(tx_buf));

    if (wait(CMD_TYPE_RESPONSE, CMD_AT_ID) == CMD_OK)
    {
        _ID = response["data"];
        return _ID;
    }

    return 0;
}
char *SSCMA::name(bool cache)
{
    if (cache && _name[0])
    {
        return _name;
    }

    snprintf(tx_buf, sizeof(tx_buf), CMD_PREFIX "%s" CMD_SUFFIX, CMD_AT_NAME);

    write(tx_buf, strlen(tx_buf));

    if (wait(CMD_TYPE_RESPONSE, CMD_AT_NAME, 3000) == CMD_OK)
    {
        strcpy(_name, response["data"]);
        return _name;
    }

    return NULL;
}