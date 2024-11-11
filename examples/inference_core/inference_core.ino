#include <SSCMA_Micro_Core.h>

#include <Arduino.h>
#include <esp_camera.h>


#define CAMERA_PIN_PWDN              -1
#define CAMERA_PIN_RESET             -1

#define CAMERA_PIN_VSYNC             38
#define CAMERA_PIN_HREF              47
#define CAMERA_PIN_PCLK              13
#define CAMERA_PIN_XCLK              10

#define CAMERA_PIN_SIOD              40
#define CAMERA_PIN_SIOC              39

#define CAMERA_PIN_D0                15
#define CAMERA_PIN_D1                17
#define CAMERA_PIN_D2                18
#define CAMERA_PIN_D3                16
#define CAMERA_PIN_D4                14
#define CAMERA_PIN_D5                12
#define CAMERA_PIN_D6                11
#define CAMERA_PIN_D7                48

#define XCLK_FREQ_HZ                 16000000


SSCMAMicroCore instance;
sensor_t* camera;


void setup()
{
    // Init serial port
    Serial.begin(115200);


    // Init esp camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = CAMERA_PIN_D0;
    config.pin_d1       = CAMERA_PIN_D1;
    config.pin_d2       = CAMERA_PIN_D2;
    config.pin_d3       = CAMERA_PIN_D3;
    config.pin_d4       = CAMERA_PIN_D4;
    config.pin_d5       = CAMERA_PIN_D5;
    config.pin_d6       = CAMERA_PIN_D6;
    config.pin_d7       = CAMERA_PIN_D7;
    config.pin_xclk     = CAMERA_PIN_XCLK;
    config.pin_pclk     = CAMERA_PIN_PCLK;
    config.pin_vsync    = CAMERA_PIN_VSYNC;
    config.pin_href     = CAMERA_PIN_HREF;
    config.pin_sscb_sda = CAMERA_PIN_SIOD;
    config.pin_sscb_scl = CAMERA_PIN_SIOC;
    config.pin_pwdn     = CAMERA_PIN_PWDN;
    config.pin_reset    = CAMERA_PIN_RESET;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_240X240;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    int ec = esp_camera_init(&config);
    if (ec != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }

    camera = esp_camera_sensor_get();
    if (camera == nullptr) {
        Serial.println("Camera sensor not found");
        return;
    }

    camera->set_vflip(camera, 1);
    camera->set_hmirror(camera, 1);


    // Init SSCMA Micro Core
    auto ret = instance.begin(SSCMAMicroCore::Config{});
    if (!ret.success) {
        Serial.println(ret.message.c_str());
        return;
    }

    auto perf_callback = [](const SSCMAMicroCore::Perf& perf, void* user_context) {
        Serial.printf("Preprocess: %d ms\n", perf.preprocess);
        Serial.printf("Inference: %d ms\n", perf.inference);
        Serial.printf("Postprocess: %d ms\n", perf.postprocess);
    };
    instance.registerPerfCallback(perf_callback);

    auto boxes_callback = [](const std::vector<SSCMAMicroCore::Box>& boxes, void* user_context) {
        Serial.printf("Boxes: %d\n", boxes.size());
        for (const auto& box : boxes) {
            Serial.printf("Box: %f %f %f %f %f %d\n", box.x, box.y, box.w, box.h, box.score, box.target);
        }
    };
    instance.registerBoxesCallback(boxes_callback);

    auto classes_callback = [](const std::vector<SSCMAMicroCore::Class>& classes, void* user_context) {
        Serial.printf("Classes: %d\n", classes.size());
        for (const auto& cls : classes) {
            Serial.printf("Class: %d %f\n", cls.target, cls.score);
        }
    };
    instance.registerClassesCallback(classes_callback);

    auto points_callback = [](const std::vector<SSCMAMicroCore::Point>& points, void* user_context) {
        Serial.printf("Points: %d\n", points.size());
        for (const auto& point : points) {
            Serial.printf("Point: %f %f %f %f %d\n", point.x, point.y, point.z, point.score, point.target);
        }
    };
    instance.registerPointsCallback(points_callback);

    auto keypoints_callback = [](const std::vector<SSCMAMicroCore::Keypoints>& keypoints, void* user_context) {
        Serial.printf("Keypoints: %d\n", keypoints.size());
        for (const auto& kp : keypoints) {
            Serial.printf("Box: %f %f %f %f %f %d\n", kp.box.x, kp.box.y, kp.box.w, kp.box.h, kp.box.score, kp.box.target);
            for (const auto& point : kp.points) {
                Serial.printf("Point: %f %f %f %f %d\n", point.x, point.y, point.z, point.score, point.target);
            }
        }
    };
    instance.registerKeypointsCallback(keypoints_callback);


    Serial.println("Init done");
}

void loop()
{
    vTaskDelay(3000 / portTICK_PERIOD_MS);      
    camera_fb_t* frame = esp_camera_fb_get();
    if (frame == nullptr) {
        Serial.println("Camera capture failed");
        return;
    }
    Serial.printf("Frame size: %d\n", frame->len);

    auto ret = instance.invoke(SSCMAMicroCore::Frame::fromCameraFrame(frame));
    if (!ret.success) {
        Serial.println(ret.message.c_str());
    }

    esp_camera_fb_return(frame);
}