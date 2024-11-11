#ifndef _MA_MODEL_POINT_DETECTOR_H_
#define _MA_MODEL_POINT_DETECTOR_H_

#include <vector>

#include "ma_model_base.h"

namespace ma::model {

class PointDetector : public Model {
   protected:
    ma_tensor_t     input_;
    ma_img_t        img_;
    const ma_img_t* input_img_;

    float threshold_score_;

    bool is_nhwc_;

    std::vector<ma_point_t> results_;

   protected:
    ma_err_t preprocess() override;

   public:
    PointDetector(Engine* engine, const char* name, ma_model_type_t type);
    virtual ~PointDetector();

    const std::vector<ma_point_t>& getResults() const;

    ma_err_t run(const ma_img_t* img);

    ma_err_t setConfig(ma_model_cfg_opt_t opt, ...) override;

    ma_err_t getConfig(ma_model_cfg_opt_t opt, ...) override;
};

}  // namespace ma::model

#endif
