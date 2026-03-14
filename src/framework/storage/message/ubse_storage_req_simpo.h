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

#ifndef UBSE_STORAGE_REQ_H
#define UBSE_STORAGE_REQ_H
#include "ubse_base_message.h"
namespace ubse::storage::message {
using namespace ubse::message;

enum class UbseStorageReqCmdType : uint8_t {
    GET = 0,
    GET_WITH_PREFIX = 1,
};

std::string ReqCmdTypeToString(UbseStorageReqCmdType type);

struct UbseStorageReq {
    UbseStorageReqCmdType cmdType;
    std::string dbName;
    std::string key;
};

class UbseStorageReqSimpo : public UbseBaseMessage {
public:
    UbseStorageReqSimpo() = default;

    explicit UbseStorageReqSimpo(const UbseStorageReq &req);

    explicit UbseStorageReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseStorageReq GetStorageReq();

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string ToString() const override;

private:
    UbseStorageReq storageReq_{};
};
using UbseStorageReqSimpoPtr = Ref<UbseStorageReqSimpo>;
}
#endif // UBSE_STORAGE_REQ_H
