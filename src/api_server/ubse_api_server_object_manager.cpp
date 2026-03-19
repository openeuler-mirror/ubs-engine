/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_api_server_object_manager.h"

#include <mutex>

#include "ubse_error.h"
#include "ubse_logger.h"

namespace api::server {
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");
UbseApiServerObjectManager::UbseApiServerObjectManager() = default;

void UbseApiServerObjectManager::AddObjectMapping(uint16_t moduleCode, uint16_t opCode, const std::string &object)
{
    std::unique_lock lock(objectMapMutex_); // 获取独占锁（写锁）
    if (objectMap_.find(std::make_pair(moduleCode, opCode)) != objectMap_.end()) {
        UBSE_LOG_WARN << "Mapping already exists for moduleCode=" << moduleCode << " ,opCode=" << opCode;
    }
    AddObjectMappingNoCheck(moduleCode, opCode, object);
}

// 获取对象字符串
std::string UbseApiServerObjectManager::GetObjectString(uint16_t moduleCode, uint16_t opCode) const
{
    std::shared_lock<std::shared_mutex> lock(objectMapMutex_);
    auto key = std::make_pair(moduleCode, opCode);
    auto it = objectMap_.find(key);
    if (it != objectMap_.end()) {
        return it->second;
    }
    return ""; // 或者返回默认值
}

void UbseApiServerObjectManager::AddObjectMappingNoCheck(uint16_t moduleCode, uint16_t opCode,
                                                         const std::string &object)
{
    objectMap_.emplace(std::make_pair(moduleCode, opCode), object);
}
} // namespace api::server