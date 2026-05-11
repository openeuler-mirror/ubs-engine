/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */


#ifndef HAM_MIGRATE_DST_INFO_MESSAGE_H
#define HAM_MIGRATE_DST_INFO_MESSAGE_H

#include "base_message.h"
#include "vm_http_util.h"

namespace vm {

struct HamMigrateDstInfo {
    std::string borrowName{};
    std::string srcNodeId{};
    int16_t srcSocket{-1};
    uint32_t vaListSize{0};
    std::vector<VirtualAddress> vaLists{};
    int64_t responseNumaId{-1};
    uint64_t dstPid{};
    std::string dstNodeId{};
    uint8_t exportAccessMode{0};
};

class HamMigrateDstInfoMessage : public BaseMessage {
public:
    HamMigrateDstInfoMessage() = default;
    explicit HamMigrateDstInfoMessage(HamMigrateDstInfo hamMigrateDstInfoInput)
        : hamMigrateDstInfo(std::move(hamMigrateDstInfoInput)) {}
    explicit HamMigrateDstInfoMessage(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    inline HamMigrateDstInfo GetHamMigrateDstInfo() const
    {
        return hamMigrateDstInfo;
    }

    inline void SetHamMigrateDstInfo(const HamMigrateDstInfo& hamMigrateDstInfoIn)
    {
        hamMigrateDstInfo.borrowName = hamMigrateDstInfoIn.borrowName;
        hamMigrateDstInfo.srcNodeId = hamMigrateDstInfoIn.srcNodeId;
        hamMigrateDstInfo.srcSocket = hamMigrateDstInfoIn.srcSocket;
        hamMigrateDstInfo.vaListSize = hamMigrateDstInfoIn.vaListSize;
        hamMigrateDstInfo.vaLists = hamMigrateDstInfoIn.vaLists;
        hamMigrateDstInfo.responseNumaId = hamMigrateDstInfoIn.responseNumaId;
        hamMigrateDstInfo.dstPid = hamMigrateDstInfoIn.dstPid;
        hamMigrateDstInfo.dstNodeId = hamMigrateDstInfoIn.dstNodeId;
        hamMigrateDstInfo.exportAccessMode = hamMigrateDstInfoIn.exportAccessMode;
    }

    inline std::string ToString() const override
    {
        return "dstPid: " + std::to_string(hamMigrateDstInfo.dstPid) + "; dstNodeId: " + hamMigrateDstInfo.dstNodeId +
               "borrowName: " + hamMigrateDstInfo.borrowName + "; srcNodeId: " + hamMigrateDstInfo.srcNodeId +
               "srcSocket: " + std::to_string(hamMigrateDstInfo.srcSocket) +
               "; vaListSize: " + std::to_string(hamMigrateDstInfo.vaListSize) +
               "responseNumaId: " + std::to_string(hamMigrateDstInfo.responseNumaId) +
               "; exportAccessMode: " + std::to_string(hamMigrateDstInfo.exportAccessMode);
    };

private:
    HamMigrateDstInfo hamMigrateDstInfo{};
};
}
#endif // HAM_MIGRATE_DST_INFO_MESSAGE_H
