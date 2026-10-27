/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_MOUNT_OPS_H
#define UBSE_SSU_MOUNT_OPS_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "debt/ubse_ssu_debt_ledger.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_mount_reconcile_msg.h"
#include "ubse_ssu_attach_record.h"
#include "ubse_ssu_collector.h"

namespace ubse::ssu::service {

using namespace ubse::plugin::service::ssu;

// 聚合块设备根目录
inline constexpr const char *SSU_AGGREGATE_DEV_ROOT = "/dev/ssu/";

// ======== 底层操作原语（恢复流程/对账/master本机直平/agent RECONCILE handler共用） ========
// 从ubse_ssu_service_imp.cpp提取

// 查找设备中的命名空间，仅按 uuid 匹配
// uuid 是 NVMe 命名空间的持久化唯一标识；nsid 在删除后可能被复用，不能作为可靠匹配依据。
// 调用方需保证 uuidStr 来自权威数据源（账本），非空。
const UbseSsuDevNameSpace *FindNsInDevice(const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                         const std::string &eid, const std::string &uuidStr);

// NQN解析：请求nqn非空优先，否则回退设备defaultNqn（空值/空指针返回空串由调用方判错）
std::string ResolveNqn(const std::string &nqn, const char *defaultNqn);

// 单NS底层原语（verify协议执行方=agent角色）：使用master验证后返回的字段构造
// UbseSsuDevNameSpace并调用adapter接口；
// agent首次挂载、RemountLocal/CleanupLocalMount、master本机直平均复用
uint32_t AgentAttachNs(const UbseSsuNameSpaceInfo &nsInfo, const message::UbseSsuNsVerifyInfo &verifyInfo,
                       const std::string &nqn);
uint32_t AgentDetachNs(const UbseSsuNameSpaceInfo &nsInfo, const message::UbseSsuNsVerifyInfo &verifyInfo,
                       const std::string &nqn);

// 聚合块设备创建/组装（带Probe）：账本无记录直接Create；有记录先Probe——
// 探测失败fail-closed禁止降级Create，元数据无残留降级Create，有残留走Assemble复用
uint32_t SetupBlockDeviceWithProbe(const std::string &tag, const std::string &reqDevName,
                                   const std::string &ledgerDevName, const std::vector<std::string> &nsDevPaths,
                                   const UbseCreateBlockDeviceOptions &options, std::string &devPath);

// ======== 本地挂载恢复/对账原语 ========

// 探测本地挂载是否全部就绪：所有 nsDevPath 存在，且（如有聚合块设备）/dev/ssu/{devName} 存在
bool ProbeLocalMountReady(const persist::UbseSsuAttachRecord &record);

// 幂等重挂：仅用本地记录字段回放底层原语，不经过verify RPC（账本可能尚未恢复，
// 且alreadyAttached分支会短路导致不重挂）
// 已存在的NS/聚合设备跳过；聚合设备经Probe决策Create/Assemble（委托SetupBlockDeviceWithProbe）
uint32_t RemountLocal(const persist::UbseSsuAttachRecord &record);

// 自愈清理（RemountLocal反向）：master确认space已被free后清理本地残留挂载。
// 单步失败仅记录日志不中断（DetachDevNameSpace/StopBlockDevice幂等，下次事件重试收敛）
void CleanupLocalMount(const persist::UbseSsuAttachRecord &record);

// agent端发送SSU状态更新RPC请求到master节点
// resourceUuid：复合键精确上报（master按(name, resourceUuid)定位账本条目）；
// 空（旧记录）时master按name维度兼容解析
uint32_t SendStatusUpdate(const std::string &requestName, UbseSsuNsState state,
                          const std::string &devName, const std::string &nodeId,
                          const std::string &resourceUuid = "");

// master以账本allocResult+collector缓存补齐完整挂载记录
// （RunMountReconcile本机直平反向补记录/远端REMOUNT项首发补齐共用）。
// nqn留空：RemountLocal回放AgentAttachNs时经ResolveNqn取verify.defaultNqn，与首次挂载行为一致；
// identity从NS customData恢复（账本无此字段）：identity虽不参与回放
// （AgentAttachDetachNs仅用nsInfo+verifyInfo+nqn），但留空会导致RPC serde
// 字段校验失败（对账批次整体被拒）且落盘记录无法重载，必须补齐；
// STRIPED的raidLevel/chunkSize账本不存储（属首次attach请求参数），仅Probe降级Create路径使用，
// 置默认值即可（该场景元数据已丢失、数据本不可恢复，见SetupBlockDeviceWithProbe的probe==0分支）
bool BuildRecordFromLedger(const debt::UbseSsuLedgerEntry &entry,
                           const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                           persist::UbseSsuAttachRecord &record);

// 双向diff（以账本为准）生成单节点平账清单：
// - 账本期望挂载（节点∈attachNodes且非FAULTED）而agent记录缺失 → REMOUNT
//   （RunMountReconcile下发前就地以账本补齐完整记录，hasRecord=true首发）
// - agent有ATTACHED记录而账本无该空间（已free）→ CLEANUP（复用SPACE_NOT_FOUND清理语义）
// - 账本有空间但节点∉attachNodes而agent记录ATTACHED（上报丢失/状态矛盾）→ REPORT
//   （非破坏性补报，master按幂等语义收敛；不直接CLEANUP，规避attach完成到上报到达窗口的误清）
// - PENDING记录为在途挂载跳过；SUSPECT记录仅告警（不自动重挂，人工介入）；
//   独占空间devName与账本不一致 → 属性级WARN（属性级漂移）
void DiffNodeMountSummary(const std::string &nodeId, const message::UbseSsuMountSummaryResp &summary,
                          std::vector<message::UbseSsuReconcileItem> &items);
} // namespace ubse::ssu::service
#endif // UBSE_SSU_MOUNT_OPS_H
