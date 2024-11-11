#include "SSCMA_Micro_Core.h"

#include "components/sscma-micro/sscma/core/ma_core.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#if MA_PORTING_ESPRESSIF_ESP32S3
#include <esp_partition.h>
#include <spi_flash_mmap.h>

#define MA_PORTING_MODEL_ADDRESS (0x400000)
#define MA_PORTING_MODEL_SIZE    (4096 * 1024)
#endif

extern "C" {
uint8_t* _ma_static_tensor_arena = nullptr;
}

static void* ma_core_context = nullptr;
static ma::Engine* ma_engine = nullptr;
static ma::Model* ma_model   = nullptr;


#if MA_PORTING_ESPRESSIF_ESP32
SSCMAMicroCore::Frame SSCMAMicroCore::Frame::fromCameraFrame(const camera_fb_t* frame) {
    Frame f;
    if (frame == nullptr) {
        return f;
    }
    switch (frame->format) {
        case PIXFORMAT_RGB888:
            f.format = kRGB888;
            break;
        case PIXFORMAT_RGB565:
            f.format = kRGB565;
            break;
        case PIXFORMAT_GRAYSCALE:
            f.format = kGRAY8;
            break;
        case PIXFORMAT_JPEG:
            f.format = kJPEG;
            break;
        default:
            f.format = kUNKNOWN;
            break;
    }
    f.width       = frame->width;
    f.height      = frame->height;
    f.orientation = 0;
    f.timestamp   = frame->timestamp;
    f.size        = frame->len;
    f.data        = frame->buf;
    return f;
}
#endif


SSCMAMicroCore::SSCMAMicroCore() {
    assert(ma_core_context == nullptr && "SSCMA Micro Core is a singleton class");
    if (ma_core_context == nullptr) {
        ma_core_context = this;
    }
}

SSCMAMicroCore::~SSCMAMicroCore() {
    ma_core_context = nullptr;
    if (ma_engine != nullptr) {
        delete ma_engine;
        ma_engine = nullptr;
    }
    if (ma_model != nullptr) {
        delete ma_model;
        ma_model = nullptr;
    }
    _initialized = false;
}

SSCMAMicroCore::Expected SSCMAMicroCore::begin(const Config& config) {
    if (_initialized) {
        return {false, "Already initialized"};
    }

    {
        if (_ma_static_tensor_arena == nullptr) {
            _ma_static_tensor_arena = new uint8_t[MA_ENGINE_TFLITE_TENSOE_ARENA_SIZE];
        }
        static EngineDefault engine;
        int ret = engine.init();
        if (ret != MA_OK) {
            return {false, "Engine init failed"};
        }
        ma_engine = &engine;
    }

#if MA_PORTING_ESPRESSIF_ESP32S3
    {
#if MA_PORTING_ESPRESSIF_PARTITIONS
        const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, "models");
        if (!partition) {
            return {false, "No models partition found"};
        }
#else
        static esp_partition_t* partition = new esp_partition_t;
        partition->address                = MA_PORTING_MODEL_ADDRESS;
        partition->size                   = MA_PORTING_MODEL_SIZE;
#endif

        static const uint8_t* mmap = nullptr;
        static uint32_t handler    = 0;
        int ret                    = spi_flash_mmap(partition->address, partition->size, SPI_FLASH_MMAP_DATA, reinterpret_cast<const void**>(&mmap), &handler);

        if (ret != ESP_OK) {
            return {false, "Failed to map models"};
        }

        static std::vector<ma_model_t> models;
        const size_t size = partition->size;
        size_t id         = 0;
        for (size_t i = 0; i < size; i += 4096) {
            const void* data = mmap + i;
            if (ma_ntohl(*(static_cast<const uint32_t*>(data) + 1)) != 0x54464C33) {  // 'TFL3'
                continue;
            }

            ma_model_t model;
            model.id   = ++id;
            model.type = MA_MODEL_TYPE_UNDEFINED;
            model.name = mmap;
            model.addr = data;
            models.push_back(model);
        }

        auto it = config.model_id >= 0 ? std::find_if(models.begin(), models.end(), [config](const ma_model_t& model) { return model.type == config.model_id; }) : models.begin();
        if (it == models.end()) {
            return {false, "Model not found"};
        }

        ret = ma_engine->load(it->addr, it->size);
        if (ret != MA_OK) {
            return {false, "Failed to load model"};
        }
    }
#else
    return {false, "Unsupported platform"};
