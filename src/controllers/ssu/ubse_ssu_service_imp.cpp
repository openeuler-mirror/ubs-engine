/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_ssu_service_imp.h"

#include <securec.h>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include "debt/ubse_ssu_debt_ledger.h"
#include "framework/misc/ubse_future_mgr.h"
#include "framework/misc/ubse_logging_lock_guard.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_ssu_direct_to_vm_manager.h"
#include "ubse_ssu_scheduler.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::service {

using namespace ubse::log;
using namespace ubse::ssu::scheduler;
using namespace ubse::election;
using namespace ubse::com;
using namespace ubse::ssu::message;
using namespace ubse::misc::future;
using namespace ubse::utils;
using namespace ubse::ssu::debt;
using namespace ubse::ssu::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

// future等待最大超时时间（秒）
constexpr uint32_t MAX_TIMEOUT_SECONDS = 30;

// 超时后最大重试次数（不含首次请求）
constexpr uint32_t MAX_RETRY_COUNT = 3;

// 获取SSU服务单例实例
UbseSsuServiceImp &UbseSsuServiceImp::GetInstance()
{
    static UbseSsuServiceImp instance;
    return instance;
}

// 启动收集器，用于定期更新设备状态，仅master端调用
uint32_t UbseSsuServiceImp::StartCollecting()
{
    // agent侧不进行collector收集，直接返回
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "StartCollecting: failed to get node role, ret=" << FormatRetCode(ret);
        return ret;
    }
    if (role != ELECTION_ROLE_MASTER) {
        UBSE_LOG_INFO << "StartCollecting: skip on non-master node, role=" << role;
        return UBSE_OK;
    }

    ret = collector_.Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "StartCollecting: collector_.Start failed, ret=" << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "StartCollecting: collector started";
    return UBSE_OK;
}

// 停止设备收集器
void UbseSsuServiceImp::StopCollecting()
{
    collector_.Stop();
    UBSE_LOG_INFO << "StopCollecting: collector stopped";
}

uint32_t UbseSsuServiceImp::StartClearTimer()
{
    return UbseSsuDirectToVmManager::GetInstance().StartClearTimer();
}

void UbseSsuServiceImp::StopClearTimer()
{
    UbseSsuDirectToVmManager::GetInstance().StopClearTimer();
}

// 从设备缓存列表重建SSU账本
void UbseSsuServiceImp::RebuildLedgerFromDevList()
{
    auto devList = collector_.GetCachedDevList();
    UbseSsuDebtLedger::GetInstance().Rebuild(devList);
}

// agent端发送SSU分配RPC请求到master节点
static uint32_t SendAllocRpcRequest(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                    const std::string &requestNodeId, const std::string &masterNodeId,
                                    const std::string &requestId)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                           static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ALLOC_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendAllocRpcRequest: get ssu alloc req endpoint failed";
        return UBSE_ERROR;
    }

    UbseSsuAllocReqMsg reqMsg(requestId, requestNodeId, identity, req);

    UbseSsuSyncRespMsg syncResp; // 同步响应消息
    auto ret = endpoint->UbseRpcSend(masterNodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendAllocRpcRequest: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }

    if (syncResp.GetErrorCode() != UBSE_OK) {
        UBSE_LOG_ERROR << "SendAllocRpcRequest: server early error, " << FormatRetCode(syncResp.GetErrorCode())
                       << ", requestId=" << requestId;
        return syncResp.GetErrorCode();
    }

    return UBSE_OK;
}

// agent端通过发送RPC分配SSU空间
static uint32_t AllocSpaceViaRpc(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                 UbseSsuAllocResult &result)
{
    UBSE_LOG_INFO << "AllocSpaceViaRpc: name=" << req.name << ", nsSize=" << req.nsSize << ", nsNum=" << req.nsNum;
    UbseRoleInfo masterInfo{}, roleInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = req.name + "_" + roleInfo.nodeId;
    std::shared_ptr<UbseFutureMgr> respMgr;
    std::future<UbseSsuAllocResp> respFuture;

    respMgr = UbseFutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: requestId=" << requestId << " create future instance failed";
        return UBSE_ERROR_NULLPTR;
    }
    respFuture = respMgr->GetFuture<UbseSsuAllocResp>();

    // 先添加agent侧账本(状态creating)，再发送RPC请求
    UbseSsuLedgerEntry entry = {
        .name = req.name,
        .allocReq = req,
        .state = UbseSsuNsState::CREATING,
    };
    if (!UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: ledger entry already exists, reject duplicate alloc: name=" << req.name;
        return UBSE_ERR_EXISTED;
    }

    ret = SendAllocRpcRequest(req, identity, roleInfo.nodeId, masterInfo.nodeId, requestId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: SendAllocRpcRequest failed, " << FormatRetCode(ret)
                       << ", requestId=" << requestId;
        UbseSsuDebtLedger::GetInstance().Remove(req.name);
        return ret;
    }

    if (respFuture.wait_for(std::chrono::seconds(MAX_TIMEOUT_SECONDS)) != std::future_status::ready) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: timeout waiting for response, requestId=" << requestId;
        UbseSsuServiceImp::GetInstance().FreeSpace(req.name, identity);
        UbseSsuDebtLedger::GetInstance().Remove(req.name);
        return UBSE_ERR_TIMED_OUT;
    }

    auto resp = respFuture.get();
    if (resp.errorCode != UBSE_OK) {
        UbseSsuDebtLedger::GetInstance().Remove(req.name);
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: alloc phase1 failed, requestId=" << requestId
                       << ", errorCode=" << resp.errorCode << ", state=" << static_cast<int>(resp.state);
        return resp.errorCode;
    }

    result = resp.allocResult;

    // 更改agent侧账本状态created
    if (!UbseSsuDebtLedger::GetInstance().Modify(entry.name, [&result](UbseSsuLedgerEntry &e) {
            e.state = UbseSsuNsState::CREATED;
            e.allocResult = result;
        })) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: ledger entry not found after alloc success: name=" << req.name;
    }

    UBSE_LOG_INFO << "AllocSpaceViaRpc success: name=" << req.name << ", nsCount=" << result.nameSpaceList.size();
    return UBSE_OK;
}

// 分配主入口：根据节点角色选择不同分配方式
uint32_t UbseSsuServiceImp::AllocSpace(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                       UbseSsuAllocResult &result)
{
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpace: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteAlloc(req, identity, result);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return AllocSpaceViaRpc(req, identity, result);
    }

    UBSE_LOG_ERROR << "AllocSpace: unsupported node role=" << role;
    return UBSE_ERROR;
}

// 执行分配调度算法：获取设备列表、执行调度、更新保留空间
uint32_t UbseSsuServiceImp::ExecuteScheduler(const UbseSsuAllocSpaceReq &req,
                                             std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList)
{
    // 加锁持有到算法完成为止。实际执行分配时间长，要有预扣除。
    std::lock_guard<std::mutex> schedLock(schedulerLock_);

    auto devList = collector_.GetDevListWithReservations();
    if (devList.empty()) {
        UBSE_LOG_ERROR << "ExecuteScheduler: GetDevList failed, no cached devices";
        return UBSE_ERROR;
    }

    UbseSsuAllocRequest schedReq = {
        .allocSize = req.nsSize,
        .nsNum = req.nsNum,
        .lbaSize = static_cast<uint32_t>(req.lbaFormat),
        .addressingType = (req.strategy == UbseSsuAllocStrategy::STRIPED) ? UbseSsuAddressingType::STRIPED :
                                                                            UbseSsuAddressingType::LINEAR,
        .tenant = req.tenant,
    };

    if (schedReq.nsNum == 0) {
        UBSE_LOG_ERROR << "ExecuteScheduler: request nsNum is 0";
        return UBSE_ERROR;
    }

    if (req.strategy == UbseSsuAllocStrategy::STRIPED) {
        if (schedReq.allocSize % schedReq.nsNum != 0) {
            UBSE_LOG_ERROR << "ExecuteScheduler: allocSize is not divisible by nsNum";
            return UBSE_ERROR;
        }
        auto singleNsSize = schedReq.allocSize / schedReq.nsNum;
        // 这里验证singleNsSize是否为sectorSize的整数倍，chunkSize验证会在AttachStripedSpace时进行
        if (singleNsSize % schedReq.lbaSize != 0) {
            UBSE_LOG_ERROR << "ExecuteScheduler: singleNsSize is not divisible by lbaSize";
            return UBSE_ERROR;
        }
    }

    UbseSsuAllocationContext ctx(devList, schedReq);
    auto allocRet = scheduler_.Execute(ctx);
    if (allocRet != UbseSsuAllocRetCode::OK) {
        UBSE_LOG_ERROR << "Scheduler allocation failed, ret=" << static_cast<int>(allocRet)
                       << ", msg=" << ctx.result.msg;
        return UBSE_ERR_ALLOCATE;
    }
    eidNsSizeList = std::move(ctx.result.eidNsSizeList);
    // 预扣除分配容量
    collector_.AddReserveSpace(eidNsSizeList);
    return UBSE_OK;
}

