/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_ssu_service_imp.h"

#include <securec.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "debt/ubse_ssu_debt_ledger.h"
#include "framework/misc/ubse_future_mgr.h"
#include "framework/misc/ubse_logging_lock_guard.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_mount_reconcile_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "src/framework/misc/ubse_env_util.h"
#include "ipc/message/ubse_ssu_obj_message.h"
#include "ubse_ssu_attach_record_store.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_ssu_direct_to_vm_manager.h"
#include "ubse_ssu_mount_ops.h"
#include "ubse_ssu_scheduler.h"
#include "ubse_ssu_utils.h"
#include "ubse_ssu_vm_cache.h"
#include "ubse_node_controller.h"
#include "ubse_timer.h"

namespace ubse::ssu::service {

using namespace ubse::log;
using namespace ubse::ssu::scheduler;
using namespace ubse::election;
using namespace ubse::com;
using namespace ubse::ssu::message;
using namespace ubse::misc::future;
using namespace ubse::utils;
using namespace ubse::ssu::debt;
using namespace ubse::ssu::persist;
using namespace ubse::ssu::utils;
using ubse::ssu::controller::UbseSsuVmCache;

UBSE_DEFINE_THIS_MODULE("ubse");

// future等待最大超时时间（秒）
constexpr uint32_t MAX_TIMEOUT_SECONDS = 30;

// 超时后最大重试次数（不含首次请求）
constexpr uint32_t MAX_RETRY_COUNT = 3;

// 分配总大小的对齐粒度：1GiB
constexpr uint64_t ONE_GIB = 1024ULL * 1024ULL * 1024ULL;

// 恢复期agent向master重报的最大次数（对齐OnNodeUp既有退避惯例；
// MASTER_NOT_READY纯重试，最长等一个采集周期）
constexpr uint32_t RECOVER_REPORT_RETRY_COUNT = 5;

// 重报间隔（ms）：默认2s；env UBSE_SSU_RECOVER_RETRY_INTERVAL_MS可覆盖（UT提速用，
// 与attach记录文件路径的env覆盖同一模式）
static uint32_t GetRecoverRetryIntervalMs()
{
    return static_cast<uint32_t>(GetEnv<int16_t>("UBSE_SSU_RECOVER_RETRY_INTERVAL_MS", 2000));
}

// 恢复流程防重入标志：MASTER_ONLINE抖动/主备切换可能连续触发，CAS保证仅一个恢复流程在跑
static std::atomic<bool> g_recoveringAttachments{false};

// 模块就绪标志：UbseSsuControllerModule::Start()成功末尾置位、Stop()复位。
// MASTER_ONLINE事件可早于模块Start到达（重加入场景），恢复阶段2的RPC端点在Start内注册，
// 不等待则5次重报全撞在endpoint未注册上。等待采用次数上限×重试间隔（复用env覆盖，UT可提速）
static std::atomic<bool> g_ssuModuleReady{false};
constexpr uint32_t MODULE_READY_WAIT_ATTEMPTS = 15; // 15×2s=最坏30s，超时放行走原有重试+对账兜底

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

    // 首次采集失败（缓存为空）时，由定时器采集成功后触发账本重建；
    // 重建完成后补执行恢复对账（MASTER_ONLINE时账本未就绪被挂起的ReconcileMasterLedger，
    // ledgerReady_已置位后不再挂起）
    collector_.SetOnFirstCollectFailRecovery([this]() {
        RebuildLedgerFromDevList();
        auto executor = ubse::ssu::utils::GetSsuExecutor();
        if (executor == nullptr) {
            UBSE_LOG_ERROR << "FirstCollectFailRecovery: get ssu executor failed, skip reconcile recovery";
            return;
        }
        executor->Execute([]() { UbseSsuServiceImp::GetInstance().RecoverLocalAttachments(); });
    });
    // 周期采集成功回调：设备故障增量对账（FAULTED防抖/回正）
    // 与离线节点离场处理，collector仅master启动，回调天然master-only
    collector_.SetOnCollectSuccess([this](const std::vector<UbseSsuDevInfo> &devList) {
        ReconcileSpaceFaultStates(devList);
        ReconcileOfflineNodes();
    });

    ret = collector_.Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "StartCollecting: collector_.Start failed, ret=" << FormatRetCode(ret);
        return ret;
    }
    // 运营对账定时器与collector同生命周期（仅master运行）；
    // 注册失败仅降级（本轮稳态对账缺失，下轮StartCollecting/主备切换自愈），不阻断采集
    ret = StartMountReconcileTimer();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "StartCollecting: StartMountReconcileTimer failed, ret=" << FormatRetCode(ret);
    }
    UBSE_LOG_INFO << "StartCollecting: collector started";
    return UBSE_OK;
}

