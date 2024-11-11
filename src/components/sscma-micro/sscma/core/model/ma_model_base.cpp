#include "ma_model_base.h"

namespace ma {

constexpr char TAG[] = "ma::model";

Model::Model(Engine* engine, const char* name, ma_model_type_t type)
    
      
      // Initialize performance metrics to 0 using initializer list
{

    p_engine_ = engine;
    p_name_   = name;
    m_type_   = type;
    p_user_ctx_ = nullptr;

    p_preprocess_done_     = nullptr;
    p_postprocess_done_    = nullptr;
    p_underlying_run_done_ = nullptr;

    perf_ = {0, 0, 0};
    // No further initialization needed since everything is already initialized
}

Model::~Model() {}

ma_err_t Model::underlyingRun() {

    ma_err_t err       = MA_OK;
    int64_t start_time = 0;

    start_time = ma_get_time_ms();

    err = preprocess();
    if (p_preprocess_done_ != nullptr) {
        p_preprocess_done_(p_user_ctx_);
    }
    perf_.preprocess = ma_get_time_ms() - start_time;
    if (err != MA_OK) {
        return err;
    }


    start_time = ma_get_time_ms();
    err        = p_engine_->run();
    if (p_underlying_run_done_ != nullptr) {
        p_underlying_run_done_(p_user_ctx_);
    }

    perf_.inference = ma_get_time_ms() - start_time;


    if (err != MA_OK) {
        return err;
    }
    start_time = ma_get_time_ms();
    err        = postprocess();
    if (p_postprocess_done_ != nullptr) {
        p_postprocess_done_(p_user_ctx_);
    }
    perf_.postprocess = ma_get_time_ms() - start_time;

    return err;
}


const ma_perf_t Model::getPerf() const {
    return perf_;
}


const char* Model::getName() const {
    return p_name_;
}

ma_model_type_t Model::getType() const {
    return m_type_;
}

void Model::setPreprocessDone(std::function<void(void*)> func) {
    p_preprocess_done_ = std::move(func);
}

void Model::setPostprocessDone(std::function<void(void*)> func) {
    p_postprocess_done_ = std::move(func);
}

void Model::setRunDone(std::function<void(void*)> func) {
    p_underlying_run_done_ = std::move(func);
}

void Model::setUserCtx(void* ctx) {
    p_user_ctx_ = ctx;
}

class ModelFactory {
public:
    static Model* create(Engine* engine);
    static ma_err_t remove(const std::string& name);
};


}  // namespace ma::model