#endif

    {
        ma_model = ma::ModelFactory::create(ma_engine, _config.model_id >= 0 ? _config.model_id : 0);
        if (ma_model == nullptr) {
            return {false, "Failed to create algorithm"};
        }
        if (config.invoke_config) {
            ma_model->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, config.invoke_config->score_threshold);
            ma_model->setConfig(MA_MODEL_CFG_OPT_NMS, config.invoke_config->nms_threshold);
        }
    }

    _config      = config;
    _initialized = true;

    return {_initialized, ""};
}


SSCMAMicroCore::Expected SSCMAMicroCore::invoke(const Frame& frame, const InvokeConfig* config, void* user_context) {
    if (!_initialized) {
        return {false, "Not initialized"};
    }

    if (frame.format == kUNKNOWN) {
        return {false, "Invalid frame format"};
    }
    if (frame.width == 0 || frame.height == 0) {
        return {false, "Invalid frame dimensions"};
    }
    if (frame.size == 0) {
        return {false, "Invalid frame size"};
    }
    if (frame.data == nullptr) {
        return {false, "Invalid frame data"};
    }

    ma_img_t img;
    img.width  = frame.width;
    img.height = frame.height;
    switch (frame.format) {
        case kRGB888:
            img.format = MA_PIXEL_FORMAT_RGB888;
            break;
        case kRGB565:
            img.format = MA_PIXEL_FORMAT_RGB565;
            break;
        case kGRAY8:
            img.format = MA_PIXEL_FORMAT_GRAYSCALE;
            break;
        case kJPEG:
            img.format = MA_PIXEL_FORMAT_JPEG;
            break;
        default:
            return {false, "Invalid frame format"};
    }
    switch (frame.orientation) {
        case 0:
            img.rotate = MA_PIXEL_ROTATE_0;
            break;
        case 90:
            img.rotate = MA_PIXEL_ROTATE_90;
            break;
        case 180:
            img.rotate = MA_PIXEL_ROTATE_180;
            break;
        case 270:
            img.rotate = MA_PIXEL_ROTATE_270;
            break;
        default:
            img.rotate = MA_PIXEL_ROTATE_0;
            break;
    }
    img.timestamp = frame.timestamp.tv_sec * 1000 + frame.timestamp.tv_usec / 1000;
    img.size      = frame.size;
    img.data      = frame.data;

    if (config) {
        _config.invoke_config = config;
        ma_model->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, _config.invoke_config->score_threshold);
        ma_model->setConfig(MA_MODEL_CFG_OPT_NMS, _config.invoke_config->nms_threshold);
    }

    switch (ma_model->getType()) {
        case MA_MODEL_TYPE_PFLD: {
            auto algorithm = static_cast<ma::model::PointDetector*>(ma_model);
            auto ret       = algorithm->run(&img);
            if (ret != MA_OK) {
                using namespace std::string_literals;
                return {false, "Failed to run model: "s + std::to_string(ret)};
            }
            if (!_points_callback) {
                break;
            }
            auto results = algorithm->getResults();
            if (_config.invoke_config && _config.invoke_config->top_k > 0) {
                std::sort(results.begin(), results.end(), [](const ma_point_t& a, const ma_point_t& b) { return a.score > b.score; });
                results.resize(std::min(results.size(), static_cast<size_t>(_config.invoke_config->top_k)));
                results.shrink_to_fit();
            }
            std::vector<SSCMAMicroCore::Point> points;
            for (const auto& result : results) {
                points.push_back(SSCMAMicroCore::Point{
                    .x      = result.x,
                    .y      = result.y,
                    .z      = 0.f,
                    .score  = result.score,
                    .target = result.target,
                });
            }
            points.shrink_to_fit();
           
            _points_callback(points, user_context);
            
        } break;

        case MA_MODEL_TYPE_IMCLS: {
            auto algorithm = static_cast<ma::model::Classifier*>(ma_model);
            auto ret       = algorithm->run(&img);
            if (ret != MA_OK) {
                using namespace std::string_literals;
                return {false, "Failed to run model: "s + std::to_string(ret)};
            }
            if (!_classes_callback) {
                break;
            }
            auto results = algorithm->getResults();
            if (_config.invoke_config && _config.invoke_config->top_k > 0) {
                auto size           = std::distance(results.begin(), results.end());
                int items_to_remove = size - _config.invoke_config->top_k;
                if (items_to_remove > 0) {
                    results.sort([](const ma_class_t& a, const ma_class_t& b) { return a.score < b.score; });
                    while (items_to_remove-- > 0) {
                        results.pop_front();
                    }
                }
            }
            std::vector<SSCMAMicroCore::Class> classes;
            for (const auto& result : results) {
                classes.push_back({result.score, result.target});
            }
            classes.shrink_to_fit();
           
            _classes_callback(classes, user_context);
    
        } break;

        case MA_MODEL_TYPE_FOMO:
        case MA_MODEL_TYPE_YOLOV5:
        case MA_MODEL_TYPE_YOLOV8:
        case MA_MODEL_TYPE_NVIDIA_DET:
        case MA_MODEL_TYPE_YOLO_WORLD: {
            auto algorithm = static_cast<ma::model::Detector*>(ma_model);
            auto ret       = algorithm->run(&img);
            if (ret != MA_OK) {
                using namespace std::string_literals;
                return {false, "Failed to run model: "s + std::to_string(ret)};
            }
            if (!_boxes_callback) {
                break;
            }
            auto results = algorithm->getResults();
            if (_config.invoke_config && _config.invoke_config->top_k > 0) {
                auto size           = std::distance(results.begin(), results.end());
                int items_to_remove = size - _config.invoke_config->top_k;
                if (items_to_remove > 0) {
                    results.sort([](const ma_bbox_t& a, const ma_bbox_t& b) { return a.score < b.score; });
                    while (items_to_remove-- > 0) {
                        results.pop_front();
                    }
                }
            }
            std::vector<SSCMAMicroCore::Box> boxes;
            for (const auto& result : results) {
                boxes.push_back({result.x, result.y, result.w, result.h, result.score, result.target});
            }
            boxes.shrink_to_fit();
           
            _boxes_callback(boxes, user_context);
            
        } break;

        case MA_MODEL_TYPE_YOLOV8_POSE: {
            auto algorithm = static_cast<ma::model::PoseDetector*>(ma_model);
            auto ret       = algorithm->run(&img);
            if (ret != MA_OK) {
                using namespace std::string_literals;
                return {false, "Failed to run model: "s + std::to_string(ret)};
            }
            if (!_keypoints_callback) {
                break;
            }
            auto results = algorithm->getResults();
            if (_config.invoke_config && _config.invoke_config->top_k > 0) {
                std::sort(results.begin(), results.end(), [](const ma_keypoint3f_t& a, const ma_keypoint3f_t& b) { return a.box.score > b.box.score; });
                results.resize(std::min(results.size(), static_cast<size_t>(_config.invoke_config->top_k)));
                results.shrink_to_fit();
            }
            std::vector<SSCMAMicroCore::Keypoints> keypoints;
            for (const auto& result : results) {
                SSCMAMicroCore::Keypoints kp;
                kp.box.x      = result.box.x;
                kp.box.y      = result.box.y;
                kp.box.w      = result.box.w;
                kp.box.h      = result.box.h;
                kp.box.score  = result.box.score;
                kp.box.target = result.box.target;
                size_t i      = 0;
                for (const auto& point : result.pts) {
                    kp.points.push_back(SSCMAMicroCore::Point{
                        .x      = point.x,
                        .y      = point.y,
                        .z      = point.z,
                        .score  = 0.f,
                        .target = i++,
                    });
                }
                kp.points.shrink_to_fit();
                keypoints.push_back(std::move(kp));
            }
            keypoints.shrink_to_fit();
        
            _keypoints_callback(keypoints, user_context);

        } break;

        default:
            break;
    }

    if (_perf_callback) {
        SSCMAMicroCore::Perf perf;
        auto perf_log    = ma_model->getPerf();
        perf.preprocess  = perf_log.preprocess;
        perf.inference   = perf_log.inference;
        perf.postprocess = perf_log.postprocess;
        _perf_callback(perf, user_context);
    }

    return {true, ""};
}

void SSCMAMicroCore::registerBoxesCallback(BoxesCallback callback) {
    _boxes_callback = callback;
}

void SSCMAMicroCore::registerClassesCallback(ClassesCallback callback) {
    _classes_callback = callback;
}

void SSCMAMicroCore::registerPointsCallback(PointsCallback callback) {
    _points_callback = callback;
}

void SSCMAMicroCore::registerKeypointsCallback(KeypointsCallback callback) {
    _keypoints_callback = callback;
}

void SSCMAMicroCore::registerPerfCallback(PerfCallback callback) {
    _perf_callback = callback;
}
