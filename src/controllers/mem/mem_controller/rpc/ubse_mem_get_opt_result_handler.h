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

#ifndef UBSE_MEM_GET_OPT_RESULT_HANDLER_H
#define UBSE_MEM_GET_OPT_RESULT_HANDLER_H

#include "ubse_com_base.h"

namespace ubse::mem::controller::rpc {
using namespace ubse::com;

class UbseMemGetOptResultHandler : public UbseComBaseMessageHandler {
public:
    UbseMemGetOptResultHandler() = default;

    static UbseResult RegUbseMemGetOptResultHandler();

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GET_OPT_RES);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
    }

    bool NeedReply() override
    {
        return true;
    }
};
} // namespace ubse::mem::controller::rpc
#endif