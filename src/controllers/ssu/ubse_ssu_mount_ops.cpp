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

#include "ubse_ssu_mount_ops.h"

#include <unistd.h>
#include <ctime>
#include <cstring>
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::service {

using namespace ubse::log;
using namespace ubse::ssu::message;
using namespace ubse::ssu::persist;
using namespace ubse::ssu::debt;
using namespace ubse::ssu::utils;
using namespace ubse::election;
using namespace ubse::com;

UBSE_DEFINE_THIS_MODULE("ubse");

// 查找设备中的命名空间，仅按 uuid 匹配
const UbseSsuDevNameSpace *FindNsInDevice(const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                         const std::string &eid, const std::string &uuidStr)
{
    if (uuidStr.empty()) {
        return nullptr;
    }
    auto devIt = devMap.find(eid);
    if (devIt == devMap.end()) {
        return nullptr;
    }
    for (const auto &ns : devIt->second->nameSpaces) {
        // 缓存中 uuid 为 16 字节二进制，转换为标准格式（36字符带连字符）后与账本记录比对
        if (StrToUuid(ns.uuid) == uuidStr) {
            return &ns;
        }
    }
    return nullptr;
}

std::string ResolveNqn(const std::string &nqn, const char *defaultNqn)
{
    if (!nqn.empty()) {
        return nqn;
    }
    if (defaultNqn == nullptr) {
        UBSE_LOG_ERROR << "ResolveNqn: defaultNqn is null";
        return {};
    }
    return std::string(defaultNqn, strnlen(defaultNqn, UBSE_SSU_MAX_NQN_LENGTH));
}

// agent端attach/detach单个命名空间：使用master验证后返回的字段构造UbseSsuDevNameSpace并调用相应adapter接口
static uint32_t AgentAttachDetachNs(bool isAttach, const UbseSsuNameSpaceInfo &nsInfo,
                                    const UbseSsuNsVerifyInfo &verifyInfo, const std::string &nqn)
{
    UbseSsuDevNameSpace nameSpace{};
    nameSpace.namespaceId = nsInfo.namespaceId;
    nameSpace.subSystem = {nsInfo.tgtEid, nsInfo.tgtNqn, verifyInfo.jettyId};
    nameSpace.guid = verifyInfo.guid;

    auto nqnFinal = ResolveNqn(nqn, verifyInfo.defaultNqn.c_str());
    if (nqnFinal.empty()) {
        UBSE_LOG_ERROR << (isAttach ? "AttachSingleNsVerified" : "DetachSingleNsVerified")
                       << ": resolve nqn failed, eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
        return UBSE_ERR_INVALID_ARG;
    }
    auto opRet = isAttach ? UbseSsuAdapterInterface::GetInstance().AttachDevNameSpace(nqnFinal, nameSpace) :
                            UbseSsuAdapterInterface::GetInstance().DetachDevNameSpace(nqnFinal, nameSpace);
    if (opRet != UBSE_OK) {
        UBSE_LOG_ERROR << (isAttach ? "AttachDevNameSpace" : "DetachDevNameSpace") << " failed, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId << ", ret=" << opRet;
        return opRet;
    }
    return UBSE_OK;
}

// agent端attach单个命名空间
uint32_t AgentAttachNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuNsVerifyInfo &verifyInfo,
                       const std::string &nqn)
{
    return AgentAttachDetachNs(true, nsInfo, verifyInfo, nqn);
}

// agent端detach单个命名空间
uint32_t AgentDetachNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuNsVerifyInfo &verifyInfo,
                       const std::string &nqn)
{
    return AgentAttachDetachNs(false, nsInfo, verifyInfo, nqn);
}

