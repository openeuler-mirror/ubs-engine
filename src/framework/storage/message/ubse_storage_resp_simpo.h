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

#ifndef UBSE_STORAGE_RESP_SIMPO_H
#define UBSE_STORAGE_RESP_SIMPO_H
#include <vector>
#include "ubse_storage.h"
#include "ubse_base_message.h"
namespace ubse::storage::message {
using namespace ubse::message;

struct UbseStorageResp {
    std::vector<KV> kvs;
};

class UbseStorageRespSimpo : public UbseBaseMessage {
public:
    ~UbseStorageRespSimpo() override;
    UbseStorageRespSimpo() = default;

    explicit UbseStorageRespSimpo(const UbseStorageResp &resp);

    explicit UbseStorageRespSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetStorageResp(const UbseStorageResp &resp);

    UbseStorageResp GetStorageResp();

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string ToString() const override;

private:
    UbseStorageResp storageResp_{};
    bool hasObj_{ false };
};
using UbseStorageRespSimpoPtr = Ref<UbseStorageRespSimpo>;
}
#endif // UBSE_STORAGE_RESP_SIMPO_H
