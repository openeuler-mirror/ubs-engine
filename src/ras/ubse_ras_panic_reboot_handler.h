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

#ifndef UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_HANDLER_H
#define UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_HANDLER_H
#include <string>
#include "ubse_common_def.h"
#include "ubse_ras.h"

namespace ubse::ras {
using ubse::common::def::UbseResult;

/*
 * 全局主根据当前拓扑将故障EID解析为nodeId。
 */
UbseResult ResolvePanicRebootFaultNode(const std::string& faultEid, std::string& faultNodeId);

/*
 * 转发前仅尝试本机柜倒换；无法判断时记录告警，不阻断转发。
 */
void TrySwitchRoleWhenLocalMasterFault(const std::string& faultEid);

/*
 * 本节点是否为全局唯一主节点（获取角色失败按非主处理）
 */
bool IsCurrentNodeMaster();

/*
 * 将本节点收到的PANIC/Kernel Reboot消息异步发送给全局主节点处理；本节点就是主节点时也走self RPC。
 * 处理结果由主节点通过 UBSE_RAS_PANIC_REBOOT_RESULT 异步回发，本节点收到后再应答本机sysSentry。
 */
UbseResult ForwardPanicRebootFaultToMaster(ALARM_FAULT_TYPE faultType, const std::string& msgId,
                                           const std::string& faultEid);

/*
 * 主节点处理成功后，按报文携带的forwardNodeId异步回发处理结果给故障接收节点（不等待应答），
 * 由其向本机sysSentry上报ack
 */
UbseResult NotifyPanicRebootFaultResult(const std::string& forwardNodeId, ALARM_FAULT_TYPE faultType,
                                        const std::string& msgId);

/*
 * 通信线程收到转发的故障后调用：发布故障事件后立即返回，不做耗时处理。
 * 事件消息格式：faultEid_msgId_faultType_forwardNodeId
 */
UbseResult PubPanicRebootForwardFaultEvent(ALARM_FAULT_TYPE faultType, const std::string& faultEid,
                                           const std::string& msgId, const std::string& forwardNodeId);

/*
 * 故障事件处理入口（由事件模块线程执行）：解析事件消息，执行主节点处理流程，
 * 成功后回发处理结果给故障接收节点
 */
UbseResult HandlePanicRebootForwardFaultEvent(const std::string& eventMsg);

/*
 * 订阅转发故障事件，StartRasHandler启动时调用
 */
UbseResult SubscribePanicRebootForwardFaultEvent();

/*
 * 故障节点是主节点时触发主备倒换：主节点故障则将其agent切换走，备节点升主
 */
void SwitchRoleWhenMasterFault(const std::string& faultNodeId);
} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_PANIC_REBOOT_HANDLER_H
