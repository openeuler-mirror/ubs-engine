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

namespace vm {

struct HamMigrateDstInfo {
    uint64_t dstPid{};
    std::string dstNodeId{};
};

class HamMigrateDstInfoMessage : public BaseMessage {
public:
    HamMigrateDstInfoMessage() = default;
    explicit HamMigrateDstInfoMessage(HamMigrateDstInfo hamMigrateDstInfoInput)
        : hamMigrateDstInfo(std::move(hamMigrateDstInfoInput))
    {
    }
    explicit HamMigrateDstInfoMessage(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    inline HamMigrateDstInfo GetHamMigrateDstInfo() const
    {
        return hamMigrateDstInfo;
    }

    inline void SetHamMigrateDstInfo(int dstPid, const std::string& dstNodeId)
    {
        hamMigrateDstInfo.dstPid = dstPid;
        hamMigrateDstInfo.dstNodeId = dstNodeId;
    }

    inline std::string ToString() const override
    {
        return "dstPid: " + std::to_string(hamMigrateDstInfo.dstPid) + "; dstNodeId: " + hamMigrateDstInfo.dstNodeId;
    };

private:
    HamMigrateDstInfo hamMigrateDstInfo{};
};
} // namespace vm
#endif // HAM_MIGRATE_DST_INFO_MESSAGE_H
