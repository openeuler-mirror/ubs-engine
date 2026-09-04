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
#include <unordered_set>
#include "debt/ubse_ssu_debt_ledger.h"
#include "framework/misc/ubse_future_mgr.h"
#include "framework/misc/ubse_logging_lock_guard.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
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

// 分配总大小的对齐粒度：1GiB
constexpr uint64_t ONE_GIB = 1024ULL * 1024ULL * 1024ULL;

// 聚合块设备根目录
constexpr const char *SSU_AGGREGATE_DEV_ROOT = "/dev/ssu/";

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

    // 首次采集失败（缓存为空）时，由定时器采集成功后触发账本重建
    collector_.SetOnFirstCollectFailRecovery([this]() { RebuildLedgerFromDevList(); });

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

// 从设备缓存列表重建SSU账本，仅master端有效
void UbseSsuServiceImp::RebuildLedgerFromDevList()
{
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK || role != ELECTION_ROLE_MASTER) {
        return;
    }
    auto devList = collector_.GetCachedDevList();
    if (devList.empty()) {
        // 设备列表为空时无从重建账本（Rebuild为空操作，跳过以避免误导性日志）
        UBSE_LOG_WARN << "RebuildLedgerFromDevList: dev list is empty, skip ledger rebuild";
        return;
    }
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
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
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

    auto requestId = "alloc_" + req.name + "_" + roleInfo.nodeId;
    std::shared_ptr<UbseFutureMgr> respMgr;
    std::future<UbseSsuAllocResp> respFuture;

    respMgr = UbseFutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: requestId=" << requestId << " create future instance failed";
        return UBSE_ERROR_NULLPTR;
    }
    respFuture = respMgr->GetFuture<UbseSsuAllocResp>();

    ret = SendAllocRpcRequest(req, identity, roleInfo.nodeId, masterInfo.nodeId, requestId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: SendAllocRpcRequest failed, " << FormatRetCode(ret)
                       << ", requestId=" << requestId;
        return ret;
    }

    if (respFuture.wait_for(std::chrono::seconds(MAX_TIMEOUT_SECONDS)) != std::future_status::ready) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: timeout waiting for response, requestId=" << requestId;
        // 超时后通知master清理，本地无账本无需删除
        UbseSsuServiceImp::GetInstance().FreeSpace(req.name, identity);
        return UBSE_ERR_TIMED_OUT;
    }

    auto resp = respFuture.get();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpaceViaRpc: alloc phase1 failed, requestId=" << requestId
                       << ", errorCode=" << resp.errorCode << ", state=" << static_cast<int>(resp.state);
        return resp.errorCode;
    }

    result = resp.allocResult;
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
    return UBSE_SSU_ERROR_ROLE_INVALID;
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
        return UBSE_SSU_ERROR_NO_DEVICE;
    }

    // 分配层：STRIPED采用均分（聚合块设备RAID成员需容量一致）；LINEAR采用权重负载均衡
    // （各NS按设备剩余空间比例分配）；NORMAL允许同设备多NS，按均分贪心放置（独立裸设备直接读写、不聚合）
    UbseSsuAllocRequest schedReq = {
        .allocSize = req.nsSize,
        .nsNum = req.nsNum,
        .lbaSize = static_cast<uint32_t>(req.lbaFormat),
        .strategy = req.strategy,
        .tenant = req.tenant,
    };

    // 分配总大小必须是1GiB的整数倍（SSU服务API的容量粒度契约）
    // nsNum非0、条带化整除、singleNsSize按lbaSize对齐等调度器不变量校验由PreCheckHandler负责
    if (schedReq.allocSize % ONE_GIB != 0) {
        UBSE_LOG_ERROR << "ExecuteScheduler: allocSize is not multiple of 1GiB, allocSize=" << schedReq.allocSize;
        return UBSE_ERR_INVALID_ARG;
    }

    UbseSsuAllocationContext ctx(devList, schedReq);
    auto allocRet = scheduler_.Execute(ctx);
    if (allocRet != UbseSsuAllocRetCode::OK) {
        UBSE_LOG_ERROR << "Scheduler allocation failed, ret=" << static_cast<int>(allocRet)
                       << ", msg=" << ctx.result.msg;
        if (allocRet == UbseSsuAllocRetCode::INSUFFICIENT_SPACE) {
            return UBSE_SSU_ERROR_INSUFFICIENT_SPACE;
        }
        if (allocRet == UbseSsuAllocRetCode::INVALID_PARAM) {
            return UBSE_ERR_INVALID_ARG;
        }
        return UBSE_SSU_ERROR_SCHEDULER_FAILED;
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
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        eidToSubNqn[eid] = it->second->subSystem.subNqn;
    }
    return UBSE_OK;
}

// 填充命名空间自定义数据：名称、用户、租户、默认NQN
static uint32_t FillNsCustomData(UbseSsuDevNameSpace &ns, const UbseSsuAllocSpaceReq &req,
                                 const UbseSsuAllocIdentityInfo &identity, const std::string &hostNqn)
{
    memset_s(ns.customData.name, sizeof(ns.customData.name), 0, sizeof(ns.customData.name));
    if (strncpy_s(ns.customData.name, sizeof(ns.customData.name), req.name.c_str(), req.name.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy name, name=" << req.name;
        return UBSE_SSU_ERROR_NS_CUSTOM_DATA_INVALID;
    }
    memset_s(ns.customData.userName, sizeof(ns.customData.userName), 0, sizeof(ns.customData.userName));
    if (strncpy_s(ns.customData.userName, sizeof(ns.customData.userName), identity.userName.c_str(),
                  identity.userName.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy userName, userName=" << identity.userName;
        return UBSE_SSU_ERROR_NS_CUSTOM_DATA_INVALID;
    }
    memset_s(ns.customData.tenant, sizeof(ns.customData.tenant), 0, sizeof(ns.customData.tenant));
    if (!req.tenant.empty()) {
        if (strncpy_s(ns.customData.tenant, sizeof(ns.customData.tenant), req.tenant.c_str(), req.tenant.size()) !=
            EOK) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy tenant, tenant=" << req.tenant;
            return UBSE_SSU_ERROR_NS_CUSTOM_DATA_INVALID;
        }
    }
    memset_s(ns.customData.defaultNqn, sizeof(ns.customData.defaultNqn), 0, sizeof(ns.customData.defaultNqn));
    if (strncpy_s(ns.customData.defaultNqn, sizeof(ns.customData.defaultNqn), hostNqn.c_str(), hostNqn.size()) != EOK) {
        UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to copy defaultNqn, defaultNqn=" << hostNqn;
        return UBSE_SSU_ERROR_NS_CUSTOM_DATA_INVALID;
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
    // 同一name下的所有namespace共享同一个hostNqn（defaultNqn），只需生成一次
    std::string hostNqn = GenerateHostNqn();
    for (const auto &[eid, nsSize] : eidNsSizeList) {
        auto subNqnIt = eidToSubNqn.find(eid);
        if (subNqnIt == eidToSubNqn.end()) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to find subNqn for eid=" << eid;
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        UbseSsuDevNameSpace ns;
        ns.subSystem = {.eid = eid, .subNqn = subNqnIt->second};
        // nsze/ncap单位为LBA数量（NVMe规范），需将字节容量除以LBA大小（512B/4K）
        const uint64_t lbaSize = static_cast<uint64_t>(req.lbaFormat);
        ns.nsze = nsSize / lbaSize;
        ns.ncap = nsSize / lbaSize;
        // LBA格式4K，对应flbas=1; LBA格式512B，对应flbas=0
        ns.nsOptions.flbas = (req.lbaFormat == UbseSsuLBAFormat::LBA_FORMAT_4K) ? 1 : 0;
        ns.customData = {.uid = identity.uid,
                         .allocStrategy = static_cast<uint8_t>(req.strategy),
                         .nsNum = static_cast<uint8_t>(req.nsNum),
                         .totalBytes = req.nsSize};
        // hostNqn同时写入customData.defaultNqn(持久化到硬件)与nsInfo.allowHostNqnList(API响应)
        if (FillNsCustomData(ns, req, identity, hostNqn) != UBSE_OK) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to fill custom data, eid=" << eid;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_SSU_ERROR_NS_CUSTOM_DATA_INVALID;
        }
        auto createRet = UbseSsuAdapterInterface::GetInstance().CreateDevNameSpace(ns);
        if (createRet != UBSE_OK) {
            UBSE_LOG_ERROR << "CreateDevNameSpace failed, eid=" << eid << ", ret=" << createRet;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_SSU_ERROR_NS_CREATE_FAILED;
        }
        createdNsList.push_back(ns);
        // 创建成功后为NS添加defaultNqn的访问权限，保证该NS可被默认主机访问
        auto allowRet = UbseSsuAdapterInterface::GetInstance().AddNameSpaceAllowHost(ns, hostNqn);
        if (allowRet != UBSE_OK) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: AddNameSpaceAllowHost failed, eid=" << eid
                           << ", nsId=" << ns.namespaceId << ", hostNqn=" << hostNqn << ", ret=" << allowRet;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_SSU_ERROR_PERMISSION_ADD_FAILED;
        }
        auto uuidStr = StrToUuid(ns.uuid);
        if (uuidStr.empty()) {
            UBSE_LOG_ERROR << "CreateDevNameSpaces: failed to convert uuid to standard format, uuid=" << ns.uuid;
            RollbackCreatedNameSpaces(createdNsList);
            return UBSE_SSU_ERROR_NS_UUID_INVALID;
        }
        auto persistentPath = std::string("/dev/disk/by-id/nvme-uuid.") + uuidStr;
        UbseSsuNameSpaceInfo nsInfo = {.tgtEid = ns.subSystem.eid,
                                       .tgtNqn = ns.subSystem.subNqn,
                                       .nsUuid = uuidStr,
                                       .namespaceId = ns.namespaceId,
                                       .nsDevPath = persistentPath,
                                       .nsSize = nsSize,
                                       .lbaFormat = req.lbaFormat,
                                       .allowHostNqnList = {hostNqn}};
        result.nameSpaceList.push_back(nsInfo);
    }
    // 校验实际创建的namespace数量是否等于请求的数量
    if (result.nameSpaceList.size() != req.nsNum) {
        UBSE_LOG_ERROR << "ExecuteAlloc: scheduler return nsNum=" << result.nameSpaceList.size()
                       << " not equal req.nsNum=" << req.nsNum;
        RollbackCreatedNameSpaces(createdNsList);
        return UBSE_SSU_ERROR_NS_COUNT_MISMATCH;
    }
    return UBSE_OK;
}

