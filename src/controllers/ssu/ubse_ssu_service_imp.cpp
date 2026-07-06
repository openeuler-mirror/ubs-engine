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
#include "message/ubse_ssu_sync_resp_msg.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"
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
constexpr uint32_t MAX_TIMEOUT_SECONDS = 1800;

// 获取SSU服务单例实例
UbseSsuServiceImp &UbseSsuServiceImp::GetInstance()
{
    static UbseSsuServiceImp instance;
    return instance;
}

// 启动设备收集器，开始定期更新设备状态
uint32_t UbseSsuServiceImp::StartCollecting()
{
    auto ret = collector_.Start();
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
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
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
                         .totalBytes = req.nsNum * nsSize};
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
    // 账本条目由调用方HandleAllocReqReceiver在调用前以CREATING状态 Put创建，
    // 此处仅返回结果，由调用方通过Modify 更新为CREATED状态，避免重复Put覆盖。
    result.name = req.name;
    result.strategy = req.strategy;
    UBSE_LOG_INFO << "ExecuteAlloc success: name=" << req.name << ", nsCount=" << result.nameSpaceList.size();
    return UBSE_OK;
}
} // namespace ubse::ssu::service
