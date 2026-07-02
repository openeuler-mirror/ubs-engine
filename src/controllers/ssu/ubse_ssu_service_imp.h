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
#include "ubse_ssu_collector.h"
#include "ubse_ssu_scheduler.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::service {

using namespace ubse::plugin::service::ssu;
using namespace ubse::ssu::scheduler;

class UbseSsuServiceImp : public UbseSsuService {
public:
    UbseSsuServiceImp(const UbseSsuServiceImp &) = delete;
    UbseSsuServiceImp &operator=(const UbseSsuServiceImp &) = delete;

    static UbseSsuServiceImp &GetInstance();

    // 启动收集器，用于定期更新设备状态
    uint32_t StartCollecting();
    // 停止收集器
    void StopCollecting();
    // 从设备列表进行账本构建，用于初始化或重启恢复账本
    void RebuildLedgerFromDevList();

    uint32_t AllocSpace(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                        UbseSsuAllocResult &result) override;

    uint32_t FreeSpace(const std::string &name, const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t AttachSpace(const std::string &name, const std::string &nqn, const UbseSsuAllocIdentityInfo &identity,
                         std::string &devPath) override;

    uint32_t DetachSpace(const std::string &name, const std::string &nqn,
                         const UbseSsuAllocIdentityInfo &identity) override;

    uint32_t AttachLinearSpace(const std::string &name, const std::string &nqn,
                               const UbseSsuAllocIdentityInfo &identity, const std::string &devName,
                               std::string &devPath) override;

    uint32_t DetachLinearSpace(const std::string &name, const std::string &nqn,
                               const UbseSsuAllocIdentityInfo &identity, const std::string &devName) override;

private:
    UbseSsuServiceImp() = default;
    ~UbseSsuServiceImp() override = default;

    uint32_t ExecuteAlloc(const UbseSsuAllocSpaceReq &req, UbseSsuAllocResult &result);

    uint32_t ExecuteFree(const std::string &name);

    uint32_t ExecuteScheduler(const UbseSsuAllocSpaceReq &req, std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList);

    UbseSsuCollector collector_;
    UbseSsuScheduler scheduler_;
    std::mutex schedulerLock_;
};
} // namespace ubse::ssu::service
#endif // UBSE_SSU_SERVICE_IMP_H
