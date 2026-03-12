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

#ifndef MIGRATE_STATE_MAP_MESSAGE_H
#define MIGRATE_STATE_MAP_MESSAGE_H

#include "base_message.h"
#include "vm_struct.h"

namespace vm {
class MigrateStateMapMessage final : public BaseMessage {
public:
    MigrateStateMapMessage() = default;

    explicit MigrateStateMapMessage(NumaVMInfoMap numaVmInfoMap) : migrateStateMap(std::move(numaVmInfoMap)) {}

    explicit MigrateStateMapMessage(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetData(const NumaVMInfoMap &numaVmInfoMap)
    {
        migrateStateMap = numaVmInfoMap;
    }

    NumaVMInfoMap GetData() const
    {
        return migrateStateMap;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    NumaVMInfoMap migrateStateMap;
};
} // namespace vm

#endif // MIGRATE_STATE_MAP_MESSAGE_H