uint32_t UbseSsuServiceImp::ExecuteAlloc(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                         UbseSsuAllocResult &result)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "ExecuteAlloc: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    UBSE_LOG_INFO << "ExecuteAlloc: name=" << req.name << ", nsSize=" << req.nsSize << ", nsNum=" << req.nsNum;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    auto existingEntry = UbseSsuDebtLedger::GetInstance().Get(req.name);
    // 已存在非CREATING状态条目（CREATED/ATTACHING/ATTACHED等）时，说明该name已被占用
    // CREATED为已分配待挂载、ATTACHING为挂载进行中、ATTACHED为已挂载，此时再收到alloc请求即属于重复分配，一律拒绝
    // CREATING是agent RPC路径预置的在途分配标记，需放行由本函数继续完成分配
    if (existingEntry != nullptr && existingEntry->state != UbseSsuNsState::CREATING) {
        UBSE_LOG_WARN << "ExecuteAlloc: ledger entry already exists, name=" << req.name
                      << ", state=" << static_cast<int>(existingEntry->state);
        // 重复分配一律报错
        return UBSE_ERR_ALREADY_ALLOCATED;
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
    auto buildRet = BuildEidToSubNqnMap(eidNsSizeList, devMap, eidToSubNqn);
    if (buildRet != UBSE_OK) {
        UBSE_LOG_ERROR << "ExecuteAlloc: failed to build eidToSubNqn map";
        collector_.ReleaseReservation(eidNsSizeList);
        collector_.OnReserveEnd();
        return buildRet;
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
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
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

    auto requestId = "free_" + name + "_" + roleInfo.nodeId;

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
    if (name.empty()) {
        UBSE_LOG_ERROR << "FreeSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeSpace: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        // agent无本地账本，直接通过RPC通知master释放
        return FreeSpaceViaRpc(name, identity);
    }

    // master端：账本判空与重复释放检查由ExecuteFree统一处理
    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteFree(name, identity);
    }

    UBSE_LOG_ERROR << "FreeSpace: unsupported node role=" << role;
    return UBSE_SSU_ERROR_ROLE_INVALID;
}

