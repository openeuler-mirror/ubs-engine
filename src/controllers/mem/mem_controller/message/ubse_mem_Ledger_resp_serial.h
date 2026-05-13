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

#ifndef UBS_ENGINE_UBSE_MEM_LEDGER_RESP_SERIAL_H
#define UBS_ENGINE_UBSE_MEM_LEDGER_RESP_SERIAL_H

#include "ubse_base_message.h"
#include "ubse_mem_controller_def.h"
namespace ubse::mem::controller::message {
using namespace ubse::mem::def;
using namespace ubse::message;

class UbseMemLedgerRespSerial : public UbseBaseMessage {
public:
    UbseMemLedgerRespSerial() = default;
    explicit UbseMemLedgerRespSerial(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetLedgerResp(LedgerResp response)
    {
        resp_ = std::move(response);
    }

    inline LedgerResp GetLedgerResp()
    {
        return resp_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    LedgerResp resp_;
};

using UbseMemLedgerRespSimpoPtr = Ref<UbseMemLedgerRespSerial>;
} // namespace ubse::mem::controller::message

#endif // UBS_ENGINE_UBSE_MEM_LEDGER_RESP_SERIAL_H
