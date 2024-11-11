#ifndef _MA_ENGINE_H_
#define _MA_ENGINE_H_

#include "ma_engine_base.h"
#include "../ma_common.h"

#ifdef MA_USE_ENGINE_TFLITE
#include "ma_engine_tflite.h"
using EngineDefault = ma::engine::EngineTFLite;
#endif

#endif  // _MA_ENGINE_H_