// 停止设备收集器
void UbseSsuServiceImp::StopCollecting()
{
    StopMountReconcileTimer();
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
    if (devList.empty() && collector_.IsFirstCollectPending()) {
        // 首次采集失败且尚未恢复（缓存从未被填充）：空列表不代表真实账本状态，保持未就绪，
        // 由定时器采集成功后的恢复回调（SetOnFirstCollectFailRecovery）重新触发重建
        UBSE_LOG_WARN << "RebuildLedgerFromDevList: first collect not finished, skip ledger rebuild";
        return;
    }
    // 采集成功但列表为空同样是真实账本状态（硬件确实无已分配空间）：
    // Rebuild对空输入安全（置位就绪+空账本），避免agent重报永久收到MASTER_NOT_READY
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
    // VM 模式不支持 alloc(盘由 openstack 分配)
    if (vmMode_) {
        UBSE_LOG_WARN << "AllocSpace: not supported in VM mode, name=" << req.name;
        return UBSE_ERR_NOT_SUPPORTED;
    }
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
// createdNsList为出参，返回实际创建成功的namespace列表（含tenant信息），供调用方回写设备缓存
static uint32_t CreateDevNameSpaces(const UbseSsuAllocSpaceReq &req,
                                    const std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList,
                                    const UbseSsuAllocIdentityInfo &identity,
                                    const std::unordered_map<std::string, std::string> &eidToSubNqn,
                                    UbseSsuAllocResult &result,
                                    std::vector<UbseSsuDevNameSpace> &createdNsList)
{
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
    // 账本未就绪门禁：master重启后首采/Rebuild完成前拒绝分配，
    // 避免空账本误判"无同名空间"导致重复分配；SDK/agent可重试（MASTER_NOT_READY为纯重试语义）
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "ExecuteAlloc: ledger not ready, name=" << req.name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
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
    std::vector<UbseSsuDevNameSpace> createdNsList;
    ret = CreateDevNameSpaces(req, eidNsSizeList, identity, eidToSubNqn, result, createdNsList);
    if (ret != UBSE_OK) {
        collector_.ReleaseReservation(eidNsSizeList);
        collector_.OnReserveEnd();
        return ret;
    }

    // 立即将新创建的NS（含tenant）合入设备缓存：租户隔离依赖缓存中的NS租户信息，
    // 若等待30s周期采集，窗口期内其他租户的分配会误占同一设备导致隔离失效
    collector_.UpdateDevNameSpaces(createdNsList);

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

    // VM 模式不支持 free(盘由 openstack 分配)
    if (vmMode_) {
        UBSE_LOG_WARN << "FreeSpace: not supported in VM mode, name=" << name;
        return UBSE_ERR_NOT_SUPPORTED;
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

// 按NVMe规范将nuse(LBA数量)换算为已用字节数，与API契约(单位字节)保持一致
// flbas==1 表示 4096 字节 LBA，其余为 512 字节
static uint64_t NsUsedSizeBytes(const UbseSsuDevNameSpace &ns)
{
    uint64_t lbaSize = (ns.nsOptions.flbas == 1) ? 4096ULL : 512ULL;
    return ns.nuse * lbaSize;
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

// ===== 本地挂载记录持久化 hook =====
// 记录是"本节点挂载了什么"的唯一事实源（重启恢复依据）。
// 挂载/卸载主流程不因记录失败而失败（挂载是主业务，记录是可靠性增强），
// Upsert/Remove 内部已记录落盘失败日志

// 挂载记录写入上下文：请求侧字段打包（name/nqn/identity恒取自同一请求对象）。
// 引用成员仅作调用点透传，生命周期由调用方局部变量保证，不可存储
struct LocalAttachRecordCtx {
    const std::string &name;
    UbseSsuAllocStrategy strategy;
    const std::string &devName;
    const std::string &nqn;
    const UbseSsuAllocIdentityInfo &identity;
    const UbseCreateBlockDeviceOptions *blockDeviceOptions;
    const std::vector<UbseSsuNameSpaceInfo> &nsList;
};

static void UpsertLocalAttachRecord(const LocalAttachRecordCtx &ctx,
                                    const std::vector<UbseSsuNsVerifyInfo> &nsVerifyList,
                                    UbseSsuAttachRecordState recordState)
{
    UbseSsuAttachRecord record;
    record.name = ctx.name;
    record.strategy = ctx.strategy;
    record.recordState = recordState;
    record.devName = ctx.devName;
    // 存用户请求的nqn（非ResolveNqn结果）：恢复重挂回放AgentAttachNs时会按nsVerifyList中
    // 各NS的defaultNqn重新解析，与首次挂载行为完全一致（空串=defaultNqn场景尤其如此）
    record.nqn = ctx.nqn;
    record.identity = ctx.identity;
    if (ctx.blockDeviceOptions != nullptr) {
        record.blockDeviceOptions = *ctx.blockDeviceOptions;
    }
    record.nsList = ctx.nsList;
    record.nsVerifyList = nsVerifyList;
    record.attachedAt = static_cast<int64_t>(time(nullptr));
    UbseSsuAttachRecordStore::GetInstance().Upsert(record);
}

// master本地挂载路径：从collector缓存构造nsVerifyList（恢复重挂回放AgentAttachNs所需字段）
// 后写ATTACHED记录；缓存查不到属异常（刚attach成功），放弃本条记录由下次操作自愈
static void UpsertLocalAttachRecordFromDevMap(const LocalAttachRecordCtx &ctx,
                                              const std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    std::vector<UbseSsuNsVerifyInfo> nsVerifyList;
    nsVerifyList.reserve(ctx.nsList.size());
    for (const auto &ns : ctx.nsList) {
        const auto *targetNs = FindNsInDevice(devMap, ns.tgtEid, ns.nsUuid);
        if (targetNs == nullptr) {
            UBSE_LOG_WARN << "UpsertLocalAttachRecord: ns not found in dev cache, skip record, name=" << ctx.name
                          << ", eid=" << ns.tgtEid << ", nsId=" << ns.namespaceId;
            return;
        }
        UbseSsuNsVerifyInfo verify;
        verify.defaultNqn = std::string(
            targetNs->customData.defaultNqn, strnlen(targetNs->customData.defaultNqn, UBSE_SSU_MAX_NQN_LENGTH));
        verify.jettyId = targetNs->subSystem.jettyId;
        verify.guid = targetNs->guid;
        nsVerifyList.emplace_back(std::move(verify));
    }
    UpsertLocalAttachRecord(ctx, nsVerifyList, UbseSsuAttachRecordState::ATTACHED);
}

// detach/free成功或attach失败回退后作废本地记录（记录不存在时幂等成功）
static void RemoveLocalAttachRecord(const std::string &name)
{
    UbseSsuAttachRecordStore::GetInstance().Remove(name);
}

// 执行释放操作：校验命名空间身份并删除命名空间
uint32_t UbseSsuServiceImp::ExecuteFree(const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "ExecuteFree: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    // 账本未就绪门禁：避免恢复窗口内Get落空被误报
    // "空间不存在"（NO_NEED_FREE），误导调用方认为已释放
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "ExecuteFree: ledger not ready, name=" << name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
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
        // 设备故障止损出口：FAULTED空间允许free——硬件缺失时
        // detach本就无法执行，"需先卸载"约束跳过；不在线的NS在下方按已释放跳过
        if (entryPtr->state == UbseSsuNsState::FAULTED) {
            UBSE_LOG_WARN << "ExecuteFree: space faulted, free as mitigation, name=" << name;
        } else {
            UBSE_LOG_WARN << "ExecuteFree: invalid state, name=" << name
                          << ", state=" << static_cast<int>(entryPtr->state);
            return UBSE_SSU_ERROR_STATE_INVALID;
        }
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t firstErr = UBSE_OK;
    std::vector<UbseSsuNameSpaceInfo> remainingNs; // 未删成功的namespace，保留在账本中便于上层重试
    std::vector<std::pair<std::string, uint64_t>> releasedCapacity;
    std::vector<std::pair<std::string, std::string>> removedNs; // 已从硬件删除/确认不存在的(eid, uuid)，用于回写缓存
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
            // collector缓存（RefreshDevCache只更新本地devMap副本）中可能仍残留该NS，一并移除
            removedNs.emplace_back(nsInfo.tgtEid, nsInfo.nsUuid);
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
        removedNs.emplace_back(nsInfo.tgtEid, nsInfo.nsUuid);
    }

    // 预增加已删成功namespace的释放容量，等下一次collector刷新时再清除
    if (!releasedCapacity.empty()) {
        collector_.AddReleasedSpace(releasedCapacity);
    }

    // 立即从设备缓存移除已删除的NS：租户隔离依赖缓存中的NS租户信息，
    // 若等待30s周期采集，窗口期内设备仍被旧租户占用，其他租户/无租户请求会被误拒
    if (!removedNs.empty()) {
        collector_.RemoveDevNameSpaces(removedNs);
    }

    // 没有失败的namespace，删除账本条目
    if (remainingNs.empty()) {
        UbseSsuDebtLedger::GetInstance().Remove(name);
        // free成功：作废本地挂载记录（仅当本机有该记录，幂等）
        RemoveLocalAttachRecord(name);
        // free成功：摘除疑似本地残留挂载标记
        suspectedMountTable_.Erase(name);
        UBSE_LOG_INFO << "FreeSpace success: name=" << name;
        return UBSE_OK;
    }

    // 部分失败：账本只保留未删成功的条目，便于上层重试
    UbseSsuDebtLedger::GetInstance().Modify(
        name, [&remainingNs](UbseSsuLedgerEntry &e) { e.allocResult.nameSpaceList = std::move(remainingNs); });
    return firstErr;
}

// NORMAL策略多节点共挂：按本机nodeId维护attachNodes，空间级state保持聚合语义（任一节点挂载=ATTACHED，全部卸载=CREATED）
static void ApplyNormalNodeAttachState(UbseSsuLedgerEntry &entry, UbseSsuNsState state, const std::string &selfNodeId,
                                       const std::string &name)
{
    switch (state) {
        case UbseSsuNsState::ATTACHING:
            // 不置空间级ATTACHING，避免阻塞其他节点并发attach；master本地attach的并发互斥由per-name resourceLock保证
            UBSE_LOG_INFO << "UpdateStateOrNotify: skip attaching mark for normal multi-node, name=" << name;
            return;
        case UbseSsuNsState::ATTACHED:
            entry.state = state;
            entry.attachNodes.insert(selfNodeId);
            return;
        case UbseSsuNsState::CREATED:
            entry.attachNodes.erase(selfNodeId);
            if (!entry.attachNodes.empty()) {
                // 其他节点仍挂载，空间级state保持ATTACHED
                UBSE_LOG_INFO << "UpdateStateOrNotify: other nodes still attached, keep ATTACHED, name=" << name
                              << ", attachedNodes=" << entry.attachNodes.size();
                return;
            }
            entry.state = state;
            return;
        default:
            entry.state = state;
            return;
    }
}

// LINEAR/STRIPED独占挂载：保持原有空间级状态机语义，并按attacher维度维护attachNodes（至多1个元素），
// 供verify强校验当前节点是否为attacher（非attacher的attach/detach一律拒绝）
static void ApplyAggDevLedgerState(UbseSsuLedgerEntry &entry, UbseSsuNsState state, const std::string &selfNodeId,
                                   const std::string &devName)
{
    entry.state = state;
    if (state == UbseSsuNsState::ATTACHED) {
        entry.attachNodes.insert(selfNodeId);
    } else if (state == UbseSsuNsState::CREATED) {
        entry.attachNodes.erase(selfNodeId);
    }
    // 进入CREATED（detach成功或attach失败回退）说明聚合块设备已停止运行态（StopBlockDevice），
    // 元数据保留在硬件上可供 re-attach 时 Assemble 复用，故 devName 不再清空。
    // 仅当 attach 失败回退且 devName 为空（首次 attach 未创建成功）时保持为空。
    // attach 成功（state==ATTACHED）时 devName 非空才覆盖，保留权威值供重复挂载回填 devPath。
    if (state == UbseSsuNsState::ATTACHED && !devName.empty()) {
        entry.devName = devName;
    }
}

// 更改空间状态：master端更新本地账本状态，agent端仅发送状态通知给master
// devName：聚合块设备名称（Linear/Striped场景），attach成功时携带，master端写入账本；Normal场景为空
// NORMAL策略多节点共挂：master端按本机nodeId维护attachNodes，agent端自动携带本机nodeId上报，
// master侧HandleStatusReceiver按节点对账；空间级state保持聚合语义（任一节点挂载=ATTACHED）
static uint32_t UpdateStateOrNotify(const std::string &name, UbseSsuNsState state, bool isMaster,
                                    const std::string &devName = "")
{
    if (isMaster) {
        // NORMAL多节点场景需本机nodeId维护attachNodes，获取失败直接透传
        std::string selfNodeId;
        auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
        if (nodeIdRet != UBSE_OK) {
            UBSE_LOG_ERROR << "UpdateStateOrNotify: get current node id failed, " << FormatRetCode(nodeIdRet)
                           << ", name=" << name;
            return nodeIdRet;
        }
        if (!UbseSsuDebtLedger::GetInstance().Modify(name, [&state, &devName, &selfNodeId,
                                                            &name](UbseSsuLedgerEntry& e) {
                // NORMAL多节点共挂：按本机nodeId维护attachNodes，空间级state保持聚合语义（任一节点挂载=ATTACHED）
                if (e.allocReq.strategy == UbseSsuAllocStrategy::NORMAL) {
                    ApplyNormalNodeAttachState(e, state, selfNodeId, name);
                } else {
                    ApplyAggDevLedgerState(e, state, selfNodeId, devName);
                }
            })) {
            UBSE_LOG_ERROR << "UpdateStateOrNotify: ledger entry not found, name=" << name
                           << ", state=" << static_cast<int>(state);
            return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
        }
        return UBSE_OK;
    }

    // agent侧：携带本机nodeId上报（NORMAL多节点对账主键，LINEAR/STRIPED仅用于日志）
    std::string nodeId;
    auto ret = UbseGetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UpdateStateOrNotify: get current node id failed, " << FormatRetCode(ret) << ", name=" << name;
        return ret;
    }
    // agent侧：仅发送状态更新，不维护本地账本
    return SendStatusUpdate(name, state, devName, nodeId);
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

// VM 模式没有 host 侧账本,从本地缓存读取分配策略并复用统一的策略匹配校验。
// 缓存未命中时保持 VM 挂载/卸载接口现有的 NS_NOT_FOUND 语义。
static uint32_t ValidateVmAllocStrategy(UbseSsuVmCache *cache, const std::string &name,
                                        UbseSsuAllocStrategy expectedStrategy)
{
    if (cache == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseSsuAllocResult allocResult{};
    if (cache->GetAllocInfo(name, allocResult) != UBSE_OK) {
        return UBSE_SSU_ERROR_NS_NOT_FOUND;
    }
    return ValidateAllocStrategyMatch(name, allocResult.strategy, expectedStrategy);
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

    // 先写PENDING再挂载：进程在挂载中途崩溃时，恢复流程对PENDING记录探测收敛
    const LocalAttachRecordCtx recordCtx{req.name, option.expectedStrategy, devName, req.nqn, req.identity,
                                         blockDeviceOptions, nameSpaceList};
    UpsertLocalAttachRecord(recordCtx, verifyResp.nsVerifyList, UbseSsuAttachRecordState::PENDING);

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
        // 决策 Create / Assemble（委托 SetupBlockDeviceWithProbe 统一处理）：
        //   - Probe 命中 → Assemble 复用元数据；否则降级 Create 并 WARN
        const std::string& ledgerDevName = verifyResp.devName;
        uint32_t setupRet = SetupBlockDeviceWithProbe("AgentAttach", devName, ledgerDevName,
            nsDevPaths, *blockDeviceOptions, devPath);
        if (setupRet != UBSE_OK) {
            std::unordered_map<std::string, UbseSsuDevInfoPtr> emptyDevMap;
            RollbackAttachedNsAndLedger(attachedNsList, req, emptyDevMap, &verifyResp);
            return setupRet;
        }
    }

    // 本地挂载成功：记录转ATTACHED（上报master失败不回滚记录，由恢复流程/重试补报收敛）
    UpsertLocalAttachRecord(recordCtx, verifyResp.nsVerifyList, UbseSsuAttachRecordState::ATTACHED);

    // 聚合块设备场景上报devName，master写入账本供重复挂载回填devPath
    UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHED, false,
                        blockDeviceOptions != nullptr ? devName : "");
    UBSE_LOG_INFO << "AgentAttach success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// agent端通用detach主流程：发送identity验证请求到master，从响应中获取namespace列表，
// 验证成功后逐个detach NS。可选先停止聚合块设备（stopBlockDevice=true，Linear/Striped场景）。
// 部分detach失败时保持ATTACHED状态（DetachDevNameSpace幂等，可重试收敛）；全部成功才回退为CREATED。
static uint32_t AgentDetach(const UbseSsuSpaceReq &req, const std::string &devName, bool stopBlockDevice,
                            bool isStriped = false)
{
    UbseSsuAttachDetachVerifyOption option; // detach场景：master校验state（ATTACHED正常/CREATED幂等）
    option.isAttach = false;
    // 卸载方式对应的分配策略（DetachSpace=NORMAL/DetachLinearSpace=LINEAR/DetachStripedSpace=STRIPED），
    // 与账本分配策略必须严格一致
    option.expectedStrategy = stopBlockDevice ?
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

    // identity验证通过后再停止聚合块设备（NS detach 之前），避免未授权操作导致状态不一致。
    // 停止前校验请求devName与账本记录一致（账本为attach时写入的权威值），防止误停他人/无关聚合块设备；
    // Stop 仅卸载运行态保留元数据，支持 re-attach 时 Assemble 复用，业务数据不丢失
    if (stopBlockDevice) {
        if (!verifyResp.devName.empty() && devName != verifyResp.devName) {
            UBSE_LOG_ERROR << "AgentDetach: devName mismatch, name=" << req.name << ", reqDevName=" << devName
                           << ", ledgerDevName=" << verifyResp.devName;
            return UBSE_ERR_INVALID_ARG;
        }
        if (!devName.empty()) {
            auto stopRet = UbseSsuAdapterInterface::GetInstance().StopBlockDevice(devName);
            if (stopRet != UBSE_OK) {
                UBSE_LOG_ERROR << "AgentDetach: StopBlockDevice failed, devName=" << devName << ", ret=" << stopRet;
                return UBSE_SSU_ERROR_BLOCK_DEVICE_STOP_FAILED;
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
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << nsRet;
            if (nsRet == UBSE_ERR_ACCESS_DENIED) {
                detachRet = nsRet; // 权限不足优先透传，上层可识别nqn白名单问题
            } else if (detachRet == UBSE_OK) {
                detachRet = UBSE_SSU_ERROR_DETACH_FAILED; // 保留原兜底语义
            }
        }
    }
    if (detachRet != UBSE_OK) {
        // 部分detach失败，保持ATTACHED状态，利用幂等性支持重试
        return detachRet;
    }

    UpdateStateOrNotify(req.name, UbseSsuNsState::CREATED, false);
    // 卸载成功：作废本地挂载记录
    RemoveLocalAttachRecord(req.name);
    UBSE_LOG_INFO << "AgentDetach success: name=" << req.name;
    return UBSE_OK;
}

// master端：校验attach/detach前置条件（identity、账本state、条带化参数）并返回构造AttachDevNameSpace所需字段。
// requestNodeId：发起节点ID，NORMAL策略多节点共挂场景下按节点维度判断幂等
uint32_t UbseSsuServiceImp::VerifyAttachDetachPrecondition(const std::string& name,
                                                           const UbseSsuAllocIdentityInfo& identity,
                                                           const std::string& requestNodeId,
                                                           const UbseSsuAttachDetachVerifyOption& option,
                                                           UbseSsuAttachDetachVerifyResp& verifyResp)
{
    if (name.empty()) {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (requestNodeId.empty()) {
        // NORMAL多节点共挂按节点判断幂等，nodeId缺失无法校验（agent端必传，属防御）
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: requestNodeId is empty, name=" << name;
        return UBSE_ERR_INVALID_ARG;
    }
    UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: name=" << name << ", requestNodeId=" << requestNodeId;

    // 账本未就绪门禁：与HandleStatusReceiver的MASTER_NOT_READY
    // 门禁对称，避免恢复窗口内Get落空被误报SPACE_NOT_FOUND误导SDK（纯重试语义）
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "VerifyAttachDetachPrecondition: ledger not ready, name=" << name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
    }

    auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(name);
    if (entryPtr == nullptr) {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: record not found, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }

    auto strategyRet = ValidateAllocStrategyMatch(name, entryPtr->allocResult.strategy, option.expectedStrategy);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // 设备故障拦截：FAULTED空间的attach/detach一律拒绝，
    // 防止对故障硬件误操作；free保留（人工止损出口）。设备恢复由collector周期对账回正，
    // 回正后本拦截自动解除
    if (entryPtr->state == UbseSsuNsState::FAULTED) {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: space faulted, name=" << name
                       << ", requestNodeId=" << requestNodeId;
        return UBSE_SSU_ERROR_SPACE_FAULTED;
    }

    // attach：ATTACHED为重复挂载（verifyResp.alreadyAttached标志由agent侧处理），CREATED为正常挂载状态，其他状态拒绝
    // detach：ATTACHED为正常卸载状态，CREATED为重复卸载（verifyResp.alreadyDetached标志由agent侧处理），其他状态拒绝
    // NORMAL策略多节点共挂：幂等判断下沉到节点维度（attachNodes），不依赖空间级state
    const bool isNormal = entryPtr->allocReq.strategy == UbseSsuAllocStrategy::NORMAL;
    if (option.isAttach) {
        if (isNormal) {
            // 白名单式校验：仅CREATED/ATTACHED可attach（本节点未挂时），
            // CREATING/IDLE/ATTACHING及未来新增状态一律拒绝
            if (entryPtr->state != UbseSsuNsState::CREATED && entryPtr->state != UbseSsuNsState::ATTACHED) {
                UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: invalid state for attach, name=" << name
                               << ", state=" << static_cast<int>(entryPtr->state);
                return UBSE_SSU_ERROR_STATE_INVALID;
            }
            // 本节点已挂载 → 置alreadyAttached，agent端回填devPath后返回UBSE_ERR_ALREADY_ATTACHED；
            // 其他节点挂载不影响本节点attach（多节点共挂）
            if (entryPtr->attachNodes.count(requestNodeId) > 0) {
                verifyResp.alreadyAttached = true;
                UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: already attached on node, name=" << name
                              << ", nodeId=" << requestNodeId;
                return UBSE_OK;
            }
        } else {
            if (entryPtr->state == UbseSsuNsState::ATTACHED) {
                // 独占策略：仅attacher重复attach为幂等；被其他节点占用时报错（不返回alreadyAttached避免误用devPath）
                if (entryPtr->attachNodes.count(requestNodeId) == 0) {
                    UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: attacher mismatch for attach, name=" << name
                                   << ", requestNodeId=" << requestNodeId
                                   << ", attacher=" << (entryPtr->attachNodes.empty() ? "" : *entryPtr->attachNodes.begin());
                    return UBSE_SSU_ERROR_ATTACHER_MISMATCH;
                }
                verifyResp.alreadyAttached = true;
                UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: already attached, name=" << name;
                return UBSE_OK;
            }
            if (entryPtr->state != UbseSsuNsState::CREATED) {
                UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: invalid state for attach, name=" << name
                               << ", state=" << static_cast<int>(entryPtr->state);
                return UBSE_SSU_ERROR_STATE_INVALID;
            }
            // 疑似本地残留挂载守卫：本机存在该空间的NS设备但
            // 本地挂载记录丢失（attacher未知），拒绝首个attach防双重挂载；
            // 人工核对后经free（自动摘除标记）或清理本地设备自愈。
            // 专用码区分于MASTER_NOT_READY（纯重试）：该状态无法自动解除，SDK据此感知需人工介入
            if (suspectedMountTable_.Contains(name)) {
                UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: suspected local mount, reject attach, name="
                               << name << ", requestNodeId=" << requestNodeId;
                return UBSE_SSU_ERROR_MOUNT_SUSPECTED;
            }
            // 恢复收敛窗口守卫：master重启后窗口内，硬件恢复的
            // 独占空间可能尚未收到attacher重报（attachNodes空、state=CREATED），拒绝其他节点
            // 首个attach防双重挂载；attacher收敛走重报不经过verify，SDK重试窗口后可通过
            if (entryPtr->restoredFromHardware && UbseSsuDebtLedger::GetInstance().IsInReconcileWindow()) {
                UBSE_LOG_WARN << "VerifyAttachDetachPrecondition: in reconcile window, defer attach, name=" << name
                               << ", requestNodeId=" << requestNodeId;
                return UBSE_SSU_ERROR_MASTER_NOT_READY;
            }
        }
    } else {
        if (isNormal) {
            // 本节点未挂载（含Rebuild后attachNodes为空）→ 置alreadyDetached，agent端不动设备并返回UBSE_ERR_NO_NEED_DETACH；
            // 其他节点挂载不影响本节点detach
            if (entryPtr->attachNodes.count(requestNodeId) == 0) {
                verifyResp.alreadyDetached = true;
                UBSE_LOG_INFO << "VerifyAttachDetachPrecondition: already detached on node, name=" << name
                              << ", nodeId=" << requestNodeId;
                return UBSE_OK;
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
            // 独占策略：仅attacher节点可卸载，防止其他节点误detach改变账本状态形成僵尸挂载
            if (entryPtr->attachNodes.count(requestNodeId) == 0) {
                UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition: attacher mismatch for detach, name=" << name
                               << ", requestNodeId=" << requestNodeId
                               << ", attacher=" << (entryPtr->attachNodes.empty() ? "" : *entryPtr->attachNodes.begin());
                return UBSE_SSU_ERROR_ATTACHER_MISMATCH;
            }
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
        return attachRet;
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
        return detachRet;
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
    // attach失败回退：作废本地PENDING记录（agent/master路径共用）
    RemoveLocalAttachRecord(req.name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RollbackAttachedNsAndLedger: partial detach failed, name=" << req.name;
    }
    return ret;
}

// ============================================================================
// VM 模式私有辅助实现
// 仅按照 UbseSsuVmCache / UbseSsuAdapterInterface 公开接口调用
// ============================================================================

// VM 模式 AttachSpace:校验缓存命中 → adapter AttachDevNameSpace → MarkAttached
// 幂等性:已 attach 直接返回成功
// 未在缓存中:返回 UBSE_SSU_ERROR_NS_NOT_FOUND
uint32_t UbseSsuServiceImp::VmAttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "VmAttachSpace: vmCache_ is null";
        return UBSE_ERROR_NULLPTR;
    }
    // 1) 校验缓存命中
    UbseSsuAllocResult allocResult{};
    auto cacheRet = vmCache_->GetAllocInfo(req.name, allocResult);
    if (cacheRet != UBSE_OK) {
        UBSE_LOG_WARN << "VmAttachSpace: name not in vm cache, name=" << req.name;
        return UBSE_SSU_ERROR_NS_NOT_FOUND;
    }
    // 2) 身份校验:与master路径AttachSingleNs语义一致,通过本地适配器customData校验identity,
    //    不匹配返回UBSE_ERR_ACCESS_DENIED(防止非属主attach任意缓存中的namespace)
    std::unordered_map<std::string, UbseSsuDevInfoPtr> devMap;
    std::unordered_set<std::string> refreshedEids;
    bool allNsFound = true;
    for (const auto &nsInfo : allocResult.nameSpaceList) {
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            // 本地适配器不可见:可能尚未直通或已被外部detach,记录后在attach前统一拒绝
            UBSE_LOG_WARN << "VmAttachSpace: namespace not found in local adapter, name=" << req.name
                          << ", eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            allNsFound = false;
            continue;
        }
        if (!IsNsIdentityMatch(*targetNs, req.identity)) {
            UBSE_LOG_ERROR << "VmAttachSpace: uid or userName not match, name=" << req.name
                           << ", eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            return UBSE_ERR_ACCESS_DENIED;
        }
    }
    // 3) 幂等性:已 attach 直接返回成功
    if (vmCache_->IsAttached(req.name)) {
        UBSE_LOG_INFO << "VmAttachSpace: already attached, name=" << req.name;
        nsDevPaths.clear();
        for (const auto &nsInfo : allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_OK;
    }
    // 4) attach前要求所有namespace在本地适配器可见(与AttachSingleNs语义一致),
    //    避免部分attach后因ns不可见失败产生回滚
    if (!allNsFound) {
        UBSE_LOG_ERROR << "VmAttachSpace: namespace not found in local adapter, name=" << req.name;
        return UBSE_SSU_ERROR_NS_NOT_FOUND;
    }
    // 5) 调用本地适配器 attach 每个 namespace
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    nsDevPaths.clear();
    nsDevPaths.reserve(allocResult.nameSpaceList.size());
    std::vector<std::string> attachedNqnList;
    attachedNqnList.reserve(allocResult.nameSpaceList.size());
    for (const auto &nsInfo : allocResult.nameSpaceList) {
        // VM 内 attach 使用已直通的设备,从缓存中的 UbseSsuNameSpaceInfo 构造 UbseSsuDevNameSpace
        // nqn 归一化与master路径一致:用户未指定时使用设备customData.defaultNqn
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        auto nqnFinal = (targetNs != nullptr) ? ResolveNqn(req.nqn, targetNs->customData.defaultNqn) : req.nqn;
        UbseSsuDevNameSpace ns{};
        ns.subSystem = {.eid = nsInfo.tgtEid, .subNqn = nsInfo.tgtNqn};
        ns.guid = nsInfo.nsUuid;
        ns.namespaceId = nsInfo.namespaceId;
        ns.nsDevPath = nsInfo.nsDevPath;
        auto attachRet = adapter.AttachDevNameSpace(nqnFinal, ns);
        if (attachRet != UBSE_OK) {
            UBSE_LOG_ERROR << "VmAttachSpace: adapter AttachDevNameSpace failed, name=" << req.name
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << attachRet;
            // 中途失败:逆序回滚已 attach 的 namespace,避免留下部分 attach 状态
            // (nsDevPaths/attachedNqnList 中记录的正是已成功 attach 的 namespace,与 nameSpaceList 前缀一一对应)
            for (size_t j = nsDevPaths.size(); j > 0; --j) {
                const auto &doneInfo = allocResult.nameSpaceList[j - 1];
                UbseSsuDevNameSpace doneNs{};
                doneNs.subSystem = {.eid = doneInfo.tgtEid, .subNqn = doneInfo.tgtNqn};
                doneNs.guid = doneInfo.nsUuid;
                doneNs.namespaceId = doneInfo.namespaceId;
                doneNs.nsDevPath = doneInfo.nsDevPath;
                auto rollbackRet = adapter.DetachDevNameSpace(attachedNqnList[j - 1], doneNs);
                if (rollbackRet != UBSE_OK) {
                    UBSE_LOG_ERROR << "VmAttachSpace: rollback DetachDevNameSpace failed, name=" << req.name
                                   << ", nsId=" << doneInfo.namespaceId << ", ret=" << rollbackRet;
                }
            }
            return attachRet;
        }
        nsDevPaths.push_back(nsInfo.nsDevPath);
        attachedNqnList.push_back(nqnFinal);
    }
    // 6) 标记已 attach
    vmCache_->MarkAttached(req.name);
    UBSE_LOG_INFO << "VmAttachSpace success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// VM 模式 DetachSpace:校验缓存命中 → adapter DetachDevNameSpace → MarkDetached
// 幂等性边界:
//   - 未在缓存中(VM 不感知该 namespace):返回 UBSE_SSU_ERROR_NS_NOT_FOUND
//   - 在缓存中但未 attach(重复 detach):返回 UBSE_OK(幂等容错)
uint32_t UbseSsuServiceImp::VmDetachSpace(const UbseSsuSpaceReq &req)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "VmDetachSpace: vmCache_ is null";
        return UBSE_ERROR_NULLPTR;
    }
    // 1) 校验缓存命中:未在缓存中视为不可 detach(VM 不感知该 namespace)
    UbseSsuAllocResult allocResult{};
    auto cacheRet = vmCache_->GetAllocInfo(req.name, allocResult);
    if (cacheRet != UBSE_OK) {
        UBSE_LOG_WARN << "VmDetachSpace: name not in vm cache, name=" << req.name;
        return UBSE_SSU_ERROR_NS_NOT_FOUND;
    }
    // 2) 幂等性:未 attach 直接返回成功
    if (!vmCache_->IsAttached(req.name)) {
        UBSE_LOG_INFO << "VmDetachSpace: not attached, idempotent success, name=" << req.name;
        return UBSE_OK;
    }
    // 3) 身份预校验:与master路径DetachSingleNs语义一致,通过本地适配器customData校验identity,
    //    任意一个ns不匹配即返回UBSE_ERR_ACCESS_DENIED(在任何设备操作前完成,避免部分detach);
    //    ns在本地适配器不可见视为已被外部detach,跳过设备操作(幂等)
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    std::unordered_map<std::string, UbseSsuDevInfoPtr> devMap;
    std::unordered_set<std::string> refreshedEids;
    std::unordered_set<uint32_t> goneNsIds;
    for (const auto &nsInfo : allocResult.nameSpaceList) {
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            UBSE_LOG_INFO << "VmDetachSpace: device or namespace not found in adapter, treat as already detached, name="
                          << req.name << ", eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            goneNsIds.insert(nsInfo.namespaceId);
            continue;
        }
        if (!IsNsIdentityMatch(*targetNs, req.identity)) {
            UBSE_LOG_ERROR << "VmDetachSpace: uid or userName not match, name=" << req.name
                           << ", eid=" << nsInfo.tgtEid << ", nsId=" << nsInfo.namespaceId;
            return UBSE_ERR_ACCESS_DENIED;
        }
    }
    // 4) 调用本地适配器 detach 每个 namespace
    for (const auto &nsInfo : allocResult.nameSpaceList) {
        if (goneNsIds.count(nsInfo.namespaceId) != 0) {
            continue;
        }
        // nqn 归一化与master路径一致:用户未指定时使用设备customData.defaultNqn
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        auto nqnFinal = (targetNs != nullptr) ? ResolveNqn(req.nqn, targetNs->customData.defaultNqn) : req.nqn;
        UbseSsuDevNameSpace ns{};
        ns.subSystem = {.eid = nsInfo.tgtEid, .subNqn = nsInfo.tgtNqn};
        ns.guid = nsInfo.nsUuid;
        ns.namespaceId = nsInfo.namespaceId;
        ns.nsDevPath = nsInfo.nsDevPath;
        auto detachRet = adapter.DetachDevNameSpace(nqnFinal, ns);
        if (detachRet != UBSE_OK) {
            UBSE_LOG_ERROR << "VmDetachSpace: adapter DetachDevNameSpace failed, name=" << req.name
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << detachRet;
            return detachRet;
        }
    }
    // 5) 标记已 detach
    vmCache_->MarkDetached(req.name);
    UBSE_LOG_INFO << "VmDetachSpace success: name=" << req.name;
    return UBSE_OK;
}

// VM 模式 AttachLinearSpace/AttachStripedSpace:复用 VmAttachSpace + adapter CreateBlockDevice
// options 由调用方构造并透传(LINEAR 或 STRIPED+level/chunkSize),返回聚合设备路径
uint32_t UbseSsuServiceImp::VmAttachAggregated(const UbseSsuLinearSpaceReq &req,
                                                std::vector<std::string> &nsDevPaths, std::string &devPath,
                                                const UbseCreateBlockDeviceOptions &options)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "VmAttachAggregated: vmCache_ is null";
        return UBSE_ERROR_NULLPTR;
    }
    // 1) 幂等性:已 attach 直接返回已挂载的 nsDevPath 列表和聚合设备路径,
    //    不重复调用 CreateBlockDevice
    if (vmCache_->IsAttached(req.name)) {
        UBSE_LOG_INFO << "VmAttachAggregated: already attached, idempotent success, name=" << req.name;
        if (!IsValidDevName(req.devName)) {
            UBSE_LOG_ERROR << "VmAttachAggregated: invalid devName, name=" << req.name
                           << ", devName=" << req.devName;
            return UBSE_ERR_INVALID_ARG;
        }
        nsDevPaths.clear();
        UbseSsuAllocResult allocResult{};
        auto cacheRet = vmCache_->GetAllocInfo(req.name, allocResult);
        if (cacheRet != UBSE_OK) {
            // 已 attach 但缓存查询失败:状态不一致,不得静默返回空设备列表
            UBSE_LOG_ERROR << "VmAttachAggregated: attached but GetAllocInfo failed, name=" << req.name
                           << ", ret=" << cacheRet;
            return UBSE_SSU_ERROR_STATE_INVALID;
        }
        for (const auto &nsInfo : allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        // 聚合块设备路径恒为/dev/ssu/{devName},与master端账本回填规则一致
        devPath = SSU_AGGREGATE_DEV_ROOT + req.devName;
        return UBSE_OK;
    }
    // 2) 先 attach 所有 namespace(复用 VmAttachSpace 逻辑)
    auto ret = VmAttachSpace(req, nsDevPaths);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 3) 创建聚合块设备,options 由调用方透传
    //    (Linear: LINEAR;Striped: STRIPED+level/chunkSize)
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    ret = adapter.CreateBlockDevice(req.devName, nsDevPaths, options, devPath);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VmAttachAggregated: CreateBlockDevice failed, name=" << req.name
                       << ", devName=" << req.devName << ", ret=" << ret;
        // 失败时回滚已 attach 的 namespace
        VmDetachSpace(req);
        return ret;
    }
    UBSE_LOG_INFO << "VmAttachAggregated success: name=" << req.name << ", devPath=" << devPath;
    return UBSE_OK;
}