uint32_t SetupBlockDeviceWithProbe(const std::string &tag, const std::string &reqDevName,
                                   const std::string &ledgerDevName, const std::vector<std::string> &nsDevPaths,
                                   const UbseCreateBlockDeviceOptions &options, std::string &devPath)
{
    // 首次挂载（账本无记录）：直接 Create，无需 Probe
    if (ledgerDevName.empty()) {
        uint32_t ret = UbseSsuAdapterInterface::GetInstance().CreateBlockDevice(reqDevName, nsDevPaths, options, devPath);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": CreateBlockDevice failed, devName=" << reqDevName << ", ret=" << ret;
            if (!reqDevName.empty()) {
                UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(reqDevName);
            }
            return ret;
        }
        return UBSE_OK;
    }

    // 重挂：先 Probe 确认元数据状态
    int probeResult = UbseSsuAdapterInterface::GetInstance().ProbeBlockDevice(ledgerDevName, options.addressingType);
    if (probeResult < 0) {
        // 探测失败（命令执行异常），fail-closed：禁止降级 Create，避免破坏保留元数据
        UBSE_LOG_ERROR << tag << ": ProbeBlockDevice failed, dev=" << ledgerDevName
                       << ", probeResult=" << probeResult;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_PROBE_FAILED;
    }
    if (probeResult == 0) {
        // 账本有记录但元数据已无残留：降级 Create（打 WARN，此场景数据已不可恢复）
        UBSE_LOG_WARN << tag << ": ledgerDevName=" << ledgerDevName
                      << " set but no metadata residual, fallback to CreateBlockDevice";
        uint32_t ret = UbseSsuAdapterInterface::GetInstance().CreateBlockDevice(reqDevName, nsDevPaths, options, devPath);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": CreateBlockDevice failed, devName=" << reqDevName << ", ret=" << ret;
            if (!reqDevName.empty()) {
                UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(reqDevName);
            }
            return ret;
        }
        return UBSE_OK;
    }
    // probeResult == 1：元数据残留，走 Assemble 复用
    if (reqDevName != ledgerDevName) {
        UBSE_LOG_ERROR << tag << ": devName mismatch, reqDevName=" << reqDevName
                       << ", ledgerDevName=" << ledgerDevName;
        return UBSE_ERR_INVALID_ARG;
    }
    return UbseSsuAdapterInterface::GetInstance().AssembleBlockDevice(ledgerDevName, nsDevPaths, devPath,
                                                                     options.addressingType);
}

// ============ 重启恢复/对账原语 ============

// 探测本地挂载是否全部就绪：所有 nsDevPath 存在，且（如有聚合块设备）/dev/ssu/{devName} 存在
bool ProbeLocalMountReady(const UbseSsuAttachRecord &record)
{
    for (const auto &ns : record.nsList) {
        if (access(ns.nsDevPath.c_str(), F_OK) != 0) {
            return false;
        }
    }
    if (record.blockDeviceOptions.has_value() && !record.devName.empty()) {
        if (access((std::string(SSU_AGGREGATE_DEV_ROOT) + record.devName).c_str(), F_OK) != 0) {
            return false;
        }
    }
    return true;
}

// 幂等重挂：仅用本地记录字段回放底层原语，不经过verify RPC（账本可能尚未恢复，
// 且alreadyAttached分支会短路导致不重挂）
// 已存在的NS/聚合设备跳过；聚合设备经Probe决策Create/Assemble（委托SetupBlockDeviceWithProbe）
uint32_t RemountLocal(const UbseSsuAttachRecord &record)
{
    if (record.nsList.size() != record.nsVerifyList.size()) {
        UBSE_LOG_ERROR << "RemountLocal: nsList/nsVerifyList size mismatch, name=" << record.name;
        return UBSE_ERR_INVALID_ARG;
    }
    std::vector<std::string> nsDevPaths;
    nsDevPaths.reserve(record.nsList.size());
    for (size_t i = 0; i < record.nsList.size(); ++i) {
        const auto &ns = record.nsList[i];
        if (access(ns.nsDevPath.c_str(), F_OK) == 0) {
            // NS已挂载（进程重启、OS未重启场景），跳过
            nsDevPaths.push_back(ns.nsDevPath);
            continue;
        }
        auto ret = AgentAttachNs(ns, record.nsVerifyList[i], record.nqn);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "RemountLocal: attach ns failed, name=" << record.name
                           << ", nsId=" << ns.namespaceId << ", ret=" << ret;
            return ret;
        }
        nsDevPaths.push_back(ns.nsDevPath);
    }
    if (record.blockDeviceOptions.has_value()) {
        if (access((std::string(SSU_AGGREGATE_DEV_ROOT) + record.devName).c_str(), F_OK) == 0) {
            return UBSE_OK; // 聚合设备已存在
        }
        // 组装前核验：全部NS设备节点必须在位才允许组装聚合设备。attach返回OK但设备节点
        // 尚未出现（udev竞态窗口）时对部分在位硬件发起组装（mdadm/LVM）有元数据互踩风险，
        // 本轮返回失败等下轮重试，不做部分组装
        for (const auto &nsPath : nsDevPaths) {
            if (access(nsPath.c_str(), F_OK) != 0) {
                UBSE_LOG_WARN << "RemountLocal: ns dev node not visible yet, defer aggregate assembly, name="
                               << record.name << ", nsPath=" << nsPath;
                return UBSE_SSU_ERROR_BLOCK_DEVICE_PROBE_FAILED;
            }
        }
        std::string devPath;
        return SetupBlockDeviceWithProbe("Recover", record.devName, record.devName, nsDevPaths,
                                         *record.blockDeviceOptions, devPath);
    }
    return UBSE_OK;
}

