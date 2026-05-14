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

#ifndef UBSE_MANAGER_UBSE_RAS_COM_HANDLER_H
#define UBSE_MANAGER_UBSE_RAS_COM_HANDLER_H

#include "ubse_com_module.h"

namespace ubse::ras {
using namespace ubse::com;
class UbseRasComHandler : public UbseComBaseMessageHandler {
public:
    UbseRasComHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_BMC_REBOOT);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseRasComHandlerPtr = Ref<UbseRasComHandler>;

class UbseRasSwitchRoleHandler : public UbseComBaseMessageHandler {
public:
    UbseRasSwitchRoleHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_SWITCH_ROLE);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseRasSwitchRoleHandlerPtr = Ref<UbseRasSwitchRoleHandler>;

class UbseOomHandler : public UbseComBaseMessageHandler {
public:
    UbseOomHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_OOM);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseOomHandlerPtr = Ref<UbseOomHandler>;
} // namespace ubse::ras

#endif // UBSE_MANAGER_UBSE_RAS_COM_HANDLER_H