// VM 模式 DetachLinearSpace/DetachStripedSpace:adapter DeleteBlockDevice + VmDetachSpace
uint32_t UbseSsuServiceImp::VmDetachAggregated(const UbseSsuLinearSpaceReq &req, bool isStriped)
{
    // 1) 删除聚合块设备
    //    聚合设备仍依赖各 namespace,删除失败时不得继续 detach 破坏依赖状态,直接报错;
    //    DeleteBlockDevice 幂等(块设备已删不会失败),调用方可重试收敛
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    auto ret = adapter.DeleteBlockDevice(req.devName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VmDetachAggregated: DeleteBlockDevice failed, strategy="
                       << (isStriped ? "STRIPED" : "LINEAR") << ", name=" << req.name
                       << ", devName=" << req.devName << ", ret=" << ret;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED;
    }
    // 2) detach 所有 namespace(幂等性保证)
    return VmDetachSpace(req);
}

// VM 模式 GetNsStats:校验 vmCache_ 命中 → 走本地适配器查询 namespace 统计
// VM 内无 master 账本与 collector 缓存,按需 RefreshDevCache(本地适配器 GetDevList)获取 nuse;
// identity 校验与 master 路径一致(IsNsIdentityMatch),不匹配返回 UBSE_ERR_ACCESS_DENIED
uint32_t UbseSsuServiceImp::VmGetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                                         const UbseSsuAllocIdentityInfo &identity)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "VmGetNsStats: vmCache_ is null";
        return UBSE_ERROR_NULLPTR;
    }
    // 1) 校验缓存命中:VM 内只感知推送过的 namespace,未在缓存中返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
    UbseSsuAllocResult allocResult{};
    auto cacheRet = vmCache_->GetAllocInfo(name, allocResult);
    if (cacheRet != UBSE_OK) {
        UBSE_LOG_WARN << "VmGetNsStats: name not in vm cache, name=" << name;
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }
    // 2) 构建本地设备映射:VM 内无 collector 缓存,起始为空,FindNsInDevice 未命中即触发 RefreshDevCache
    std::unordered_map<std::string, UbseSsuDevInfoPtr> devMap;
    std::unordered_set<std::string> refreshedEids;
    statsList.clear();
    statsList.reserve(allocResult.nameSpaceList.size());
    for (const auto &nsInfo : allocResult.nameSpaceList) {
        const auto *targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        if (targetNs == nullptr) {
            RefreshDevCache(nsInfo.tgtEid, nsInfo.tgtNqn, refreshedEids, devMap);
            targetNs = FindNsInDevice(devMap, nsInfo.tgtEid, nsInfo.nsUuid);
        }
        if (targetNs == nullptr) {
            UBSE_LOG_ERROR << "VmGetNsStats: namespace not found in local adapter, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            statsList.clear();
            return UBSE_SSU_ERROR_NS_NOT_FOUND;
        }
        if (!IsNsIdentityMatch(*targetNs, identity)) {
            UBSE_LOG_ERROR << "VmGetNsStats: identity not match, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId;
            statsList.clear();
            return UBSE_ERR_ACCESS_DENIED;
        }
        UbseSsuNsStats stats;
        stats.nsUuid = nsInfo.nsUuid;
        stats.nsId = nsInfo.namespaceId;
        stats.totalSize = nsInfo.nsSize;
        // targetNs->nuse为LBA数量，统一走NsUsedSizeBytes换算为字节(与master路径一致)
        stats.usedSize = NsUsedSizeBytes(*targetNs);
        statsList.push_back(std::move(stats));
    }
    UBSE_LOG_INFO << "VmGetNsStats success: name=" << name << ", count=" << statsList.size();
    return UBSE_OK;
}

