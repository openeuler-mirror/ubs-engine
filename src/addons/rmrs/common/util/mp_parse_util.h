/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_PARSE_UTILS_H
#define MP_PARSE_UTILS_H

#include <vector>

#include "ubse_common_def.h"
#include "ubse_logger.h"
#include "mp_configuration.h"
#include "mp_error.h"

namespace mempooling {
using ubse::common::def::UbseResult;
using namespace ubse::log;

struct RackSrcMemoryBorrowParam {
    std::string srcNid;
    std::string srcSocketId;
    std::string srcNumaId;
};

struct RackVMPresetParam {
    std::string pid;   // vm对应pid
    std::string ratio; // 迁出最大比例
};

} // namespace mempooling
#endif // MP_PARSE_UTILS_H