// 查找设备中的命名空间，仅按 uuid 匹配
// uuid 是 NVMe 命名空间的持久化唯一标识；nsid 在删除后可能被复用，不能作为可靠匹配依据。
// 调用方需保证 uuidStr 来自权威数据源（账本），非空。
static const UbseSsuDevNameSpace *FindNsInDevice(const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
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

// 缓存未命中时从硬件实时刷新指定eid的设备缓存
// 用于刚分配的namespace尚未被定时采集（30s间隔）覆盖的场景
static void RefreshDevCache(const std::string &eid, const std::string &subNqn,
                            std::unordered_set<std::string> &refreshedEids,
                            std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    if (refreshedEids.find(eid) != refreshedEids.end()) {
        return;
    }
    refreshedEids.insert(eid);
    std::vector<UbseSsuDevInfo> freshDevList(1);
    freshDevList[0].subSystem.eid = eid;
    freshDevList[0].subSystem.subNqn = subNqn;
    auto ret = UbseSsuAdapterInterface::GetInstance().GetDevList(freshDevList);
    if (ret == UBSE_OK && !freshDevList.empty()) {
        devMap[eid] = std::make_shared<const UbseSsuDevInfo>(std::move(freshDevList[0]));
        UBSE_LOG_INFO << "RefreshDevCache: refresh dev cache success, eid=" << eid;
    } else {
        UBSE_LOG_WARN << "RefreshDevCache: refresh dev from hardware failed, eid=" << eid
                      << ", ret=" << ret;
    }
}

// 执行释放操作：校验命名空间身份并删除命名空间
uint32_t UbseSsuServiceImp::ExecuteFree(const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "ExecuteFree: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    UBSE_LOG_INFO << "ExecuteFree: name=" << name;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "ExecuteFree: record not found, name=" << name;
        // 释放不存在的空间，重复释放一律报错
        return UBSE_ERR_NO_NEED_FREE;
    }

    if (entryPtr->state != UbseSsuNsState::CREATED) {
        if (entryPtr->state == UbseSsuNsState::ATTACHED) {
            UBSE_LOG_WARN << "ExecuteFree: namespace is attached, need detach first, name=" << name;
            return UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE;
        }
        UBSE_LOG_WARN << "ExecuteFree: invalid state, name=" << name << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t firstErr = UBSE_OK;
    std::vector<UbseSsuNameSpaceInfo> remainingNs; // 未删成功的namespace，保留在账本中便于上层重试
    std::vector<std::pair<std::string, uint64_t>> releasedCapacity;
    std::unordered_set<std::string> refreshedEids;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        // 按 uuid 匹配：缓存可能滞后于硬件（nsid 复用重建时 guid/uuid 已变化），
        // uuid 一致才命中，避免拿过期缓存条目的 guid 去执行删除
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            // 缓存未命中（含 uuid 不匹配）：从硬件实时刷新（刚分配的ns可能尚未被定时采集覆盖）
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            UBSE_LOG_INFO << "ExecuteFree: refresh dev cache, eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            // 刷新后仍查不到，才认定NS已不在硬件中（可能已被删除），视为已释放，跳过
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
static uint32_t SendStatusUpdate(const std::string &requestName, UbseSsuNsState state, const std::string &devName)
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

    UbseSsuStatusReqMsg reqMsg(requestName, state, devName);
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

// 更改进程状态：master端更新本地账本状态，agent端仅发送状态通知给master
// devName：聚合块设备名称（Linear/Striped场景），attach成功时携带，master端写入账本；通用场景为空
static uint32_t UpdateStateOrNotify(const std::string &name, UbseSsuNsState state, bool isMaster,
                                    const std::string &devName = "")
{
    if (isMaster) {
        UbseSsuNsState oldState;
        if (!UbseSsuDebtLedger::GetInstance().Modify(name, [&state, &oldState, &devName](UbseSsuLedgerEntry &e) {
                oldState = e.state;
                e.state = state;
                // 进入CREATED（detach成功或attach失败回退）说明聚合块设备已删除/未创建成功，
                if (state == UbseSsuNsState::CREATED) {
                    e.devName.clear();
                } else if (!devName.empty()) {
                    // 非空才覆盖（attach成功时携带），保留历史devName供重复挂载回填devPath
                    e.devName = devName;
                }
            })) {
            UBSE_LOG_ERROR << "UpdateStateOrNotify: ledger entry not found, name=" << name
                           << ", state=" << static_cast<int>(state);
            return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
        }
        return UBSE_OK;
    }

    // agent侧：仅发送状态更新，不维护本地账本
    auto ret = SendStatusUpdate(name, state, devName);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "UpdateStateOrNotify: SendStatusUpdate failed, name=" << name
                      << ", state=" << static_cast<int>(state) << ", ret=" << ret;
        return ret;
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
        return UBSE_ERR_INVALID_ARG;
    }
    auto opRet = isAttach ? UbseSsuAdapterInterface::GetInstance().AttachDevNameSpace(nqnFinal, nameSpace) :
                            UbseSsuAdapterInterface::GetInstance().DetachDevNameSpace(nqnFinal, nameSpace);
    if (opRet != UBSE_OK) {
        UBSE_LOG_ERROR << (isAttach ? "AttachDevNameSpace" : "DetachDevNameSpace") << " failed, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId << ", ret=" << opRet;
        return isAttach ? UBSE_SSU_ERROR_ATTACH_FAILED : UBSE_SSU_ERROR_DETACH_FAILED;
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

// agent端：发送attach/detach前置条件校验请求到master并同步等待响应（attach/detach通用）。
// 校验内容包括identity、账本state、条带化参数，master端为纯内存读取，通过sync resp直接返回结果
// option携带attach/detach意图与条带化参数（master端据此校验state与条带化配置）
static uint32_t VerifyAttachDetachPreconditionViaRpc(const std::string &name,
                                                     const UbseSsuAllocIdentityInfo &identity,
                                                     const UbseSsuAttachDetachVerifyOption &option,
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

    auto requestId = "AttachDetachVerify_" + name + "_" + roleInfo.nodeId;

    auto endpoint =
        UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                               static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ATTACH_DETACH_VERIFY_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "VerifyIdViaRpc: get endpoint failed, requestId=" << requestId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuAttachDetachVerifyReqMsg reqMsg(requestId, roleInfo.nodeId, name, identity, option);
    UbseSsuAttachDetachVerifyRespMsg syncResp;
    ret = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VerifyIdViaRpc: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }

    verifyResp = syncResp.GetAttachDetachVerifyResp();
    if (verifyResp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "VerifyIdViaRpc: identity verify failed, requestId=" << requestId
                       << ", errorCode=" << verifyResp.errorCode << ", name=" << name;
        return verifyResp.errorCode;
    }
    return UBSE_OK;
}

// agent端回滚已attach的NS的前向声明（实现在文件后部，供master/agent共用）
static uint32_t RollbackAttachedNsAndLedger(const std::vector<UbseSsuNameSpaceInfo> &attachedNsList,
                                            const UbseSsuSpaceReq &req,
                                            std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                            const UbseSsuAttachDetachVerifyResp *verifyResp = nullptr);

// 校验条带化场景下的参数
// 保证所有namespace大小一致，且为chunkSize的整数倍
// Master本地AttachStripedSpace与verify RPC（agent attach）共用
static uint32_t ValidateStripedNsConfig(const std::string &name, UbseSsuAggregationRaidLevel level,
                                        UbseSsuChunkSize chunkSize,
                                        const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList)
{
    // 校验 RAID 级别枚举合法性
    if (level != UbseSsuAggregationRaidLevel::RAID0 && level != UbseSsuAggregationRaidLevel::RAID5) {
        UBSE_LOG_ERROR << "ValidateStripedNsConfig: invalid raid level, name=" << name
                       << ", level=" << static_cast<uint32_t>(level);
        return UBSE_ERR_INVALID_ARG;
    }
    // RAID5至少需要3个成员设备
    if (level == UbseSsuAggregationRaidLevel::RAID5 && nameSpaceList.size() < 3 ||
        level == UbseSsuAggregationRaidLevel::RAID0 && nameSpaceList.size() < 2) {
        UBSE_LOG_ERROR << "ValidateStripedNsConfig: "
                       << (level == UbseSsuAggregationRaidLevel::RAID5 ? "RAID5 requires at least 3 namespaces" :
                                                                         "RAID0 requires at least 2 namespaces")
                       << ", name=" << name << ", nsCount=" << nameSpaceList.size();
        return UBSE_ERR_INVALID_ARG;
    }

    if (chunkSize != UbseSsuChunkSize::CHUNK_SIZE_4K && chunkSize != UbseSsuChunkSize::CHUNK_SIZE_16K &&
        chunkSize != UbseSsuChunkSize::CHUNK_SIZE_32K && chunkSize != UbseSsuChunkSize::CHUNK_SIZE_64K &&
        chunkSize != UbseSsuChunkSize::CHUNK_SIZE_128K && chunkSize != UbseSsuChunkSize::CHUNK_SIZE_256K &&
        chunkSize != UbseSsuChunkSize::CHUNK_SIZE_512K) {
        UBSE_LOG_ERROR << "ValidateStripedNsConfig: invalid chunkSize, name=" << name
                       << ", chunkSize=" << static_cast<uint32_t>(chunkSize);
        return UBSE_ERR_INVALID_ARG;
    }
    
    const uint64_t chunkSizeBytes = static_cast<uint64_t>(chunkSize) * 1024; // chunkSize单位KB，转字节
    if (nameSpaceList.empty()) {
        UBSE_LOG_ERROR << "ValidateStripedNsConfig: empty nameSpaceList, name=" << name;
        return UBSE_SSU_ERROR_STRIPED_CONFIG_INVALID;
    }
    const uint64_t firstNsSize = nameSpaceList.front().nsSize;
    for (const auto &nsInfo : nameSpaceList) {
        if (nsInfo.nsSize != firstNsSize) {
            UBSE_LOG_ERROR << "ValidateStripedNsConfig: namespace sizes not equal, name=" << name
                           << ", expected=" << firstNsSize << ", actual=" << nsInfo.nsSize;
            return UBSE_SSU_ERROR_STRIPED_CONFIG_INVALID;
        }
        if (nsInfo.nsSize % chunkSizeBytes != 0) {
            UBSE_LOG_ERROR << "ValidateStripedNsConfig: namespace size not multiple of chunkSize, name=" << name
                           << ", nsSize=" << nsInfo.nsSize << ", chunkSize=" << static_cast<uint32_t>(chunkSize)
                           << "KB";
            return UBSE_SSU_ERROR_STRIPED_CONFIG_INVALID;
        }
    }
    return UBSE_OK;
}

// 校验分配策略与挂载/卸载策略是否匹配：条带化分配只能通过AttachStripedSpace挂载、DetachStripedSpace卸载，
// 线性分配只能通过AttachLinearSpace/DetachLinearSpace，避免编址方式与分配策略不一致
static uint32_t ValidateAllocStrategyMatch(const std::string &name, UbseSsuAllocStrategy allocStrategy,
                                           UbseSsuAllocStrategy expectedStrategy)
{
    // 分配策略与挂载/卸载方式必须严格一一对应，禁止混用：
    // NORMAL对应AttachSpace/DetachSpace
    // LINEAR对应AttachLinearSpace/DetachLinearSpace
    // STRIPED对应AttachStripedSpace/DetachStripedSpace
    if (allocStrategy != expectedStrategy) {
        UBSE_LOG_ERROR << "ValidateAllocStrategyMatch: alloc strategy mismatch, name=" << name
                       << ", allocStrategy=" << static_cast<int>(allocStrategy)
                       << ", expectedStrategy=" << static_cast<int>(expectedStrategy);
        return UBSE_SSU_ERROR_STRATEGY_MISMATCH;
    }
    return UBSE_OK;
}

// agent端通用attach主流程：发送identity验证请求到master，从响应中获取namespace列表，
// 验证成功后逐个attach NS。可选创建聚合块设备（blockDeviceOptions非空时，Linear/Striped场景）。
// 失败时回滚已attach的NS；成功时通过RPC通知master更新状态。
static uint32_t AgentAttach(const UbseSsuSpaceReq &req, const std::string &devName,
                            const UbseCreateBlockDeviceOptions *blockDeviceOptions,
                            std::vector<std::string> &nsDevPaths, std::string &devPath)
{
    // 挂载方式对应的分配策略（AttachSpace=NORMAL/AttachLinearSpace=LINEAR/AttachStripedSpace=STRIPED），
    // 随verify请求提交master校验，与账本分配策略严格一致
    const bool isStriped =
        (blockDeviceOptions != nullptr && blockDeviceOptions->addressingType == UbseSsuAddressingType::STRIPED);

    UbseSsuAttachDetachVerifyOption option = {
        .isAttach = true,
        .expectedStrategy = (blockDeviceOptions == nullptr) ?
                                UbseSsuAllocStrategy::NORMAL :
                                (isStriped ? UbseSsuAllocStrategy::STRIPED : UbseSsuAllocStrategy::LINEAR)};
    if (isStriped) {
        option.raidLevel = static_cast<uint32_t>(blockDeviceOptions->raidLevel);
        option.chunkSize = blockDeviceOptions->chunkSize;
    }

    UbseSsuAttachDetachVerifyResp verifyResp{};
    auto ret = VerifyAttachDetachPreconditionViaRpc(req.name, req.identity, option, verifyResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AgentAttach: VerifyAttachDetachPreconditionViaRpc failed, ret=" << ret << ", name="
                       << req.name;
        return ret;
    }

    // 已挂载空间重复attach，一律报错
    if (verifyResp.alreadyAttached) {
        for (const auto &nsInfo : verifyResp.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        // 聚合块设备场景（blockDeviceOptions非空）：devPath恒为/dev/ssu/{devName}，
        // devName由master账本在verify响应中带回（agent无本地账本，不能取请求参数避免与首次创建不一致）
        if (blockDeviceOptions != nullptr && !verifyResp.devName.empty()) {
            if (!IsValidDevName(verifyResp.devName)) {
                UBSE_LOG_ERROR << "AgentAttach: invalid devName format, name=" << req.name
                               << ", devName=" << verifyResp.devName;
                return UBSE_ERR_INVALID_ARG;
            }
            devPath = SSU_AGGREGATE_DEV_ROOT + verifyResp.devName;
        }
        UBSE_LOG_INFO << "AgentAttach: already attached, name=" << req.name;
        return UBSE_ERR_ALREADY_ATTACHED;
    }

    // 从verify响应中获取namespace列表（agent无本地账本）
    const auto &nameSpaceList = verifyResp.nameSpaceList;
    if (verifyResp.nsVerifyList.size() != nameSpaceList.size()) {
        UBSE_LOG_ERROR << "AgentAttach: ns count mismatch, verify=" << verifyResp.nsVerifyList.size()
                       << ", nsList=" << nameSpaceList.size() << ", name=" << req.name;
        return UBSE_SSU_ERROR_NS_COUNT_MISMATCH;
    }

    nsDevPaths.clear();
    nsDevPaths.reserve(nameSpaceList.size());
    std::vector<UbseSsuNameSpaceInfo> attachedNsList;
    attachedNsList.reserve(nameSpaceList.size());
    for (size_t i = 0; i < nameSpaceList.size(); ++i) {
        const auto &nsInfo = nameSpaceList[i];
        const auto &verifyInfo = verifyResp.nsVerifyList[i];
        auto nsRet = AgentAttachNs(nsInfo, verifyInfo, req.nqn);
        if (nsRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentAttach: AttachSingleNsVerified failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            std::unordered_map<std::string, UbseSsuDevInfoPtr> emptyDevMap;
            RollbackAttachedNsAndLedger(attachedNsList, req, emptyDevMap, &verifyResp);
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
            std::unordered_map<std::string, UbseSsuDevInfoPtr> emptyDevMap;
            RollbackAttachedNsAndLedger(attachedNsList, req, emptyDevMap, &verifyResp);
            return createRet;
        }
    }

    // 聚合块设备场景上报devName，master写入账本供重复挂载回填devPath
    UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHED, false,
                        blockDeviceOptions != nullptr ? devName : "");
    UBSE_LOG_INFO << "AgentAttach success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// agent端通用detach主流程：发送identity验证请求到master，从响应中获取namespace列表，
// 验证成功后逐个detach NS。可选先删除聚合块设备（deleteBlockDevice=true，Linear/Striped场景）。
// 部分detach失败时保持ATTACHED状态（DetachDevNameSpace幂等，可重试收敛）；全部成功才回退为CREATED。
static uint32_t AgentDetach(const UbseSsuSpaceReq &req, const std::string &devName, bool deleteBlockDevice,
                            bool isStriped = false)
{
    UbseSsuAttachDetachVerifyOption option; // detach场景：master校验state（ATTACHED正常/CREATED幂等）
    option.isAttach = false;
    // 卸载方式对应的分配策略（DetachSpace=NORMAL/DetachLinearSpace=LINEAR/DetachStripedSpace=STRIPED），
    // 与账本分配策略必须严格一致
    option.expectedStrategy = deleteBlockDevice ?
                                  (isStriped ? UbseSsuAllocStrategy::STRIPED : UbseSsuAllocStrategy::LINEAR) :
                                  UbseSsuAllocStrategy::NORMAL;
    UbseSsuAttachDetachVerifyResp verifyResp{};
    auto ret = VerifyAttachDetachPreconditionViaRpc(req.name, req.identity, option, verifyResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AgentDetach: VerifyAttachDetachPreconditionViaRpc failed, ret=" << ret << ", name="
                       << req.name;
        return ret;
    }

    // 已卸载空间重复detach，一律报错
    if (verifyResp.alreadyDetached) {
        UBSE_LOG_ERROR << "AgentDetach: no need to detach, name=" << req.name;
        return UBSE_ERR_NO_NEED_DETACH;
    }

    // 从verify响应中获取namespace列表（agent无本地账本）
    const auto &nameSpaceList = verifyResp.nameSpaceList;
    if (verifyResp.nsVerifyList.size() != nameSpaceList.size()) {
        UBSE_LOG_ERROR << "AgentDetach: ns count mismatch, name=" << req.name;
        return UBSE_SSU_ERROR_NS_COUNT_MISMATCH;
    }

    // identity验证通过后再删除聚合块设备（NS detach 之前），避免未授权删除导致状态不一致。
    // 删除前校验请求devName与账本记录一致（账本为attach时写入的权威值），防止误删他人/无关聚合块设备；
    if (deleteBlockDevice) {
        if (!verifyResp.devName.empty() && devName != verifyResp.devName) {
            UBSE_LOG_ERROR << "AgentDetach: devName mismatch, name=" << req.name << ", reqDevName=" << devName
                           << ", ledgerDevName=" << verifyResp.devName;
            return UBSE_ERR_INVALID_ARG;
        }
        if (!devName.empty()) {
            auto deleteRet = UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(devName);
            if (deleteRet != UBSE_OK) {
                UBSE_LOG_ERROR << "AgentDetach: DeleteBlockDevice failed, devName=" << devName << ", ret=" << deleteRet;
                return UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED;
            }
        }
    }

    uint32_t detachRet = UBSE_OK;
    for (size_t i = 0; i < nameSpaceList.size(); ++i) {
        const auto &nsInfo = nameSpaceList[i];
        const auto &verifyInfo = verifyResp.nsVerifyList[i];
        auto nsRet = AgentDetachNs(nsInfo, verifyInfo, req.nqn);
        if (nsRet != UBSE_OK) {
            UBSE_LOG_ERROR << "AgentDetach: DetachSingleNsVerified failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            detachRet = UBSE_SSU_ERROR_DETACH_FAILED;
        }
    }
    if (detachRet != UBSE_OK) {
        // 部分detach失败，保持ATTACHED状态，利用幂等性支持重试
        return detachRet;
    }

    UpdateStateOrNotify(req.name, UbseSsuNsState::CREATED, false);
    UBSE_LOG_INFO << "AgentDetach success: name=" << req.name;
    return UBSE_OK;
}

// master端：校验attach/detach前置条件（identity、账本state、条带化参数）并返回构造AttachDevNameSpace所需字段。
uint32_t UbseSsuServiceImp::VerifyAttachDetachPrecondition(const std::string& name,
                                                           const UbseSsuAllocIdentityInfo& identity,
                                                           const UbseSsuAttachDetachVerifyOption& option,
                                                           UbseSsuAttachDetachVerifyResp& verifyResp)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: name=" << name;

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: record not found, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    auto strategyRet = ValidateAllocStrategyMatch(name, entryPtr->allocResult.strategy, option.expectedStrategy);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // attach：ATTACHED为重复挂载（verifyResp.alreadyAttached标志由agent侧处理），CREATED为正常挂载状态，其他状态拒绝
    // detach：ATTACHED为正常卸载状态，CREATED为重复卸载（verifyResp.alreadyDetached标志由agent侧处理），其他状态拒绝
    if (option.isAttach) {
        if (entryPtr->state == UbseSsuNsState::ATTACHED) {
            verifyResp.alreadyAttached = true;
            UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: already attached, name=" << name;
            return UBSE_OK;
        }
        if (entryPtr->state != UbseSsuNsState::CREATED) {
            UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: invalid state for attach, name=" << name
                           << ", state=" << static_cast<int>(entryPtr->state);
            return UBSE_SSU_ERROR_STATE_INVALID;
        }
    } else {
        if (entryPtr->state == UbseSsuNsState::CREATED) {
            verifyResp.alreadyDetached = true;
            UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: already detached, name=" << name;
            return UBSE_OK;
        }
        if (entryPtr->state != UbseSsuNsState::ATTACHED) {
            UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: invalid state for detach, name=" << name
                           << ", state=" << static_cast<int>(entryPtr->state);
            return UBSE_SSU_ERROR_STATE_INVALID;
        }
    }

    // 仅attach场景校验条带化参数（chunkSize合法性、RAID5成员数、NS大小对齐）。
    // detach场景不校验：块设备已存在，chunkSize/raidLevel为创建时参数，且detach请求未携带该信息（默认0）
    if (option.isAttach && option.expectedStrategy == UbseSsuAllocStrategy::STRIPED) {
        auto cfgRet = ValidateStripedNsConfig(name, static_cast<UbseSsuAggregationRaidLevel>(option.raidLevel),
                                              static_cast<UbseSsuChunkSize>(option.chunkSize),
                                              entryPtr->allocResult.nameSpaceList);
        if (cfgRet != UBSE_OK) {
            UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: ValidateStripedNsConfig failed, name=" << name;
            return cfgRet;
        }
    }

    auto devMap = collector_.GetCachedDevMap();
    auto& nsVerifyList = verifyResp.nsVerifyList;
    nsVerifyList.clear();
    nsVerifyList.reserve(entryPtr->allocResult.nameSpaceList.size());
    std::unordered_set<std::string> refreshedEids;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            // 缓存未命中：从硬件实时刷新（刚分配的ns可能尚未被定时采集覆盖）
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: namespace not found in cache, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: identity not match, eid=" << nsInfo.tgtEid
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
    UBSE_LOG_INFO << "VerifyAttachDetachPrecondition success: name=" << name << ", nsCount=" << nsVerifyList.size();
    return UBSE_OK;
}

// master端attach单个命名空间
static uint32_t AttachSingleNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuAllocIdentityInfo &identity,
                               const std::string &nqn, std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
    if (targetNs == nullptr) {
        // 缓存未命中：从硬件实时刷新（刚分配的ns可能尚未被定时采集覆盖）
        std::unordered_set<std::string> refreshedEids;
        RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
        targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
    }
    if (targetNs == nullptr) {
        UBSE_LOG_ERROR << "AttachSingleNs: device or namespace not found in cache, eid=" << nsInfo.tgtEid
                       << ", nsId=" << nsInfo.namespaceId;
        return UBSE_SSU_ERROR_NS_NOT_FOUND;
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
        return UBSE_SSU_ERROR_ATTACH_FAILED;
    }
    return UBSE_OK;
}

// master端detach单个命名空间
static uint32_t DetachSingleNs(const UbseSsuNameSpaceInfo &nsInfo, const UbseSsuAllocIdentityInfo &identity,
                               const std::string &nqn, std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
    if (targetNs == nullptr) {
        // 缓存未命中：从硬件实时刷新（刚attach的ns可能尚未被定时采集覆盖）
        std::unordered_set<std::string> refreshedEids;
        RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
        targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
    }
    if (targetNs == nullptr) {
        // 刷新后仍查不到，说明已被detach或删除，视为幂等成功
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
        return UBSE_SSU_ERROR_DETACH_FAILED;
    }
    return UBSE_OK;
}

// 回滚已attach的NS（逐个detach）并将账本状态回退到CREATED
// verifyResp非空：agent端路径，用verifyInfo构造nameSpace并detach，isMaster=false
// verifyResp为空：master端路径，用collector缓存detach，isMaster=true
static uint32_t RollbackAttachedNsAndLedger(const std::vector<UbseSsuNameSpaceInfo> &attachedNsList,
                                            const UbseSsuSpaceReq &req,
                                            std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
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
            ret = UBSE_SSU_ERROR_ROLLBACK_FAILED;
        }
    }
    // 无论全部还是部分失败，均回退到CREATED状态
    // 部分detach失败时，NS处于"部分CREATED、部分ATTACHED"的不一致状态
    // 调用方可重试AttachLinearSpace/AttachStripedSpace，利用AttachSingleNs的幂等性重新attach所有NS
    UpdateStateOrNotify(req.name, UbseSsuNsState::CREATED, isMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RollbackAttachedNsAndLedger: partial detach failed, name=" << req.name;
    }
    return ret;
}