// master本地挂载入口FAULTED拦截（与VerifyAttachDetachPrecondition对称）：
// 设备故障空间拒绝attach返回SPACE_FAULTED（1204，free为止损出口），collector周期对账回正后自动解除。
// 调用时机：幂等分支之前（verify侧同序——FAULTED空间即使本节点重复attach也报1204而非幂等）
static uint32_t CheckMasterAttachFaulted(const UbseSsuLedgerEntry &entry)
{
    if (entry.state == UbseSsuNsState::FAULTED) {
        UBSE_LOG_ERROR << "Master attach: space faulted, reject attach, name=" << entry.name;
        return UBSE_SSU_ERROR_SPACE_FAULTED;
    }
    return UBSE_OK;
}

// master本地挂载入口恢复期守卫（与verify侧对称，仅独占策略
// CREATED路径调用）：疑似本地残留（本机有NS设备但记录丢失，attacher未知）与恢复收敛窗口
// （attacher重报未达，attachNodes空）期间拒绝首个attach防独占设备双重挂载。
// 疑似残留回MOUNT_SUSPECTED（需人工核对，无法自动解除）；收敛窗口回MASTER_NOT_READY（纯重试，自动过期）。
// NORMAL多节点共挂无独占双重挂载风险，不设该守卫（与verify侧分支一致）
static uint32_t CheckMasterAttachRecoveryGuards(const UbseSsuLedgerEntry &entry,
                                               const UbseSsuSuspectedMountTable &suspectedTable)
{
    if (suspectedTable.Contains(entry.name)) {
        UBSE_LOG_ERROR << "Master attach: suspected local mount, reject attach, name=" << entry.name;
        return UBSE_SSU_ERROR_MOUNT_SUSPECTED;
    }
    if (entry.restoredFromHardware && UbseSsuDebtLedger::GetInstance().IsInReconcileWindow()) {
        UBSE_LOG_WARN << "Master attach: in reconcile window, defer attach, name=" << entry.name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
    }
    return UBSE_OK;
}

// 挂载空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::AttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "AttachSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "AttachSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachSpace: name=" << req.name << ", nqn=" << req.nqn;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    // VM 模式:校验 vmCache_ 命中 → 调 adapter AttachDevNameSpace → MarkAttached
    // 幂等性:已 attach 直接返回成功;未在缓存中返回 UBSE_SSU_ERROR_NS_NOT_FOUND
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::NORMAL);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        return VmAttachSpace(req, nsDevPaths);
    }

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
    // 账本未就绪门禁（与verify侧对称）：恢复期Get落空会误报
    // SPACE_NOT_FOUND误导SDK，先拒未就绪请求（MASTER_NOT_READY纯重试语义）
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "AttachSpace: ledger not ready, name=" << req.name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
    }
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

    // FAULTED拦截（与verify侧对称）
    auto faultedRet = CheckMasterAttachFaulted(*entryPtr);
    if (faultedRet != UBSE_OK) {
        return faultedRet;
    }

    // NORMAL多节点共挂：幂等与状态校验按节点维度判断（上方策略校验已保证strategy必为NORMAL）
    std::string selfNodeId;
    auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace: get current node id failed, ret=" << nodeIdRet;
        return nodeIdRet;
    }
    // 本节点已挂载，再次attach返回已挂载的nsDevPath列表；其他节点挂载不影响本节点attach
    if (entryPtr->attachNodes.count(selfNodeId) > 0) {
        UBSE_LOG_INFO << "AttachSpace: already attached on node, name=" << req.name << ", nodeId=" << selfNodeId;
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            nsDevPaths.push_back(nsInfo.nsDevPath);
        }
        return UBSE_ERR_ALREADY_ATTACHED;
    }
    // 白名单式校验：仅CREATED/ATTACHED（本节点未挂时）可attach，与verify侧校验保持一致
    if (entryPtr->state != UbseSsuNsState::CREATED && entryPtr->state != UbseSsuNsState::ATTACHED) {
        UBSE_LOG_ERROR << "AttachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    // 本函数仅处理NORMAL空间，无需置空间级ATTACHING：
    // NORMAL多节点共挂下该标记会阻塞其他节点并发attach，
    // master本地并发互斥已由入口处per-name resourceLock保证（见UpdateStateOrNotify的策略分支注释）

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

    // master本地挂载成功：写ATTACHED记录（NORMAL无聚合设备，从collector缓存构造verify回放字段）
    const LocalAttachRecordCtx recordCtx{req.name, UbseSsuAllocStrategy::NORMAL, "", req.nqn, req.identity,
                                         nullptr, attachedNsList};
    UpsertLocalAttachRecordFromDevMap(recordCtx, devMap);

    UBSE_LOG_INFO << "AttachSpace success: name=" << req.name << ", nsCount=" << nsDevPaths.size();
    return UBSE_OK;
}

// detach空间主入口，agent/master节点都可调用
// 卸载空间主入口（NORMAL 分配策略）。
// NORMAL 策略无聚合块设备，仅 detach 底层 NS；不会触碰 LVM/mdadm 元数据。
// LINEAR/STRIPED 场景由 DetachLinearSpace/DetachStripedSpace 路由，先 Stop 聚合块设备（保留元数据）再 detach NS，
uint32_t UbseSsuServiceImp::DetachSpace(const UbseSsuSpaceReq &req)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "DetachSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "DetachSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachSpace: name=" << req.name << ", nqn=" << req.nqn;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    // VM 模式:调 adapter DetachDevNameSpace → MarkDetached
    // 幂等性:未 attach 直接返回成功
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::NORMAL);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        return VmDetachSpace(req);
    }

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

    // 分配策略校验：NORMAL策略只能通过DetachSpace卸载（不删除聚合块设备），
    // LINEAR/STRIPED策略必须使用配套的DetachLinearSpace/DetachStripedSpace。
    // 先于幂等/状态校验执行：策略不匹配直接拒绝，避免对聚合设备空间做NORMAL语义的节点级判断
    auto strategyRet =
        ValidateAllocStrategyMatch(req.name, entryPtr->allocResult.strategy, UbseSsuAllocStrategy::NORMAL);
    if (strategyRet != UBSE_OK) {
        return strategyRet;
    }

    // NORMAL多节点共挂：幂等判断按节点维度（本节点未挂载即为重复卸载）；
    // 上方策略校验已保证strategy必为NORMAL
    std::string selfNodeId;
    auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DetachSpace: get current node id failed, ret=" << nodeIdRet;
        return nodeIdRet;
    }
    // 本节点未挂载，再次detach一律报错（含Rebuild后attachNodes为空的场景）；其他节点挂载不影响本节点detach
    if (entryPtr->attachNodes.count(selfNodeId) == 0) {
        UBSE_LOG_ERROR << "DetachSpace: no need to detach on node, name=" << req.name << ", nodeId=" << selfNodeId;
        return UBSE_ERR_NO_NEED_DETACH;
    }
    // 白名单式校验：节点在挂载记录中时state必为ATTACHED/CREATED（rebuild兜底），其余状态拒绝
    if (entryPtr->state != UbseSsuNsState::ATTACHED && entryPtr->state != UbseSsuNsState::CREATED) {
        UBSE_LOG_ERROR << "DetachSpace: invalid state, name=" << req.name
                       << ", state=" << static_cast<int>(entryPtr->state);
        return UBSE_SSU_ERROR_STATE_INVALID;
    }

    auto devMap = collector_.GetCachedDevMap();
    uint32_t detachRet = UBSE_OK;
    for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
        auto ret = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "DetachSpace: DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << ret;
            if (ret == UBSE_ERR_ACCESS_DENIED) {
                detachRet = ret; // 权限不足优先透传，上层可识别nqn白名单问题
            } else if (detachRet == UBSE_OK) {
                detachRet = UBSE_SSU_ERROR_DETACH_FAILED; // 保留原兜底语义
            }
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
    // 卸载成功：作废本地挂载记录
    RemoveLocalAttachRecord(req.name);

    UBSE_LOG_INFO << "DetachSpace success: name=" << req.name
                  << ", nsCount=" << entryPtr->allocResult.nameSpaceList.size();
    return UBSE_OK;
}

// master本地attach/detach路径：校验本机节点为attacher（LINEAR/STRIPED独占策略仅attacher节点可操作）
static uint32_t ValidateMasterNodeIsAttacher(const UbseSsuLedgerEntry &entry, const std::string &name)
{
    std::string selfNodeId;
    auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        // 透传错误码保证可诊断性
        UBSE_LOG_ERROR << "ValidateMasterNodeIsAttacher: get current node id failed, " << FormatRetCode(nodeIdRet)
                       << ", name=" << name;
        return nodeIdRet;
    }
    if (entry.attachNodes.count(selfNodeId) == 0) {
        UBSE_LOG_ERROR << "ValidateMasterNodeIsAttacher: attacher mismatch, name=" << name
                       << ", selfNodeId=" << selfNodeId
                       << ", attacher=" << (entry.attachNodes.empty() ? "" : *entry.attachNodes.begin());
        return UBSE_SSU_ERROR_ATTACHER_MISMATCH;
    }
    return UBSE_OK;
}

// AttachStripedSpace和AttachLinearSpace通用辅助输出
struct AttachNsCreateBlockDeviceOutput {
    std::vector<std::string> nsDevPaths;
    std::string devPath;
};