static uint32_t BuildEidToSubNqnMap(const std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList,
                                    const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                    std::unordered_map<std::string, std::string> &eidToSubNqn)
{
    for (const auto &[eid, _] : eidNsSizeList) {
        auto it = devMap.find(eid);
        if (it == devMap.end() || it->second == nullptr) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to find dev for eid=" << eid;
            return UBSE_ERROR;
        }
        eidToSubNqn[eid] = it->second->subSystem.subNqn;
    }
    return UBSE_OK;
}

// 填充命名空间自定义数据：名称、用户、租户、默认NQN
static uint32_t FillNsCustomData(UbseSsuDevNameSpace &ns, const UbseSsuAllocSpaceReq &req,
                                 const UbseSsuAllocIdentityInfo &identity)
{
    memset_s(ns.customData.name, sizeof(ns.customData.name), 0, sizeof(ns.customData.name));
    if (strncpy_s(ns.customData.name, sizeof(ns.customData.name), req.name.c_str(), req.name.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy name, name=" << req.name;
        return UBSE_ERROR;
    }
    memset_s(ns.customData.userName, sizeof(ns.customData.userName), 0, sizeof(ns.customData.userName));
    if (strncpy_s(ns.customData.userName, sizeof(ns.customData.userName), identity.userName.c_str(),
                  identity.userName.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy userName, userName=" << identity.userName;
        return UBSE_ERROR;
    }
    memset_s(ns.customData.tenant, sizeof(ns.customData.tenant), 0, sizeof(ns.customData.tenant));
    if (!req.tenant.empty()) {
        if (strncpy_s(ns.customData.tenant, sizeof(ns.customData.tenant), req.tenant.c_str(), req.tenant.size()) !=
            EOK) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy tenant, tenant=" << req.tenant;
            return UBSE_ERROR;
        }
    }
    std::string hostNqn = GenerateHostNqn();
    memset_s(ns.customData.defaultNqn, sizeof(ns.customData.defaultNqn), 0, sizeof(ns.customData.defaultNqn));
    if (strncpy_s(ns.customData.defaultNqn, sizeof(ns.customData.defaultNqn), hostNqn.c_str(), hostNqn.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy defaultNqn, defaultNqn=" << hostNqn;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static void RollbackCreatedNameSpaces(const std::vector<UbseSsuDevNameSpace> &createdNsList)
{
    for (const auto &createdNs : createdNsList) {
        auto rollbackRet = UbseSsuAdapterInterface::GetInstance().DeleteDevNameSpace(createdNs);
        if (rollbackRet != UBSE_OK) {
            UBSE_LOG_ERROR << "Rollback DeleteDevNameSpace failed, eid=" << createdNs.subSystem.eid
                           << ", nsId=" << createdNs.namespaceId << ", ret=" << rollbackRet;
        }
    }
}

// 创建命名空间：根据设备ID和保留容量，创建对应命名空间
static uint32_t CreateDevNameSpaces(const UbseSsuAllocSpaceReq &req,
                                    const std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList,
                                    const UbseSsuAllocIdentityInfo &identity,
                                    const std::unordered_map<std::string, std::string> &eidToSubNqn,
                                    UbseSsuAllocResult &result)
{
    std::vector<UbseSsuDevNameSpace> createdNsList;
    for (const auto &[eid, nsSize] : eidNsSizeList) {
        auto subNqnIt = eidToSubNqn.find(eid);
        if (subNqnIt == eidToSubNqn.end()) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to find subNqn for eid=" << eid;
            return UBSE_ERROR;
        }
        UbseSsuDevNameSpace ns;
        ns.subSystem = {.eid = eid, .subNqn = subNqnIt->second};
        ns.nsze = nsSize;
        ns.ncap = nsSize;
        // LBA格式4K，对应flbas=1; LBA格式512B，对应flbas=0
        ns.nsOptions.flbas = (req.lbaFormat == UbseSsuLBAFormat::LBA_FORMAT_4K) ? 1 : 0;
        ns.customData = {.uid = identity.uid,
                         .allocStrategy = static_cast<uint8_t>(req.strategy),
                         .nsNum = static_cast<uint8_t>(req.nsNum),
                         .totalBytes = req.nsSize};
        if (FillNsCustomData(ns, req, identity) != UBSE_OK) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to fill custom data, eid=" << eid;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_ERROR;
        }
        auto createRet = UbseSsuAdapterInterface::GetInstance().CreateDevNameSpace(ns);
        if (createRet != UBSE_OK) {
            UBSE_LOG_ERROR << "CreateDevNameSpace failed, eid=" << eid << ", ret=" << createRet;
            RollbackCreatedNameSpaces(createdNsList);
            return createRet;
        }
        createdNsList.push_back(ns);
        auto uuidHexStr = StrToHex(ns.uuid);
        if (uuidHexStr.empty()) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to convert guid to hex, uuid=" << ns.uuid;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_ERROR;
        }
        auto persistentPath = std::string("/dev/disk/by-id/nvme-uuid.") + uuidHexStr;
        UbseSsuNameSpaceInfo nsInfo = {.tgtEid = ns.subSystem.eid,
                                       .tgtNqn = ns.subSystem.subNqn,
                                       .nsUuid = ns.uuid,
                                       .namespaceId = ns.namespaceId,
                                       .nsDevPath = persistentPath,
                                       .nsSize = ns.nsze,
                                       .lbaFormat = req.lbaFormat};
        result.nameSpaceList.push_back(nsInfo);
    }
    // 校验实际创建的namespace数量是否等于请求的数量
    if (result.nameSpaceList.size() != req.nsNum) {
        UBSE_LOG_ERROR << "ExecuteAlloc: scheduler return nsNum=" << result.nameSpaceList.size()
                       << " not equal req.nsNum=" << req.nsNum;
        RollbackCreatedNameSpaces(createdNsList);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseSsuServiceImp::ExecuteAlloc(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                         UbseSsuAllocResult &result)
{
    UBSE_LOG_INFO << "ExecuteAlloc: name=" << req.name << ", nsSize=" << req.nsSize << ", nsNum=" << req.nsNum;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto existingEntry = UbseSsuDebtLedger::GetInstance().Get(req.name);
    // 已存在CREATED状态条目，拒绝重复alloc
    if (existingEntry != nullptr && existingEntry->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_WARN << "ExecuteAlloc: ledger entry already exists in CREATED state, name=" << req.name;
        // 幂等性，返回成功
        return UBSE_OK;
    }

    // 标记预留操作开始，防止CollectDeviceList 在AddReserveSpace ~ CreateDevNameSpaces之间清理预留
    // 导致另一个线程的 GetDevListWithReservations 看到错误（偏大）的可用容量而超分。
    collector_.OnReserveBegin();

    std::vector<std::pair<std::string, uint64_t>> eidNsSizeList;
    auto ret = ExecuteScheduler(req, eidNsSizeList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ExecuteAlloc: scheduler failed, ret=" << ret;
        collector_.OnReserveEnd();
        return ret;
    }

    auto devMap = collector_.GetCachedDevMap();
    std::unordered_map<std::string, std::string> eidToSubNqn;
    if (BuildEidToSubNqnMap(eidNsSizeList, devMap, eidToSubNqn) != UBSE_OK) {
        UBSE_LOG_ERROR << "ExecuteAlloc: failed to build eidToSubNqn map";
        collector_.ReleaseReservation(eidNsSizeList);
        collector_.OnReserveEnd();
        return UBSE_ERROR;
    }
    ret = CreateDevNameSpaces(req, eidNsSizeList, identity, eidToSubNqn, result);
    if (ret != UBSE_OK) {
        collector_.ReleaseReservation(eidNsSizeList);
        collector_.OnReserveEnd();
        return ret;
    }

    // 创建成功，下次collector刷新（pendingOps_ == 0 时）会一并清除 reservationMgr_。
    // 刷新后预留容量由hardware usedBytes 自动反映，
    collector_.OnReserveEnd();
    result.name = req.name;
    result.strategy = req.strategy;
    // existingEntry为nullptr代表master端本地发起的分配请求，由ExecuteAlloc在成功时直接Put(CREATED)
    if (existingEntry == nullptr) {
        UbseSsuLedgerEntry entry;
        entry.name = req.name;
        entry.allocReq = req;
        entry.state = UbseSsuNsState::CREATED;
        entry.allocResult = result;
        if (!UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
            UBSE_LOG_ERROR << "ExecuteAlloc: ledger Put failed after alloc success: name=" << req.name;
        }
    }
    UBSE_LOG_INFO << "ExecuteAlloc success: name=" << req.name << ", nsCount=" << result.nameSpaceList.size();
    return UBSE_OK;
}

// agent端发送SSU释放RPC请求到master节点
static uint32_t SendFreeRpcRequest(const std::string &name, const UbseSsuAllocIdentityInfo &identity,
                                   const std::string &requestNodeId, const std::string &masterNodeId,
                                   const std::string &requestId)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                           static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_FREE_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendFreeRpcRequest: get ssu free req endpoint failed, requestId=" << requestId;
        return UBSE_ERROR;
    }

    UbseSsuFreeReqMsg reqMsg(requestId, requestNodeId, name, identity);

    UbseSsuSyncRespMsg syncResp; // 占位，后续通过异步线程返回真正结果
    auto ret = endpoint->UbseRpcSend(masterNodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendFreeRpcRequest: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }
    auto freeResp = syncResp.GetErrorCode();
    if (freeResp != UBSE_OK) {
        UBSE_LOG_WARN << "SendFreeRpcRequest: master returned error, code=" << freeResp << ", requestId=" << requestId;
        return freeResp;
    }

    return UBSE_OK;
}

// agent端释放ns：发送RPC请求到master节点
static uint32_t FreeSpaceViaRpc(const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeSpaceViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }

    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeSpaceViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = name + "_" + roleInfo.nodeId;

    // 超时后有限次同步重试：每次循环重新创建future并重新发送RPC请求，保证重试后仍会等待响应。
    // UbseFutureMgr同一requestId的promise只能set一次，须先释放旧respMgr再重建。
    for (uint32_t attempt = 0; attempt <= MAX_RETRY_COUNT; ++attempt) {
        auto respMgr = UbseFutureMgr::CreateInstance(requestId);
        if (respMgr == nullptr) {
            UBSE_LOG_ERROR << "FreeSpaceViaRpc: requestId=" << requestId << " create future instance failed";
            return UBSE_ERROR_NULLPTR;
        }
        auto respFuture = respMgr->GetFuture<UbseSsuFreeResp>();

        ret = SendFreeRpcRequest(name, identity, roleInfo.nodeId, masterInfo.nodeId, requestId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "FreeSpaceViaRpc: send rpc failed, requestId=" << requestId << ", ret=" << ret;
            return ret;
        }

        if (respFuture.wait_for(std::chrono::seconds(MAX_TIMEOUT_SECONDS)) == std::future_status::ready) {
            auto resp = respFuture.get();
            if (resp.errorCode != UBSE_OK) {
                UBSE_LOG_ERROR << "FreeSpaceViaRpc: free failed, requestId=" << requestId
                               << ", errorCode=" << resp.errorCode;
                return resp.errorCode;
            }
            // 更新agent侧账本
            UbseSsuDebtLedger::GetInstance().Remove(name);
            UBSE_LOG_INFO << "FreeSpaceViaRpc success: name=" << name;
            return UBSE_OK;
        }

        UBSE_LOG_WARN << "FreeSpaceViaRpc: attempt=" << attempt << " timed out, requestId=" << requestId
                      << ", will retry up to " << MAX_RETRY_COUNT << " times";
        // respMgr 在本次循环结束时自动释放，避免下一次 CreateInstance 因同一requestId阻塞
    }

    UBSE_LOG_ERROR << "FreeSpaceViaRpc: reached max retry count, requestId=" << requestId;
    return UBSE_ERR_TIMED_OUT;
}

