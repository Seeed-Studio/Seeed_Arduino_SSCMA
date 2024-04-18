// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified by nullptr, Seeed Technology Inc (c) 2024
//

#include "app_httpd.h"

#include <ArduinoJson.h>
#include <FreeRTOS.h>
#include <Seeed_Arduino_SSCMA.h>
#include <Wire.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <mbedtls/base64.h>
#include <sdkconfig.h>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "BYTETracker.h"
#include "web_index.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
    #include <HardwareSerial.h>
    #include <esp32-hal-log.h>
#endif

#define RESULT_TIMEOUT_MS 3000
#define CMD_TIMEOUT_MS    3000

#define PTR_BUFFER_SIZE   8
#define RSP_BUFFER_SIZE   (1024 * 196)
#define JPG_BUFFER_SIZE   (1024 * 128)
#define RST_BUFFER_SIZE   (1024 * 128)
#define QRY_BUFFER_SIZE   (1024 * 16)
#define CMD_BUFFER_SIZE   (1024 * 12)

#define CMD_TAG_FMT_STR   "HTTPD%.8X@"
#define CMD_TAG_SIZE      snprintf(NULL, 0, CMD_TAG_FMT_STR, 0)

#define MSG_IMAGE_KEY     "\"image\": "
#define MSG_COMMA_STR     ", "
#define MSG_QUOTE_STR     "\""
#define MSG_REPLY_STR     "\"type\": 0"
#define MSG_EVENT_STR     "\"type\": 1"
#define MSG_LOGGI_STR     "\"type\": 2"
#define MSG_TERMI_STR     "\r\n"

enum MsgType : uint16_t {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_REPLY   = 0xff & (1 << 1),
    MSG_TYPE_EVENT   = 0xff & (1 << 2),
    MSG_TYPE_LOGGI   = 0xff & (1 << 3),
};

#define CMD_SAMPLE_STR "SAMPLE"
#define CMD_INVOKE_STR "INVOKE"

enum CmdType : uint16_t {
    CMD_TYPE_UNKNOWN = 0,
    CMD_TYPE_SAMPLE  = 0xff00 & (1 << 8),
    CMD_TYPE_INVOKE  = 0xff00 & (2 << 8),
    CMD_TYPE_SENSOR  = 0xff00 & (3 << 8),
};

struct PtrBuffer {
    struct Slot {
        size_t   id   = 0;
        uint16_t type = 0;
        void*    data = NULL;
        size_t   size = 0;
        timeval  timestamp;
    };

    SemaphoreHandle_t                 mutex;
    std::deque<std::shared_ptr<Slot>> slots;
    volatile size_t                   id    = 1;
    const size_t                      limit = PTR_BUFFER_SIZE;
};

struct StatInfo {
    size_t            last_frame_id = 0;
    timeval           last_frame_timestamp;
    SemaphoreHandle_t mutex;
};

PtrBuffer PB;
StatInfo  SI;
SSCMA     AI;

void initSharedBuffer() { PB.mutex = xSemaphoreCreateMutex(); }

void initStatInfo() {
    SI.mutex                        = xSemaphoreCreateMutex();
    TickType_t ticks                = xTaskGetTickCount();
    SI.last_frame_timestamp.tv_sec  = ticks / configTICK_RATE_HZ;
    SI.last_frame_timestamp.tv_usec = (ticks % configTICK_RATE_HZ) * 1e6 / configTICK_RATE_HZ;
}