// master端AttachStripedSpace和AttachLinearSpace通用辅助
// attach所有NS + 创建/重组聚合块设备 + 账本状态管理（ATTACHING -> ATTACHED / 失败回退）
// ledgerDevName：账本记录的 devName（可能为空=首次挂载；非空=重挂，需 Probe 判定走 Assemble 或降级 Create）
static uint32_t AttachNsAndCreateBlockDevice(const UbseSsuLinearSpaceReq &req,
                                             const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList,
                                             std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap,
                                             const UbseCreateBlockDeviceOptions &options,
                                             const std::string &ledgerDevName,
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

    // 决策 Create / Assemble（委托 SetupBlockDeviceWithProbe 统一处理）：
    uint32_t setupRet = SetupBlockDeviceWithProbe(tag, req.devName, ledgerDevName,
        output.nsDevPaths, options, output.devPath);
    if (setupRet != UBSE_OK) {
        RollbackAttachedNsAndLedger(attachedNsList, req, devMap);
        return setupRet;
    }

    // 同步写入聚合块设备名到账本，供重复挂载回填devPath（devPath恒为/dev/ssu/{devName}）
    UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHED, true, req.devName);

    // master本地挂载成功：写ATTACHED记录（LINEAR/STRIPED聚合设备，从collector缓存构造verify回放字段）
    const auto recordStrategy = (options.addressingType == UbseSsuAddressingType::STRIPED) ?
                                    UbseSsuAllocStrategy::STRIPED :
                                    UbseSsuAllocStrategy::LINEAR;
    const LocalAttachRecordCtx recordCtx{req.name, recordStrategy, req.devName, req.nqn, req.identity, &options,
                                         attachedNsList};
    UpsertLocalAttachRecordFromDevMap(recordCtx, devMap);
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
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "AttachLinearSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    UbseCreateBlockDeviceOptions options;
    options.addressingType = UbseSsuAddressingType::LINEAR;

    // VM 模式:走本地适配器,返回聚合块设备路径,透传 LINEAR options
    // 幂等性:已 attach 直接返回成功
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::LINEAR);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        UBSE_LOG_INFO << "AttachLinearSpace: VM mode, name=" << req.name << ", devName=" << req.devName;
        return VmAttachAggregated(req, nsDevPaths, devPath, options);
    }

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
    // 账本未就绪门禁（与verify侧对称）：恢复期Get落空会误报
    // SPACE_NOT_FOUND误导SDK，先拒未就绪请求（MASTER_NOT_READY纯重试语义）
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "AttachLinearSpace: ledger not ready, name=" << req.name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
    }
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

    // FAULTED拦截（与verify侧对称）
    auto faultedRet = CheckMasterAttachFaulted(*entryPtr);
    if (faultedRet != UBSE_OK) {
        return faultedRet;
    }

    // 重复挂载：已经attached，再次attach需要返回已挂载的nsDevPath列表
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        // 独占策略：仅attacher节点重复attach为幂等；被其他节点占用时报错，避免误用devPath
        auto attacherRet = ValidateMasterNodeIsAttacher(*entryPtr, req.name);
        if (attacherRet != UBSE_OK) {
            return attacherRet;
        }
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

    // 恢复期守卫（与verify侧对称，仅独占CREATED路径）
    auto guardRet = CheckMasterAttachRecoveryGuards(*entryPtr, suspectedMountTable_);
    if (guardRet != UBSE_OK) {
        return guardRet;
    }

    // 标记为attaching中
    if (UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHING, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachLinearSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
    }

    auto devMap = collector_.GetCachedDevMap();
    AttachNsCreateBlockDeviceOutput output;
    auto ret = AttachNsAndCreateBlockDevice(req, entryPtr->allocResult.nameSpaceList, devMap, options,
                                            entryPtr->devName, output);
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
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "AttachStripedSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "AttachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName
                  << ", level=" << static_cast<int>(req.level)
                  << ", chunkSize=" << static_cast<uint32_t>(req.chunkSize);
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    UbseCreateBlockDeviceOptions options = {.addressingType = UbseSsuAddressingType::STRIPED,
                                            .raidLevel = req.level == UbseSsuAggregationRaidLevel::RAID0 ?
                                                             UbseSsuRaidLevel::RAID0 :
                                                             UbseSsuRaidLevel::RAID5,
                                            .chunkSize = static_cast<uint32_t>(req.chunkSize)};

    // VM 模式:走本地适配器,返回聚合块设备路径,透传 level/chunkSize
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::STRIPED);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        UBSE_LOG_INFO << "AttachStripedSpace: VM mode, name=" << req.name << ", devName=" << req.devName;
        return VmAttachAggregated(req, nsDevPaths, devPath, options);
    }

    std::string role;
    auto roleRet = UbseGetRole(role);
    if (roleRet != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to get node role, ret=" << roleRet;
        return roleRet;
    }

    // Agent: 无本地账本，直接通过verify RPC获取ns列表后attach + create block device
    if (role != ELECTION_ROLE_MASTER) {
        return AgentAttach(req, req.devName, &options, nsDevPaths, devPath);
    }

    // Master: 检查本地账本后执行
    // 账本未就绪门禁（与verify侧对称）：恢复期Get落空会误报
    // SPACE_NOT_FOUND误导SDK，先拒未就绪请求（MASTER_NOT_READY纯重试语义）
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "AttachStripedSpace: ledger not ready, name=" << req.name;
        return UBSE_SSU_ERROR_MASTER_NOT_READY;
    }
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

    // FAULTED拦截（与verify侧对称）
    auto faultedRet = CheckMasterAttachFaulted(*entryPtr);
    if (faultedRet != UBSE_OK) {
        return faultedRet;
    }

    // 重复挂载：已经attached，再次attach返回已挂载的nsDevPath列表
    if (entryPtr->state == UbseSsuNsState::ATTACHED) {
        // 独占策略：仅attacher重复attach为幂等；被其他节点占用时报错，避免误用devPath
        auto attacherRet = ValidateMasterNodeIsAttacher(*entryPtr, req.name);
        if (attacherRet != UBSE_OK) {
            return attacherRet;
        }
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

    // 恢复期守卫（与verify侧对称，仅独占CREATED路径）
    auto guardRet = CheckMasterAttachRecoveryGuards(*entryPtr, suspectedMountTable_);
    if (guardRet != UBSE_OK) {
        return guardRet;
    }

    // 标记为attaching中
    if (UpdateStateOrNotify(req.name, UbseSsuNsState::ATTACHING, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachStripedSpace: failed to update ledger state to ATTACHING, name=" << req.name;
        return UBSE_SSU_ERROR_LEDGER_MODIFY_FAILED;
    }

    auto devMap = collector_.GetCachedDevMap();
    AttachNsCreateBlockDeviceOutput output;
    ret = AttachNsAndCreateBlockDevice(req, entryPtr->allocResult.nameSpaceList, devMap, options,
                                       entryPtr->devName, output);
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
// 停止聚合块设备运行态(保留元数据) + detach所有NS + 账本状态回退（ATTACHED -> CREATED）
static uint32_t DetachNsAndStopBlockDevice(const std::string& tag, const UbseSsuLinearSpaceReq& req,
                                           const std::vector<UbseSsuNameSpaceInfo>& nameSpaceList,
                                           std::unordered_map<std::string, UbseSsuDevInfoPtr>& devMap,
                                           const std::string& expectedDevName)
{
    // 停止前校验请求的devName与账本记录一致（账本为attach时写入的权威值），防止误停他人/无关聚合块设备；
    if (!expectedDevName.empty() && req.devName != expectedDevName) {
        UBSE_LOG_ERROR << tag << ": devName mismatch, name=" << req.name << ", reqDevName=" << req.devName
                       << ", ledgerDevName=" << expectedDevName;
        return UBSE_ERR_INVALID_ARG;
    }
    // Stop 仅卸载运行态保留元数据，支持 re-attach 时 Assemble 复用，业务数据不丢失
    auto stopRet = UbseSsuAdapterInterface::GetInstance().StopBlockDevice(req.devName);
    if (stopRet != UBSE_OK) {
        UBSE_LOG_ERROR << tag << ": StopBlockDevice failed, devName=" << req.devName << ", ret=" << stopRet;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_STOP_FAILED;
    }

    uint32_t ret = UBSE_OK;
    for (const auto &nsInfo : nameSpaceList) {
        auto detachRet = DetachSingleNs(nsInfo, req.identity, req.nqn, devMap);
        if (detachRet != UBSE_OK) {
            UBSE_LOG_ERROR << tag << ": DetachSingleNs failed, eid=" << nsInfo.tgtEid
                           << ", nsId=" << nsInfo.namespaceId << ", ret=" << detachRet;
            if (detachRet == UBSE_ERR_ACCESS_DENIED) {
                ret = detachRet; // 权限不足优先透传，上层可识别nqn白名单问题
            } else if (ret == UBSE_OK) {
                ret = UBSE_SSU_ERROR_DETACH_FAILED; // 保留原兜底语义
            }
        }
    }
    if (ret != UBSE_OK) {
        // 部分NS detach失败：账本state刻意保持ATTACHED不变。
        // 1) DetachDevNameSpace幂等（见适配器契约），调用方可重试，已detach的NS会再次返回成功，重试收敛；
        //    StopBlockDevice同样幂等，重试时不会因块设备已停而失败
        // 2) ExecuteFree要求state==CREATED才允许释放NS，保持ATTACHED可阻止在仍有NS挂在host上时误删NS，
        //    也可阻止在"块设备已停但部分NS仍挂在host上"的脏状态上误调Attach*重建聚合设备
        //    （Attach入口会先 Probe 命中已停的元数据走 Assemble，而非盲目 Create）
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
    // 卸载成功：作废本地挂载记录
    RemoveLocalAttachRecord(req.name);
    return UBSE_OK;
}

// detach线性空间主入口，agent/master节点都可调用
uint32_t UbseSsuServiceImp::DetachLinearSpace(const UbseSsuLinearSpaceReq &req)
{
    if (req.name.empty()) {
        UBSE_LOG_ERROR << "DetachLinearSpace: name is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "DetachLinearSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachLinearSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    // VM 模式:DeleteBlockDevice + VmDetachSpace
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::LINEAR);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        UBSE_LOG_INFO << "DetachLinearSpace: VM mode, name=" << req.name << ", devName=" << req.devName;
        return VmDetachAggregated(req, false);
    }

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

    // 独占策略：仅attacher可卸载
    auto detacherRet = ValidateMasterNodeIsAttacher(*entryPtr, req.name);
    if (detacherRet != UBSE_OK) {
        return detacherRet;
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndStopBlockDevice("DetachLinearSpace", req, entryPtr->allocResult.nameSpaceList, devMap,
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
    if (!IsOptionalNqnValid(req.nqn)) {
        UBSE_LOG_ERROR << "DetachStripedSpace: invalid nqn format, nqn=" << req.nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    UBSE_LOG_INFO << "DetachStripedSpace: name=" << req.name << ", nqn=" << req.nqn << ", devName=" << req.devName;
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(req.name);

    // VM 模式:DeleteBlockDevice + VmDetachSpace
    if (vmMode_) {
        auto strategyRet = ValidateVmAllocStrategy(vmCache_, req.name, UbseSsuAllocStrategy::STRIPED);
        if (strategyRet != UBSE_OK) {
            return strategyRet;
        }
        UBSE_LOG_INFO << "DetachStripedSpace: VM mode, name=" << req.name << ", devName=" << req.devName;
        return VmDetachAggregated(req, true);
    }

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

    // 独占策略：仅attacher可卸载
    auto detacherRet = ValidateMasterNodeIsAttacher(*entryPtr, req.name);
    if (detacherRet != UBSE_OK) {
        return detacherRet;
    }

    auto devMap = collector_.GetCachedDevMap();
    auto ret = DetachNsAndStopBlockDevice("DetachStripedSpace", req, entryPtr->allocResult.nameSpaceList, devMap,
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

    // 空nqn表示使用defaultNqn（内部由ResolveNqn处理，为defaultNqn时跳过），不校验；非空则须为合法HostNqn格式
    if (!IsOptionalNqnValid(nqn)) {
        UBSE_LOG_ERROR << "AddAccessPermission: invalid nqn format, nqn=" << nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    // VM 模式不支持权限管理(权限由 ubse@host 管理)
    if (vmMode_) {
        UBSE_LOG_WARN << "AddAccessPermission: not supported in VM mode, name=" << name;
        return UBSE_ERR_NOT_SUPPORTED;
    }

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

    // 空nqn表示使用defaultNqn（内部由ResolveNqn处理，为defaultNqn时跳过），不校验；非空则须为合法HostNqn格式
    if (!IsOptionalNqnValid(nqn)) {
        UBSE_LOG_ERROR << "RemoveAccessPermission: invalid nqn format, nqn=" << nqn;
        return UBSE_ERR_INVALID_ARG;
    }

    // VM 模式不支持权限管理(权限由 ubse@host 管理)
    if (vmMode_) {
        UBSE_LOG_WARN << "RemoveAccessPermission: not supported in VM mode, name=" << name;
        return UBSE_ERR_NOT_SUPPORTED;
    }

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
    // VM 模式不支持 FeDevice 查询(设备直通由 ubse@host 负责)
    if (vmMode_) {
        UBSE_LOG_WARN << "GetFeDeviceList: not supported in VM mode";
        return UBSE_ERR_NOT_SUPPORTED;
    }
    return UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList);
}

uint32_t UbseSsuServiceImp::FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid)
{
    // VM 模式不支持 FeDevice 分配(设备直通由 ubse@host 负责)
    if (vmMode_) {
        UBSE_LOG_WARN << "FeDeviceAlloc: not supported in VM mode, upi=" << upi;
        return UBSE_ERR_NOT_SUPPORTED;
    }
    return UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(upi, vfe, busInstanceGuid);
}

uint32_t UbseSsuServiceImp::FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe)
{
    // VM 模式不支持 FeDevice 释放(设备直通由 ubse@host 负责)
    if (vmMode_) {
        UBSE_LOG_WARN << "FeDeviceFree: not supported in VM mode, upi=" << upi;
        return UBSE_ERR_NOT_SUPPORTED;
    }
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
        // targetNs->nuse为LBA数量，统一走NsUsedSizeBytes换算为字节，与API契约(单位字节)保持一致
        stats.usedSize = NsUsedSizeBytes(*targetNs);
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

    // VM 模式走本地适配器查询 namespace 统计
    // VM 内无 master 账本与 collector,通过本地适配器 GetDevList 获取 usedSize(nuse)
    if (vmMode_) {
        UBSE_LOG_INFO << "GetNsStats: VM mode, query via local adapter, name=" << name;
        return VmGetNsStats(name, statsList, identity);
    }

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

// 校验allocResult是否属于指定identity:所有能在本地适配器中查到的namespace的identity都必须匹配,
// 且至少查到过1个namespace(anyChecked);VM 模式查询分支与 master 账本校验共用该语义
// (VM 内无账本,identity 权威源为本地适配器 customData)
static bool IsAllocResultIdentityMatch(const UbseSsuAllocResult &allocResult,
                                       const UbseSsuAllocIdentityInfo &identity,
                                       std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    std::unordered_set<std::string> refreshedEids;
    bool anyChecked = false;
    for (const auto &nsInfo : allocResult.nameSpaceList) {
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
    return anyChecked;
}

// 校验ledger entry是否属于指定identity(委托 IsAllocResultIdentityMatch,语义一致)
static bool IsLedgerEntryIdentityMatch(const UbseSsuLedgerEntry &entry, const UbseSsuAllocIdentityInfo &identity,
                                       std::unordered_map<std::string, UbseSsuDevInfoPtr> &devMap)
{
    return IsAllocResultIdentityMatch(entry.allocResult, identity, devMap);
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
    // VM 模式走本地缓存,缓存为只读,VM 内不可修改分配信息;未收到推送时返回空列表
    // 身份校验:identity 权威源为本地适配器 customData,过滤非本 identity 的条目
    if (vmMode_) {
        UBSE_LOG_INFO << "ListAllocInfo: VM mode, query from local cache";
        if (vmCache_ == nullptr) {
            UBSE_LOG_ERROR << "ListAllocInfo: vmCache_ is null";
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = vmCache_->ListAllocInfo(result);
        if (ret != UBSE_OK) {
            return ret;
        }
        auto devMap = collector_.GetCachedDevMap();
        for (auto it = result.begin(); it != result.end();) {
            if (!IsAllocResultIdentityMatch(*it, identity, devMap)) {
                UBSE_LOG_WARN << "ListAllocInfo: VM mode, identity not match, name=" << it->name;
                it = result.erase(it);
            } else {
                ++it;
            }
        }
        return UBSE_OK;
    }

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

    // VM 模式走本地缓存
    // 缓存中不存在返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
    // 身份校验:不匹配返回 UBSE_ERR_ACCESS_DENIED,语义与 master 路径一致
    if (vmMode_) {
        UBSE_LOG_INFO << "GetAllocInfoByName: VM mode, query from local cache, name=" << name;
        if (vmCache_ == nullptr) {
            UBSE_LOG_ERROR << "GetAllocInfoByName: vmCache_ is null";
            return UBSE_ERROR_NULLPTR;
        }
        auto cacheRet = vmCache_->GetAllocInfo(name, result);
        if (cacheRet != UBSE_OK) {
            UBSE_LOG_WARN << "GetAllocInfoByName: not found in vm cache, name=" << name;
            return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
        }
        auto devMap = collector_.GetCachedDevMap();
        if (!IsAllocResultIdentityMatch(result, identity, devMap)) {
            UBSE_LOG_WARN << "GetAllocInfoByName: VM mode, identity not match, name=" << name;
            return UBSE_ERR_ACCESS_DENIED;
        }
        return UBSE_OK;
    }

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

// 设置连接信息中的srcEid字段，从ubse.service环境变量SSU_SRC_EID读取源端EID
// 校验逻辑：与适配器GetSrcEid保持一致
static uint32_t SetSrcEidInfo(const UbseSsuVfe *vfe, std::vector<UbseSsuConnectInfo> &connectInfoList)
{
    (void)vfe;
    constexpr size_t maxEidStrLen = ipc::message::MAX_EID_LEN - 1;
    // 从ubse.service环境变量SSU_SRC_EID读取源端EID
    std::string eidStr = ubse::utils::GetEnv<std::string>("SSU_SRC_EID", "");
    if (eidStr.empty()) {
        return UBSE_OK;
    }
    // 与适配器GetSrcEid保持一致：超长时截断到EID_SIZE，不返回错误
    if (eidStr.size() > maxEidStrLen) {
        eidStr.resize(maxEidStrLen);
    }
    for (auto &info : connectInfoList) {
        info.srcEid = eidStr;
    }
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

    // VM 模式走本地缓存
    // 身份校验:通过缓存中的分配信息走本地适配器校验 identity,不匹配返回 UBSE_ERR_ACCESS_DENIED;
    // 分配信息未推送(缓存miss)时无法校验身份,fail-closed直接拒绝,避免非属主读取连接信息
    // (与master路径ExecuteGetConnectInfo的fail-closed语义一致)
    if (vmMode_) {
        UBSE_LOG_INFO << "GetConnectInfo: VM mode, query from local cache, name=" << name;
        if (vmCache_ == nullptr) {
            UBSE_LOG_ERROR << "GetConnectInfo: vmCache_ is null";
            return UBSE_ERROR_NULLPTR;
        }
        (void)vfe; // VM 模式下 vfe 参数无意义,连接信息已由 ub-device-manager 推送
        UbseSsuAllocResult allocResult{};
        if (vmCache_->GetAllocInfo(name, allocResult) != UBSE_OK) {
            UBSE_LOG_WARN << "GetConnectInfo: VM mode, name not in vm cache, name=" << name;
            return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
        }
        auto devMap = collector_.GetCachedDevMap();
        if (!IsAllocResultIdentityMatch(allocResult, identity, devMap)) {
            UBSE_LOG_WARN << "GetConnectInfo: VM mode, identity not match, name=" << name;
            return UBSE_ERR_ACCESS_DENIED;
        }
        return vmCache_->GetConnectInfo(name, connectInfoList);
    }

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

// 疑似本地残留挂载集合构建：master账本恢复后对本机反查探测——
// 独占策略空间任一nsDevPath本机存在，但本地挂载记录中无该空间 → 挂载记录丢失、attacher未知，
// 记入疑似集合（verify拒绝首个attach防双重挂载）并告警提示人工核对。
// 幂等：每次恢复全量重建；账本未就绪时跳过（避免空账本清空守卫）
void UbseSsuServiceImp::BuildSuspectedLocalMounts()
{
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        return;
    }
    std::unordered_set<std::string> recordedNames;
    for (const auto &record : UbseSsuAttachRecordStore::GetInstance().GetAll()) {
        recordedNames.insert(record.name);
    }
    std::unordered_set<std::string> suspected;
    for (const auto &entryPtr : UbseSsuDebtLedger::GetInstance().GetAll()) {
        // NORMAL多节点共挂无独占双重挂载风险，不守卫
        if (entryPtr->allocReq.strategy == UbseSsuAllocStrategy::NORMAL) {
            continue;
        }
        // 本地有挂载记录：正常恢复路径已覆盖（ReconcileMasterLedger），不守卫
        if (recordedNames.count(entryPtr->name) > 0) {
            continue;
        }
        for (const auto &nsInfo : entryPtr->allocResult.nameSpaceList) {
            // 本机存在该空间的NS设备但无挂载记录：记录丢失且设备残留，attacher未知
            if (access(nsInfo.nsDevPath.c_str(), F_OK) == 0) {
                suspected.insert(entryPtr->name);
                UBSE_LOG_ERROR << "SuspectedLocalMount: local ns device exists but attach record missing, name="
                               << entryPtr->name << ", nsDevPath=" << nsInfo.nsDevPath
                               << ". Attach will be rejected until manual check (free space or clean local devices)";
                break;
            }
        }
    }
    suspectedMountTable_.Swap(std::move(suspected));
}

// 设备故障周期增量对账：每个采集成功周期由collector回调触发。
// 防抖：空间连续SSU_FAULT_DEBOUNCE_CYCLES个周期未出现在硬件采集列表才判FAULTED，规避单次
// 采集抖动误判；重新出现即清零，FAULTED回正（attachNodes非空回ATTACHED、否则回CREATED）。
// 告警：FAULTED置位/回正均输出带关键字的可检索日志（xalarm面向sysSentry故障事件，SSU设备
// 告警类型接入留作后续）
void UbseSsuServiceImp::ReconcileSpaceFaultStates(const std::vector<UbseSsuDevInfo> &devList)
{
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    if (!ledger.IsReady()) {
        return; // 账本未就绪（Rebuild前），硬件缺失语义未知，不判故障
    }
    // 提取本周期硬件在线空间名集合（与Rebuild的customData.name提取口径一致）
    std::unordered_set<std::string> onlineNames;
    for (const auto &dev : devList) {
        for (const auto &ns : dev.nameSpaces) {
            if (ns.customData.name[0] != '\0') {
                onlineNames.insert(std::string(ns.customData.name,
                                                strnlen(ns.customData.name, sizeof(ns.customData.name))));
            }
        }
    }
    auto entries = ledger.GetAll();
    std::unordered_set<std::string> managedNames;
    for (const auto &entry : entries) {
        const auto &name = entry->name;
        // 仅管理"已落到硬件"的空间：CREATING（创建中，NS尚未写入）与IDLE（失败回退）不判故障
        if (entry->state == UbseSsuNsState::CREATING || entry->state == UbseSsuNsState::IDLE) {
            continue;
        }
        managedNames.insert(name);
        if (onlineNames.count(name) > 0) {
            // 恢复即清零；已FAULTED的回正（attachNodes非空=attacher仍在挂载→ATTACHED，否则CREATED）
            faultDebouncer_.Reset(name);
            if (entry->state == UbseSsuNsState::FAULTED) {
                ledger.Modify(name, [](UbseSsuLedgerEntry &e) {
                    e.state = e.attachNodes.empty() ? UbseSsuNsState::CREATED : UbseSsuNsState::ATTACHED;
                });
                UBSE_LOG_INFO << "SpaceFaultRecover: space back online, state restored, name=" << name;
            }
            continue;
        }
        uint32_t count = faultDebouncer_.Increment(name);
        if (count >= SSU_FAULT_DEBOUNCE_CYCLES && entry->state != UbseSsuNsState::FAULTED) {
            ledger.Modify(name, [](UbseSsuLedgerEntry &e) { e.state = UbseSsuNsState::FAULTED; });
            // RAS告警：关键字SpaceFaultAlarm供运维检索（设备故障告警接入xalarm留作后续）
            UBSE_LOG_ERROR << "SpaceFaultAlarm: space missing from hardware for " << count
                           << " consecutive collect cycles, mark FAULTED, name=" << name
                           << ", strategy=" << static_cast<int>(entry->allocReq.strategy);
        }
    }
    // 清理已不在账本的空间的残留计数（free/Remove后防抖表不泄漏）
    faultDebouncer_.PruneStale(managedNames);
}

// 节点离线事件记录：仅记录首次离线时刻（幂等，抖动重复事件不刷新），
// 离场判定由周期对账按持续时间做出；仅master收到NODE_DOWN事件
void UbseSsuServiceImp::OnNodeDownEvent(const std::string &nodeId)
{
    if (nodeId.empty()) {
        return;
    }
    nodeOfflineTracker_.OnDown(nodeId);
    UBSE_LOG_INFO << "NodeOffline: node down detected, nodeId=" << nodeId
                  << ", offlineSinceCount=" << nodeOfflineTracker_.DownCount();
}

// 节点回归事件：清除离线记录，节点回归后其重报/运营对账自动补回挂载关系
void UbseSsuServiceImp::OnNodeUpEvent(const std::string &nodeId)
{
    nodeOfflineTracker_.OnUp(nodeId);
    UBSE_LOG_INFO << "NodeOffline: node back online, nodeId=" << nodeId;
}

// 离线超阈值节点的周期处理（由collector采集成功回调触发）：
// - NORMAL空间：从attachNodes摘除该节点（幂等），摘空后空间级state回CREATED；
//   节点回归后其重报（HandleStatusReceiver对账式处理）自动补回
// - LINEAR/STRIPED独占空间：仅告警提示人工detach/free，不自动清理（节点可能回归，
//   自动detach有数据面风险）；每个节点仅告警一次（nodeOfflineTracker_已处理记录去重）
void UbseSsuServiceImp::ReconcileOfflineNodes()
{
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    if (!ledger.IsReady()) {
        return;
    }
    // 挑出离线持续超阈值且未处理过的节点（锁内快照，锁外做账本操作避免锁交叉）
    auto expiredNodes = nodeOfflineTracker_.TakeExpired(SSU_NODE_OFFLINE_THRESHOLD_SECONDS);
    if (expiredNodes.empty()) {
        return;
    }
    for (const auto &nodeId : expiredNodes) {
        for (const auto &entry : ledger.GetAll()) {
            if (entry->attachNodes.count(nodeId) == 0) {
                continue;
            }
            if (entry->allocReq.strategy == UbseSsuAllocStrategy::NORMAL) {
                // NORMAL多节点共挂：摘除离线节点的挂载关系，摘空后空间级state回CREATED
                ledger.Modify(entry->name, [&nodeId](UbseSsuLedgerEntry &e) {
                    if (e.attachNodes.erase(nodeId) > 0 && e.attachNodes.empty()) {
                        e.state = UbseSsuNsState::CREATED;
                    }
                });
                UBSE_LOG_WARN << "NodeOffline: remove offline node from normal space attachNodes, name="
                               << entry->name << ", nodeId=" << nodeId;
            } else {
                // 独占策略：attacher节点疑似永久离场，告警提示人工介入（关键字NodeOfflineAlarm供检索）
                UBSE_LOG_ERROR << "NodeOfflineAlarm: exclusive space attacher offline over "
                               << SSU_NODE_OFFLINE_THRESHOLD_SECONDS << "s, manual detach/free required, name="
                               << entry->name << ", nodeId=" << nodeId;
            }
        }
    }
}


// ============ 重启恢复辅助函数 ============

// master账本对账（前置：ledgerReady_已置位）。Rebuild后账本仅含空间清单（state=CREATED、
// attachNodes空），此处恢复挂载视图；Modify失败=账本就绪但查无该空间=space确已被free，走自愈清理
static void ReconcileMasterLedger(const UbseSsuAttachRecord &record)
{
    std::string selfNodeId;
    auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        UBSE_LOG_ERROR << "ReconcileMasterLedger: get current node id failed, " << FormatRetCode(nodeIdRet)
                       << ", name=" << record.name;
        return;
    }
    if (!UbseSsuDebtLedger::GetInstance().Modify(record.name, [&record, &selfNodeId](UbseSsuLedgerEntry &e) {
            // 设备故障拦截：与HandleStatusReceiver拒收FAULTED上报对称，故障期间的对账不
            // 覆盖FAULTED状态掩盖告警；设备恢复由collector对账回正后本对账再收敛
            if (e.state == UbseSsuNsState::FAULTED) {
                UBSE_LOG_WARN << "ReconcileMasterLedger: space faulted, reconcile skipped, name=" << record.name;
                return;
            }
            // 独占空间单attacher不变式：与HandleStatusReceiver拒收逻辑对称。
            // 账本已归属他节点（agent重报先到）时自身记录为矛盾态，
            // 不抢占，告警人工核对（双方物理挂载并存属异常，归属方由diff CLEANUP保持）
            if (e.allocReq.strategy != UbseSsuAllocStrategy::NORMAL && !e.attachNodes.empty() &&
                e.attachNodes.count(selfNodeId) == 0) {
                UBSE_LOG_ERROR << "AttacherConflictAlarm: ReconcileMasterLedger: exclusive space held by other node, "
                                  "self reconcile skipped, name="
                               << record.name << ", holder=" << *e.attachNodes.begin()
                               << ". Manual check required (both physical mounts may exist)";
                return; // 不修改账本，保留归属方；本地记录待人工/后续对账清理
            }
            // NORMAL=本节点之一；LINEAR/STRIPED=attacher
            e.attachNodes.insert(selfNodeId);
            e.state = UbseSsuNsState::ATTACHED;
            if (e.allocReq.strategy != UbseSsuAllocStrategy::NORMAL && IsValidDevName(record.devName)) {
                e.devName = record.devName;
            }
        })) {
        UBSE_LOG_WARN << "ReconcileMasterLedger: ledger entry not found, space freed, cleanup local, name="
                      << record.name;
        CleanupLocalMount(record);
        UbseSsuAttachRecordStore::GetInstance().Remove(record.name);
    }
}

// agent重报对账：复用既有STATUS_UPDATE RPC通道。
// MASTER_NOT_READY纯重试；SPACE_NOT_FOUND（账本就绪且查无）=space确已被free，自愈清理本地
static void ReportToMasterWithRetry(const UbseSsuAttachRecord &record)
{
    std::string selfNodeId;
    auto nodeIdRet = UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        UBSE_LOG_ERROR << "ReportToMasterWithRetry: get current node id failed, " << FormatRetCode(nodeIdRet)
                       << ", name=" << record.name;
        return;
    }
    const std::string devName = record.blockDeviceOptions.has_value() ? record.devName : "";
    for (uint32_t attempt = 0; attempt < RECOVER_REPORT_RETRY_COUNT; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(GetRecoverRetryIntervalMs()));
        }
        auto ret = SendStatusUpdate(record.name, UbseSsuNsState::ATTACHED, devName, selfNodeId);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "ReportToMasterWithRetry: report success, name=" << record.name;
            return;
        }
        if (ret == UBSE_SSU_ERROR_SPACE_NOT_FOUND) {
            UBSE_LOG_WARN << "ReportToMasterWithRetry: space freed on master, cleanup local, name=" << record.name;
            CleanupLocalMount(record);
            UbseSsuAttachRecordStore::GetInstance().Remove(record.name);
            return;
        }
        // MASTER_NOT_READY（账本未就绪）及其他瞬时错误（网络/超时）：纯重试
        UBSE_LOG_WARN << "ReportToMasterWithRetry: report failed, name=" << record.name << ", attempt=" << attempt
                      << ", ret=" << FormatRetCode(ret);
    }
    UBSE_LOG_ERROR << "ReportToMasterWithRetry: report failed after " << RECOVER_REPORT_RETRY_COUNT
                   << " retries, keep record for next recovery, name=" << record.name;
}

// 恢复流程防重入RAII守卫：任意退出路径复位g_recoveringAttachments
class RecoveringFlagGuard {
public:
    ~RecoveringFlagGuard()
    {
        g_recoveringAttachments.store(false, std::memory_order_release);
    }
};

// ======== 运营期持续对账平账========

// 运营对账定时器注册名
constexpr const char *SSU_MOUNT_RECONCILE_TIMER_NAME = "SsuMountReconcile";

// 本地挂载重挂对账定时器注册名
constexpr const char *SSU_LOCAL_MOUNT_RECONCILE_TIMER_NAME = "SsuLocalMountReconcile";

// master→agent：拉取节点挂载摘要（响应经sync resp直接返回，无需RESP opcode）
static uint32_t SendMountSummaryReq(const std::string &nodeId, UbseSsuMountSummaryResp &resp)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_MOUNT_SUMMARY_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendMountSummaryReq: get endpoint failed, nodeId=" << nodeId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }
    UbseSsuMountSummaryReqMsg reqMsg;
    UbseSsuMountSummaryRespMsg respMsg;
    auto ret = endpoint->UbseRpcSend(nodeId, reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "SendMountSummaryReq: RpcSend failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    resp = respMsg.GetMountSummaryResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_WARN << "SendMountSummaryReq: agent returned error, nodeId=" << nodeId << ", "
                      << FormatRetCode(resp.errorCode);
        return resp.errorCode;
    }
    return UBSE_OK;
}

// master→agent：下发平账指令（逐项result经sync resp同步返回）
static uint32_t SendReconcileReq(const std::string &nodeId, const UbseSsuReconcileReq &req,
                                 UbseSsuReconcileResp &resp)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                           static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_RECONCILE_REQ));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendReconcileReq: get endpoint failed, nodeId=" << nodeId;
        return UBSE_SSU_ERROR_RPC_SEND_FAILED;
    }
    UbseSsuReconcileReqMsg reqMsg(req);
    UbseSsuReconcileRespMsg respMsg;
    auto ret = endpoint->UbseRpcSend(nodeId, reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "SendReconcileReq: RpcSend failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    resp = respMsg.GetReconcileResp();
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_WARN << "SendReconcileReq: agent returned error, nodeId=" << nodeId << ", "
                      << FormatRetCode(resp.errorCode);
        return resp.errorCode;
    }
    return UBSE_OK;
}