// 自愈清理（RemountLocal反向）：master确认space已被free后清理本地残留挂载。
// 单步失败仅记录日志不中断（DetachDevNameSpace/StopBlockDevice幂等，下次事件重试收敛）
void CleanupLocalMount(const UbseSsuAttachRecord &record)
{
    if (record.nsList.size() != record.nsVerifyList.size()) {
        UBSE_LOG_ERROR << "CleanupLocalMount: nsList/nsVerifyList size mismatch, name=" << record.name;
        return;
    }
    if (record.blockDeviceOptions.has_value() && !record.devName.empty()) {
        auto stopRet = UbseSsuAdapterInterface::GetInstance().StopBlockDevice(record.devName);
        if (stopRet != UBSE_OK) {
            UBSE_LOG_ERROR << "CleanupLocalMount: StopBlockDevice failed, name=" << record.name
                           << ", devName=" << record.devName << ", ret=" << stopRet;
        }
    }
    for (size_t i = 0; i < record.nsList.size(); ++i) {
        auto ret = AgentDetachNs(record.nsList[i], record.nsVerifyList[i], record.nqn);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "CleanupLocalMount: detach ns failed, name=" << record.name
                           << ", nsId=" << record.nsList[i].namespaceId << ", ret=" << ret;
        }
    }
}

// agent端发送SSU状态更新RPC请求到master节点
uint32_t SendStatusUpdate(const std::string &requestName, UbseSsuNsState state,
                          const std::string &devName, const std::string &nodeId,
                          const std::string &resourceUuid)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SendStatusUpdate: get master info failed, " << FormatRetCode(res);
        return res;
    }

    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU), static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_STATUS_UPDATE));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendStatusUpdate: get ssu status update endpoint failed, requestId=" << requestName;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuStatusReqMsg reqMsg(requestName, state, devName, nodeId, resourceUuid);
    UbseSsuSyncRespMsg rspMsg; // 占位，后续通过异步线程返回真正结果
    res = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, rspMsg);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SendStatusUpdate: RpcSend failed, " << FormatRetCode(res) << ", requestId=" << requestName;
        return res;
    }
    auto statusRsp = rspMsg.GetErrorCode();
    if (statusRsp != UBSE_OK) {
        UBSE_LOG_WARN << "SendStatusUpdate: master returned error, code=" << statusRsp << ", requestId=" << requestName;
        return statusRsp;
    }
    return UBSE_OK;
}

