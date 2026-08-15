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
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseModuleCode;
using ubse::com::UbseRasOpCode;

// 管理组信息查询的响应协议值，使用可读文本避免 0/1 隐含业务语义。
inline constexpr char MANAGING_GROUP_INFO_AVAILABLE[] = "available";
inline constexpr char MANAGING_GROUP_INFO_MISSING[] = "missing";

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

// 级联组主节点侧：查询当前拓扑中是否存在管理组信息
class UbseRasManagingGroupInfoHandler : public UbseComBaseMessageHandler {
public:
    UbseRasManagingGroupInfoHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_QUERY_MANAGING_GROUP_INFO);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseRasManagingGroupInfoHandlerPtr = Ref<UbseRasManagingGroupInfoHandler>;

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

// 全局主节点侧：接收故障接收节点转发的PANIC/内核重启故障，处理成功后异步回发结果
class UbseRasPanicRebootHandler : public UbseComBaseMessageHandler {
public:
    UbseRasPanicRebootHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_PANIC_REBOOT);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseRasPanicRebootHandlerPtr = Ref<UbseRasPanicRebootHandler>;

// 故障接收节点侧：接收主节点回发的故障处理结果，向本机sysSentry上报ack
class UbseRasPanicRebootResultHandler : public UbseComBaseMessageHandler {
public:
    UbseRasPanicRebootResultHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_PANIC_REBOOT_RESULT);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::RAS);
    }
};
using UbseRasPanicRebootResultHandlerPtr = Ref<UbseRasPanicRebootResultHandler>;
} // namespace ubse::ras

#endif // UBSE_MANAGER_UBSE_RAS_COM_HANDLER_H