// 挂载空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "AttachSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachSpace: name=" << req.name << ", nqn=" << req.nqn;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后attach
    if (role != ELECTION_ROLE_MASTER) {
        std::string devPath;
        return AgentAttach(req, "", nullptr, nsDevPaths, devPath);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 分配策略校验：NORMAL策略只能通过AttachSpace挂载（不聚合块设备），
    // LINEAR/STRIPED策略必须使用配套的AttachLinearSpace/AttachStripedSpace
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::NORMAL);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 重复挂载：已经attached，再次attach需要返回已挂载的nsDevPath列表
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachSpace: already attached, name=" << req.name;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_ERR_ALREADY_ATTACHED;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    // 标记为attaching中
    if (UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHING, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace: failed to update ledger state and notify, name=" << req.name
                       << ", state=" << static_cast<int>(UbseSsuNsState::ATTACHING);
        return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
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

    UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHED, true);

    UBSE_LOG_INFO << "AttachSpace success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// detach空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachSpace(const UbseSsuSpaceReq &req)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "DetachSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachSpace: name=" << req.name << ", nqn=" << req.nqn;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后detach
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, "", false);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 重复卸载：已经detach的NS，再次detach一律报错
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "DetachSpace: no need to detach, name=" << req.name;
        return UBSE_ERR_NO_NEED_DETACH;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    // 分配策略校验：NORMAL策略只能通过DetachSpace卸载（不删除聚合块设备），
    // LINEAR/STRIPED策略必须使用配套的DetachLinearSpace/DetachStripedSpace
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::NORMAL);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t detachRet = UBSE_OK;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto ret = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "DetachSpace: DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            detachRet = UBSE_SSU_ERROR_DETACH_FAILED;
        }
    }

    if (detachRet != UBSE_OK) {
        // 部分detach失败，保持ATTACHED状态，利用幂等性支持调用方重试
        // 1) DetachDevNameSpace幂等（见适配器契约），调用方可重试DetachSpace
        // 2) ExecuteFree要求state==CREATED才允许释放NS，保持ATTACHED可阻止在NS仍挂在host上时误删NS
        UBSE_LOG_ERROR << "DetachSpace: partial detach failed, keeping ATTACHED state, name=" << req.name;
        return detachRet;
    }

    // 更新本地账本
    UpdateStateOrNotify(req.name, UbseSsuNsState::CREATED, true);

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
                                             std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
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
        return UBSE_SSU_ERROR_BLOCK_DEVICE_CREATE_FAILED;
    }

    // 同步写入聚合块设备名到账本，供重复挂载回填devPath（devPath恒为/dev/ssu/{devName}）
    UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHED, true, req.devName);
    return UBSE_OK;
}

