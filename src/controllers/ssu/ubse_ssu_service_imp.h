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

namespace ubse::ssu::service {

using namespace ubse::plugin::service::ssu;
using namespace ubse::ssu::scheduler;

class UbseSsuServiceImp : public UbseSsuService {
public:
    UbseSsuServiceImp(const UbseSsuServiceImp &) = delete;
    UbseSsuServiceImp &operator=(const UbseSsuServiceImp &) = delete;

    static UbseSsuServiceImp &GetInstance();

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

    // master端：验证identity并返回构造Attach/Detach所需的字段(defaultNqn/jettyId/guid)
    uint32_t VerifyAttachDetachIdentity(const std::string &name, const UbseSsuAllocIdentityInfo &identity,
                                        std::vector<ubse::ssu::message::UbseSsuNsVerifyInfo> &nsVerifyList);

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

    UbseSsuCollector collector_;
    UbseSsuScheduler scheduler_;
    std::mutex schedulerLock_;
};
} // namespace ubse::ssu::service
#endif // UBSE_SSU_SERVICE_IMP_H
