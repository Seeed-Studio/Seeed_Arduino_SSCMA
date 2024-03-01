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

bool SSCMA::begin(TwoWire *wire, uint16_t address, uint32_t wait_delay,
                  uint32_t clock)
{
    _wire = wire;
    _serial = NULL;
    _address = address;
    _wire->begin();
    _wire->setClock(clock);
    _wait_delay = wait_delay;
    i2c_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_RESET);
    rx_ofs = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(payload, 0, sizeof(payload));
    response.clear();

    return ID(false) && name(false);
}

bool SSCMA::begin(HardwareSerial *serial, uint32_t baud,
                  uint32_t wait_delay)
{
    _serial = serial;
    _wire = NULL;
    _baud = baud;
    _wait_delay = wait_delay;
    _serial->begin(_baud);
    _serial->setTimeout(1000);
    _serial->flush();

    rx_ofs = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(payload, 0, sizeof(payload));
    response.clear();

    return ID(false) && name(false);
}

bool SSCMA::begin(SPIClass *spi, int32_t cs, int32_t sync, uint32_t baud, uint32_t wait_delay)
{
    _spi = spi;
    _cs = cs;
    _sync = sync;
    _baud = baud;
    _wait_delay = wait_delay;

    _spi->begin();

    if (_cs >= 0)
    {
        pinMode(_cs, OUTPUT);
        digitalWrite(_cs, HIGH);
    }

    if (_sync >= 0)
    {
        pinMode(_sync, INPUT);
    }

    rx_ofs = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(payload, 0, sizeof(payload));
    response.clear();

    return true;
}

int SSCMA::write(const char *data, int length)
{
    // Serial.print("write: ");
    // Serial.write(data, length);
    if (_serial)
    {
        return _serial->write(data, length);
    }
    else if (_spi)
    {
        return spi_write(data, length);
    }
    else
    {
        return i2c_write(data, length);
    }
}

int SSCMA::read(char *data, int length)
{
    if (_serial)
    {
        return _serial->readBytes(data, length);
    }
    else if (_spi)
    {
        return spi_read(data, length);
    }
    else
    {
        return i2c_read(data, length);
    }
}

int SSCMA::available()
{
    if (_serial)
    {
        return _serial->available();
    }
    else if (_spi)
    {
        return spi_available();
    }
    else
    {
        return i2c_available();
    }
}

void SSCMA::i2c_cmd(uint8_t feature, uint8_t cmd, uint16_t len, uint8_t *data)
{
    delay(_wait_delay);
    _wire->beginTransmission(_address);
    _wire->write(feature);
    _wire->write(cmd);
    _wire->write(len >> 8);
    _wire->write(len & 0xFF);
    if (data != NULL)
    {
        _wire->write(data, len);
    }
    // TODO checksum
    _wire->write(0);
    _wire->write(0);
    _wire->endTransmission();
}

int SSCMA::i2c_available()
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
        _wire->requestFrom(_address, (uint8_t)2);
        _wire->readBytes(buf, (uint8_t)2);
    }

    return (buf[0] << 8) | buf[1];
}

int SSCMA::i2c_read(char *data, int length)
{
    uint16_t packets = length / MAX_PL_LEN;
    uint8_t remain = length % MAX_PL_LEN;
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

int SSCMA::i2c_write(const char *data, int length)
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
        _wire->write((const uint8_t *)(data + i * MAX_PL_LEN), MAX_PL_LEN);
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
        _wire->write((const uint8_t *)(data + packets * MAX_PL_LEN), remain);
        _wire->endTransmission();
    }
    return length;
}

void SSCMA::spi_cmd(uint8_t feature, uint8_t cmd, uint16_t len, uint8_t *data)
{
    delay(_wait_delay);
    tx_buf[0] = feature;
    tx_buf[1] = cmd;
    tx_buf[2] = len >> 8;
    tx_buf[3] = len & 0xFF;
    if (data != NULL)
    {
        memcpy(&tx_buf[4], data, len);
        tx_buf[4 + len] = 0xFF;
        tx_buf[5 + len] = 0xFF;
    }
    else
    {
        tx_buf[4] = 0xFF;
        tx_buf[5] = 0xFF;
    }

    if (_cs >= 0)
    {
        digitalWrite(_cs, LOW);
    }
    _spi->beginTransaction(SPISettings(_baud, MSBFIRST, SPI_MODE0));
    _spi->transfer(tx_buf, PACKET_SIZE);
    _spi->endTransaction();
    if (_cs >= 0)
    {
        digitalWrite(_cs, HIGH);
    }
    delay(_wait_delay);
}

