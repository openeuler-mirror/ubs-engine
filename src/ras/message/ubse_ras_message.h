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

#ifndef UBSE_MANAGER_UBSE_RAS_MESSAGE_H
#define UBSE_MANAGER_UBSE_RAS_MESSAGE_H
#include <string>
#include <utility>
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "src/include/ubse_base_message.h"

namespace ubse::ras {
using namespace ubse::message;
using namespace ubse::serial;

struct RasData {
    std::string msgId;
    std::string data{};
    UbseResult result = UBSE_OK;
};

class UbseRasMessage : public UbseBaseMessage {
public:
    UbseRasMessage() = default;
    explicit UbseRasMessage(std::string setData)
    {
        data.data = std::move(setData);
    }
    inline void SetData(std::string setData)
    {
        data.data = std::move(setData);
    }
    inline void SetMsg(const std::string msg)
    {
        data.msgId = msg;
    }
    inline void SetResult(UbseResult setResult)
    {
        data.result = setResult;
    }
    inline std::string GetData()
    {
        return data.data;
    }
    inline std::string GetMsg()
    {
        return data.msgId;
    }
    inline UbseResult GetResult()
    {
        return data.result;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    static void Serialization(UbseSerialization& out, RasData& serialData);

    static UbseResult Deserialization(UbseDeSerialization& in, RasData& deSerialData);

private:
    RasData data;
};

using UbseRasMessagePtr = Ref<UbseRasMessage>;
} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_MESSAGE_H