void startRemoteProxy(Proto through = PROTO_UART) {
    switch (through) {
    case PROTO_UART: {
#ifdef ESP32
        static HardwareSerial atSerial(0);
        // the esp32 arduino library may have a bug in setRxBufferSize
        // we cannot set the buffer size larger than uint16_t max value
        // a workaround is to modify uartBegin() in
        //     esp32/hardware/esp32/2.0.14/cores/esp32/esp32-hal-uart.c
        atSerial.setRxBufferSize(128 * 1024);
        atSerial.begin(921600);
#else
    #define atSerial Serial1
        atSerial.setRxBufferSize(128 * 1024);
        atSerial.begin(921600);
#endif
        AI.begin(&atSerial, D3);
        break;
    }
    case PROTO_I2C: {
        Wire.setBufferSize(128 * 1024);
        Wire.begin();
        AI.begin(&Wire, D3);
        break;
    };
    case PROTO_SPI: {
        SPI.begin(SCK, MOSI, MISO, -1);
        AI.begin(&SPI, D1, D0, D3, 15000000);
        break;
    };
    default:
        assert(false && "Unknown proto...");
    }

    const char* cmd = CMD_PREFIX "INVOKE=-1,0,0" CMD_SUFFIX;
    AI.write(cmd, strlen(cmd));
}

inline uint16_t getMsgType(const char* resp, size_t len) {
    uint16_t type = MSG_TYPE_UNKNOWN;

    if (strnstr(resp, MSG_REPLY_STR, len) != NULL) {
        type |= MSG_TYPE_REPLY;
    } else if (strnstr(resp, MSG_EVENT_STR, len) != NULL) {
        type |= MSG_TYPE_EVENT;
    } else if (strnstr(resp, MSG_LOGGI_STR, len) != NULL) {
        type |= MSG_TYPE_LOGGI;
    } else {
        log_w("Unknown message type...");
    }

    return type;
}

inline uint16_t getCmdType(const char* resp, size_t len) {
    uint16_t type = CMD_TYPE_UNKNOWN;

    if (strnstr(resp, CMD_SAMPLE_STR, len) != NULL) {
        type |= CMD_TYPE_SAMPLE;
    } else if (strnstr(resp, CMD_INVOKE_STR, len) != NULL) {
        type |= CMD_TYPE_INVOKE;
    }

    return type;
}

static void proxyCallback(const char* resp, size_t len) {
    static timeval timestamp;
    TickType_t     ticks = xTaskGetTickCount();
    timestamp.tv_sec     = ticks / configTICK_RATE_HZ;
    timestamp.tv_usec    = (ticks % configTICK_RATE_HZ) * 1e6 / configTICK_RATE_HZ;

    if (!len) {
        log_i("Response is empty...");
        return;
    }

    uint16_t type = 0;
    type |= getMsgType(resp, len);
    if (type == MSG_TYPE_UNKNOWN) {
        return;
    }
    type |= getCmdType(resp, len);

    char* copy = (char*)malloc(len);
    if (copy == NULL) {
        log_e("Failed to allocate resp copy...");
        return;
    }
    memcpy(copy, resp, len);

    size_t           limit  = PB.limit;
    PtrBuffer::Slot* p_slot = (PtrBuffer::Slot*)malloc(sizeof(PtrBuffer::Slot));
    if (p_slot == NULL) {
        log_e("Failed to allocate slot...");
        return;
    }

    p_slot->id        = PB.id;
    p_slot->type      = type;
    p_slot->data      = copy;
    p_slot->size      = len;
    p_slot->timestamp = timestamp;

    size_t discarded = 0;
    xSemaphoreTake(PB.mutex, portMAX_DELAY);
    while (PB.slots.size() >= limit) {
        PB.slots.pop_front();
        discarded += 1;
    }
    PB.slots.emplace_back(std::shared_ptr<PtrBuffer::Slot>(p_slot, [](PtrBuffer::Slot* p) {
        if (p == NULL) {
            return;
        }
        if (p->data != NULL) {
            free(p->data);
            p->data = NULL;
        }
        free(p);
    }));
    xSemaphoreGive(PB.mutex);
    PB.id += 1;

    if (discarded > 0) {
        log_i("Discarded %u old responses...", discarded);
    }

    log_i("Received %u bytes...", len);
}