// 挂载线性空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachLinearSpace(const UbseSsuLinearSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                              std::string &devPath)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "AttachLinearSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    UbseCreateBlockDeviceOptions options;
    options.addressingType = UbseSsuAddressingType::LINEAR;

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后attach + create block device
    if (role != ELECTION_ROLE_MASTER) {
        return AgentAttach(req, req.devName, &options, nsDevPaths, devPath);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachLinearSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 校验分配策略与挂载策略匹配：LINEAR分配只能通过AttachLinearSpace挂载
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::LINEAR);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 重复挂载：已经attached，再次attach需要返回已挂载的nsDevPath列表
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachLinearSpace: already attached, name=" << req.name;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        // 聚合块设备场景：devPath恒为/dev/ssu/{devName}，从账本取（master重启重建后devName为空，无法回填）
        if (!entryPtr->devName.empty()) {
            if (!IsValidDevName(entryPtr->devName)) {
                UBSE_LOG_ERROR << "AttachLinearSpace: invalid devName in ledger, name=" << req.name
                               << ", devName=" << entryPtr->devName;
                return UBSE_ERR_INVALID_ARG;
            }
            devPath = SSU_AGGREGATE_DEV_ROOT + entryPtr->devName;
        }
        return UBSE_ERR_ALREADY_ATTACHED;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachLinearSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    // 标记为attaching中
    if (UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHING, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
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

// 挂载条带化空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachStripedSpace(const UbseSsuStripedSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                               std::string &devPath)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "AttachStripedSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName
                  << ", level=" << static_cast<int>(req.level)
                  << ", chunkSize=" << static_cast<uint32_t>(req.chunkSize);
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    UbseCreateBlockDeviceOptions options = {.addressingType = UbseSsuAddressingType::STRIPED,
                                            .raidLevel = req.level == UbseSsuAggregationRaidLevel::RAID0 ?
                                                             UbseSsuRaidLevel::RAID0 :
                                                             UbseSsuRaidLevel::RAID5,
                                            .chunkSize = static_cast<uint32_t>(req.chunkSize)};

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后attach + create block device
    if (role != ELECTION_ROLE_MASTER) {
        return AgentAttach(req, req.devName, &options, nsDevPaths, devPath);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "AttachStripedSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 校验分配策略与挂载策略匹配：STRIPED分配只能通过AttachStripedSpace挂载
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::STRIPED);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 重复挂载：已经attached，再次attach返回已挂载的nsDevPath列表
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        UBSE_LOG_INFO << "AttachStripedSpace: already attached, name=" << req.name;
        for (const auto& nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        // 聚合块设备场景：devPath恒为/dev/ssu/{devName}，从账本取（master重启重建后devName为空，无法回填）
        if (!entryPtr->devName.empty()) {
            if (!IsValidDevName(entryPtr->devName)) {
                UBSE_LOG_ERROR << "AttachStripedSpace: invalid devName in ledger, name=" << req.name
                               << ", devName=" << entryPtr->devName;
                return UBSE_ERR_INVALID_ARG;
            }
            devPath = SSU_AGGREGATE_DEV_ROOT + entryPtr->devName;
        }
        return UBSE_ERR_ALREADY_ATTACHED;
    }

    // 只能在created状态下attach
    if (entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "AttachStripedSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    // 校验条带化场景下的参数
    auto ret = ValidateStripedNsConfig(req.name, req.level, req.chunkSize, entryPtr->allocResult.nameSpaceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: ValidateStripedNsConfig failed, name=" << req.name << ", ret=" << ret;
        return ret;
    }

    // 标记为attaching中
    if (UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHING, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
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
                                             std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                             const std::string &expectedDevName)
{
    // 删除前校验请求的devName与账本记录一致（账本为attach时写入的权威值），防止误删他人/无关聚合块设备；
    if (!expectedDevName.empty() && req.devName != expectedDevName) {
        UBSE_LOG_ERROR << tag << ": devName mismatch, name=" << req.name << ", reqDevName=" << req.devName
                       << ", ledgerDevName=" << expectedDevName;
        return UBSE_ERR_INVALID_ARG;
    }
    auto deleteRet = UbseSsuAdapterInterface::GetInstance().DeleteBlockDevice(req.devName);
    if (deleteRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": DeleteBlockDevice failed, devName=" << req.devName << ", ret=" << deleteRet;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED;
    }

    uint32_t ret = UBSE_OK;
    for (const auto &nsInfo : nameSpaceList) {
        auto detachRet = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (detachRet != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            ret = UBSE_SSU_ERROR_DETACH_FAILED;
        }
    }
    if (ret != UBSE_OK) {
        // 部分NS detach失败：账本state刻意保持ATTACHED不变。
        // 1) DetachDevNameSpace幂等（见适配器契约），调用方可重试，已detach的NS会再次返回成功，重试收敛；
        //    DeleteBlockDevice同样幂等，重试时不会因块设备已删而失败
        // 2) ExecuteFree要求state==CREATED才允许释放NS，保持ATTACHED可阻止在仍有NS挂在host上时误删NS，
        //    也可阻止在"块设备已删"的脏状态上误调Attach*重建聚合设备
        UBSE_LOG_ERROR << tag << ": failed to detach all namespaces, name=" << req.name;
        return ret;
    }

    // 更新本地账本，非主节点同步通知master
    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": failed to get node role for status update, ret=" << roleRet;
        return roleRet;
    }
    UpdateStateOrNotify(req.name, UbseSsuNsState::CREATED, role == ELECTION_ROLE_MASTER);
    return UBSE_OK;
}

// detach线性空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachLinearSpace(const UbseSsuLinearSpaceReq &req)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "DetachLinearSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachLinearSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后detach
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, req.devName, true, false);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachLinearSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 校验分配策略与卸载策略匹配：LINEAR分配只能通过DetachLinearSpace卸载
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::LINEAR);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 重复卸载：已经detach的NS，再次detach一律报错
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "DetachLinearSpace: no need to detach, name=" << req.name;
        return UBSE_ERR_NO_NEED_DETACH;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachLinearSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndDeleteBlockDevice("DetachLinearSpace", req, entryPtr->allocResult.nameSpaceList, devMap,
                                            entryPtr->devName);
    if (ret != UBSE_OK) {
        return ret;
    }

    UBSE_LOG_INFO << "DetachLinearSpace success: name=" << req.name << ", devName=" << req.devName;
    return UBSE_OK;
}

