
#pragma once

#ifndef _SSCMA_MICRO_CORE_H_
#define _SSCMA_MICRO_CORE_H_

#if defined(__AVR__)
#error "Insufficient memory: This code cannot run on any AVR platform due to limited memory."
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if (CONFIG_IDF_TARGET_ESP32 | CONFIG_IDF_TARGET_ESP32S3 | CONFIG_IDF_TARGET_ESP32S2)
#define MA_PORTING_ESPRESSIF_ESP32 1
#endif

#if MA_PORTING_ESPRESSIF_ESP32
#include <esp_camera.h>
#endif

class SSCMAMicroCore {
public:
    struct InvokeConfig {
        int top_k             = -1;
        float score_threshold = 0.5;
        float nms_threshold   = 0.5;
    };

    struct Config {
        int model_id                      = 0;
        int algorithm_id                  = 0;
        const InvokeConfig* invoke_config = nullptr;
    };

    struct Box {
        float x;
        float y;
        float w;
        float h;
        float score;
        int target;
    };

    struct Class {
        int target;
        float score;
    };

    struct Point {
        float x;
        float y;
        float z;
        float score;
        int target;
    };

    struct Keypoints {
        Box box;
        std::vector<Point> points;
    };

    using BoxesCallback     = std::function<void(const std::vector<Box>&, void*)>;
    using ClassesCallback   = std::function<void(const std::vector<Class>&, void*)>;
    using PointsCallback    = std::function<void(const std::vector<Point>&, void*)>;
    using KeypointsCallback = std::function<void(const std::vector<Keypoints>&, void*)>;

    struct Perf {
        uint32_t preprocess;
        uint32_t inference;
        uint32_t postprocess;
    };

    using PerfCallback = std::function<void(const Perf&, void*)>;

    enum PixelFormat {
        kUNKNOWN = 0,
        kRGB888  = 1,
        kRGB565,
        kGRAY8,
        kJPEG,
    };

    struct Frame {
        PixelFormat format;
        uint16_t width;
        uint16_t height;
        uint16_t orientation;
        struct timeval timestamp;
        uint32_t size;
        uint8_t* data;

#if MA_PORTING_ESPRESSIF_ESP32
        static Frame fromCameraFrame(const camera_fb_t* frame);
#endif
    };

    struct Expected {
        bool success;
        std::string message;
    };

public:
    SSCMAMicroCore();
    ~SSCMAMicroCore();

    Expected begin(const Config& config);
    Expected invoke(const Frame& frame, const InvokeConfig* config = nullptr, void* user_context = nullptr);

    void registerBoxesCallback(BoxesCallback callback);
    void registerClassesCallback(ClassesCallback callback);
    void registerPointsCallback(PointsCallback callback);
    void registerKeypointsCallback(KeypointsCallback callback);

    void registerPerfCallback(PerfCallback callback);

private:
    bool _initialized = false;
    Config _config;

    BoxesCallback _boxes_callback;
    ClassesCallback _classes_callback;
    PointsCallback _points_callback;
    KeypointsCallback _keypoints_callback;

    PerfCallback _perf_callback;
};

#endif