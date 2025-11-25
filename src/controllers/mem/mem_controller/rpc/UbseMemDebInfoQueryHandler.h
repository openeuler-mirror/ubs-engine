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

#ifndef UBSE_UBSEMEMDEBINFOQUERYHANDLER_H
#define UBSE_UBSEMEMDEBINFOQUERYHANDLER_H
#include "ubse_com_base.h"
namespace ubse::mem_controller::rpc {
using namespace ubse::com;
class UbseMemDebInfoQueryHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebInfoQueryHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
        UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_DEBINFO_QUERY);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER);
    }

    bool NeedReply() override
    {
        return true;
    }

    static UbseResult RegUbseMemDebInfoQueryHandler();
};
};


#endif // UBSE_UBSEMEMDEBINFOQUERYHANDLER_H