// master以账本allocResult+collector缓存补齐完整挂载记录
bool BuildRecordFromLedger(const UbseSsuLedgerEntry &entry,
                           const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                           UbseSsuAttachRecord &record)
{
    record = UbseSsuAttachRecord{};
    record.name = entry.name;
    record.resourceUuid = entry.allocResult.resourceUuid;
    record.strategy = entry.allocReq.strategy;
    record.recordState = UbseSsuAttachRecordState::ATTACHED;
    record.devName = entry.devName;
    record.nsList = entry.allocResult.nameSpaceList;
    record.nsVerifyList.reserve(record.nsList.size());
    for (const auto &ns : record.nsList) {
        const auto *targetNs = FindNsInDevice(devMap, ns.tgtEid, ns.nsUuid);
        if (targetNs == nullptr) {
            UBSE_LOG_WARN << "BuildRecordFromLedger: ns not found in dev cache, name=" << entry.name
                          << ", eid=" << ns.tgtEid;
            return false; // 缓存缺失（采集抖动）：放弃本条，下轮重试
        }
        UbseSsuNsVerifyInfo verify;
        verify.defaultNqn = std::string(
            targetNs->customData.defaultNqn, strnlen(targetNs->customData.defaultNqn, UBSE_SSU_MAX_NQN_LENGTH));
        verify.jettyId = targetNs->subSystem.jettyId;
        verify.guid = targetNs->guid;
        record.nsVerifyList.emplace_back(std::move(verify));
        // identity从NS customData恢复（分配时写入，同空间NS一致）：
        std::string userName(
            targetNs->customData.userName, strnlen(targetNs->customData.userName, UBSE_SSU_MAX_USER_NAME_LENGTH));
        if (record.identity.userName.empty()) {
            record.identity.uid = targetNs->customData.uid;
            record.identity.userName = std::move(userName);
        } else if (record.identity.uid != targetNs->customData.uid || record.identity.userName != userName) {
            UBSE_LOG_WARN << "BuildRecordFromLedger: ns identity mismatch, name=" << entry.name
                          << ", eid=" << ns.tgtEid;
            return false; // 同空间NS identity异常：放弃本条，下轮重试
        }
    }
    if (record.strategy != UbseSsuAllocStrategy::NORMAL) {
        UbseCreateBlockDeviceOptions options;
        options.addressingType = (record.strategy == UbseSsuAllocStrategy::STRIPED) ?
                                     UbseSsuAddressingType::STRIPED : UbseSsuAddressingType::LINEAR;
        record.blockDeviceOptions = options;
    }
    record.attachedAt = static_cast<int64_t>(time(nullptr));
    // 出口终检：产出必须通过字段校验（RPC serde与落盘共用同一校验）
    if (!IsRecordFieldsValid(record)) {
        UBSE_LOG_WARN << "BuildRecordFromLedger: built record failed field validation, name=" << entry.name;
        return false;
    }
    return true;
}

