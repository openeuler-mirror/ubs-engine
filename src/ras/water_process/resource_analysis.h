/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef MEM_STRATEGY_RACK_MEM_RESOURCE_ANALYSIS_H
#define MEM_STRATEGY_RACK_MEM_RESOURCE_ANALYSIS_H

#include <ubse_mem_meta_data.h>

namespace ubse::mem::strategy {
// 水线处理函数，高低水线发生后，处理逻辑
UbseResult WaterWarningProcess(WatermarkWarningType warningType, const UbseMemNumaLoc& warningNumaLoc, bool isOom);
} // namespace ubse::mem::strategy

#endif