// 释放主入口：根据节点角色选择不同释放方式
uint32_t UbseSsuServiceImp::FreeSpace(const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeSpace: failed to get node role, ret=" << ret;
        return ret;
    }

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    // 释放操作具有幂等性，释放不存在的空间应返回成功
    if (entryPtr == nullptr) {
        UBSE_LOG_WARN << "FreeSpace: record not found, name=" << name;
        return UBSE_OK;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteFree(name, identity);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return FreeSpaceViaRpc(name, identity);
    }

    UBSE_LOG_ERROR << "FreeSpace: unsupported node role=" << role;
    return UBSE_ERROR;
}

// 查找设备中指定命名空间ID的命名空间
static const UbseSsuDevNameSpace *FindNsInDevice(const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                                 const std::string &eid, uint32_t namespaceId)
{
    auto devIt = devMap.find(eid);
    if (devIt == devMap.end()) {
        return nullptr;
    }
    for (const auto &ns : devIt->second->nameSpaces) {
        if (ns.namespaceId == namespaceId) {
            return &ns;
        }
    }
    return nullptr;
}

// 校验命名空间是否与请求身份匹配
static bool IsNsIdentityMatch(const UbseSsuDevNameSpace &targetNs, const UbseSsuAllocIdentityInfo &identity)
{
    if (targetNs.customData.uid != identity.uid) {
        return false;
    }
    auto nsNameLen = strnlen(targetNs.customData.userName, sizeof(targetNs.customData.userName));
    if (nsNameLen != identity.userName.size()) {
        return false;
    }
    return strncmp(targetNs.customData.userName, identity.userName.c_str(), nsNameLen) == 0;
}

// 执行释放操作：校验命名空间身份并删除命名空间
uint32_t UbseSsuServiceImp::ExecuteFree(const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    UBSE_LOG_INFO << "ExecuteFree: name=" << name;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_WARN << "ExecuteFree: record not found, name=" << name;
        // 释放操作具有幂等性，释放不存在的空间应返回成功
        return UBSE_OK;
    }

    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_WARN << "ExecuteFree: invalid state, name=" << name << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t firstErr = UBSE_OK;
    std::vector<UbseSsuNameSpaceInfo> remainingNs; // 未删成功的namespace，保留在账本中便于上层重试
    std::vector<std::pair<std::string, uint64_t>> releasedCapacity;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.namespaceId);
        if (targetNs == nullptr) {
            // namespace已不在硬件中（可能已被删除或缓存未刷新），视为已释放，跳过
            UBSE_LOG_WARN << "ExecuteFree: namespace not found in device cache, treat as already freed, eid="
                          << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            continue;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "ExecuteFree: namespace identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            return UBSE_ERR_ACCESS_DENIED;
        }

        auto ret = UbseSsuAdapterInterface::GetInstance().DeleteDevNameSpace(*targetNs);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "ExecuteFree: DeleteDevNameSpace failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << ret;
            remainingNs.push_back(nsInfo);
            if (firstErr == UBSE_OK) {
                firstErr = ret;
            }
            continue;
        }
        releasedCapacity.emplace_back(nsInfo.tgtEid, nsInfo.nsSize);
    }

    // 预增加已删成功namespace的释放容量，等下一次collector刷新时再清除
    if (!releasedCapacity.empty()) {
        collector_.AddReleasedSpace(releasedCapacity);
    }

    // 没有失败的namespace，删除账本条目
    if (remainingNs.empty()) {
        UbseSsuDebtLedger::GetInstance().Remove(name);
        UBSE_LOG_INFO << "FreeSpace success: name=" << name;
        return UBSE_OK;
    }

    // 部分失败：账本只保留未删成功的条目，便于上层重试
    UbseSsuDebtLedger::GetInstance().Modify(
        name, [&remainingNs](UbseSsuLedgerEntry &e) { e.allocResult.nameSpaceList = std::move(remainingNs); });
    return firstErr;
}

