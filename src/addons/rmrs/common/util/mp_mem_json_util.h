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

#ifndef MP_MEM_JSON_UTIL_H
#define MP_MEM_JSON_UTIL_H

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "ubse_logger.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_module.h"
#include "mp_error.h"

namespace mempooling {
using namespace mempooling::migrate;
using namespace ubse::log;

struct MpMigrateParam {
    std::string ToJson();
    bool FromJson(const std::string& jsonString);

    std::string borrowInNode;
    std::vector<VMPresetParam> vmInfoList;
    std::uint64_t borrowSize;
};

struct MpMigrateStrategyResultParam {
    std::string ToJson();
    bool FromJson(const std::string& jsonString);

    std::vector<VMMigrateOutParam> vmInfoList;
    uint64_t waitingTime; // 单位ms
};

struct MpMemBorrRBParam {
    std::string ToJson();
    bool FromJson(const std::string& jsonString);

    std::string borrowInNode;
    std::vector<std::string> borrowIdsList;

    // 打印 MpMemBorrRBParam 的内容
    void log() const
    {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "borrowInNode: " << borrowInNode;

        for (const auto& borrowId : borrowIdsList) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "borrowIdsList: " << borrowId;
        }
    }
};

struct MpFaultMemIdParam {
    std::string ToJson();
    bool FromJson(const std::string& jsonString);

    std::string importNodeId;
    uint64_t importMemId;

    void log() const
    {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "importNodeId: " << importNodeId;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "importMemId: " << importMemId;
    }
};

} // namespace mempooling

#endif // MP_MEM_JSON_UTIL_H