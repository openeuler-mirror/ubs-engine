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

#ifndef UBS_ENGINE_UBSE_MEM_REMOTE_NUMA_STATUS_H
#define UBS_ENGINE_UBSE_MEM_REMOTE_NUMA_STATUS_H

#include <vector>

#include "ubse_base_message.h"

namespace ubse::mem::controller::message {
using namespace ubse::message;
class UbseMemRemoteNumaStatus : public UbseBaseMessage {
public:
    UbseMemRemoteNumaStatus() = default;
    explicit UbseMemRemoteNumaStatus(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseMemRemoteNumaStatus(std::vector<std::pair<int64_t, int>> request)
    {
        numaStatus_ = std::move(request);
    }
    inline std::vector<std::pair<int64_t, int>> GetUbseRemoteNumaStatus()
    {
        return numaStatus_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    std::vector<std::pair<int64_t, int>> numaStatus_;
};
using UbseMemRemoteNumaStatusPtr = Ref<UbseMemRemoteNumaStatus>;
}

#endif // UBS_ENGINE_UBSE_MEM_REMOTE_NUMA_STATUS_H