// agent端发送SSU状态更新RPC请求到master节点
static uint32_t SendStatusUpdate(const std::string &requestName, UbseSsuNsState state)
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
        return UBSE_ERROR;
    }

    UbseSsuStatusReqMsg reqMsg(requestName, state);
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

static std::string ResolveNqn(const std::string &nqn, const char *defaultNqn)
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

// 修改账本状态，非主节点同步通知master
static uint32_t UpdateLedgerStateAndNotify(const std::string &name, UbseSsuNsState state, bool isMaster)
{
    UbseSsuNsState oldState;
    if (!UbseSsuDebtLedger::GetInstance().Modify(name, [&state, &oldState](UbseSsuLedgerEntry &e) {
            oldState = e.state;
            e.state = state;
        })) {
        UBSE_LOG_ERROR << "UpdateLedgerStateAndNotify: ledger entry not found, name=" << name
                       << ", state=" << static_cast<int>(state);
        return UBSE_ERROR;
    }
    if (!isMaster) {
        auto ret = SendStatusUpdate(name, state);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "UpdateLedgerStateAndNotify: SendStatusUpdate failed, name=" << name
                          << ", state=" << static_cast<int>(state) << ", ret=" << ret;
            // RPC失败，回退本地账本到修改前状态，保证主备一致性
            UbseSsuDebtLedger::GetInstance().Modify(name, [&oldState](UbseSsuLedgerEntry &e) { e.state = oldState; });
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
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
        return UBSE_ERROR;
    }
    auto opRet = isAttach ? UbseSsuAdapterInterface::GetInstance().AttachDevNameSpace(nqnFinal, nameSpace) :
                            UbseSsuAdapterInterface::GetInstance().DetachDevNameSpace(nqnFinal, nameSpace);
    if (opRet != UBSE_OK) {
        UBSE_LOG_ERROR << (isAttach ? "AttachDevNameSpace" : "DetachDevNameSpace") << " failed, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId << ", ret=" << opRet;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// agent端attach单个命名空间
static uint32_t AgentAttachNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuNsVerifyInfo &verifyInfo,
                              const std::string &nqn)
{
    return AgentAttachDetachNs(true, nsInfo, verifyInfo, nqn);
}

// agent端detach单个命名空间
static uint32_t AgentDetachNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuNsVerifyInfo &verifyInfo,
                              const std::string &nqn)
{
    return AgentAttachDetachNs(false, nsInfo, verifyInfo, nqn);
}

// agent端发送identity验证RPC请求到master节点
static uint32_t SendAttachDetachVerifyRpcRequest(const std::string &name, const UbseSsuAllocIdentityInfo &identity,
                                                 const std::string &requestNodeId, const std::string &masterNodeId,
                                                 const std::string &requestId)
{
    auto endpoint =
        UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                               static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ATTACH_DETACH_VERIFY_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendAttachDetachVerifyRpcRequest: get endpoint failed, requestId=" << requestId;
        return UBSE_ERROR;
    }
    UbseSsuAttachDetachVerifyReqMsg reqMsg(requestId, requestNodeId, name, identity);
    UbseSsuSyncRespMsg syncResp;
    auto ret = endpoint->UbseRpcSend(masterNodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendAttachDetachVerifyRpcRequest: RpcSend failed, " << FormatRetCode(ret)
                       << ", requestId=" << requestId;
        return ret;
    }
    auto syncErr = syncResp.GetErrorCode();
    if (syncErr != UBSE_OK) {
        UBSE_LOG_ERROR << "SendAttachDetachVerifyRpcRequest: master sync resp error, code=" << syncErr
                       << ", requestId=" << requestId;
        return syncErr;
    }
    return UBSE_OK;
}

// agent端：发送identity验证请求到master并等待异步响应（attach/detach通用）
static uint32_t VerifyAttachDetachIdentityViaRpc(const std::string &name, const UbseSsuAllocIdentityInfo &identity,
                                                 UbseSsuAttachDetachVerifyResp &verifyResp)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VerifyIdViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VerifyIdViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = name + "_verify_" + roleInfo.nodeId;
    for (uint32_t attempt = 0; attempt <= MAX_RETRY_COUNT; ++attempt) {
        auto respMgr = UbseFutureMgr::CreateInstance(requestId);
        if (respMgr == nullptr) {
            UBSE_LOG_ERROR << "VerifyIdViaRpc: create future failed, requestId=" << requestId;
            return UBSE_ERROR_NULLPTR;
        }
        auto respFuture = respMgr->GetFuture<UbseSsuAttachDetachVerifyResp>();

        ret = SendAttachDetachVerifyRpcRequest(name, identity, roleInfo.nodeId, masterInfo.nodeId, requestId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "VerifyIdViaRpc: send rpc failed, requestId=" << requestId << ", ret=" << ret;
            return ret;
        }

        if (respFuture.wait_for(std::chrono::seconds(MAX_TIMEOUT_SECONDS)) == std::future_status::ready) {
            verifyResp = respFuture.get();
            return UBSE_OK;
        }
        UBSE_LOG_WARN << "VerifyIdViaRpc: attempt=" << attempt << " timed out, requestId=" << requestId;
    }
    UBSE_LOG_ERROR << "VerifyIdViaRpc: reached max retry count, requestId=" << requestId;
    return UBSE_ERR_TIMED_OUT;
}

// agent端回滚已attach的NS的前向声明（实现在文件后部，供master/agent共用）
static uint32_t RollbackAttachedNsAndLedger(const std::vector<UbseSsuNameSpaceInfo> &attachedNsList,
                                            const UbseSsuSpaceReq &req,
                                            const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                            const UbseSsuAttachDetachVerifyResp *verifyResp = nullptr);

// agent端通用attach主流程：发送identity验证请求到master，验证成功后逐个attach NS。
// 可选创建聚合块设备（blockDeviceOptions非空时，Linear/Striped场景）。
// 失败时回滚已attach的NS并回退ledger状态为CREATED；成功时更新ledger状态为ATTACHED。
static uint32_t AgentAttach(const UbseSsuSpaceReq &req, const UbseSsuLedgerEntry &entry, const std::string &devName,
                            const UbseCreateBlockDeviceOptions *blockDeviceOptions,
                            std::vector<std::string> &nsDevPaths, std::string &devPath)
{
    UbseSsuAttachDetachVerifyResp verifyResp{};
    auto ret = VerifyAttachDetachIdentityViaRpc(req.name, req.identity, verifyResp);
    if (ret != UBSE_OK) {
        UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, false);
        return ret;
    }
    if (verifyResp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "AgentAttach: master verify failed, code=" << verifyResp.errorCode << ", name=" << req.name;
        UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, false);
        return verifyResp.errorCode;
    }

    // master返回的验证信息按ledger中NS顺序对齐
    if (verifyResp.nsVerifyList.size() != entry.allocResult.nameSpaceList.size()) {
        UBSE_LOG_ERROR << "AgentAttach: ns count mismatch, ledger=" << entry.allocResult.nameSpaceList.size()
                       << ", verify=" << verifyResp.nsVerifyList.size() << ", name=" << req.name;
        UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, false);
        return UBSE_ERROR;
    }

    nsDevPaths.clear();
    nsDevPaths.reserve(entry.allocResult.nameSpaceList.size());
    std::vector<UbseSsuNameSpaceInfo> attachedNsList;
    attachedNsList.reserve(entry.allocResult.nameSpaceList.size());
    for (size_t i = 0; i < entry.allocResult.nameSpaceList.size(); ++i) {
        const auto &nsInfo = entry.allocResult.nameSpaceList[i];
        const auto &verifyInfo = verifyResp.nsVerifyList[i];
        auto nsRet = AgentAttachNs(nsInfo, verifyInfo, req.nqn);
        if (nsRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentAttach: AttachSingleNsVerified failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            RollbackAttachedNsAndLedger(attachedNsList, req, {}, &verifyResp);
            return nsRet;
        }
        attachedNsList.push_back(nsInfo);
        nsDevPaths.push_back(nsInfo.nsDevPath);
    }

    if (blockDeviceOptions != nullptr) {
        auto createRet =
            UbseSsuAdapterInterface::GetInstance().CreateBlockDevice(devName, nsDevPaths, *blockDeviceOptions, devPath);
        if (createRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentAttach: CreateBlockDevice failed, devName=" << devName << ", ret=" << createRet;
            // 块设备未创建成功也尝试删除（幂等），再回滚已attach的NS
            if (!devName.empty()) {
                UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(devName);
            }
            RollbackAttachedNsAndLedger(attachedNsList, req, {}, &verifyResp);
            return createRet;
        }
    }

    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHED, false);
    UBSE_LOG_INFO << "AgentAttach success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// agent端通用detach主流程：发送identity验证请求到master，验证成功后逐个detach NS。