// 卸载条带化空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachStripedSpace(const UbseSsuStripedSpaceReq &req)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "DetachStripedSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachStripedSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后detach
    if (role != ELECTION_ROLE_MASTER) {
        return AgentDetach(req, req.devName, true, true);
    }

    // Master: 检查本地账本后执行
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(req.name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "DetachStripedSpace: record not found, name=" << req.name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 校验分配策略与卸载策略匹配：STRIPED分配只能通过DetachStripedSpace卸载
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::STRIPED);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 重复卸载：已经detach的NS，再次detach一律报错
    if (entryPtr->state == UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "DetachStripedSpace: no need to detach, name=" << req.name;
        return UBSE_ERR_NO_NEED_DETACH;
    }

    // 只能在attached状态下detach
    if (entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "DetachStripedSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndDeleteBlockDevice("DetachStripedSpace", req, entryPtr->allocResult.nameSpaceList, devMap,
                                            entryPtr->devName);
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
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
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
    std::string uuid;
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
        auto targetNs = FindNsInDevice(devMap, item.eid, item.uuid);
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
    if (name.empty()) {
        UBSE_LOG_ERROR << funcName << ": name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << funcName << ": name=" << name << ", nqn=" << nqn;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << funcName << ": record not found, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    // 命名空间必须已创建完成，CREATING状态下NS可能尚未落盘，IDLE表示初始/失败回退
    if (entryPtr->state == UbseSsuNsState::IDLE || entryPtr->state == UbseSsuNsState::CREATING) {
        UBSE_LOG_ERROR << funcName << ": invalid state, name=" << name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    auto devMap = collector_.GetCachedDevMap();
    // 记录已成功操作的NS信息，用于失败时回滚
    std::vector<SucceededPermNs> succeededNsList;
    std::unordered_set<std::string> refreshedEids;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            // 缓存未命中：从硬件实时刷新（刚分配的ns可能尚未被定时采集覆盖）
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            // 刷新后仍查不到，才可认定NS确实不存在
            if (isAdd) {
                UBSE_LOG_ERROR << funcName << ": namespace not found in cache, eid=" << nsInfo.tgtEid
                               << ", nsId=" << nsInfo.namespaceId;
                RollbackPermOperations(succeededNsList, isAdd, devMap);
                return UBSE_SSU_ERROR_NS_NOT_FOUND;
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
            return isAdd ? UBSE_SSU_ERROR_PERMISSION_ADD_FAILED : UBSE_SSU_ERROR_PERMISSION_REMOVE_FAILED;
        }
        // 记录成功操作，用于后续回滚
        succeededNsList.push_back({nsInfo.tgtEid, nsInfo.namespaceId, nsInfo.nsUuid, nqnFinal});
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
    return UBSE_SSU_ERROR_ROLE_INVALID;
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
    return UBSE_SSU_ERROR_ROLE_INVALID;
}

uint32_t UbseSsuServiceImp::GetFeDeviceList(std::vector<UbseSsuFe> &feList)
{
    return UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList);
}

uint32_t UbseSsuServiceImp::FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid)
{
    return UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(upi, vfe, busInstanceGuid);
}

uint32_t UbseSsuServiceImp::FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe)
{
    return UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(upi, vfe);
}

// agent端通过RPC查询命名空间统计信息：查询耗时较短，直接通过UbseRpcSend的sync resp返回结果，无需future等待
static uint32_t GetNsStatsViaRpc(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                                 const UbseSsuAllocIdentityInfo &identity)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStatsViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStatsViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = "getnsstats_" + name + "_" + roleInfo.nodeId;

    auto endpoint =
        UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                               static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_NS_STATS_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "GetNsStatsViaRpc: get endpoint failed, requestId=" << requestId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuGetNsStatsReqMsg reqMsg(requestId, roleInfo.nodeId, name, identity);
    UbseSsuGetNsStatsRespMsg syncResp;
    ret = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStatsViaRpc: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }

    auto resp = syncResp.GetGetNsStatsResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStatsViaRpc: failed, requestId=" << requestId << ", errorCode=" << resp.errorCode;
        return resp.errorCode;
    }
    statsList = std::move(resp.statsList);
    UBSE_LOG_INFO << "GetNsStatsViaRpc success: name=" << name << ", count=" << statsList.size();
    return UBSE_OK;
}