// 双向diff（以账本为准）生成单节点平账清单
void DiffNodeMountSummary(const std::string &nodeId, const UbseSsuMountSummaryResp &summary,
                          std::vector<UbseSsuReconcileItem> &items)
{
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    std::unordered_map<std::string, std::vector<const UbseSsuMountSummaryItem *>> agentRecords;
    for (const auto &item : summary.items) {
        agentRecords[item.name].push_back(&item);
    }
    // 复合键匹配agent记录：resourceUuid精确命中优先；兼容旧记录无uuid，
    // 仅在同名唯一记录时兜底；同名多uuid且有记录uuid不匹配视为无该条目的记录
    auto findAgentRecord = [&agentRecords](const std::string &name,
                                           const std::string &resourceUuid) -> const UbseSsuMountSummaryItem * {
        auto it = agentRecords.find(name);
        if (it == agentRecords.end()) {
            return nullptr;
        }
        const auto &records = it->second;
        for (const auto *rec : records) {
            if (rec->resourceUuid == resourceUuid) {
                return rec;
            }
        }
        if (records.size() == 1 && records[0]->resourceUuid.empty()) {
            return records[0];
        }
        return nullptr;
    };
    for (const auto &entry : ledger.GetAll()) {
        if (entry->state == UbseSsuNsState::FAULTED) {
            continue;
        }
        const bool expected = entry->attachNodes.count(nodeId) > 0;
        const auto *agentItem = findAgentRecord(entry->name, entry->allocResult.resourceUuid);
        if (expected) {
            if (agentItem == nullptr) {
                // 账本有、agent记录缺失/uuid不匹配（旧代残留）/未挂载：REMOUNT
                UbseSsuReconcileItem item;
                item.name = entry->name;
                item.resourceUuid = entry->allocResult.resourceUuid;
                item.action = UbseSsuReconcileAction::REMOUNT;
                items.push_back(std::move(item));
                continue;
            }
            if (agentItem->recordState == UbseSsuAttachRecordState::PENDING) {
                continue; // 在途挂载，主流程收敛
            }
            if (agentItem->recordState == UbseSsuAttachRecordState::SUSPECT) {
                UBSE_LOG_WARN << "MountReconcile: suspect record needs manual check, name=" << entry->name
                              << ", nodeId=" << nodeId;
                continue;
            }
            // 一致；独占空间devName属性级比对（属性级漂移告警，master账本为准）
            if (entry->allocReq.strategy != UbseSsuAllocStrategy::NORMAL && !agentItem->devName.empty() &&
                agentItem->devName != entry->devName) {
                UBSE_LOG_WARN << "MountReconcile: devName mismatch between ledger and agent, name=" << entry->name
                              << ", ledgerDev=" << entry->devName << ", agentDev=" << agentItem->devName;
            }
            continue;
        }
        if (agentItem != nullptr) {
            if (agentItem->recordState != UbseSsuAttachRecordState::ATTACHED) {
                continue; // PENDING在途/SUSPECT人工介入
            }
            UbseSsuReconcileItem item;
            item.name = entry->name;
            item.resourceUuid = entry->allocResult.resourceUuid;
            if (entry->allocReq.strategy != UbseSsuAllocStrategy::NORMAL && !entry->attachNodes.empty()) {
                // 独占空间归属明确为他节点：账本已权威裁决attacher，本节点ATTACHED记录必为
                // stale残留（或已发生的双挂异常），REPORT会被attacher校验拒收形成死循环、
                // 残留永不收敛——直接发CLEANUP让agent自愈清理（复用SPACE_NOT_FOUND清理语义）
                UBSE_LOG_WARN << "MountReconcile: exclusive space owned by other node, cleanup stale mount, name="
                              << entry->name << ", nodeId=" << nodeId
                              << ", attacher=" << *entry->attachNodes.begin();
                item.action = UbseSsuReconcileAction::CLEANUP;
            } else {
                // NORMAL多节点 或 独占但attachNodes为空（归属未知，存在attach完成到上报
                // 到达窗口的误清风险）：非破坏性补报，master按幂等语义收敛
                item.action = UbseSsuReconcileAction::REPORT;
            }
            items.push_back(std::move(item));
            continue;
        }
        // uuid不匹配（同名free后重分配旧代残留等形态）：不做自动CLEANUP，仅告警人工介入。
        // 自动清理在账本同名多代并存场景（如脑裂合并收编出第二条目）存在误清活挂载风险
        // ——agent记录uuid可能仍被同名其他账本条目正常跟踪，不满足"旧代已free"前提。
        // 告警前先核验归属：记录uuid仍被任一同名账本条目跟踪（含FAULTED，其可观测性由
        // SpaceFaultAlarm覆盖）则属合法活挂载，静默跳过由归属条目自身diff分流；
        // 仅uuid不属于账本任何条目（真旧代已free）才告警人工介入
        auto it = agentRecords.find(entry->name);
        if (it == agentRecords.end() || it->second.size() != 1 ||
            it->second[0]->recordState != UbseSsuAttachRecordState::ATTACHED ||
            it->second[0]->resourceUuid.empty() ||
            it->second[0]->resourceUuid == entry->allocResult.resourceUuid) {
            continue;
        }
        bool uuidTracked = false;
        for (const auto &sameNameEntry : ledger.GetByName(entry->name)) {
            if (sameNameEntry->allocResult.resourceUuid == it->second[0]->resourceUuid) {
                uuidTracked = true;
                break;
            }
        }
        if (uuidTracked) {
            continue;
        }
        UBSE_LOG_ERROR << "StaleGenerationAlarm: agent mount uuid mismatch with ledger, manual check required, name="
                       << entry->name << ", nodeId=" << nodeId << ", recordUuid=" << it->second[0]->resourceUuid
                       << ", ledgerUuid=" << entry->allocResult.resourceUuid;
    }
    // agent有记录而账本无该空间（已free）：CLEANUP
    for (const auto &[name, records] : agentRecords) {
        for (const auto *agentItem : records) {
            if (agentItem->recordState == UbseSsuAttachRecordState::PENDING) {
                continue;
            }
            // 复合键账本按name维度判断：该name下任意resourceUuid条目仍存在则不清理
            if (!ledger.GetByName(name).empty()) {
                continue;
            }
            // 收敛窗口内保守跳过：账本无记录还可能是脑裂分区期间对端分配的空间尚未收编
            // （Absorb与对账存在时序竞争），此时CLEANUP会拆掉agent合法挂载；REPORT同样会因
            // SPACE_NOT_FOUND诱发agent自愈误清。窗口过后（Absorb已收编）下轮对账再正确分流
            if (ledger.IsInReconcileWindow()) {
                UBSE_LOG_WARN << "MountReconcile: ledger in reconcile window, defer cleanup-or-report decision, name="
                              << name << ", nodeId=" << nodeId;
                continue;
            }
            UbseSsuReconcileItem item;
            item.name = name;
            item.resourceUuid = agentItem->resourceUuid;
            item.action = UbseSsuReconcileAction::CLEANUP;
            items.push_back(std::move(item));
        }
    }
}
} // namespace ubse::ssu::service