// 可选先删除聚合块设备（deleteBlockDevice=true，Linear/Striped场景）。
// 部分detach失败时保持ATTACHED状态（DetachDevNameSpace幂等，可重试收敛）；全部成功才回退为CREATED。
static uint32_t AgentDetach(const UbseSsuSpaceReq &req, const UbseSsuLedgerEntry &entry, const std::string &devName,
                            bool deleteBlockDevice)
{
    UbseSsuAttachDetachVerifyResp verifyResp{};
    auto ret = VerifyAttachDetachIdentityViaRpc(req.name, req.identity, verifyResp);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (verifyResp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "AgentDetach: master verify failed, code=" << verifyResp.errorCode << ", name=" << req.name;
        return verifyResp.errorCode;
    }
    if (verifyResp.nsVerifyList.size() != entry.allocResult.nameSpaceList.size()) {
        UBSE_LOG_ERROR << "AgentDetach: ns count mismatch, name=" << req.name;
        return UBSE_ERROR;
    }

    // identity验证通过后再删除聚合块设备（NS detach 之前），避免未授权删除导致状态不一致
    if (deleteBlockDevice && !devName.empty()) {
        auto deleteRet = UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(devName);
        if (deleteRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentDetach: DeleteBlockDevice failed, devName=" << devName << ", ret=" << deleteRet;
            return UBSE_ERROR;
        }
    }

    uint32_t detachRet = UBSE_OK;
    for (size_t i = 0; i < entry.allocResult.nameSpaceList.size(); ++i) {
        const auto &nsInfo = entry.allocResult.nameSpaceList[i];
        const auto &verifyInfo = verifyResp.nsVerifyList[i];
        auto nsRet = AgentDetachNs(nsInfo, verifyInfo, req.nqn);
        if (nsRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentDetach: DetachSingleNsVerified failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            detachRet = UBSE_ERROR;
        }
    }
    if (detachRet != UBSE_OK) {
        // 部分detach失败，保持ATTACHED状态，利用幂等性支持重试
        return UBSE_ERROR;
    }

    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, false);
    UBSE_LOG_INFO << "AgentDetach success: name=" << req.name;
    return UBSE_OK;
}

// master端：验证identity并返回构造AttachDevNameSpace所需的字段（defaultNqn/jettyId/guid/subNqn）。
uint32_t UbseSsuServiceImp::VerifyAttachDetachIdentity(const std::string &name,
                                                       const UbseSsuAllocIdentityInfo &identity,
                                                       std::vector<UbseSsuNsVerifyInfo> &nsVerifyList)
{
    UBSE_LOG_INFO << "VerifyAttachDetachIdentity: name=" << name;

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "VerifyAttachDetachIdentity: record not found, name=" << name;
        return UBSE_ERROR;
    }

    auto devMap = collector_.GetCachedDevMap();
    nsVerifyList.clear();
    nsVerifyList.reserve(entryPtr->allocResult.nameSpaceList.size());
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.namespaceId);
        if (targetNs == nullptr) {
            UBSE_LOG_ERROR << "VerifyAttachDetachIdentity: namespace not found in cache, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            return UBSE_ERROR;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "VerifyAttachDetachIdentity: identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            return UBSE_ERR_ACCESS_DENIED;
        }
        UbseSsuNsVerifyInfo verifyInfo;
        verifyInfo.defaultNqn =
            std::string(targetNs->customData.defaultNqn,
                        strnlen(targetNs->customData.defaultNqn, sizeof(targetNs->customData.defaultNqn)));
        verifyInfo.jettyId = targetNs->subSystem.jettyId;
        verifyInfo.guid = targetNs->guid;
        nsVerifyList.push_back(std::move(verifyInfo));
    }
    UBSE_LOG_INFO << "VerifyAttachDetachIdentity success: name=" << name << ", nsCount=" << nsVerifyList.size();
    return UBSE_OK;
}

// master端attach单个命名空间
static uint32_t AttachSingleNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuAllocIdentityInfo &identity,
                               const std::string &nqn, const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.namespaceId);
    if (targetNs == nullptr) {
        UBSE_LOG_ERROR << "AttachSingleNs: device or namespace not found in cache, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId;
        return UBSE_ERROR;
    }
    if (!IsNsIdentityMatch(*targetNs, identity)) {
        UBSE_LOG_ERROR << "AttachSingleNs: uid or userName not match, eid=" << nsInfo.tgtEid << ", uid=" << identity.uid
                       << ", userName=" << identity.userName;
        return UBSE_ERR_ACCESS_DENIED;
    }
    auto nqnFinal = ResolveNqn(nqn, targetNs->customData.defaultNqn);
    auto attachRet = UbseSsuAdapterInterface::GetInstance().AttachDevNameSpace(nqnFinal, *targetNs);
    if (attachRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSingleNs: AttachDevNameSpace failed, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId << ", ret=" << attachRet;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// master端detach单个命名空间
static uint32_t DetachSingleNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuAllocIdentityInfo &identity,
                               const std::string &nqn, const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.namespaceId);
    if (targetNs == nullptr) {
        // 设备或NS不在缓存中，说明已被detach，视为幂等成功
        UBSE_LOG_INFO << "DetachSingleNs: device or namespace not found in cache, treat as already detached, eid="
                      << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
        return UBSE_OK;
    }
    if (!IsNsIdentityMatch(*targetNs, identity)) {
        UBSE_LOG_ERROR << "DetachSingleNs: uid or userName not match, eid=" << nsInfo.tgtEid << ", uid=" << identity.uid
                       << ", userName=" << identity.userName;
        return UBSE_ERR_ACCESS_DENIED;
    }
    auto nqnFinal = ResolveNqn(nqn, targetNs->customData.defaultNqn);
    auto detachRet = UbseSsuAdapterInterface::GetInstance().DetachDevNameSpace(nqnFinal, *targetNs);
    if (detachRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachSingleNs: DetachDevNameSpace failed, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId << ", ret=" << detachRet;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 回滚已attach的NS（逐个detach）并将账本状态回退到CREATED
// verifyResp非空：agent端路径，用verifyInfo构造nameSpace并detach，isMaster=false
// verifyResp为空：master端路径，用collector缓存detach，isMaster=true
static uint32_t RollbackAttachedNsAndLedger(const std::vector<UbseSsuNameSpaceInfo> &attachedNsList,
                                            const UbseSsuSpaceReq &req,
                                            const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                            const UbseSsuAttachDetachVerifyResp *verifyResp)
{
    bool isMaster = verifyResp == nullptr ? true : false;
    uint32_t ret = UBSE_OK;
    for (size_t i = 0; i < attachedNsList.size(); ++i) {
        auto rollbackRet = isMaster ? DetachSingleNs(attachedNsList[i], req.identity, req.nqn, devMap) :
                                      AgentDetachNs(attachedNsList[i], verifyResp->nsVerifyList[i], req.nqn);

        if (rollbackRet != UBSE_OK) {
            UBSE_LOG_ERROR << "RollbackAttachedNsAndLedger: DetachDevNameSpace failed, eid=" << attachedNsList[i].tgtEid
                           << ", nsId=" << attachedNsList[i].namespaceId << ", ret=" << rollbackRet;
            ret = UBSE_ERROR;
        }
    }
    // 无论全部还是部分失败，均回退到CREATED状态
    // 部分detach失败时，NS处于"部分CREATED、部分ATTACHED"的不一致状态
    // 调用方可重试AttachLinearSpace/AttachStripedSpace，利用AttachSingleNs的幂等性重新attach所有NS
    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, isMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RollbackAttachedNsAndLedger: partial detach failed, name=" << req.name;
    }
    return ret;
}

