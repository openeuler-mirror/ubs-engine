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

#ifndef UBS_ENGINE_UBSE_MEM_REPAIR_GUARD_H
#define UBS_ENGINE_UBSE_MEM_REPAIR_GUARD_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_def.h"
#include "ubse_common_def.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_def.h"
#include "message/ubse_mem_cabinet_ledger_serial.h"

namespace ubse::mem::controller {
using ubse::common::def::UbseResult;

 /**
 * @brief 破坏性修复二次确认结果（设计文档 §5.3 决策表）
 */
enum class UbseRepairConfirmResult : uint8_t {
    CONFIRM_ALIVE = 0,   // 对端记录仍存在：放弃本次破坏性动作，按 MasterRunning*Handler 语义刷新本侧账本
    CONFIRM_MISSING = 1, // 对端记录确认不存在（含终态销毁）：可执行破坏性动作
    CONFIRM_FAULT = 2,   // 对端节点确认 FAULT/PRE_BMC/UNKNOWN：可执行，走既有清理条件
    CONFIRM_FAILED = 3,  // 点查失败/超时：保守放弃本次动作，交下轮再判（不设重试循环）
};

 /**
 * @brief 二次确认点查的对端记录方向
 */
enum class UbseRepairQuerySide : uint8_t {
    QUERY_PEER_IMPORT = 0, // 查对端导入记录（清理本侧导出前确认）
    QUERY_PEER_EXPORT = 1, // 查对端导出记录（清理本侧导入相关动作前确认）
};

 /**
 * @brief A 侧破坏性动作前跨柜二次确认（设计文档 §5.3）
 *        经对端柜主 CLOS 点查实时状态后判定，避免用陈旧副本误删在途关系；决策：
 *        对端记录仍存在 -> CONFIRM_ALIVE（放弃并刷新）；确认不存在/终态销毁 -> CONFIRM_MISSING；
 *        对端节点确认故障 -> CONFIRM_FAULT；查询失败/超时 -> CONFIRM_FAILED（保守放弃）
 * @param borrowType [in] 借用类型（FD/NUMA/ADDR），决定 CLOS 点查变体
 * @param side [in] 点查方向：查对端导入或导出记录
 * @param request [in] 既有 UbseMemDebtQueryRequest，与柜内点查构造方式一致
 * @return 见 UbseRepairConfirmResult；调用方对 ALIVE/FAILED 均不执行破坏性动作
 */
UbseRepairConfirmResult ConfirmBeforeDestructiveRepair(UbseMemBorrowType borrowType, UbseRepairQuerySide side,
                                                       const def::UbseMemDebtQueryRequest& request);

 /**
 * @brief A 侧批量跨柜失效通知转发（0x0019，柜主 -> 柜主，设计文档 §5.4）
 *        按对端柜主分组，同一柜的失效通知合并为一次转发，避免跨柜 RPC 放大；
 *        转发失败不回滚本侧清理，交对端周期对账兜底
 * @param items [in] 待失效的导入记录列表（nodeId 为目标节点 R）
 * @param failCount [out] 转发失败条数（含记录不存在等无副作用失败，调用方按已失效处理）
 * @return 全部转发完成返回 UBSE_OK，存在失败返回 UBSE_ERROR（调用方仅记录，不重试）
 */
UbseResult ForwardCrossCabinetInvalidate(const std::vector<UbseCabinetInvalidateItem>& items, uint32_t& failCount);

 /**
 * @brief B 侧跨柜失效通知转发（0x0019，柜主 -> 柜主 -> 柜内节点 R）
 *        按 UbseMemBorrowType 分发到既有 0x0017 单导入失效通道；详见设计文档 §5.4
 * @param req [in] UbseCabinetInvalidateForwardReq 序列化报文
 * @param resp [out] UbseCabinetInvalidateForwardResp 序列化报文
 * @return UBSE_OK 表示报文已正常回填，其它为构造失败
 */
UbseResult CabinetInvalidateForwardHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

 /**
 * @brief B 侧跨柜单对象点查应答（0x001B~0x0020 六个 CLOS 变体共用实现）
 *        柜内直达目标节点现拉实时账本后原样回包，不读柜主副本；详见设计文档 §4.1.3
 * @param opCode [in] 触发本次转发的 CLOS 变体 OpCode（com 层 handler 不携带 opcode，由注册处注入）
 * @param req [in] 既有 UbseMemDebtQueryRequest 序列化报文
 * @param resp [out] 对应类型 obj simpo 序列化报文
 * @return UBSE_OK 表示报文已正常回填，其它为构造失败
 */
UbseResult CrossCabinetQueryForwardHandler(uint16_t opCode, const UbseByteBuffer& req, UbseByteBuffer& resp);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_REPAIR_GUARD_H