// master端：查询命名空间统计信息，实时从硬件刷新设备信息获取usedSize
uint32_t UbseSsuServiceImp::ExecuteGetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                                              const UbseSsuAllocIdentityInfo &identity)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "ExecuteGetNsStats: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "ExecuteGetNsStats: name=" << name;
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "ExecuteGetNsStats: record not found, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    auto devMap = collector_.GetCachedDevMap();
    std::unordered_set<std::string> refreshedEids;
    statsList.clear();
    statsList.reserve(entryPtr->allocResult.nameSpaceList.size());
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        // 实时从硬件刷新设备缓存，确保usedSize为最新值
        RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            UBSE_LOG_ERROR << "ExecuteGetNsStats: namespace not found, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            statsList.clear();
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "ExecuteGetNsStats: identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            statsList.clear();
            return UBSE_ERR_ACCESS_DENIED;
        }
        UbseSsuNsStats stats;
        stats.nsUuid = nsInfo.nsUuid;
        stats.nsId = nsInfo.namespaceId;
        stats.totalSize = nsInfo.nsSize;
        // targetNs->nuse按NVMe规范为LBA数量，需乘LBA Size换算为字节，与API契约(单位字节)保持一致
        uint64_t lbaSize = (targetNs->nsOptions.flbas == 1) ? 4096ULL : 512ULL;
        stats.usedSize = targetNs->nuse * lbaSize;
        statsList.push_back(std::move(stats));
    }
    UBSE_LOG_INFO << "ExecuteGetNsStats success: name=" << name << ", count=" << statsList.size();
    return UBSE_OK;
}

// GetNsStats主入口：根据节点角色选择查询方式
uint32_t UbseSsuServiceImp::GetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                                       const UbseSsuAllocIdentityInfo &identity)
{
    UBSE_LOG_INFO << "GetNsStats: name=" << name;

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStats: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteGetNsStats(name, statsList, identity);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return GetNsStatsViaRpc(name, statsList, identity);
    }

    UBSE_LOG_ERROR << "GetNsStats: unsupported node role=" << role;
    return UBSE_SSU_ERROR_ROLE_INVALID;
}

// agent端通过RPC查询所有分配信息：查询耗时较短，直接通过UbseRpcSend的sync resp返回结果，无需future等待
static uint32_t ListAllocInfoViaRpc(std::vector<UbseSsuAllocResult> &result, const UbseSsuAllocIdentityInfo &identity)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfoViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfoViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = "listalloc_" + roleInfo.nodeId;

    auto endpoint =
        UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                               static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "ListAllocInfoViaRpc: get endpoint failed, requestId=" << requestId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuListAllocInfoReqMsg reqMsg(requestId, roleInfo.nodeId, identity);
    UbseSsuListAllocInfoRespMsg syncResp;
    ret = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfoViaRpc: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }

    const auto &resp = syncResp.GetListAllocInfoResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfoViaRpc: identity verify failed, requestId=" << requestId
                       << ", errorCode=" << resp.errorCode;
        return resp.errorCode;
    }

    // master端直接返回完整分配结果列表，无需本地账本
    result = resp.results;
    UBSE_LOG_INFO << "ListAllocInfoViaRpc success: count=" << result.size();
    return UBSE_OK;
}

// agent端通过RPC根据名称查询分配信息：查询耗时较短，直接通过UbseRpcSend的sync resp返回结果，无需future等待
static uint32_t GetAllocInfoByNameViaRpc(const std::string &name, UbseSsuAllocResult &result,
                                         const UbseSsuAllocIdentityInfo &identity)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByNameViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByNameViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = "getalloc_" + name + "_" + roleInfo.nodeId;

    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "GetAllocInfoByNameViaRpc: get endpoint failed, requestId=" << requestId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuGetAllocInfoReqMsg reqMsg(requestId, roleInfo.nodeId, name, identity);
    UbseSsuGetAllocInfoRespMsg syncResp;
    ret = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByNameViaRpc: RpcSend failed, " << FormatRetCode(ret)
                       << ", requestId=" << requestId;
        return ret;
    }

    const auto &resp = syncResp.GetGetAllocInfoResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByNameViaRpc: identity verify failed, requestId=" << requestId
                       << ", errorCode=" << resp.errorCode;
        return resp.errorCode;
    }

    // master端直接返回完整的分配结果，无需本地账本
    result = resp.result;
    UBSE_LOG_INFO << "GetAllocInfoByNameViaRpc success: name=" << name;
    return UBSE_OK;
}

// 校验ledger entry是否属于指定identity：所有可在设备缓存中找到的namespace的identity都必须匹配
static bool IsLedgerEntryIdentityMatch(const UbseSsuLedgerEntry &entry, const UbseSsuAllocIdentityInfo &identity,
                                       std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    std::unordered_set<std::string> refreshedEids;
    bool anyChecked = false;
    for (const auto &nsInfo : entry.allocResult.nameSpaceList) {
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
            if (targetNs == nullptr) {
                continue;
            }
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            // 任意一个已查到的namespace不匹配，即视为非本人所有
            return false;
        }
        anyChecked = true;
    }
    // 所有能查到的namespace都匹配且至少查到过1个，才算放行
    return anyChecked;
}

// master端：校验identity并返回该identity有权访问的name列表
// name为空时返回所有匹配的name列表；name非空时只验证该name，验证通过则verifiedNames=[name]，不通过返回错误码
// master端校验identity并返回该identity有权访问的name列表
static uint32_t VerifyGetInfoIdentity(const UbseSsuAllocIdentityInfo &identity, const std::string &name,
                                      std::vector<std::string> &verifiedNames,
                                      std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    UBSE_LOG_INFO << "VerifyGetInfoIdentity: name=" << name << ", uid=" << identity.uid
                  << ", userName=" << identity.userName;

    verifiedNames.clear();

    // name非空：只验证指定name的identity
    if (!name.empty()) {
        auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
        if (entryPtr == nullptr) {
            UBSE_LOG_ERROR << "VerifyGetInfoIdentity: record not found, name=" << name;
            return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
        }
        // CREATING/IDLE状态的条目allocResult为空，不返回
        if (entryPtr->state == UbseSsuNsState::CREATING) {
            UBSE_LOG_ERROR << "VerifyGetInfoIdentity: still creating, name=" << name;
            return UBSE_ERR_CREATING;
        }
        if (entryPtr->state == UbseSsuNsState::IDLE) {
            UBSE_LOG_ERROR << "VerifyGetInfoIdentity: record idle, name=" << name;
            return UBSE_ERR_NOT_EXIST;
        }
        if (!IsLedgerEntryIdentityMatch(*entryPtr, identity, devMap)) {
            UBSE_LOG_ERROR << "VerifyGetInfoIdentity: identity not match, name=" << name;
            return UBSE_ERR_ACCESS_DENIED;
        }
        verifiedNames.push_back(name);
        UBSE_LOG_INFO << "VerifyGetInfoIdentity success: name=" << name;
        return UBSE_OK;
    }

    // name为空：返回所有匹配identity的name列表
    auto entries = UbseSsuDebtLedger::GetInstance().GetAll();
    for (const auto &entryPtr : entries) {
        if (entryPtr == nullptr) {
            continue;
        }
        // CREATING/IDLE状态的条目allocResult为空，不返回
        if (entryPtr->state == UbseSsuNsState::CREATING || entryPtr->state == UbseSsuNsState::IDLE) {
            continue;
        }
        if (!IsLedgerEntryIdentityMatch(*entryPtr, identity, devMap)) {
            continue;
        }
        verifiedNames.push_back(entryPtr->name);
    }
    UBSE_LOG_INFO << "VerifyGetInfoIdentity success: count=" << verifiedNames.size();
    return UBSE_OK;
}

