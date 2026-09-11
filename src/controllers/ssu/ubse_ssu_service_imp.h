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
#include <mutex>
#include <string>
#include <vector>
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_ssu_collector.h"
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
    // 从设备列表进行账本构建，用于初始化或重启恢复账本
    void RebuildLedgerFromDevList();

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
    uint32_t VerifyAttachDetachPrecondition(const std::string& name, const UbseSsuAllocIdentityInfo& identity,
                                            const ubse::ssu::message::UbseSsuAttachDetachVerifyOption& option,
                                            ubse::ssu::message::UbseSsuAttachDetachVerifyResp& verifyResp);

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

    UbseSsuCollector collector_;
    UbseSsuScheduler scheduler_;
    std::mutex schedulerLock_;

    // === VM 模式成员 ===
    // vmMode_ 在 Initialize 阶段读取一次,后续只读
    bool vmMode_{false};
    // vmCache_ 由 controller 模块注入,VM 模式下查询走本地缓存、attach 前校验缓存命中
    // host 模式下保持 nullptr,所有方法走现有分支
    ubse::ssu::controller::UbseSsuVmCache *vmCache_{nullptr};
};
} // namespace ubse::ssu::service
#endif // UBSE_SSU_SERVICE_IMP_H