int SSCMA::spi_available()
{
    uint16_t size;

    if (_sync >= 0)
    {
        if (digitalRead(_sync) == LOW)
            return 0;
    }

    spi_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_AVAILABLE, 0, NULL);
    if (_cs >= 0)
    {
        digitalWrite(_cs, LOW);
    }
    _spi->beginTransaction(SPISettings(_baud, MSBFIRST, SPI_MODE0));
    size = _spi->transfer16(0xFFFF);
    _spi->endTransaction();
    if (_cs >= 0)
    {
        digitalWrite(_cs, HIGH);
    }
    delay(_wait_delay);
    return size;
}

int SSCMA::spi_read(char *data, int length)
{
    uint16_t packets = length / MAX_SPI_PL_LEN;
    uint16_t remain = length % MAX_SPI_PL_LEN;
    for (uint16_t i = 0; i < packets; i++)
    {
        spi_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_READ, MAX_SPI_PL_LEN, NULL);
        if (_cs >= 0)
        {
            digitalWrite(_cs, LOW);
        }
        _spi->beginTransaction(SPISettings(_baud, MSBFIRST, SPI_MODE0));
        _spi->transfer(data + i * MAX_SPI_PL_LEN, MAX_SPI_PL_LEN);
        _spi->endTransaction();
        if (_cs >= 0)
        {
            digitalWrite(_cs, HIGH);
        }
    }
    if (remain)
    {
        spi_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_READ, remain, NULL);
        if (_cs >= 0)
        {
            digitalWrite(_cs, LOW);
        }
        _spi->beginTransaction(SPISettings(_baud, MSBFIRST, SPI_MODE0));
        _spi->transfer(data + packets * MAX_SPI_PL_LEN, remain);

        _spi->endTransaction();

        if (_cs >= 0)
        {
            digitalWrite(_cs, HIGH);
        }
    }
    return length;
}

int SSCMA::spi_write(const char *data, int length)
{
    uint16_t packets = length / MAX_PL_LEN;
    uint16_t remain = length % MAX_PL_LEN;
    for (uint16_t i = 0; i < packets; i++)
    {
        spi_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_WRITE, MAX_PL_LEN, (uint8_t *)data);
    }
    if (remain)
    {
        spi_cmd(FEATURE_TRANSPORT, FEATURE_TRANSPORT_CMD_WRITE, remain, (uint8_t *)data);
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

        if (response["data"].containsKey("keypoints"))
        {
            _keypoints.clear();
            JsonArray keypoints = response["data"]["keypoints"];
            for (size_t i = 0; i < keypoints.size(); i++)
            {
                keypoints_t k;
                JsonArray box = keypoints[i][0];
                JsonArray points = keypoints[i][1];
                k.box.x = box[0];
                k.box.y = box[1];
                k.box.w = box[2];
                k.box.h = box[3];
                k.box.score = box[4];
                k.box.target = box[5];

                for (size_t j = 0; j < points.size(); j++)
                {
                    point_t p;
                    p.x = points[j][0];
                    p.y = points[j][1];
                    // p.z = points[j][2];
                    p.score = points[j][2];
                    p.target = points[j][3];
                    k.points.push_back(p);
                }
                _keypoints.push_back(k);
            }
        }
        if (response["data"].containsKey("image"))
        {
            _image = response["data"]["image"].as<String>();
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
        int len = available();
        if (len) {
            if (len + rx_end > sizeof(rx_buf)) {
                rx_end = 0;
                return CMD_ENOMEM;
            }
            rx_end += read(rx_buf + rx_end, len);
            rx_buf[rx_end] = '\0';
        }
        while (char *suffix = strnstr(rx_buf + rx_ofs, RESPONSE_SUFFIX, rx_end - rx_ofs)) {
            if (char *prefix = strnstr(rx_buf + rx_ofs, RESPONSE_PREFIX, suffix - rx_buf - rx_ofs))
            {
                // get json payload
                len = suffix - prefix;
                memcpy(payload, prefix + 1, len);
                payload[len] = '\0';
                // Serial.printf("\npayload :%s", payload);
                // parse json response
                response.clear();
                DeserializationError error = deserializeJson(response, payload);
                if (error) {
                    // Serial.print(F("deserializeJson() failed: "));
                    // Serial.println(error.c_str());
                    continue;
                }

                if (response["type"] == CMD_TYPE_EVENT) {
                    praser_event();
                }

                if (response["type"] == CMD_TYPE_LOG) {
                    praser_log();
                }

                ret = response["code"];
     
                if (response["type"] == type && strcmp(response["name"], cmd) == 0) {
                    return ret;
                }
            } else {
                len = suffix - rx_buf;
            }
            rx_ofs += len + strlen(RESPONSE_SUFFIX);
        }
        memmove(rx_buf, rx_buf + rx_ofs, rx_end - rx_ofs);
        rx_end -= rx_ofs;
        rx_ofs = 0;
    }

    return CMD_ETIMEDOUT;
}

