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

#ifndef UBSE_SSU_SERVICE_IMP_H
#define UBSE_SSU_SERVICE_IMP_H
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "debt/ubse_ssu_debt_ledger.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_ssu_collector.h"
#include "ubse_ssu_reconcile_state.h"
#include "ubse_ssu_scheduler.h"

namespace ubse::ssu::controller {
class UbseSsuVmCache;
} // namespace ubse::ssu::controller

namespace ubse::adapter_plugins::ssu::def {
struct UbseCreateBlockDeviceOptions;
} // namespace ubse::adapter_plugins::ssu::def

namespace ubse::ssu::service {

using namespace ubse::plugin::service::ssu;
using namespace ubse::ssu::scheduler;

class UbseSsuServiceImp : public UbseSsuService {
public:
    UbseSsuServiceImp(const UbseSsuServiceImp &) = delete;
    UbseSsuServiceImp &operator=(const UbseSsuServiceImp &) = delete;

    static UbseSsuServiceImp &GetInstance();

    // === 模块就绪标志 ===
    // UbseSsuControllerModule::Start()成功末尾置位、Stop()复位。
    static void MarkModuleReady();
    static void ResetModuleReady();

    // === VM 模式扩展 ===
    // 设置 VM 模式开关:Initialize 阶段由 UbseSsuControllerModule 调用一次
    void SetVmMode(bool enabled) { vmMode_ = enabled; }
    bool IsVmMode() const { return vmMode_; }
    // 注入 VM 模式本地缓存指针(所有权归 UbseSsuControllerModule,ServiceImp 不持有)
    // vmCache_ 由 controller 模块在 InitializeVmMode 时注入
    void SetVmCache(ubse::ssu::controller::UbseSsuVmCache *cache) { vmCache_ = cache; }

    // 启动收集器，用于定期更新设备状态，仅master端调用
    uint32_t StartCollecting();
    // 停止收集器
    void StopCollecting();
    // 启动空VM BusInstance清理定时器
    uint32_t StartClearTimer();
    // 停止清理定时器
    void StopClearTimer();
    // 运营对账定时器：低频300s轮询各挂载节点拉取
    uint32_t StartMountReconcileTimer();
    void StopMountReconcileTimer();
    // 启动本地挂载重挂对账定时器：60s周期遍历本地挂载记录：统一探测缺失进行重挂
    // 重挂成功/收敛后按角色上报账本，随模块Start/Stop启停
    uint32_t StartLocalMountReconcileTimer();
    void StopLocalMountReconcileTimer();
    // 从设备列表进行账本构建，用于初始化或重启恢复账本
    void RebuildLedgerFromDevList();

    // 重启恢复：基于本地挂载记录（attach_records.json）探测/幂等重挂本地挂载，
    // 并按节点角色做账本对账（master=本地对账，agent=向master重报）。
    // 由 MASTER_ONLINE 事件经 executor 异步触发，
    // 内部防重入（CAS），重复触发安全
    uint32_t RecoverLocalAttachments();

    uint32_t AllocSpace(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                        UbseSsuAllocResult &result) override;

    uint32_t FreeSpace(const std::string &name, const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t AttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths) override;

    uint32_t DetachSpace(const UbseSsuSpaceReq &req) override;

    uint32_t AttachLinearSpace(const UbseSsuLinearSpaceReq &req, std::vector<std::string> &nsDevPaths,
                               std::string &devPath) override;

    uint32_t DetachLinearSpace(const UbseSsuLinearSpaceReq &req) override;

    uint32_t AttachStripedSpace(const UbseSsuStripedSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                std::string &devPath) override;

    uint32_t DetachStripedSpace(const UbseSsuStripedSpaceReq &req) override;

