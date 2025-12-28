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

#ifndef UBSE_URMA_H
#define UBSE_URMA_H

#include <stdint.h>
#include <map>
#include <string> // for string, basic_string
#include <vector>

namespace ubse::urma {
struct UbseQosProfile {
    std::string proflieName;
    uint32_t maxBandWidth;
    uint32_t minBandWidth;
};

enum class UrmaDevType {
    UNIQUE = 0,
    SHARED = 1,
    SELF_USED = 2,
    BUTT
};

enum class UrmaDevState {
    ACTIVED = 0,     // 激活
    INACTIVED = 1,   // 未知
    BUTT             // 未参考业界定义枚举类型最大值用BUTT表示
};

enum class FeType {
    PHYSICAL_TYPE = 0,      // pfe0, 物理类型FE用于集群通信
    VIRTUAL_TYPE = 1,       // vfe1, 虚拟类型VFE
    BUTT_TYPE               // 参考业界定义枚举类型最大值用BUTT表示
};

struct UbseFeInfo {
    std::string slotId;
    std::string ubpuId;
    std::string iouId;
    std::string entityId;
    FeType fetype;
    std::map<uint32_t, std::string> primaryEid;
    std::map<uint32_t, std::string> portEidInfos; // 将根据planning-urma-eid确认每个Fe的urma-eid-info的个数
};

struct UbseUrmaInfo {
    uint32_t nodeId;
    std::string name;
    std::string urmaDevEid;
    std::vector<UbseFeInfo> feInfoLists;
    UrmaDevType urmaDevType;
    UrmaDevState state;
};
} // namespace ubse::urma
#endif // UBSE_QOS_H