// 挂载空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths)
{
    UBSE_LOG_INFO << "AttachSpace: name=" << req.name << ", nqn=" << req.nqn;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性：已经attached，再次attach返回成功
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachSpace: already attached, name=" << req.name;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_OK;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace: failed to get node role for status update, ret=" << roleRet;
        return UBSE_ERROR;
    }
    // 标记为attaching中
    if (UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHING, role == ELECTION_ROLE_MASTER) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace: failed to update ledger state and notify, name=" << req.name
                       << ", state=" << static_cast<int>(UbseSsuNsState::ATTACHING);
        return UBSE_ERROR;
    }

    // Agent节点需要通过master验证identity
    // 成功后返回defaultNqn等字段，agent据此调用AttachDevNameSpace
    if (role != ELECTION_ROLE_MASTER) {
        std::string devPath; // AttachSpace不创建聚合块设备
        return AgentAttach(req, *entryPtr, "", nullptr, nsDevPaths, devPath);
    }

    auto devMap = collector_.GetCachedDevMap();
    nsDevPaths.clear();
    nsDevPaths.reserve(entryPtr->allocResult.nameSpaceList.size());
    std::vector<UbseSsuNameSpaceInfo> attachedNsList;
    attachedNsList.reserve(entryPtr->allocResult.nameSpaceList.size());
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto ret = AttachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "AttachSpace: AttachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            RollbackAttachedNsAndLedger(attachedNsList, req, devMap);
            return ret;
        }
        attachedNsList.push_back(nsInfo);
        nsDevPaths.push_back(nsInfo.nsDevPath);
    }

    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHED, role == ELECTION_ROLE_MASTER);

    UBSE_LOG_INFO << "AttachSpace success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// detach空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachSpace(const UbseSsuSpaceReq &req)
{
    UBSE_LOG_INFO << "DetachSpace: name=" << req.name << ", nqn=" << req.nqn;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性 已经detach的NS，再次detach返回成功
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_INFO << "DetachSpace: no need to detach, name=" << req.name;
        return UBSE_OK;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachSpace: failed to get node role, ret=" << roleRet;
        return UBSE_ERROR;
    }

    // Master节点：本地有collector缓存，直接验证identity并detach
    // Agent节点：collector无数据，需通过RPC发送identity到master验证，获取字段后调用DetachDevNameSpace
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, *entryPtr, "", false);
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t detachRet = UBSE_OK;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto ret = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "DetachSpace: DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            detachRet = UBSE_ERROR;
        }
    }

    if (detachRet != UBSE_OK) {
        // 部分detach失败，保持ATTACHED状态，利用幂等性支持调用方重试
        // 1) DetachDevNameSpace幂等（见适配器契约），调用方可重试DetachSpace
        // 2) ExecuteFree要求state==CREATED才允许释放NS，保持ATTACHED可阻止在NS仍挂在host上时误删NS
        UBSE_LOG_ERROR << "DetachSpace: partial detach failed, keeping ATTACHED state, name=" << req.name;
        return UBSE_ERROR;
    }

    // 更新本地账本，非主节点同步通知master（role已在上方获取）
    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, role == ELECTION_ROLE_MASTER);

    UBSE_LOG_INFO << "DetachSpace success: name=" << req.name
                  << ", nsCount=" << entryPtr->allocResult.nameSpaceList.size();
    return UBSE_OK;
}

// AttachStripedSpace和AttachLinearSpace通用辅助输出
struct AttachNsCreateBlockDeviceOutput {
    std::vector<std::string> nsDevPaths;
    std::string devPath;
};

// master端AttachStripedSpace和AttachLinearSpace通用辅助
// attach所有NS + 创建聚合块设备 + 账本状态管理（ATTACHING -> ATTACHED / 失败回退）
static uint32_t AttachNsAndCreateBlockDevice(const UbseSsuLinearSpaceReq &req,
                                             const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList,
                                             const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                             const UbseCreateBlockDeviceOptions &options,
                                             AttachNsCreateBlockDeviceOutput &output)
{
    std::string tag = (options.addressingType == UbseSsuAddressingType::STRIPED) ? "AttachStripedSpace" :
                                                                                   "AttachLinearSpace";

    std::vector<UbseSsuNameSpaceInfo> attachedNsList;
    output.nsDevPaths.clear();
    output.nsDevPaths.reserve(nameSpaceList.size());
    for (const auto &nsInfo : nameSpaceList) {
        auto ret = AttachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": AttachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            RollbackAttachedNsAndLedger(attachedNsList, req, devMap);
            return ret;
        }
        attachedNsList.push_back(nsInfo);
        output.nsDevPaths.push_back(nsInfo.nsDevPath);
    }

    auto createRet = UbseSsuAdapterInterface::GetInstance().CreateBlockDevice(req.devName, output.nsDevPaths, options,
                                                                              output.devPath);
    if (createRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": CreateBlockDevice failed, devName=" << req.devName << ", ret=" << createRet;
        RollbackAttachedNsAndLedger(attachedNsList, req, devMap);
        return UBSE_ERROR;
    }

    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHED, true);
    return UBSE_OK;
}

// 挂载线性空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachLinearSpace(const UbseSsuLinearSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                              std::string &devPath)
{
    UBSE_LOG_INFO << "AttachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachLinearSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性：已经attached，再次attach返回成功
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachLinearSpace: already attached, name=" << req.name;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_OK;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachLinearSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    // Master节点：本地有collector缓存，直接验证identity并attach
    // Agent节点：需通过RPC发送identity到master验证，获取字段后调用AttachDevNameSpace + CreateBlockDevice
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: failed to get node role, ret=" << roleRet;
        return UBSE_ERROR;
    }
    // 标记为attaching中
    if (UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHING, role == ELECTION_ROLE_MASTER) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_ERROR;
    }
    UbseCreateBlockDeviceOptions options;
    options.addressingType = UbseSsuAddressingType::LINEAR;
    if (role != ELECTION_ROLE_MASTER) {
        return AgentAttach(req, *entryPtr, req.devName, &options, nsDevPaths, devPath);
    }

    auto devMap = collector_.GetCachedDevMap();
    AttachNsCreateBlockDeviceOutput output;
    auto ret = AttachNsAndCreateBlockDevice(req, entryPtr->allocResult.nameSpaceList, devMap, options, output);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: AttachNsAndCreateBlockDevice failed, name=" << req.name
                       << ", ret=" << ret;
        return ret;
    }
    nsDevPaths = std::move(output.nsDevPaths);
    devPath = std::move(output.devPath);

    UBSE_LOG_INFO << "AttachLinearSpace success: name=" << req.name << ", devName=" << req.devName
                  << ", devPath=" << devPath;
    return UBSE_OK;
}

