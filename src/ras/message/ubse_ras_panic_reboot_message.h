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

#ifndef UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_MESSAGE_H
#define UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_MESSAGE_H
#include <string>
#include <utility>
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "src/include/ubse_base_message.h"

namespace ubse::ras {
using ubse::common::def::UbseResult;
using ubse::message::UbseBaseMessage;
using ubse::serial::UbseDeSerialization;
using ubse::serial::UbseSerialization;
using ubse::utils::Ref;

// PANIC/内核重启故障跨机柜转发与结果通知共用的消息体。
// 转发方向（UBSE_RAS_PANIC_REBOOT）：faultEid + msgId + faultType + forwardNodeId 有效，
// forwardNodeId 为转发方（故障接收节点）本机ID，主节点按它回发处理结果（多跳场景下ctx只能拿到中继节点）；
// 结果通知方向（UBSE_RAS_PANIC_REBOOT_RESULT）：msgId + faultType 有效，faultEid/forwardNodeId 置空，
// result 携带处理结果。
struct RasPanicRebootData {
    std::string faultEid;
    std::string msgId;
    uint32_t faultType{};
    UbseResult result = UBSE_OK;
    std::string forwardNodeId;
};

class UbseRasPanicRebootMessage : public UbseBaseMessage {
public:
    UbseRasPanicRebootMessage() = default;
    UbseRasPanicRebootMessage(std::string setFaultEid, std::string setMsgId, uint32_t setFaultType,
                              std::string setForwardNodeId = "")
    {
        data.faultEid = std::move(setFaultEid);
        data.msgId = std::move(setMsgId);
        data.faultType = setFaultType;
        data.forwardNodeId = std::move(setForwardNodeId);
    }
    inline void SetFaultEid(std::string setFaultEid)
    {
        data.faultEid = std::move(setFaultEid);
    }
    inline void SetMsgId(std::string setMsgId)
    {
        data.msgId = std::move(setMsgId);
    }
    inline void SetFaultType(uint32_t setFaultType)
    {
        data.faultType = setFaultType;
    }
    inline void SetResult(UbseResult setResult)
    {
        data.result = setResult;
    }
    inline void SetForwardNodeId(std::string setForwardNodeId)
    {
        data.forwardNodeId = std::move(setForwardNodeId);
    }
    inline std::string GetFaultEid() const
    {
        return data.faultEid;
    }
    inline std::string GetMsgId()
    {
        return data.msgId;
    }
    inline uint32_t GetFaultType()
    {
        return data.faultType;
    }
    inline UbseResult GetResult()
    {
        return data.result;
    }
    inline std::string GetForwardNodeId()
    {
        return data.forwardNodeId;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    static void Serialization(UbseSerialization& out, RasPanicRebootData& serialData);

    static UbseResult Deserialization(UbseDeSerialization& in, RasPanicRebootData& deSerialData);

private:
    RasPanicRebootData data;
};

using UbseRasPanicRebootMessagePtr = Ref<UbseRasPanicRebootMessage>;
} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_MESSAGE_H