    uint32_t AddAccessPermission(const std::string &name, const std::string &nqn,
                                 const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t RemoveAccessPermission(const std::string &name, const std::string &nqn,
                                    const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t GetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                        const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t ListAllocInfo(std::vector<UbseSsuAllocResult> &result, const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t GetAllocInfoByName(const std::string &name, UbseSsuAllocResult &result,
                                const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t GetConnectInfo(const std::string &name, const UbseSsuVfe *vfe,
                            std::vector<UbseSsuConnectInfo> &connectInfoList,
                            const UbseSsuAllocIdentityInfo &identity) override;

    // master端：校验attach/detach前置条件（identity、账本state、条带化参数）并返回所需字段(defaultNqn/jettyId/guid)
    // identity校验先于幂等/state判断，防止幂等分支向非属主泄露nsDevPath/devPath
    // requestNodeId：发起节点ID，NORMAL策略多节点共挂场景下按节点维度判断
    uint32_t VerifyAttachDetachPrecondition(const std::string& name, const UbseSsuAllocIdentityInfo& identity,
                                            const std::string& requestNodeId,
                                            const ubse::ssu::message::UbseSsuAttachDetachVerifyOption& option,
                                            ubse::ssu::message::UbseSsuAttachDetachVerifyResp& verifyResp);

    // 设备故障周期增量对账
    void ReconcileSpaceFaultStates(const std::vector<UbseSsuDevInfo> &devList);

    // 节点离场处理：master感知NODE_DOWN/NODE_UP事件（仅master收到），
    // 记录首次离线时刻；离线持续超SSU_NODE_OFFLINE_THRESHOLD_SECONDS后由周期对账处理——
    // NORMAL空间从attachNodes摘除该节点（幂等，节点回归重报自动补回）；
    // LINEAR/STRIPED独占空间仅告警提示人工detach/free（节点可能回归，自动清理有数据面风险）
    void OnNodeDownEvent(const std::string &nodeId);
    void OnNodeUpEvent(const std::string &nodeId);
    // 离线超阈值节点的周期处理，由collector采集成功回调触发（与故障对账同周期）
    void ReconcileOfflineNodes();

    // 立即执行一轮运营对账（StartMountReconcileTimer定时器经executor异步触发）
    void RunMountReconcile();

    // 单轮本地挂载对账（StartLocalMountReconcileTimer定时器回调触发）
    void RunLocalMountReconcile();

    uint32_t GetFeDeviceList(std::vector<UbseSsuFe> &feList) override;

    uint32_t FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid) override;

    uint32_t FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe) override;

private:
    UbseSsuServiceImp() = default;
    ~UbseSsuServiceImp() override = default;

    // Execute函数只在master端调用，用于处理分配/释放/添加/移除访问权限请求
    uint32_t ExecuteAlloc(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                          UbseSsuAllocResult &result);

    uint32_t ExecuteFree(const std::string &name, const UbseSsuAllocIdentityInfo &identity);

    uint32_t ExecuteScheduler(const UbseSsuAllocSpaceReq &req,
                              std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList);

    uint32_t ExecuteAccessPermission(const std::string &name, const std::string &nqn,
                                     const UbseSsuAllocIdentityInfo &identity, bool isAdd);

    uint32_t ExecuteGetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                               const UbseSsuAllocIdentityInfo &identity);

    uint32_t ExecuteGetConnectInfo(const std::string &name, const UbseSsuVfe *vfe,
                                   std::vector<UbseSsuConnectInfo> &connectInfoList,
                                   const UbseSsuAllocIdentityInfo &identity);

    // === VM 模式私有辅助 ===
    // VM 模式 AttachSpace:校验 vmCache_ 命中 → adapter AttachDevNameSpace → MarkAttached
    // 幂等性:已 attach 直接返回成功;未在缓存中返回 UBSE_SSU_ERROR_NS_NOT_FOUND
    uint32_t VmAttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths);
    // VM 模式 DetachSpace:校验 vmCache_ 命中 → adapter DetachDevNameSpace → MarkDetached
    // 幂等性:未 attach 直接返回成功
    uint32_t VmDetachSpace(const UbseSsuSpaceReq &req);
    // VM 模式 AttachLinearSpace/AttachStripedSpace:复用 VmAttachSpace + adapter CreateBlockDevice
    // options 由调用方构造并透传(LINEAR 或 STRIPED+level/chunkSize),聚合参数不在此处丢失
    // VM 模式 DetachLinearSpace/DetachStripedSpace:adapter DeleteBlockDevice + VmDetachSpace
    uint32_t VmAttachAggregated(const UbseSsuLinearSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                std::string &devPath,
                                const ubse::adapter_plugins::ssu::def::UbseCreateBlockDeviceOptions &options);
    uint32_t VmDetachAggregated(const UbseSsuLinearSpaceReq &req, bool isStriped);
    // VM 模式 GetNsStats:校验 vmCache_ 命中 → 走本地适配器查询 namespace 统计(usedSize 取自 nuse)
    // VM 内无 collector 缓存,按需 RefreshDevCache 触发本地适配器 GetDevList 查询;identity 校验与 master 一致
    uint32_t VmGetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                          const UbseSsuAllocIdentityInfo &identity);