// 启动运营对账定时器：CAS幂等，失败回滚允许重试；间隔固定SSU_MOUNT_RECONCILE_INTERVAL_SECONDS
uint32_t UbseSsuServiceImp::StartMountReconcileTimer()
{
    if (!mountReconcileFlags_.TryMarkStarted()) {
        return UBSE_OK; // 已启动
    }
    const uint32_t interval = static_cast<uint32_t>(SSU_MOUNT_RECONCILE_INTERVAL_SECONDS);
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        SSU_MOUNT_RECONCILE_TIMER_NAME,
        []() {
            UbseSsuServiceImp::GetInstance().RunMountReconcile();
            return UBSE_OK;
        },
        interval);
    if (ret != UBSE_OK) {
        mountReconcileFlags_.ClearStarted(); // 回滚，允许后续重试
        UBSE_LOG_ERROR << "StartMountReconcileTimer: register timer failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "StartMountReconcileTimer: started, interval=" << interval << "s";
    return UBSE_OK;
}

// 停止运营对账定时器（随StopCollecting对称反注册）
void UbseSsuServiceImp::StopMountReconcileTimer()
{
    if (!mountReconcileFlags_.TryStop()) {
        return;
    }
    ubse::timer::UbseTimerHandlerUnregister(SSU_MOUNT_RECONCILE_TIMER_NAME);
    UBSE_LOG_INFO << "StopMountReconcileTimer: stopped";
}