// 校验条带化场景下的参数
// 保证所有namespace大小一致，且为chunkSize的整数倍
static uint32_t ValidateStripedNsConfig(const UbseSsuStripedSpaceReq &req,
                                        const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList)
{
    // RAID5至少需要3个成员设备
    if (req.level == UbseSsuAggregationRaidLevel::RAID5 && nameSpaceList.size() < 3) {
        UBSE_LOG_ERROR << "AttachStripedSpace: RAID5 requires at least 3 namespaces, name=" << req.name
                       << ", nsCount=" << nameSpaceList.size();
        return UBSE_ERR_INVALID_ARG;
    }

    if (req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_4K && req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_16K &&
        req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_32K && req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_64K &&
        req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_128K && req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_256K &&
        req.chunkSize != UbseSsuChunkSize::CHUNK_SIZE_512K) {
        UBSE_LOG_ERROR << "AttachStripedSpace: invalid chunkSize, name=" << req.name
                       << ", chunkSize=" << static_cast<uint32_t>(req.chunkSize);
        return UBSE_ERR_INVALID_ARG;
    }
    const uint64_t chunkSizeBytes = static_cast<uint64_t>(req.chunkSize) * 1024; // chunkSize单位KB，转字节
    if (nameSpaceList.empty()) {
        UBSE_LOG_ERROR << "ValidateStripedNsConfig: empty nameSpaceList, name=" << req.name;
        return UBSE_ERROR;
    }
    const uint64_t firstNsSize = nameSpaceList.front().nsSize;
    for (const auto &nsInfo : nameSpaceList) {
        if (nsInfo.nsSize != firstNsSize) {
            UBSE_LOG_ERROR << "ValidateStripedNsConfig: namespace sizes not equal, name=" << req.name
                           << ", expected=" << firstNsSize << ", actual=" << nsInfo.nsSize;
            return UBSE_ERROR;
        }
        if (nsInfo.nsSize % chunkSizeBytes != 0) {
            UBSE_LOG_ERROR << "ValidateStripedNsConfig: namespace size not multiple of chunkSize, name=" << req.name
                           << ", nsSize=" << nsInfo.nsSize << ", chunkSize=" << static_cast<uint32_t>(req.chunkSize)
                           << "KB";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

// 挂载条带化空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachStripedSpace(const UbseSsuStripedSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                               std::string &devPath)
{
    UBSE_LOG_INFO << "AttachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName
                  << ", level=" << static_cast<int>(req.level)
                  << ", chunkSize=" << static_cast<uint32_t>(req.chunkSize);

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachStripedSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性：已经attached，再次attach返回成功
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachStripedSpace: already attached, name=" << req.name;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_OK;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachStripedSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    // 校验条带化场景下的参数
    auto ret = ValidateStripedNsConfig(req, entryPtr->allocResult.nameSpaceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: ValidateStripedNsConfig failed, name=" << req.name << ", ret=" << ret;
        return ret;
    }

    UbseCreateBlockDeviceOptions options = {.addressingType = UbseSsuAddressingType::STRIPED,
                                            .raidLevel = req.level == UbseSsuAggregationRaidLevel::RAID0 ?
                                                             UbseSsuRaidLevel::RAID0 :
                                                             UbseSsuRaidLevel::RAID5,
                                            .chunkSize = static_cast<uint32_t>(req.chunkSize)};

    // Master节点：本地有collector缓存，直接验证identity并attach
    // Agent节点：需通过RPC发送identity到master验证，获取字段后调用AttachDevNameSpace + CreateBlockDevice
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to get node role, ret=" << roleRet;
        return UBSE_ERROR;
    }
    // 标记为attaching中
    if (UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::ATTACHING, role == ELECTION_ROLE_MASTER) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_ERROR;
    }
    if (role != ELECTION_ROLE_MASTER) {
        return AgentAttach(req, *entryPtr, req.devName, &options, nsDevPaths, devPath);
    }

    auto devMap = collector_.GetCachedDevMap();
    AttachNsCreateBlockDeviceOutput output;
    ret = AttachNsAndCreateBlockDevice(req, entryPtr->allocResult.nameSpaceList, devMap, options, output);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: AttachNsAndCreateBlockDevice failed, name=" << req.name
                       << ", ret=" << ret;
        return ret;
    }
    nsDevPaths = std::move(output.nsDevPaths);
    devPath = std::move(output.devPath);

    UBSE_LOG_INFO << "AttachStripedSpace success: name=" << req.name << ", devName=" << req.devName
                  << ", devPath=" << devPath;
    return UBSE_OK;
}

// master端DetachLinearSpace和DetachStripedSpace通用辅助
// 删除块设备 + detach所有NS + 账本状态回退（ATTACHED -> CREATED）
static uint32_t DetachNsAndDeleteBlockDevice(const std::string &tag, const UbseSsuLinearSpaceReq &req,
                                             const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList,
                                             const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    auto deleteRet = UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(req.devName);
    if (deleteRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": DeleteBlockDevice failed, devName=" << req.devName << ", ret=" << deleteRet;
        return UBSE_ERROR;
    }

    uint32_t ret = UBSE_OK;
    for (const auto &nsInfo : nameSpaceList) {
        auto detachRet = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (detachRet != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            ret = UBSE_ERROR;
        }
    }
    if (ret != UBSE_OK) {
        // 部分NS detach失败：账本state刻意保持ATTACHED不变。
        // 1) DetachDevNameSpace幂等（见适配器契约），调用方可重试，已detach的NS会再次返回成功，重试收敛；
        //    DeleteBlockDevice同样幂等，重试时不会因块设备已删而失败
        // 2) ExecuteFree要求state==CREATED才允许释放NS，保持ATTACHED可阻止在仍有NS挂在host上时误删NS，
        //    也可阻止在"块设备已删"的脏状态上误调Attach*重建聚合设备
        UBSE_LOG_ERROR << tag << ": failed to detach all namespaces, name=" << req.name;
        return UBSE_ERROR;
    }

    // 更新本地账本，非主节点同步通知master
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": failed to get node role for status update, ret=" << roleRet;
        return UBSE_ERROR;
    }
    UpdateLedgerStateAndNotify(req.name, UbseSsuNsState::CREATED, role == ELECTION_ROLE_MASTER);
    return UBSE_OK;
}

// detach线性空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachLinearSpace(const UbseSsuLinearSpaceReq &req)
{
    UBSE_LOG_INFO << "DetachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachLinearSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性 已经detach的NS，再次detach返回成功
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_INFO << "DetachLinearSpace: no need to detach, name=" << req.name;
        return UBSE_OK;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachLinearSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    // Master节点：本地有collector缓存，直接验证identity并detach
    // Agent节点：需通过RPC发送identity到master验证，获取字段后调用DetachDevNameSpace
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachLinearSpace: failed to get node role, ret=" << roleRet;
        return UBSE_ERROR;
    }
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, *entryPtr, req.devName, true);
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndDeleteBlockDevice("DetachLinearSpace", req, entryPtr->allocResult.nameSpaceList, devMap);
    if (ret != UBSE_OK) {
        return ret;
    }

    UBSE_LOG_INFO << "DetachLinearSpace success: name=" << req.name << ", devName=" << req.devName;
    return UBSE_OK;
}

// 卸载条带化空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachStripedSpace(const UbseSsuStripedSpaceReq &req)
{
    UBSE_LOG_INFO << "DetachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachStripedSpace: record not found, name=" << req.name;
        return UBSE_ERROR;
    }

    // 幂等性 已经detach的NS，再次detach返回成功
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_INFO << "DetachStripedSpace: no need to detach, name=" << req.name;
        return UBSE_OK;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachStripedSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    // Master节点：本地有collector缓存，直接验证identity并detach
    // Agent节点：需通过RPC发送identity到master验证，获取字段后调用DetachDevNameSpace
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachStripedSpace: failed to get node role, ret=" << roleRet;
        return UBSE_ERROR;
    }
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, *entryPtr, req.devName, true);
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndDeleteBlockDevice("DetachStripedSpace", req, entryPtr->allocResult.nameSpaceList, devMap);
    if (ret != UBSE_OK) {
        return ret;
    }

    UBSE_LOG_INFO << "DetachStripedSpace success: name=" << req.name << ", devName=" << req.devName;
    return UBSE_OK;
}

// agent端发送SSU访问权限RPC请求到master节点（添加/移除共用，通过opCode区分）
static uint32_t SendPermRpcRequest(const UbseSsuPermReq &permReq, const std::string &masterNodeId, uint16_t opCode)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU), opCode);
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendPermRpcRequest: get ssu perm req endpoint failed, requestId=" << permReq.requestId;
        return UBSE_ERROR;
    }

    UbseSsuPermReqMsg reqMsg(permReq.requestId, permReq.requestNodeId, permReq.name, permReq.nqn, permReq.identityInfo);

    UbseSsuSyncRespMsg respMsg; // 占位，后续通过异步线程返回真正结果
    auto ret = endpoint->UbseRpcSend(masterNodeId, reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendPermRpcRequest: RpcSend failed, " << FormatRetCode(ret)
                       << ", requestId=" << permReq.requestId;
        return ret;
    }

    auto errorCode = respMsg.GetErrorCode();
    if (errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "SendPermRpcRequest: failed, requestId=" << permReq.requestId << ", errorCode=" << errorCode;
        return errorCode;
    }

    return UBSE_OK;
}

