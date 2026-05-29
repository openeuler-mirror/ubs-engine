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

#ifndef HAM_MIGRATE_VM_INFO_MESSAGE_H
#define HAM_MIGRATE_VM_INFO_MESSAGE_H

#include "base_message.h"
#include "ham_migrate_vm_info.h"

namespace vm {
class HamMigrateVmInfoMessage : public BaseMessage {
public:
    HamMigrateVmInfoMessage() = default;

    explicit HamMigrateVmInfoMessage(std::vector<HamMigrateVmInfo> hamMigrateVmInfos)
        : hamMigrateVmInfos_(std::move(hamMigrateVmInfos))
    {
    }

    explicit HamMigrateVmInfoMessage(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetData(const std::vector<HamMigrateVmInfo>& hamMigrateVmInfos)
    {
        hamMigrateVmInfos_ = hamMigrateVmInfos;
    }

    std::vector<HamMigrateVmInfo> GetData() const
    {
        return hamMigrateVmInfos_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos_{};
};
} // namespace vm

#endif // HAM_MIGRATE_VM_INFO_MESSAGE_H
