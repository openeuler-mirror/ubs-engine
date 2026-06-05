/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef MEM_STRATEGY_RACK_MEM_RESOURCE_ANALYSIS_H
#define MEM_STRATEGY_RACK_MEM_RESOURCE_ANALYSIS_H

#include "ubse_mmi_def.h"
#include "ubse_common_def.h"
namespace ubse::mem::strategy {

enum class WatermarkWarningType {
    NO_WARN = 0,
    HIGH_WATERMARK,
    LOW_WATERMARK
};
// 水线处理函数，高低水线发生后，处理逻辑
common::def::UbseResult WaterWarningProcess(WatermarkWarningType warningType,
                                            const adapter_plugins::mmi::UbseMemNumaLoc &warningNumaLoc, bool isOom);
} // namespace ubse::mem::strategy

#endif