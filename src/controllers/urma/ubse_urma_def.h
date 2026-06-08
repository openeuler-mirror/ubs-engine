/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_URMA_DEF_H
#define UBSE_URMA_DEF_H

#include <cstdint>
#include <map>
#include <memory>
#include <string> // for string, basic_string
#include <vector>

#include "ubse_serial_util.h"

namespace ubse::urma {

enum class UrmaDevType
{
    UNIQUE = 0,
    SHARED = 1,
    BUTT
};

enum class UrmaDevState
{
    ACTIVED = 0,   // 激活
    INACTIVED = 1, // 未激活
    UNKNOWN = 2,   // 未知
    PORT_DOWN = 3, // 端口down
    BUTT           // 未参考业界定义枚举类型最大值用BUTT表示
};

enum class FeType
{
    PHYSICAL_TYPE = 0, // pfe0, 物理类型FE用于集群通信
    VIRTUAL_TYPE = 1,  // vfe1, 虚拟类型VFE
    BUTT_TYPE          // 参考业界定义枚举类型最大值用BUTT表示
};

// FE 拓扑组合的枚举
enum class FeTopoType
{
    INVALID = 0,        // 无效值
    ALL_PFE = 1,        // 6 PFE，全物理FE
    PFE_VFE_HYBRID = 2, // 1 PFE + 5 VFE，物理/虚拟混合
};

struct UbseFeInfo {
    std::string name;
    std::string slotId;
    std::string ubpuId;
    std::string iouId;
    std::string entityId;
    FeType fetype;

    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const UbseFeInfo& info)
    {
        serializer << info.name << info.slotId << info.ubpuId << info.iouId << info.entityId
                   << ubse::serial::enum_v(info.fetype);
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         UbseFeInfo& info)
    {
        deserializer >> info.name >> info.slotId >> info.ubpuId >> info.iouId >> info.entityId >>
            ubse::serial::enum_v(info.fetype);
        return deserializer;
    }
};

struct EidGroup {
    std::string primaryEid;
    std::map<std::string, std::string> portEids;
    std::shared_ptr<UbseFeInfo> feInfo;

    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const EidGroup& group)
    {
        if (group.feInfo == nullptr) {
            uint32_t invalidVal = 0;
            serializer << invalidVal; // 指针为空则写入一个无效值，反序列化时类型不匹配，触发异常流程
            return serializer;
        }
        serializer << group.primaryEid << group.portEids << *(group.feInfo);
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         EidGroup& group)
    {
        deserializer >> group.primaryEid >> group.portEids;
        group.feInfo = std::make_shared<UbseFeInfo>();
        if (group.feInfo == nullptr) {
            return deserializer; // 内存分配失败，直接返回，后续流程会触发异常
        }
        UbseFeInfo& refFeInfo = *group.feInfo;
        deserializer >> refFeInfo;
        return deserializer;
    }
};

struct UbseUrmaInfo {
    std::string subPath;    // 对应uvs中的boundingname，用于组成设备path
    std::string urmaDevEid; // [BEID+podId, slotId, fe0Id, fe1Id]，每个字段占32位
    std::vector<EidGroup> eidGroups;
    UrmaDevType urmaDevType;
    UrmaDevState state;
    uint64_t hwResId; // 第一个vfe的iouId << 32 | entityId

    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const UbseUrmaInfo& info)
    {
        serializer << info.subPath << info.urmaDevEid << info.eidGroups << ubse::serial::enum_v(info.urmaDevType)
                   << ubse::serial::enum_v(info.state);
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         UbseUrmaInfo& info)
    {
        deserializer >> info.subPath >> info.urmaDevEid >> info.eidGroups >> ubse::serial::enum_v(info.urmaDevType) >>
            ubse::serial::enum_v(info.state);
        return deserializer;
    }
};

struct UrmaNameCompare {
    static std::pair<bool, uint64_t> ExtractNumber(const std::string& s)
    {
        size_t numPos = s.find_last_of('_');
        if (numPos == std::string::npos || numPos == s.length() - 1) {
            return {false, 0};
        }
        std::string numStr = s.substr(numPos + 1);
        if (numStr.empty()) {
            return {false, 0};
        }
        try {
            uint64_t num = std::stoull(numStr);
            return {true, num};
        } catch (...) {
            return {false, 0};
        }
    }

    bool operator()(const std::string& a, const std::string& b) const
    {
        auto pairA = ExtractNumber(a);
        auto pairB = ExtractNumber(b);
        bool aHasNum = pairA.first;
        uint64_t aNum = pairA.second;
        bool bHasNum = pairB.first;
        uint64_t bNum = pairB.second;
        // 如果都符合格式，按数字比较
        if (aHasNum && bHasNum) {
            if (aNum != bNum) {
                return aNum < bNum;
            }
            // 数字相同，按整个字符串字典序排序
            return a < b;
        }
        return a < b;
    }
};

struct UbseUrmaNodeInfo {
    std::string nodeId;
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare>
        urmaList; // <urmaName, urmaInfo>，urmaName (urma_urmaId)是对北向唯一标识，由ubse生成
    uint64_t updateTimeStamp{0}; // 表示节点信息的更新序号，用于判断当前的是否为最新
    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const UbseUrmaNodeInfo& info)
    {
        serializer << info.nodeId << info.urmaList << info.updateTimeStamp;
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         UbseUrmaNodeInfo& info)
    {
        deserializer >> info.nodeId >> info.urmaList >> info.updateTimeStamp;
        return deserializer;
    }
};

typedef struct {
    std::vector<std::string> vfePaths;
    std::string bondingPath;
    std::string bondingEid;
} UbseUrmaDevPath;

struct UbseUrmaDevBrief {
    std::string urmaName;
    std::vector<std::string> feNames;
    std::vector<std::string> feEids;
    std::string devEid;
    UrmaDevType bondingType;
    UrmaDevState state;
    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const UbseUrmaDevBrief& info)
    {
        serializer << info.urmaName << info.feNames << info.feEids << info.devEid
                   << ubse::serial::enum_v(info.bondingType) << ubse::serial::enum_v(info.state);
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         UbseUrmaDevBrief& info)
    {
        deserializer >> info.urmaName >> info.feNames >> info.feEids >> info.devEid >>
            ubse::serial::enum_v(info.bondingType) >> ubse::serial::enum_v(info.state);
        return deserializer;
    }
};
} // namespace ubse::urma

#endif // UBSE_URMA_DEF_H