// 填充UbseSsuAllocResult中每个namespace的allowHostNqnList：
// defaultNqn置于首位（与http_handler取front作为defaultHostNqn的约定一致），后接硬件allow列表中其余NQN（去重）。
// 设备缓存未命中时尝试按需刷新；GetNameSpaceAllowHostList失败时仅以defaultNqn降级填充该ns，不影响其余ns。
static void FillAllowHostNqnList(UbseSsuAllocResult &result, std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    std::unordered_set<std::string> refreshedEids;
    for (auto &nsInfo : result.nameSpaceList) {
        const UbseSsuDevNameSpace *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
            if (targetNs == nullptr) {
                UBSE_LOG_WARN << "FillAllowHostNqnList: namespace not found in cache, eid=" << nsInfo.tgtEid
                              << ", nsId=" << nsInfo.namespaceId;
                continue;
            }
        }
        std::string defaultNqn(targetNs->customData.defaultNqn,
                               strnlen(targetNs->customData.defaultNqn, sizeof(targetNs->customData.defaultNqn)));
        nsInfo.allowHostNqnList.clear();
        std::unordered_set<std::string> seenNqns;
        if (!defaultNqn.empty()) {
            nsInfo.allowHostNqnList.push_back(defaultNqn);
            seenNqns.insert(defaultNqn);
        }
        std::vector<std::string> hwAllowList;
        auto ret = UbseSsuAdapterInterface::GetInstance().GetNameSpaceAllowHostList(*targetNs, hwAllowList);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "FillAllowHostNqnList: GetNameSpaceAllowHostList failed, eid=" << nsInfo.tgtEid
                          << ", nsId=" << nsInfo.namespaceId << ", ret=" << ret;
            // 防御性：失败时不依赖适配器对输出参数的清理行为，避免残留数据透传给调用方
            hwAllowList.clear();
        }
        for (auto &hostNqn : hwAllowList) {
            // 去重：defaultNqn 与 hwAllowList 内部重复均排除，seenNqns.insert().second 为 true 表示新插入
            if (seenNqns.insert(hostNqn).second) {
                nsInfo.allowHostNqnList.push_back(std::move(hostNqn));
            }
        }
    }
}

// ListAllocInfo主入口：根据节点角色选择查询方式
uint32_t UbseSsuServiceImp::ListAllocInfo(std::vector<UbseSsuAllocResult> &result,
                                          const UbseSsuAllocIdentityInfo &identity)
{
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfo: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        // master端：先校验identity获取有权访问的name列表，再从本地账本读取完整数据
        std::vector<std::string> verifiedNames;
        auto devMap = collector_.GetCachedDevMap();
        ret = VerifyGetInfoIdentity(identity, "", verifiedNames, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "ListAllocInfo: identity verify failed, ret=" << ret;
            return ret;
        }
        result.clear();
        for (const auto &name : verifiedNames) {
            auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
            if (entryPtr == nullptr) {
                UBSE_LOG_WARN << "ListAllocInfo: local ledger entry not found, name=" << name;
                continue;
            }
            result.push_back(entryPtr->allocResult);
        }
        // 从硬件实时获取每个ns的allowHostNqnList，账本中的快照不承载可变权限状态
        for (auto &allocResult : result) {
            FillAllowHostNqnList(allocResult, devMap);
        }
        return UBSE_OK;
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return ListAllocInfoViaRpc(result, identity);
    }

    UBSE_LOG_ERROR << "ListAllocInfo: unsupported node role=" << role;
    return UBSE_SSU_ERROR_ROLE_INVALID;
}

// GetAllocInfoByName主入口：根据节点角色选择查询方式
uint32_t UbseSsuServiceImp::GetAllocInfoByName(const std::string &name, UbseSsuAllocResult &result,
                                               const UbseSsuAllocIdentityInfo &identity)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "GetAllocInfoByName: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    UBSE_LOG_INFO << "GetAllocInfoByName: name=" << name;

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByName: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        // master端：先校验identity，验证成功后从本地账本读取完整数据
        std::vector<std::string> verifiedNames;
        auto devMap = collector_.GetCachedDevMap();
        ret = VerifyGetInfoIdentity(identity, name, verifiedNames, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "GetAllocInfoByName: identity verify failed, name=" << name << ", ret=" << ret;
            return ret;
        }
        auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
        if (entryPtr == nullptr) {
            UBSE_LOG_ERROR << "GetAllocInfoByName: local ledger entry not found, name=" << name;
            return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
        }
        result = entryPtr->allocResult;
        // 从硬件实时获取每个ns的allowHostNqnList，账本中的快照不承载可变权限状态
        FillAllowHostNqnList(result, devMap);
        return UBSE_OK;
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return GetAllocInfoByNameViaRpc(name, result, identity);
    }

    UBSE_LOG_ERROR << "GetAllocInfoByName: unsupported node role=" << role;
    return UBSE_SSU_ERROR_ROLE_INVALID;
}

// agent端通过RPC查询连接信息：查询耗时较短，直接通过UbseRpcSend的sync resp返回结果，无需future等待
static uint32_t GetConnectInfoViaRpc(const std::string &name, const UbseSsuVfe *vfe,
                                     std::vector<UbseSsuConnectInfo> &connectInfoList,
                                     const UbseSsuAllocIdentityInfo &identity)
{
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfoViaRpc: get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfoViaRpc: get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    auto requestId = "getconnectinfo_" + name + "_" + roleInfo.nodeId;

    auto endpoint =
        UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                               static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_CONNECT_INFO_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "GetConnectInfoViaRpc: get endpoint failed, requestId=" << requestId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }

    UbseSsuGetConnectInfoReqMsg reqMsg(requestId, roleInfo.nodeId, name, identity, vfe);
    UbseSsuGetConnectInfoRespMsg syncResp;
    ret = endpoint->UbseRpcSend(masterInfo.nodeId, reqMsg, syncResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfoViaRpc: RpcSend failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
        return ret;
    }

    const auto &resp = syncResp.GetGetConnectInfoResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfoViaRpc: failed, requestId=" << requestId << ", errorCode=" << resp.errorCode;
        return resp.errorCode;
    }
    connectInfoList = resp.connectInfoList;
    UBSE_LOG_INFO << "GetConnectInfoViaRpc success: name=" << name << ", count=" << connectInfoList.size();
    return UBSE_OK;
}

// 设置连接信息中的srcEid字段，后续添加实现
static uint32_t SetSrcEidInfo(const UbseSsuVfe *vfe, std::vector<UbseSsuConnectInfo> &connectInfoList)
{
    return UBSE_OK;
}

// master端：查询连接信息，校验identity后从设备缓存获取hostNqn等字段
uint32_t UbseSsuServiceImp::ExecuteGetConnectInfo(const std::string &name, const UbseSsuVfe *vfe,
                                                  std::vector<UbseSsuConnectInfo> &connectInfoList,
                                                  const UbseSsuAllocIdentityInfo &identity)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "ExecuteGetConnectInfo: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "ExecuteGetConnectInfo: name=" << name;
    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "ExecuteGetConnectInfo: record not found, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    auto devMap = collector_.GetCachedDevMap();
    std::unordered_set<std::string> refreshedEids;
    connectInfoList.clear();
    connectInfoList.reserve(entryPtr->allocResult.nameSpaceList.size());
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            // 缓存未命中：从硬件直接查询此eid（刚分配的ns可能尚未被定时采集覆盖）
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            UBSE_LOG_ERROR << "ExecuteGetConnectInfo: namespace not found, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            connectInfoList.clear();
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "ExecuteGetConnectInfo: identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            connectInfoList.clear();
            return UBSE_ERR_ACCESS_DENIED;
        }

        UbseSsuConnectInfo info = {
            .tgtEid = nsInfo.tgtEid,
            .tgtNqn = nsInfo.tgtNqn,
            .hostNqn = std::string(targetNs->customData.defaultNqn,
                                   strnlen(targetNs->customData.defaultNqn, sizeof(targetNs->customData.defaultNqn))),
            .nsUuid = nsInfo.nsUuid,
            .nsId = nsInfo.namespaceId,
        };
        connectInfoList.push_back(std::move(info));
    }

    auto ret = SetSrcEidInfo(vfe, connectInfoList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ExecuteGetConnectInfo: failed to set srcEid info, name=" << name << ", ret=" << ret;
        connectInfoList.clear();
        return ret;
    }

    UBSE_LOG_INFO << "ExecuteGetConnectInfo success: name=" << name << ", count=" << connectInfoList.size();
    return UBSE_OK;
}

// GetConnectInfo主入口：根据节点角色选择查询方式
uint32_t UbseSsuServiceImp::GetConnectInfo(const std::string &name, const UbseSsuVfe *vfe,
                                           std::vector<UbseSsuConnectInfo> &connectInfoList,
                                           const UbseSsuAllocIdentityInfo &identity)
{
    UBSE_LOG_INFO << "GetConnectInfo: name=" << name;

    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfo: failed to get node role, ret=" << ret;
        return ret;
    }

    if (role == ELECTION_ROLE_MASTER) {
        return ExecuteGetConnectInfo(name, vfe, connectInfoList, identity);
    }

    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        return GetConnectInfoViaRpc(name, vfe, connectInfoList, identity);
    }

    UBSE_LOG_ERROR << "GetConnectInfo: unsupported node role=" << role;
    return UBSE_SSU_ERROR_ROLE_INVALID;
}
} // namespace ubse::ssu::service
