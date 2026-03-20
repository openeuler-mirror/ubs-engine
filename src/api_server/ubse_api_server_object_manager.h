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
#ifndef UBSE_API_SERVER_OBJECT_MANAGER_H
#define UBSE_API_SERVER_OBJECT_MANAGER_H

#include <cstdint>
#include <shared_mutex>
#include <string>

#include "ubse_common_def.h"
#include "ubse_ipc_common.h"
#include "ubse_map_util.h"

namespace api::server {
using namespace ubse::common::def;

class UbseApiServerObjectManager {
public:
    UbseApiServerObjectManager();

    void AddObjectMapping(uint16_t moduleCode, uint16_t opCode, const std::string &object);

    std::string GetObjectString(uint16_t moduleCode, uint16_t opCode) const;

private:
    inline void AddObjectMappingNoCheck(uint16_t moduleCode, uint16_t opCode, const std::string &object);

    ubse::utils::PairMap<uint16_t, uint16_t, std::string> objectMap_;

    mutable std::shared_mutex objectMapMutex_;
};
} // namespace api::server
#endif // UBSE_API_SERVER_OBJECT_MANAGER_H
