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

#ifndef UBSE_MEM_CONTROLLER_HANDLER_H
#define UBSE_MEM_CONTROLLER_HANDLER_H

#include "ubse_com_module.h"
#include "ubse_com_op_code.h"

namespace ubse::mem::controller::agent {
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseMemRespCtrlOpCode;
using ubse::com::UbseModuleCode;
class UbseMemOperationRespHandler : public UbseComBaseMessageHandler {
public:
    UbseMemOperationRespHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_BORROW_RESULT_NOTIFY);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
    }

    static UbseResult RegUbseMemOperationRespHandlerToServer();

private:
};
} // namespace ubse::mem::controller::agent

#endif // UBSE_MEM_CONTROLLER_HANDLER_H