    // 疑似本地残留挂载集合构建：账本恢复后对本机做反查探测——
    // 独占策略空间任一nsDevPath本机存在但本地无挂载记录 → 记入疑似集合并告警。
    // 由RecoverLocalAttachments在master角色对账完成后调用
    void BuildSuspectedLocalMounts();

    // master本机挂载本地直平（对master自身挂载执行，不走RPC，per-name resourceLock下复用
    // Probe/Remount/Cleanup/直报原语）：由RunMountReconcile调用
    void ReconcileSelfLocalMounts(const std::string &selfNodeId);

    UbseSsuCollector collector_;
    UbseSsuScheduler scheduler_;
    std::mutex schedulerLock_;

    // === VM 模式成员 ===
    // vmMode_ 在 Initialize 阶段读取一次,后续只读
    bool vmMode_{false};
    // vmCache_ 由 controller 模块注入,VM 模式下查询走本地缓存、attach 前校验缓存命中
    // host 模式下保持 nullptr,所有方法走现有分支
    ubse::ssu::controller::UbseSsuVmCache *vmCache_{nullptr};
    
    UbseSsuSuspectedMountTable suspectedMountTable_;  // 疑似本地残留挂载集合（master反查探测构建）
    UbseSsuFaultDebouncer faultDebouncer_;           // 设备故障防抖计数（空间名→连续缺失周期数）
    UbseSsuRemountFailCounter remountFailCounter_;   // 重挂失败计数（恢复流程与本地挂载对账定时器并发）
    UbseSsuNodeOfflineTracker nodeOfflineTracker_;  // 节点离线追踪（首次离线时刻/已处理记录）
    UbseSsuReconcileTimerFlags mountReconcileFlags_; // 运营对账定时器启停/单轮防重入标志
    UbseSsuReconcileTimerFlags localMountReconcileFlags_; // 本地挂载对账定时器启停/单轮防重入标志
};

// 设备故障防抖阈值：连续N个采集周期（默认3，约90s）缺失才判FAULTED，
// 规避单次采集抖动/瞬时链路故障误判
inline constexpr uint32_t SSU_FAULT_DEBOUNCE_CYCLES = 3;

// 重挂失败收敛阈值：重挂连续失败N次（默认3）后记录标记SUSPECT并告警
// （仅作告警可视的状态标记，自动重挂不停止、持续重试直到挂上，防冲击由重挂节奏限频保证）；
// 计数为内存级，进程重启清零可接受
inline constexpr uint32_t SSU_REMOUNT_FAIL_SUSPECT_LIMIT = 3;

// 节点离场判定阈值：NODE_DOWN后持续离线超该时长才做离场处理
// （NORMAL摘除attachNodes/独占告警），规避主备抖动/瞬时离线误处理
inline constexpr int64_t SSU_NODE_OFFLINE_THRESHOLD_SECONDS = 600;

// 运营对账周期：300s
inline constexpr int64_t SSU_MOUNT_RECONCILE_INTERVAL_SECONDS = 300;

// 本地挂载重挂对账周期：60s。探测为纯access()成本可忽略，
// 重挂防冲击由tick限频兜底（SUSPECT标记后仍持续重试直到挂上）
inline constexpr uint32_t SSU_LOCAL_MOUNT_RECONCILE_INTERVAL_SECONDS = 60;
} // namespace ubse::ssu::service
#endif // UBSE_SSU_SERVICE_IMP_H
