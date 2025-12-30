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

#ifndef UBSE_UVS_INTERFACE_H
#define UBSE_UVS_INTERFACE_H
#include <stdint.h>
#include <map>
#include <string> // for string, basic_string
#include <vector>

namespace ubse::urma {
struct UbseUrmaUvsFe {
    std::string ubpuId;
    std::string primaryEid;
    std::map<std::string, std::string> portEid;
};

struct UbseUrmaUvsAggrDev {
    std::string urmaDevEid;
    std::vector<UbseUrmaUvsFe> feList;
};

struct UbseUrmaUvsNodeInfo {
    std::string nodeId;
    std::vector<UbseUrmaUvsAggrDev> devList;
};
}

#endif //UBSE_UVS_INTERFACE_H
