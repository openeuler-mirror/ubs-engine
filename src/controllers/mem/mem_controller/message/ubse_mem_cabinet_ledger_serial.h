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

#ifndef UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_SERIAL_H
#define UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_SERIAL_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_def.h"
#include "ubse_common_def.h"
#include "ubse_mem_controller.h"
#include "ubse_mmi_interface.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::common::def::UbseResult;

// 柜内节点采集结果三态：SKIPPED 表示采集失败（本轮不修复），严禁按空账本处理
enum class UbseCollectStatus : uint8_t
{
    SUCCESS = 0,        // 采集成功，debtInfo 有效（已按请求柜过滤）
    SKIPPED = 1,        // 节点采集失败/不可达，关联关系本轮遮蔽
    CONFIRMED_FAULT = 2 // 节点确认 FAULT/PRE_BMC/UNKNOWN，走既有清理条件
};

// 柜级聚合账本查询请求（0x001A）：A 柜主 -> B 柜主
struct UbseCabinetLedgerQueryReq {
    std::string requesterMasterNodeId; // A 柜主 nodeId，仅用于日志与一致观察
    uint32_t requesterGroupId{0};      // A 柜号：B 按此过滤"借自 A / 借给 A"的记录
};

// 柜级聚合账本条目：B 柜内每个成员节点必须产出一条
struct UbseCabinetLedgerItem {
    std::string nodeId;
    UbseCollectStatus status{UbseCollectStatus::SKIPPED};
    NodeMemDebtInfo debtInfo{}; // 仅 SUCCESS 有效；无相关记录时为空账本
};

struct UbseCabinetLedgerQueryResp {
    UbseResult ret{0};               // 整体查询结果，部分节点失败不算整体失败
    std::string cabinetMasterNodeId; // 应答柜主 nodeId，与预期不一致时 A 丢弃本轮该柜结果
    std::vector<UbseCabinetLedgerItem> items;
};

// 跨柜失效通知转发（0x0019）：A 柜主 -> B 柜主 -> 柜内节点 R
struct UbseCabinetInvalidateItem {
    std::string nodeId;                                         // 目标导入节点 R
    UbseMemBorrowType borrowType{UbseMemBorrowType::FD_BORROW}; // FD/NUMA/ADDR，SHM 不走本通道
    std::string debtName;                                       // 待失效的导入记录名
};

struct UbseCabinetInvalidateForwardReq {
    std::string requesterMasterNodeId; // A 柜主 nodeId，B 侧仅做一致观察
    std::vector<UbseCabinetInvalidateItem> items;
};

struct UbseCabinetInvalidateForwardResp {
    UbseResult ret{0};     // 整体转发结果
    uint32_t failCount{0}; // 转发失败条数，失败不回滚 A 侧清理
};
} // namespace ubse::mem::controller

namespace ubse::mem::serial {
using ubse::common::def::UbseResult;
using ubse::mem::controller::UbseCabinetInvalidateForwardReq;
using ubse::mem::controller::UbseCabinetInvalidateForwardResp;
using ubse::mem::controller::UbseCabinetInvalidateItem;
using ubse::mem::controller::UbseCabinetLedgerItem;
using ubse::mem::controller::UbseCabinetLedgerQueryReq;
using ubse::mem::controller::UbseCabinetLedgerQueryResp;
using ubse::mem::controller::UbseCollectStatus;
using ubse::mem::controller::UbseMemBorrowType;
using ubse::serial::UbseDeSerialization;
using ubse::serial::UbseSerialization;

void UbseCabinetLedgerQueryReqSerialization(UbseSerialization& out, const UbseCabinetLedgerQueryReq& data);

bool UbseCabinetLedgerQueryReqDeserialization(UbseDeSerialization& in, UbseCabinetLedgerQueryReq& data);

void UbseCabinetLedgerQueryRespSerialization(UbseSerialization& out, const UbseCabinetLedgerQueryResp& data);

bool UbseCabinetLedgerQueryRespDeserialization(UbseDeSerialization& in, UbseCabinetLedgerQueryResp& data);

void UbseCabinetInvalidateForwardReqSerialization(UbseSerialization& out, const UbseCabinetInvalidateForwardReq& data);

bool UbseCabinetInvalidateForwardReqDeserialization(UbseDeSerialization& in, UbseCabinetInvalidateForwardReq& data);

void UbseCabinetInvalidateForwardRespSerialization(UbseSerialization& out,
                                                   const UbseCabinetInvalidateForwardResp& data);

bool UbseCabinetInvalidateForwardRespDeserialization(UbseDeSerialization& in, UbseCabinetInvalidateForwardResp& data);

 /**
 * @brief 把序列化结果拷贝到 com 层响应缓冲区，freeFunc 由 com 框架在回包后回调释放
 * @param out [in] 已完成的序列化对象
 * @param resp [out] com 层响应缓冲区
 * @return UBSE_OK 成功，其它失败（resp 置空）
 */
UbseResult UbseCabinetRespBuffer(UbseSerialization& out, UbseByteBuffer& resp);
} // namespace ubse::mem::serial

#endif // UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_SERIAL_H