// master本机挂载本地直平（对master自身执行，不走RPC；master自身也是挂载节点）。
// 正向：本地记录逐条对账（free残留清理/上报丢失直报收敛/漂移探测重挂）；
// 反向：账本期望本机挂载而本地记录缺失（记录文件损坏/误删）→ 以账本补齐记录并重挂。
// 疑似残留独占空间跳过自动动作，等人工核对
void UbseSsuServiceImp::ReconcileSelfLocalMounts(const std::string &selfNodeId)
{
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    auto records = UbseSsuAttachRecordStore::GetInstance().GetAll();
    std::unordered_set<std::string> localNames;
    for (const auto &record : records) {
        localNames.insert(record.name);
        if (record.recordState == UbseSsuAttachRecordState::PENDING) {
            continue; // 在途挂载，主流程收敛
        }
        auto resourceLock = ubse::utils::UbseLoggingLockGuard(record.name);
        auto entry = ledger.Get(record.name);
        if (entry == nullptr) {
            // 账本无（已free）：残留清理（复用SPACE_NOT_FOUND清理语义）
            UBSE_LOG_WARN << "MountReconcile: space freed on ledger, cleanup local mount, name=" << record.name;
            CleanupLocalMount(record);
            UbseSsuAttachRecordStore::GetInstance().Remove(record.name);
            continue;
        }
        if (entry->state == UbseSsuNsState::FAULTED) {
            continue;
        }
        if (entry->attachNodes.count(selfNodeId) == 0) {
            if (record.recordState == UbseSsuAttachRecordState::SUSPECT) {
                UBSE_LOG_WARN << "MountReconcile: suspect local record needs manual check, name=" << record.name;
                continue;
            }
            // 上报丢失/状态矛盾：REPORT语义（本机直报，幂等收敛）。
            // 复用ReconcileMasterLedger：内置独占单attacher冲突校验与FAULTED拦截
            // （与HandleStatusReceiver拒收对称），且避免Get预检+二次Modify的TOCTOU窗口
            ReconcileMasterLedger(record);
            continue;
        }
        if (record.recordState == UbseSsuAttachRecordState::SUSPECT) {
            UBSE_LOG_WARN << "MountReconcile: suspect record needs manual check, name=" << record.name;
            continue;
        }
        // 一致：探测漂移（误detach/误删设备）→ 幂等重挂
        if (!ProbeLocalMountReady(record)) {
            if (suspectedMountTable_.Contains(record.name)) {
                UBSE_LOG_WARN << "MountReconcile: suspected exclusive mount skip auto remount, name=" << record.name;
                continue; // 疑似残留独占空间等人工核对
            }
            UBSE_LOG_WARN << "MountReconcile: local mount drift detected, remount, name=" << record.name;
            auto ret = RemountLocal(record);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "MountReconcile: local remount failed, name=" << record.name
                               << ", ret=" << FormatRetCode(ret);
            }
        }
    }
    // 反向：账本期望本机挂载但本地记录缺失 → 补记录并重挂
    for (const auto &entry : ledger.GetAll()) {
        if (entry->state == UbseSsuNsState::FAULTED || entry->attachNodes.count(selfNodeId) == 0 ||
            localNames.count(entry->name) > 0) {
            continue;
        }
        if (suspectedMountTable_.Contains(entry->name)) {
            UBSE_LOG_WARN << "MountReconcile: suspected exclusive mount skip auto remount, name=" << entry->name;
            continue;
        }
        auto resourceLock = ubse::utils::UbseLoggingLockGuard(entry->name);
        UbseSsuAttachRecord record;
        if (!BuildRecordFromLedger(*entry, collector_.GetCachedDevMap(), record)) {
            UBSE_LOG_WARN << "MountReconcile: build record from ledger failed, retry next round, name=" << entry->name;
            continue;
        }
        UBSE_LOG_WARN << "MountReconcile: ledger expects local mount but record missing, remount, name=" << entry->name;
        auto ret = RemountLocal(record);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "MountReconcile: remount failed, retry next round, name=" << entry->name
                           << ", ret=" << FormatRetCode(ret);
            continue;
        }
        record.recordState = UbseSsuAttachRecordState::ATTACHED;
        UbseSsuAttachRecordStore::GetInstance().Upsert(record);
    }
}

// 运营对账单轮执行：本机直平 + 轮询各远端节点摘要→diff→下发平账（REMOUNT项首发
// 即以账本补齐完整记录，agent受理后异步执行，结果经SendStatusUpdate异步回报收敛）。
// 账本未就绪跳过（无从对账）；恢复收敛窗口内跳过
void UbseSsuServiceImp::RunMountReconcile()
{
    // 防重入：单轮未结束（RPC阻塞等）时跳过本轮触发
    if (!mountReconcileFlags_.TryEnterRound()) {
        UBSE_LOG_WARN << "MountReconcile: previous round still running, skip";
        return;
    }
    struct RunningGuard {
        ~RunningGuard()
        {
            UbseSsuServiceImp::GetInstance().mountReconcileFlags_.ExitRound();
        }
    } guard;

    auto &ledger = UbseSsuDebtLedger::GetInstance();
    if (!ledger.IsReady()) {
        return; // 账本未就绪（首采未成功）
    }
    if (ledger.IsInReconcileWindow()) {
        UBSE_LOG_INFO << "MountReconcile: in recovery reconcile window, skip this round";
        return;
    }
    std::string selfNodeId;
    if (UbseGetCurrentNodeId(selfNodeId) != UBSE_OK) {
        UBSE_LOG_ERROR << "MountReconcile: get current node id failed";
        return;
    }

    // 本机（master自身也是挂载节点）本地直平
    ReconcileSelfLocalMounts(selfNodeId);

    // 各远端节点：拉摘要 → diff → 平账（agent/standby均可响应）
    for (const auto &[nodeId, nodeInfo] : nodeController::UbseNodeController::GetInstance().GetAllNodes()) {
        (void)nodeInfo;
        if (nodeId == selfNodeId) {
            continue;
        }
        UbseSsuMountSummaryResp summary;
        if (SendMountSummaryReq(nodeId, summary) != UBSE_OK) {
            continue; // 节点不可达/未注册handler：下轮重试
        }
        std::vector<UbseSsuReconcileItem> items;
        DiffNodeMountSummary(nodeId, summary, items);
        if (items.empty()) {
            continue;
        }
        // REMOUNT项就地补齐完整记录：agent受理即回（批量异步执行），无法同步回
        // NEED_FULL_INFO触发补齐重发，master必须在首发时携带完整记录（hasRecord=true），
        // 否则agent本地记录缺失时无据可重挂
        std::vector<UbseSsuReconcileItem> readyItems;
        readyItems.reserve(items.size());
        for (auto &item : items) {
            if (item.action != UbseSsuReconcileAction::REMOUNT || item.hasRecord) {
                readyItems.push_back(std::move(item));
                continue;
            }
            auto entry = ledger.Get(item.name);
            UbseSsuAttachRecord record;
            if (entry != nullptr && BuildRecordFromLedger(*entry, collector_.GetCachedDevMap(), record)) {
                item.hasRecord = true;
                item.record = std::move(record);
                readyItems.push_back(std::move(item));
            } else {
                UBSE_LOG_WARN << "MountReconcile: build full record failed, retry next round, name=" << item.name;
            }
        }
        if (readyItems.empty()) {
            continue;
        }
        UbseSsuReconcileReq req;
        req.items = std::move(readyItems);
        UbseSsuReconcileResp resp;
        if (SendReconcileReq(nodeId, req, resp) != UBSE_OK) {
            continue; // agent拒绝（上批仍在执行）/不可达：下轮重试
        }
    }
    UBSE_LOG_INFO << "MountReconcile: round finished";
}

void UbseSsuServiceImp::MarkModuleReady()
{
    g_ssuModuleReady.store(true, std::memory_order_release);
}

void UbseSsuServiceImp::ResetModuleReady()
{
    g_ssuModuleReady.store(false, std::memory_order_release);
}

// 阶段2前置等待：模块Start未完成时阻塞（不能睡死——Start失败/挂死时超时放行，
// 由ReportToMasterWithRetry重试与300s运营对账兜底，恢复不能被启动异常无限拖延）
static void WaitModuleReadyForSync()
{
    if (g_ssuModuleReady.load(std::memory_order_acquire)) {
        return;
    }
    const auto intervalMs = GetRecoverRetryIntervalMs();
    UBSE_LOG_INFO << "RecoverLocalAttachments: module not ready, wait before report/reconcile, intervalMs="
                  << intervalMs;
    for (uint32_t i = 0; i < MODULE_READY_WAIT_ATTEMPTS; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        if (g_ssuModuleReady.load(std::memory_order_acquire)) {
            UBSE_LOG_INFO << "RecoverLocalAttachments: module ready after wait, attempts=" << (i + 1);
            return;
        }
    }
    UBSE_LOG_WARN << "RecoverLocalAttachments: module still not ready after " << MODULE_READY_WAIT_ATTEMPTS
                  << " attempts, proceed with retry/reconcile fallback";
}

// 阶段2共用：本地挂载已就绪的空间按角色统一对账账本（master本地Modify，agent经STATUS_UPDATE重报），
// 恢复流程阶段2与本地对账定时器上报共用同一实现。
// master账本未就绪时本轮整体挂起（待首次采集成功重建后由回调链补执行）；
// 上报前逐空间锁内重取，过滤此前被free自愈、SDK detach删除或状态变更的过期快照。
// tag为调用来源日志前缀（RecoverLocalAttachments/LocalMountReconcile），便于检索区分
static void SyncReadyRecordsToLedger(const std::vector<std::string> &names, bool isMaster, const char *tag)
{
    if (names.empty()) {
        return;
    }
    // 依赖RPC端点（agent重报）/master账本对账链路，MASTER_ONLINE事件可早于模块Start到达：
    // 先等待模块就绪再对账，5次重报额度留给真正的网络错误
    WaitModuleReadyForSync();
    if (isMaster) {
        if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
            UBSE_LOG_INFO << tag << ": ledger not ready, defer reconcile, count=" << names.size();
            return;
        }
        for (const auto &name : names) {
            auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);
            auto recordOpt = UbseSsuAttachRecordStore::GetInstance().Get(name);
            if (!recordOpt.has_value() || recordOpt->recordState != UbseSsuAttachRecordState::ATTACHED) {
                continue;
            }
            ReconcileMasterLedger(*recordOpt);
        }
    } else {
        for (const auto &name : names) {
            auto resourceLock = ubse::utils::UbseLoggingLockGuard(name);
            auto recordOpt = UbseSsuAttachRecordStore::GetInstance().Get(name);
            if (!recordOpt.has_value() || recordOpt->recordState != UbseSsuAttachRecordState::ATTACHED) {
                continue;
            }
            ReportToMasterWithRetry(*recordOpt);
        }
    }
}