// agent端通过RPC向master添加/移除SSU访问权限
static uint32_t AccessPermissionViaRpc(const std::string &name, const std::string &nqn,
                                       const UbseSsuAllocIdentityInfo &identity, bool isAdd)
{
    std::string funcName = isAdd ? "AddAccessPermissionViaRpc" : "RemoveAccessPermissionViaRpc";
    UBSE_LOG_INFO << funcName << ": name=" << name << ", nqn=" << nqn;
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << funcName << ": get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    UbseRoleInfo roleInfo{};
    ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << funcName << ": get current node info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = (isAdd ? "addperm_" : "removeperm_") + name + "_" + roleInfo.nodeId;
    auto respMgr = UbseFutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << funcName << ": requestId=" << requestId << " create future instance failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto respFuture = respMgr->GetFuture<UbseSsuPermResp>();

    UbseSsuPermReq permReq = {
        .requestId = requestId,
        .requestNodeId = roleInfo.nodeId,
        .name = name,
        .nqn = nqn,
        .identityInfo = identity,
    };

    auto opCode = isAdd ? UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_REQ :
                          UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_REQ;
    ret = SendPermRpcRequest(permReq, masterInfo.nodeId, static_cast<uint16_t>(opCode));
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << funcName << ": SendPermRpcRequest failed, " << FormatRetCode(ret)
                       << ", requestId=" << requestId;
        return ret;
    }

    if (respFuture.wait_for(std::chrono::seconds(MAX_TIMEOUT_SECONDS)) != std::future_status::ready) {
        UBSE_LOG_ERROR << funcName << ": timeout waiting for response, requestId=" << requestId;
        return UBSE_ERR_TIMED_OUT;
    }

    auto resp = respFuture.get();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << funcName << ": failed, requestId=" << requestId << ", errorCode=" << resp.errorCode;
        return resp.errorCode;
    }

    UBSE_LOG_INFO << funcName << " success: name=" << name << ", nqn=" << nqn;
    return UBSE_OK;
}

struct SucceededPermNs {
    std::string eid;
    uint32_t nsId;
    std::string nqn;
};

// 回滚已成功的权限操作。isAdd表示原始操作类型，回滚时执行反向操作。
// 移除权限失败时不回滚（权限只减不增，重试可幂等收敛），仅添加权限失败时需要回滚。
// 底层接口已保证幂等性，回滚失败仅记录日志，不影响原始错误返回。
static void RollbackPermOperations(const std::vector<SucceededPermNs> &succeededList, bool isAdd,
                                   const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    if (!isAdd) {
        // 移除权限不执行回滚：已移除的NS重试时幂等跳过，失败的NS重试时继续执行，
        // 调用方重试整个RemoveAccessPermission即可收敛到一致状态（无需手动补偿）
        UBSE_LOG_WARN << "RollbackPermOperations: skip rollback for remove operation, "
                      << "succeededList.size=" << succeededList.size() << ", caller can retry to converge";
        return;
    }
    for (const auto &item : succeededList) {
        auto targetNs = FindNsInDevice(devMap, item.eid, item.nsId);
        if (targetNs == nullptr) {
            UBSE_LOG_WARN << "RollbackPermOperations: namespace not found, eid=" << item.eid << ", nsId=" << item.nsId
                          << ", skip";
            continue;
        }
        auto ret = UbseSsuAdapterInterface::GetInstance().RemoveNameSpaceAllowHost(*targetNs, item.nqn);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "RollbackPermOperations: rollback RemoveNameSpaceAllowHost failed, eid=" << item.eid
                          << ", nsId=" << item.nsId << ", nqn=" << item.nqn << ", ret=" << ret;
        }
    }
}

uint32_t UbseSsuServiceImp::ExecuteAccessPermission(const std::string &name, const std::string &nqn,
                                                    const UbseSsuAllocIdentityInfo &identity, bool isAdd)
{
    const std::string funcName = isAdd ? "ExecuteAddAccessPermission" : "ExecuteRemoveAccessPermission";
    UBSE_LOG_INFO << funcName << ": name=" << name << ", nqn=" << nqn;

    auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << funcName << ": record not found, name=" << name;
        return UBSE_ERROR;
    }

    // 命名空间必须已创建完成，CREATING状态下NS可能尚未落盘，IDLE表示初始/失败回退
    if (entryPtr->state == UbseSsuNsState::IDLE || entryPtr->state == UbseSsuNsState::CREATING) {
        UBSE_LOG_ERROR << funcName << ": invalid state, name=" << name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_ERROR;
    }

    auto devMap = collector_.GetCachedDevMap();
    // 记录已成功操作的NS信息，用于失败时回滚
    std::vector<SucceededPermNs> succeededNsList;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.namespaceId);
        if (targetNs == nullptr) {
            if (isAdd) {
                UBSE_LOG_ERROR << funcName << ": namespace not found in cache, eid=" << nsInfo.tgtEid
                               << ", nsId=" << nsInfo.namespaceId;
                RollbackPermOperations(succeededNsList, isAdd, devMap);
                return UBSE_ERROR;
            }
            // NS不在缓存中，说明已被删除，权限随之失效，视为幂等成功跳过
            UBSE_LOG_INFO << funcName << ": namespace not found in cache, skip, eid=" << nsInfo.tgtEid
                          << ", nsId=" << nsInfo.namespaceId;
            continue;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << funcName << ": identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId << ", uid=" << identity.uid
                           << ", userName=" << identity.userName;
            RollbackPermOperations(succeededNsList, isAdd, devMap);
            return UBSE_ERR_ACCESS_DENIED;
        }
        auto nqnFinal = ResolveNqn(nqn, targetNs->customData.defaultNqn);
        // 为默认NQN时，无需添加/移除权限
        if (nqnFinal == targetNs->customData.defaultNqn) {
            continue;
        }
        uint32_t ret = UBSE_OK;
        if (isAdd) {
            ret = UbseSsuAdapterInterface::GetInstance().AddNameSpaceAllowHost(*targetNs, nqnFinal);
        } else {
            ret = UbseSsuAdapterInterface::GetInstance().RemoveNameSpaceAllowHost(*targetNs, nqnFinal);
        }
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << funcName << ": " << (isAdd ? "AddNameSpaceAllowHost" : "RemoveNameSpaceAllowHost")
                           << " failed, eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId
                           << ", nqn=" << nqnFinal << ", ret=" << ret;
            RollbackPermOperations(succeededNsList, isAdd, devMap);
            return ret;
        }
        // 记录成功操作，用于后续回滚
        succeededNsList.push_back({nsInfo.tgtEid, nsInfo.namespaceId, nqnFinal});
    }

    UBSE_LOG_INFO << funcName << " success: name=" << name << ", nqn=" << nqn;
    return UBSE_OK;
}

uint32_t UbseSsuServiceImp::AddAccessPermission(const std::string &name, const std::string &nqn,
                                                const UbseSsuAllocIdentityInfo &identity)
{
    UBSE_LOG_INFO << "AddAccessPermission: name=" << name << ", nqn=" << nqn;

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AddAccessPermission: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteAccessPermission(name, nqn, identity, true);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return AccessPermissionViaRpc(name, nqn, identity, true);
    }

    UBSE_LOG_ERROR << "AddAccessPermission: unsupported node role=" << role;
    return UBSE_ERROR;
}

uint32_t UbseSsuServiceImp::RemoveAccessPermission(const std::string &name, const std::string &nqn,
                                                   const UbseSsuAllocIdentityInfo &identity)
{
    UBSE_LOG_INFO << "RemoveAccessPermission: name=" << name << ", nqn=" << nqn;

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RemoveAccessPermission: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteAccessPermission(name, nqn, identity, false);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return AccessPermissionViaRpc(name, nqn, identity, false);
    }

    UBSE_LOG_ERROR << "RemoveAccessPermission: unsupported node role=" << role;
    return UBSE_ERROR;
}

uint32_t UbseSsuServiceImp::GetFeDeviceList(std::vector<UbseSsuFe> &feList)
{
    return UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList);
}

uint32_t UbseSsuServiceImp::FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid)
{
    return UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(upi, vfe, busInstanceGuid);
}

uint32_t UbseSsuServiceImp::FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe, const std::string &busInstanceGuid)
{
    return UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(upi, vfe, busInstanceGuid);
}
} // namespace ubse::ssu::service