void SSCMA::fetch(ResponseCallback RespCallback)
{
    int len = available();
    if (len) {
        if (len + rx_end > sizeof(rx_buf)) {
            rx_end = 0;
        }
        rx_end += read(rx_buf + rx_end, len);
        rx_buf[rx_end] = '\0';
        Serial.printf("[%d] ->", len);
    }
    while (char *suffix = strnstr(rx_buf + rx_ofs, RESPONSE_SUFFIX, rx_end - rx_ofs))
    {
        if (char *prefix = strnstr(rx_buf + rx_ofs, RESPONSE_PREFIX, suffix - rx_buf - rx_ofs)) {
            len = suffix - prefix + strlen(RESPONSE_SUFFIX);
            memcpy(payload, prefix, len);
            payload[len] = '\0';
            RespCallback(payload);
        } else {
            len = suffix - rx_buf + strlen(RESPONSE_SUFFIX);
            Serial.print(" X");
        }
        rx_ofs += len;
    }
    memmove(rx_buf, rx_buf + rx_ofs, rx_end - rx_ofs);
    rx_end -= rx_ofs;
    rx_ofs = 0;
}

int SSCMA::invoke(int times, bool filter, bool show)
{
    snprintf(payload, sizeof(payload), CMD_PREFIX "%s=%d,%d,%d" CMD_SUFFIX,
             CMD_AT_INVOKE, times, filter, !show);
    write(payload, strlen(payload));

    if (wait(CMD_TYPE_RESPONSE, CMD_AT_INVOKE) == CMD_OK)
    {
        if (wait(CMD_TYPE_EVENT, CMD_AT_INVOKE) == CMD_OK)
        {
            return CMD_OK;
        }
    }

    return CMD_ETIMEDOUT;
}

int SSCMA::WIFI(wifi_t &wifi)
{
    sprintf(payload, CMD_PREFIX "%s?" CMD_SUFFIX, CMD_AT_WIFI);
    write(payload, strlen(payload));

    if (wait(CMD_TYPE_RESPONSE, "WIFI?", 1000) == CMD_OK)
    {
        strcpy(wifi.ssid, response["data"]["config"]["name"]);
        strcpy(wifi.password, response["data"]["config"]["password"]);
        return CMD_OK;
    }

    return CMD_ETIMEDOUT;
}

int SSCMA::MQTT(mqtt_t &mqtt)
{
    sprintf(payload, CMD_PREFIX "%s?" CMD_SUFFIX, CMD_AT_MQTTSERVER);
    write(payload, strlen(payload));

    if (wait(CMD_TYPE_RESPONSE, "MQTTSERVER?", 1000) == CMD_OK)
    {
        strcpy(mqtt.server, response["data"]["config"]["address"]);
        mqtt.port = response["data"]["config"]["port"];
        strcpy(mqtt.username, response["data"]["config"]["username"]);
        strcpy(mqtt.password, response["data"]["config"]["password"]);
        mqtt.use_ssl = response["data"]["config"]["use_ssl"] == 1;
        strcpy(mqtt.client_id, response["data"]["config"]["client_id"]);
        return CMD_OK;
    }

    return CMD_ETIMEDOUT;
}

char *SSCMA::ID(bool cache)
{
    if (cache && _ID)
    {
        return _ID;
    }

    snprintf(tx_buf, sizeof(tx_buf), CMD_PREFIX "%s" CMD_SUFFIX, CMD_AT_ID);

    write(tx_buf, strlen(tx_buf));

    if (wait(CMD_TYPE_RESPONSE, CMD_AT_ID) == CMD_OK)
    {
        strcpy(_ID, response["data"]);
        return _ID;
    }

    return NULL;
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