// 重启恢复主流程：两阶段执行。
// 阶段1逐空间探测/幂等重挂本地挂载（纯本地，不依赖master可达性；锁内重取记录防快照过期复活）；
// 阶段2待全部本地恢复完成后按角色统一对账账本（master本地Modify/agent重报），
// 避免账本对账（含重试退避sleep）阻塞后续空间的重挂。单空间失败不影响其他空间；
// 由MASTER_ONLINE事件经executor异步触发
uint32_t UbseSsuServiceImp::RecoverLocalAttachments()
{
    // 独占防重入：MASTER_ONLINE抖动/主备切换可能连续触发，CAS保证仅一个恢复流程在跑
    bool expected = false;
    if (!g_recoveringAttachments.compare_exchange_strong(expected, true)) {
        UBSE_LOG_WARN << "RecoverLocalAttachments: recovery already in progress, skip";
        return UBSE_OK;
    }
    RecoveringFlagGuard guard{};

    // 惰性兜底：MASTER_ONLINE事件理论上可早于模块Start完成（事件先于store Init到达），
    // 启动时Init失败（权限漂移/文件损坏）也可经后续恢复触发（主备切换）重试加载
    auto ensureRet = UbseSsuAttachRecordStore::GetInstance().EnsureInited();
    if (ensureRet != UBSE_OK) {
        UBSE_LOG_WARN << "RecoverLocalAttachments: attach record store init failed, ret="
                      << FormatRetCode(ensureRet) << ", recovery skipped, wait for next trigger";
        return ensureRet;
    }
    auto records = UbseSsuAttachRecordStore::GetInstance().GetAll(); // 快照仅确定处理范围
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RecoverLocalAttachments: failed to get node role, " << FormatRetCode(ret);
        return ret;
    }
    const bool isMaster = (role == ELECTION_ROLE_MASTER);
    if (records.empty()) {
        // 记录为空（含"记录丢失"场景）时master仍需反查探测残留设备
        UBSE_LOG_INFO << "RecoverLocalAttachments: no local attach records, role=" << role;
        if (isMaster) {
            BuildSuspectedLocalMounts();
        }
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "RecoverLocalAttachments: start, recordCount=" << records.size()
                  << ", role=" << role;

    // ===== 阶段1：本地恢复（PENDING/SUSPECT收敛、探测、幂等重挂） =====
    std::vector<std::string> pendingSync; // 本地挂载已就绪、待阶段2账本对账的空间
    for (const auto &snapshot : records) {
        // per-name resourceLock：与SDK并发attach/detach互斥
        auto resourceLock = ubse::utils::UbseLoggingLockGuard(snapshot.name);
        // 锁内重取守卫：GetAll快照可能过期（轮到本空间前已被SDK detach并发删除），
        // 沿用过期快照重挂会把用户已卸载的空间复活并重报账本，必须以锁内最新记录为准
        auto recordOpt = UbseSsuAttachRecordStore::GetInstance().Get(snapshot.name);
        if (!recordOpt.has_value()) {
            UBSE_LOG_INFO << "RecoverLocalAttachments: record gone before recovery, skip, name=" << snapshot.name;
            continue;
        }
        auto record = std::move(*recordOpt);

        if (record.recordState == UbseSsuAttachRecordState::PENDING) {
            // 崩溃窗口残留：已全部挂载则收敛为ATTACHED，否则作废（挂载未完成）
            if (ProbeLocalMountReady(record)) {
                UBSE_LOG_INFO << "RecoverLocalAttachments: pending record fully mounted, converge to attached, name="
                              << record.name;
                record.recordState = UbseSsuAttachRecordState::ATTACHED;
                UbseSsuAttachRecordStore::GetInstance().Upsert(record);
            } else {
                UBSE_LOG_WARN << "RecoverLocalAttachments: pending record not mounted, discard, name=" << record.name;
                UbseSsuAttachRecordStore::GetInstance().Remove(record.name);
                continue;
            }
        }

        // SUSPECT（重挂连续失败疑似故障）与ATTACHED统一走"探测缺失即重试重挂"：
        // 不放弃重试直到挂上为止（防冲击由重挂节奏限频保证），SUSPECT仅作为告警可视的状态标记。
        // 探测命中→SUSPECT收敛回ATTACHED并清计数；仍缺失→幂等重挂
        bool needRemount = !ProbeLocalMountReady(record);
        if (!needRemount && record.recordState == UbseSsuAttachRecordState::SUSPECT) {
            record.recordState = UbseSsuAttachRecordState::ATTACHED;
            UbseSsuAttachRecordStore::GetInstance().Upsert(record);
            remountFailCounter_.Reset(record.name);
            UBSE_LOG_INFO << "SuspectMountRecover: device back online, record restored to attached, name="
                          << record.name;
        }
        if (needRemount) {
            // 确保本地挂载就绪：探测有缺失（OS重启/设备异常丢失）则幂等重挂
            auto remountRet = RemountLocal(record);
            if (remountRet != UBSE_OK) {
                // 失败收敛升级：连续失败达阈值标记SUSPECT并告警；此后仍每轮持续重试
                auto failCount = remountFailCounter_.Increment(record.name);
                if (failCount >= SSU_REMOUNT_FAIL_SUSPECT_LIMIT) {
                    if (record.recordState != UbseSsuAttachRecordState::SUSPECT) {
                        record.recordState = UbseSsuAttachRecordState::SUSPECT;
                        UbseSsuAttachRecordStore::GetInstance().Upsert(record);
                    }
                    // 告警：关键字SuspectMountAlarm供运维检索（agent侧设备告警接入xalarm留作后续）
                    UBSE_LOG_ERROR << "SuspectMountAlarm: remount failed for " << failCount
                                   << " consecutive rounds, keep retrying until success, name=" << record.name
                                   << ", ret=" << FormatRetCode(remountRet);
                } else {
                    UBSE_LOG_ERROR << "RecoverLocalAttachments: remount failed (" << failCount << "/"
                                   << SSU_REMOUNT_FAIL_SUSPECT_LIMIT << "), name=" << record.name
                                   << ", ret=" << FormatRetCode(remountRet);
                }
                continue;
            }
            if (record.recordState == UbseSsuAttachRecordState::SUSPECT) {
                // SUSPECT状态下重挂成功：收敛回ATTACHED
                record.recordState = UbseSsuAttachRecordState::ATTACHED;
                UbseSsuAttachRecordStore::GetInstance().Upsert(record);
                UBSE_LOG_INFO << "SuspectMountRecover: remount success, record restored to attached, name="
                              << record.name;
            }
            remountFailCounter_.Reset(record.name); // 成功即清零（偶发失败不累积）
            UBSE_LOG_INFO << "RecoverLocalAttachments: remount success, name=" << record.name;
        }
        pendingSync.push_back(record.name); // 本地挂载已就绪，纳入阶段2对账
    }

    // ===== 阶段2：账本对账（本地全部就绪后统一执行） =====
    SyncReadyRecordsToLedger(pendingSync, isMaster, "RecoverLocalAttachments");
    // master角色：对账完成后构建疑似本地残留挂载集合（反查探测，账本未就绪时内部跳过，
    // 由首采成功回调链补执行RecoverLocalAttachments时再建）
    if (isMaster) {
        BuildSuspectedLocalMounts();
    }
    UBSE_LOG_INFO << "RecoverLocalAttachments: finished, recordCount=" << records.size();
    return UBSE_OK;
}

// ===== 本地挂载重挂对账定时器（收敛增强） =====
// 触发背景：agent节点无collector/首采回调触发源，重挂重试仅靠MASTER_ONLINE事件驱动，
// 事件可能长期不来（主稳态运行），设备恢复后无人推动收敛。定时器给所有节点一个
// 确定性的本地对账节奏：记录=期望态声明，探测缺失=漂移，重挂=收敛。

// 启动本地挂载对账定时器：CAS幂等，失败回滚允许重试（模块Start内调用，所有节点运行）
uint32_t UbseSsuServiceImp::StartLocalMountReconcileTimer()
{
    if (!localMountReconcileFlags_.TryMarkStarted()) {
        return UBSE_OK; // 已启动
    }
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        SSU_LOCAL_MOUNT_RECONCILE_TIMER_NAME,
        []() {
            UbseSsuServiceImp::GetInstance().RunLocalMountReconcile();
            return UBSE_OK;
        },
        SSU_LOCAL_MOUNT_RECONCILE_INTERVAL_SECONDS);
    if (ret != UBSE_OK) {
        localMountReconcileFlags_.ClearStarted(); // 回滚，允许后续重试
        UBSE_LOG_ERROR << "StartLocalMountReconcileTimer: register timer failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "StartLocalMountReconcileTimer: started, interval=" << SSU_LOCAL_MOUNT_RECONCILE_INTERVAL_SECONDS
                  << "s";
    return UBSE_OK;
}

// 停止本地挂载对账定时器（随模块Stop对称反注册）
void UbseSsuServiceImp::StopLocalMountReconcileTimer()
{
    if (!localMountReconcileFlags_.TryStop()) {
        return;
    }
    ubse::timer::UbseTimerHandlerUnregister(SSU_LOCAL_MOUNT_RECONCILE_TIMER_NAME);
    UBSE_LOG_INFO << "StopLocalMountReconcileTimer: stopped";
}

// 单轮本地挂载对账：遍历本地记录做探测/重挂收敛，发生动作的空间按角色上报账本。
// 状态过滤（两阶段语义）：PENDING在途挂载跳过（主流程收敛，定时器不得干扰）；
// SUSPECT/ATTACHED统一探测缺失即幂等重挂、持续重试直到挂上（防冲击由tick限频保证），
// SUSPECT仅为告警可视的状态标记，重挂成功/探测命中收敛回ATTACHED。
// per-name锁+锁内重取与恢复流程/SDK路径互斥且防快照过期。
// 恢复流程运行中跳过本轮（检查存在TOCTOU窗口，但per-name锁保证最坏为冗余顺序重试，无并发双挂）
void UbseSsuServiceImp::RunLocalMountReconcile()
{
    // 防重入：单轮未结束（重试退避sleep等）时跳过本次tick
    if (!localMountReconcileFlags_.TryEnterRound()) {
        UBSE_LOG_INFO << "LocalMountReconcile: previous round still running, skip";
        return;
    }
    struct RunningGuard {
        UbseSsuServiceImp &self_;
        ~RunningGuard() { self_.localMountReconcileFlags_.ExitRound(); }
    } guard{*this};

    if (g_recoveringAttachments.load(std::memory_order_acquire)) {
        UBSE_LOG_INFO << "LocalMountReconcile: recovery in progress, skip this round";
        return;
    }
    auto records = UbseSsuAttachRecordStore::GetInstance().GetAll(); // 快照仅确定处理范围
    if (records.empty()) {
        return;
    }
    std::string role;
    if (UbseGetRole(role) != UBSE_OK) {
        UBSE_LOG_WARN << "LocalMountReconcile: get node role failed, skip this round";
        return;
    }
    const bool isMaster = (role == ELECTION_ROLE_MASTER);
    std::vector<std::string> pendingSync; // 发生重挂/SUSPECT收敛、需上报账本的空间

    for (const auto &snapshot : records) {
        auto resourceLock = ubse::utils::UbseLoggingLockGuard(snapshot.name);
        // 锁内重取守卫（与恢复流程同款）：tick遍历期间记录可能被SDK detach/free自愈删除，
        // 沿用过期快照会把用户已卸载的空间复活并重报账本
        auto recordOpt = UbseSsuAttachRecordStore::GetInstance().Get(snapshot.name);
        if (!recordOpt.has_value()) {
            UBSE_LOG_INFO << "LocalMountReconcile: record gone before reconcile, skip, name=" << snapshot.name;
            continue;
        }
        auto record = std::move(*recordOpt);

        if (record.recordState == UbseSsuAttachRecordState::PENDING) {
            continue; // 在途挂载：主流程收敛
        }
        // SUSPECT与ATTACHED统一走"探测缺失即重试重挂"：不放弃重试直到挂上为止
        // （防冲击由tick限频保证），SUSPECT仅作为告警可视的状态标记
        if (ProbeLocalMountReady(record)) {
            // 探测命中：SUSPECT收敛回ATTACHED并上报；ATTACHED零操作（稳态）
            if (record.recordState == UbseSsuAttachRecordState::SUSPECT) {
                record.recordState = UbseSsuAttachRecordState::ATTACHED;
                UbseSsuAttachRecordStore::GetInstance().Upsert(record);
                remountFailCounter_.Reset(record.name);
                UBSE_LOG_INFO << "SuspectMountRecover: device back online, record restored to attached, name="
                              << record.name;
                pendingSync.push_back(record.name);
            } else {
                UBSE_LOG_INFO << "LocalMountReconcile: local mount ready, skip, name=" << record.name;
            }
            continue;
        }
        // 探测缺失（设备静默丢失/NVMe断连/OS重启）：幂等重挂（SUSPECT同样持续重试）
        UBSE_LOG_INFO << "LocalMountReconcile: local mount drift detected, remount, name=" << record.name;
        auto remountRet = RemountLocal(record);
        if (remountRet != UBSE_OK) {
            auto failCount = remountFailCounter_.Increment(record.name);
            if (failCount >= SSU_REMOUNT_FAIL_SUSPECT_LIMIT) {
                if (record.recordState != UbseSsuAttachRecordState::SUSPECT) {
                    record.recordState = UbseSsuAttachRecordState::SUSPECT;
                    UbseSsuAttachRecordStore::GetInstance().Upsert(record);
                }
                UBSE_LOG_ERROR << "SuspectMountAlarm: remount failed for " << failCount
                               << " consecutive rounds, keep retrying until success, name=" << record.name
                               << ", ret=" << FormatRetCode(remountRet);
            } else {
                UBSE_LOG_WARN << "LocalMountReconcile: remount failed (" << failCount << "/"
                              << SSU_REMOUNT_FAIL_SUSPECT_LIMIT << "), name=" << record.name
                              << ", ret=" << FormatRetCode(remountRet);
            }
            continue;
        }
        if (record.recordState == UbseSsuAttachRecordState::SUSPECT) {
            // SUSPECT状态下重挂成功：收敛回ATTACHED
            record.recordState = UbseSsuAttachRecordState::ATTACHED;
            UbseSsuAttachRecordStore::GetInstance().Upsert(record);
            UBSE_LOG_INFO << "SuspectMountRecover: remount success, record restored to attached, name="
                          << record.name;
        }
        remountFailCounter_.Reset(record.name); // 成功即清零（偶发失败不累积）
        UBSE_LOG_INFO << "LocalMountReconcile: remount success, name=" << record.name;
        pendingSync.push_back(record.name);
    }

    // 上报/对账（与恢复流程阶段2同款语义）：master本地Modify，agent经STATUS_UPDATE重报
    SyncReadyRecordsToLedger(pendingSync, isMaster, "LocalMountReconcile");
}
} // namespace ubse::ssu::service