void loopRemoteProxy() { AI.fetch(proxyCallback); }

typedef struct {
    httpd_req_t* req;
    size_t       len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t web_httpd    = NULL;
httpd_handle_t stream_httpd = NULL;

typedef struct {
    size_t size;   //number of values used for filtering
    size_t index;  //current value index
    size_t count;  //value count
    int    sum;
    int*   values;  //array to be filled with values
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t* ra_filter_init(ra_filter_t* filter, size_t sample_size) {
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int*)malloc(sample_size * sizeof(int));
    if (!filter->values) {
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t* filter, int value) {
    if (!filter->values) {
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }
    return filter->sum / filter->count;
}
#endif

static esp_err_t results_handler(httpd_req_t* req) {
    esp_err_t     res     = ESP_OK;
    static size_t last_id = 0;
    static char*  hdr_buf[128];
    static char*  rst_buf = NULL;
    if (rst_buf == NULL) {
        rst_buf = (char*)malloc(RST_BUFFER_SIZE);
        if (rst_buf == NULL) {
            log_e("Failed to allocate results buffer...");
            httpd_resp_send_500(req);
            return ESP_ERR_NO_MEM;
        }
    }

    std::shared_ptr<PtrBuffer::Slot> slot = nullptr;

    TickType_t time_begin = xTaskGetTickCount();
    while ((xTaskGetTickCount() - time_begin) < RESULT_TIMEOUT_MS) {
        xSemaphoreTake(PB.mutex, portMAX_DELAY);
        auto slots = PB.slots;
        xSemaphoreGive(PB.mutex);

        for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
            if (it->get()->id <= last_id) {
                break;
            }
            if (it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_SAMPLE) ||
                it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_INVOKE)) {
                slot    = *it;
                last_id = slot->id;
                break;
            }
        }

        if (!slot) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        break;
    }

    if (slot == nullptr) {
        log_w("Find newer results slot timeout...");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    const char* img_head = strnstr((const char*)slot->data, MSG_IMAGE_KEY MSG_QUOTE_STR, slot->size);
    if (img_head != NULL) {
        size_t offset = (img_head - (const char*)slot->data) + strlen(MSG_IMAGE_KEY MSG_QUOTE_STR);

        bool        found_prefix_comma = false;
        const char* img_head_full      = img_head - strlen(MSG_COMMA_STR);
        if (img_head_full >= (const char*)slot->data) {
            if (strncmp(img_head_full, MSG_COMMA_STR, strlen(MSG_COMMA_STR)) == 0) {
                img_head           = img_head_full;
                found_prefix_comma = true;
            }
        }

        const char* img_tail = strnstr((const char*)slot->data + offset, MSG_QUOTE_STR, slot->size - offset);
        if (img_tail == NULL) {
            log_e("Broken json format...");
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        offset = (img_tail - (const char*)slot->data) + strlen(MSG_QUOTE_STR);

        if (!found_prefix_comma) {
            const char* img_tail_full = strnstr((const char*)slot->data + offset, MSG_COMMA_STR, slot->size - offset);
            if (img_tail_full != NULL) {
                img_tail = img_tail_full;
            }
        }

        if (slot->size - (img_tail - img_head) >= RST_BUFFER_SIZE) {
            log_e("Results buffer is not enough...");
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        memset(rst_buf, 0, RST_BUFFER_SIZE);
        size_t size   = img_head - (const char*)slot->data;
        size_t copied = 0;
        strncpy(rst_buf, (const char*)slot->data, size);
        copied += size;
        size = ((const char*)slot->data + slot->size) - img_tail;
        strncpy(rst_buf + copied, img_tail, size);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char ts[32] = {0};
    snprintf(ts, sizeof(ts), "%ld", slot->id);
    httpd_resp_set_hdr(req, "X-Id", (const char*)ts);

    memset(ts, 0, sizeof(ts));
    snprintf(ts, sizeof(ts), "%ld.%06ld", slot->timestamp.tv_sec, slot->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char*)ts);

    size_t  last_frame_id;
    timeval last_frame_timestamp;

    xSemaphoreTake(SI.mutex, portMAX_DELAY);
    last_frame_id        = SI.last_frame_id;
    last_frame_timestamp = SI.last_frame_timestamp;
    xSemaphoreGive(SI.mutex);

    memset(ts, 0, sizeof(ts));
    snprintf(ts, sizeof(ts), "%ld", last_frame_id);
    httpd_resp_set_hdr(req, "X-Last-Frame-Id", (const char*)ts);

    memset(ts, 0, sizeof(ts));
    snprintf(ts, sizeof(ts), "%ld.%06ld", last_frame_timestamp.tv_sec, last_frame_timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Last-Frame-Timestamp", (const char*)ts);

    res = httpd_resp_send(req, (const char*)rst_buf, strlen(rst_buf));
    if (res != ESP_OK) {
        log_e("Send results failed...");
    }

    return res;
}

static esp_err_t stream_frame_handler(httpd_req_t* req) {
    esp_err_t res = ESP_OK;
    char*     part_buf[128];
    char*     jpeg_buf = NULL;
    size_t    last_id  = 0;

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    jpeg_buf = (char*)malloc(JPG_BUFFER_SIZE);
    if (jpeg_buf == NULL) {
        log_e("Failed to allocate jpeg buffer...");
        return ESP_ERR_NO_MEM;
    }

    while (true) {
        std::shared_ptr<PtrBuffer::Slot> slot = nullptr;

        {
            xSemaphoreTake(PB.mutex, portMAX_DELAY);
            auto slots = PB.slots;
            xSemaphoreGive(PB.mutex);

            for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
                if (it->get()->id <= last_id) {
                    break;
                }
                if (it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_SAMPLE) ||
                    it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_INVOKE)) {
                    slot    = *it;
                    last_id = slot->id;
                    break;
                }
            }

            if (!slot) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
        }

        const char* slice = strnstr((const char*)slot->data, MSG_IMAGE_KEY MSG_QUOTE_STR, slot->size);
        if (slice == NULL) {
            log_w("No image data found...");
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        size_t      offset = (slice - (const char*)slot->data) + strlen(MSG_IMAGE_KEY MSG_QUOTE_STR);
        const char* data   = (const char*)slot->data + offset;
        const char* quote  = strnstr(data, MSG_QUOTE_STR, slot->size - offset);
        if (quote == NULL) {
            log_w("Invalid image data size...");
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        size_t len = quote - data;
        if (len == 0) {
            log_w("Empty image data...");
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        size_t jpeg_size = 0;
        memset(jpeg_buf, 0, JPG_BUFFER_SIZE);
        if (mbedtls_base64_decode(
              (unsigned char*)jpeg_buf, JPG_BUFFER_SIZE, &jpeg_size, (const unsigned char*)data, len) != 0) {
            log_e("Failed to decode image data...");
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        xSemaphoreTake(SI.mutex, portMAX_DELAY);
        SI.last_frame_id        = slot->id;
        SI.last_frame_timestamp = slot->timestamp;
        xSemaphoreGive(SI.mutex);

        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) {
            goto SendError;
        }

        {
            memset(part_buf, 0, sizeof(part_buf));
            size_t hlen = snprintf((char*)part_buf,
                                   sizeof(part_buf),
                                   _STREAM_PART,
                                   jpeg_size,
                                   slot->timestamp.tv_sec,
                                   slot->timestamp.tv_usec);
            res         = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
        }
        if (res != ESP_OK) {
            goto SendError;
        }

        res = httpd_resp_send_chunk(req, jpeg_buf, jpeg_size);
        if (res != ESP_OK) {
            goto SendError;
        }

        continue;

    SendError:
        log_e("Send frame failed...");
        break;
    }

    free(jpeg_buf);

    return res;
}

static esp_err_t stream_result_handler(httpd_req_t* req) {
    esp_err_t                        res = ESP_OK;
    JsonDocument                     response;
    BYTETracker                      tracker;
    std::vector<BYTETracker::Object> boxes_list;
    static size_t                    last_id = 0;
    static char*                     rsp_buf = NULL;
    if (rsp_buf == NULL) {
        rsp_buf = (char*)malloc(RSP_BUFFER_SIZE);
        if (rsp_buf == NULL) {
            log_e("Failed to allocate response buffer...");
            httpd_resp_send_500(req);
            return ESP_ERR_NO_MEM;
        }
    }

    res |= httpd_resp_set_status(req, HTTPD_200);
    res |= httpd_resp_set_type(req, "application/json");
    res |= httpd_resp_set_hdr(req, "Connection", "keep-alive");
    res |= httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (res != ESP_OK) {
        log_e("Failed to set response headers...");
        return res;
    }

    while (res == ESP_OK) {
        std::shared_ptr<PtrBuffer::Slot> slot = nullptr;

        xSemaphoreTake(PB.mutex, portMAX_DELAY);
        auto slots = PB.slots;
        xSemaphoreGive(PB.mutex);

        for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
            if (it->get()->id <= last_id) {
                break;
            }
            if (it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_SAMPLE) ||
                it->get()->type == (MSG_TYPE_EVENT | CMD_TYPE_INVOKE)) {
                slot    = *it;
                last_id = slot->id;
                break;
            }
        }

        if (!slot) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        switch (slot->type) {
        case MSG_TYPE_EVENT | CMD_TYPE_SAMPLE: {
            res = httpd_resp_send(req, (const char*)slot->data, slot->size);
            break;
        }

        case MSG_TYPE_EVENT | CMD_TYPE_INVOKE: {
            response.clear();
            DeserializationError err = deserializeJson(response, (const char*)slot->data, slot->size);
            if (err != DeserializationError::Ok) {
                log_e("Failed to parse json...");
                printf("%s\n", (const char*)slot->data);
                break;
            }

            if (!response.containsKey("data")) {
                log_e("No data found in json...");
                break;
            }

            boxes_list.clear();
            if (response["data"].containsKey("boxes")) {
                JsonArray boxes = response["data"]["boxes"];
                for (JsonArray box : boxes) {
                    if (box.size() != 6) {
                        log_w("Invalid box size...");
                        continue;
                    }
                    BYTETracker::Object cxcywh;
                    cxcywh.rect.x      = box[0];
                    cxcywh.rect.y      = box[1];
                    cxcywh.rect.width  = box[2];
                    cxcywh.rect.height = box[3];
                    cxcywh.label       = box[5];
                    cxcywh.prob        = box[4];
                    boxes_list.push_back(cxcywh);
                }

                std::vector<STrack> output_stracks = tracker.update(boxes_list);

                boxes.clear();
                for (STrack& strack : output_stracks) {
                    JsonDocument doc;
                    JsonArray    box = doc.to<JsonArray>();
                    box.add(static_cast<int32_t>(strack.tlwh[0]));
                    box.add(static_cast<int32_t>(strack.tlwh[1]));
                    box.add(static_cast<int32_t>(strack.tlwh[2]));
                    box.add(static_cast<int32_t>(strack.tlwh[3]));
                    box.add(static_cast<int32_t>(strack.score));
                    box.add(static_cast<int32_t>(strack.label));
                    box.add(static_cast<int32_t>(strack.track_id));
                    boxes.add(box);
                }

            } else if (response["data"].containsKey("keypoints")) {
                JsonArray keypoints = response["data"]["keypoints"];

                size_t id = 0;
                for (JsonArray keypoint : keypoints) {
                    if (keypoint.size() != 2) {
                        log_w("Invalid keypoint size...");
                        continue;
                    }
                    JsonArray box = keypoint[0];
                    if (box.size() != 6) {
                        log_w("Invalid box size...");
                        continue;
                    }
                    BYTETracker::Object cxcywh;
                    cxcywh.rect.x      = box[0];
                    cxcywh.rect.y      = box[1];
                    cxcywh.rect.width  = box[2];
                    cxcywh.rect.height = box[3];
                    cxcywh.prob        = box[4];
                    cxcywh.label       = box[5];
                    cxcywh.label       = id++ << 16 | (cxcywh.label & 0xffff);
                    box[5]             = cxcywh.label;

                    boxes_list.push_back(cxcywh);
                }

                std::vector<STrack> output_stracks = tracker.update(boxes_list);

                for (JsonArray keypoint : keypoints) {
                    JsonArray box = keypoint[0];
                    if (box.size() != 6) {
                        log_w("Invalid box size...");
                        continue;
                    }

                    int  label = box[5];
                    auto it = std::find_if(output_stracks.begin(), output_stracks.end(), [label](const STrack& strack) {
                        return strack.label == label;
                    });
                    if (it != output_stracks.end()) {
                        box[0] = static_cast<int32_t>(it->tlwh[0]);
                        box[1] = static_cast<int32_t>(it->tlwh[1]);
                        box[2] = static_cast<int32_t>(it->tlwh[2]);
                        box[3] = static_cast<int32_t>(it->tlwh[3]);
                        box[4] = static_cast<int32_t>(it->score);
                        box[5] = static_cast<int32_t>(it->label & 0xffff);
                        box.add(static_cast<int32_t>(it->track_id));

                        output_stracks.erase(it);
                    } else {
                        box[5] = static_cast<int32_t>(label & 0xffff);
                        box.add(0);
                    }
                }
            }

            size_t len = serializeJson(response, rsp_buf, RSP_BUFFER_SIZE - sizeof(MSG_TERMI_STR));
            for (size_t i = 0; i < strlen(MSG_TERMI_STR); ++i, ++len) {
                rsp_buf[len] = MSG_TERMI_STR[i];
            }
            rsp_buf[len] = '\0';
            res          = httpd_resp_send_chunk(req, rsp_buf, len);

            break;
        }

        default:;
        }

        if (res != ESP_OK) {
            log_e("Send results failed...");
            break;
        }
    }

    return res;
}

static esp_err_t parse_get(httpd_req_t* req, char** obuf) {
    char*  buf     = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t command_handler(httpd_req_t* req) {
    char* buf = NULL;

    if (parse_get(req, &buf) != ESP_OK) {
        log_e("Failed to parse get data...");
        return ESP_FAIL;
    }

    char* qry_buf = (char*)malloc(QRY_BUFFER_SIZE);
    if (qry_buf == NULL) {
        free(buf);
        log_e("Failed to allocate query buffer...");
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    memset(qry_buf, 0, QRY_BUFFER_SIZE);
    if (httpd_query_key_value(buf, "base64", qry_buf, QRY_BUFFER_SIZE - 1) != ESP_OK) {
        free(buf);
        free(qry_buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    free(buf);

    char* cmd_buf = (char*)malloc(CMD_BUFFER_SIZE);
    if (cmd_buf == NULL) {
        free(qry_buf);
        log_e("Failed to allocate cmd buffer...");
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    size_t cmd_size = 0;
    memset(cmd_buf, 0, CMD_BUFFER_SIZE);
    if (mbedtls_base64_decode(
          (unsigned char*)cmd_buf, CMD_BUFFER_SIZE, &cmd_size, (const unsigned char*)qry_buf, strlen(qry_buf)) != 0) {
        free(qry_buf);
        free(cmd_buf);
        log_e("Failed to decode cmd data...");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    free(qry_buf);

    TickType_t ticks           = xTaskGetTickCount();
    char       cmd_tag_buf[32] = {0};
    size_t     cmd_tag_size    = snprintf(cmd_tag_buf, sizeof(cmd_tag_buf), CMD_TAG_FMT_STR, ticks);

    size_t last_id = PB.id;

    AI.write(CMD_PREFIX, strlen(CMD_PREFIX));
    AI.write(cmd_tag_buf, cmd_tag_size);
    AI.write(cmd_buf, cmd_size);
    free(cmd_buf);
    AI.write(CMD_SUFFIX, strlen(CMD_SUFFIX));

    std::shared_ptr<PtrBuffer::Slot> slot = nullptr;

    TickType_t time_begin = xTaskGetTickCount();
    while ((xTaskGetTickCount() - time_begin) < (RESULT_TIMEOUT_MS / portTICK_PERIOD_MS)) {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        xSemaphoreTake(PB.mutex, portMAX_DELAY);
        auto slots = PB.slots;
        xSemaphoreGive(PB.mutex);

        auto it = std::find_if(slots.begin(), slots.end(), [&](std::shared_ptr<PtrBuffer::Slot> p) {
            if (p->id - last_id <= 0) {
                return false;
            }

            if (p->type & MSG_TYPE_REPLY || p->type & MSG_TYPE_LOGGI) {
                const char* tag = strnstr((const char*)p->data, cmd_tag_buf, p->size);
                if (tag != NULL) {
                    return true;
                }
            }

            last_id = p->id;
            return false;
        });
        if (it == slots.end()) {
            continue;
        }

        slot = *it;
        break;
    }

    if (slot == nullptr) {
        log_w("Wait client reply slot timeout...");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Cmd-Tag", cmd_tag_buf);

    return httpd_resp_send(req, (const char*)slot->data, slot->size);
}

static esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    return httpd_resp_send(req, (const char*)index_html_gz, index_html_gz_len);
}

void startCameraServer() {
    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size       = 20480;

    httpd_uri_t index_uri = {.uri      = "/",
                             .method   = HTTP_GET,
                             .handler  = index_handler,
                             .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
                             ,
                             .is_websocket             = true,
                             .handle_ws_control_frames = false,
                             .supported_subprotocol    = NULL
#endif
    };

    httpd_uri_t command_uri = {.uri      = "/command",
                               .method   = HTTP_GET,
                               .handler  = command_handler,
                               .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
                               ,
                               .is_websocket             = true,
                               .handle_ws_control_frames = false,
                               .supported_subprotocol    = NULL
#endif
    };

    httpd_uri_t result_uri = {.uri      = "/result",
                              .method   = HTTP_GET,
                              .handler  = results_handler,
                              .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
                              ,
                              .is_websocket             = true,
                              .handle_ws_control_frames = false,
                              .supported_subprotocol    = NULL
#endif
    };

    httpd_uri_t stream_frame_uri = {.uri      = "/stream/frame",
                                    .method   = HTTP_GET,
                                    .handler  = stream_frame_handler,
                                    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
                                    ,
                                    .is_websocket             = true,
                                    .handle_ws_control_frames = false,
                                    .supported_subprotocol    = NULL
#endif
    };

    httpd_uri_t stream_result_uri = {.uri      = "/stream/result",
                                     .method   = HTTP_GET,
                                     .handler  = stream_result_handler,
                                     .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
                                     ,
                                     .is_websocket             = true,
                                     .handle_ws_control_frames = false,
                                     .supported_subprotocol    = NULL
#endif
    };

    ra_filter_init(&ra_filter, 20);

    log_i("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&web_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(web_httpd, &index_uri);
        httpd_register_uri_handler(web_httpd, &result_uri);
        httpd_register_uri_handler(web_httpd, &command_uri);
    }

    config.server_port = 8080;
    config.ctrl_port   = 8080;

    log_i("Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_frame_uri);
        httpd_register_uri_handler(stream_httpd, &stream_result_uri);
    }
}
