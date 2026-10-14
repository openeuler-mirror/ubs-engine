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
#include "ubse_ssu_debt_ledger.h"

#include <shared_mutex>
#include <unordered_map>
#include "ubse_logger.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::debt {

using namespace ubse::log;
using namespace ubse::ssu::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseSsuDebtLedger &UbseSsuDebtLedger::GetInstance()
{
    static UbseSsuDebtLedger instance;
    return instance;
}

bool UbseSsuDebtLedger::Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry)
{
    if (!entry) {
        UBSE_LOG_ERROR << "Ledger put: entry is null";
        return false;
    }
    std::unique_lock<std::shared_mutex> lock(mtx_);
    bool isNew = ledger_.find(name) == ledger_.end();
    if (!isNew) {
        UBSE_LOG_ERROR << "Ledger put: entry already exists, reject overwrite: name=" << name;
        return false;
    }
    ledger_[name] = std::move(entry);
    UBSE_LOG_INFO << "Ledger put: name=" << name;
    return true;
}

std::shared_ptr<const UbseSsuLedgerEntry> UbseSsuDebtLedger::Get(const std::string &name) const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(name);
    return it != ledger_.end() ? it->second : nullptr;
}

bool UbseSsuDebtLedger::Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(name);
    if (it == ledger_.end()) {
        UBSE_LOG_WARN << "Ledger entry not found for modify: name=" << name;
        return false;
    }
    auto oldEntry = it->second;
    auto newEntry = std::make_shared<UbseSsuLedgerEntry>(*oldEntry);
    modify(*newEntry);
    ledger_[name] = std::move(newEntry);
    return true;
}

bool UbseSsuDebtLedger::Remove(const std::string &name)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto erased = ledger_.erase(name);
    if (erased > 0) {
        UBSE_LOG_INFO << "Ledger remove: name=" << name;
        return true;
    }
    UBSE_LOG_WARN << "Ledger entry not found for remove: name=" << name;
    return false;
}

std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> UbseSsuDebtLedger::GetAll() const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> entries;
    entries.reserve(ledger_.size());
    for (const auto &[_, entry] : ledger_) {
        entries.push_back(entry);
    }
    return entries;
}

static void RebuildEntry(const std::string &name, const std::vector<const UbseSsuDevNameSpace *> &nsList,
                         UbseSsuDebtLedger &ledger, const std::string &tenant)
{
    if (nsList.empty()) {
        UBSE_LOG_ERROR << "Rebuild entry: nsList is empty: name=" << name;
        return;
    }
    const auto &first = *(nsList[0]);
    // 校验所有NS的customData关键字段一致性
    for (size_t i = 1; i < nsList.size(); i++) {
        if (nsList[i]->customData.nsNum != first.customData.nsNum ||
            nsList[i]->customData.totalBytes != first.customData.totalBytes) {
            UBSE_LOG_WARN << "RebuildEntry: customData inconsistency detected, name=" << name
                          << ", ns[0].nsNum=" << static_cast<int>(first.customData.nsNum) << ", ns[" << i
                          << "].nsNum=" << static_cast<int>(nsList[i]->customData.nsNum);
        }
    }

    UbseSsuAllocSpaceReq req;
    req.name = name;
    req.tenant = tenant;
    req.nsNum = first.customData.nsNum;
    req.nsSize = first.customData.totalBytes;
    req.lbaFormat = (first.nsOptions.flbas == 1) ? UbseSsuLBAFormat::LBA_FORMAT_4K : UbseSsuLBAFormat::LBA_FORMAT_512;
    // 三态恢复分配策略（STRIPED/LINEAR/NORMAL），与CreateDevNameSpaces写入customData.allocStrategy一致；
    // 非法值防御性回退NORMAL（最保守默认值，不假定存在聚合块设备）
    if (first.customData.allocStrategy > static_cast<uint8_t>(UbseSsuAllocStrategy::NORMAL)) {
        UBSE_LOG_ERROR << "RebuildEntry: invalid allocStrategy=" << static_cast<int>(first.customData.allocStrategy)
                       << ", fallback to NORMAL, name=" << name;
        req.strategy = UbseSsuAllocStrategy::NORMAL;
    } else {
        req.strategy = static_cast<UbseSsuAllocStrategy>(first.customData.allocStrategy);
    }
    std::string userName(first.customData.userName,
                         strnlen(first.customData.userName, sizeof(first.customData.userName)));

    UbseSsuAllocResult allocResult;
    allocResult.name = name;
    allocResult.strategy = req.strategy;
    for (const auto *ns : nsList) {
        UbseSsuNameSpaceInfo info;
        info.tgtEid = ns->subSystem.eid;
        info.tgtNqn = ns->subSystem.subNqn;
        auto uuidStr = StrToUuid(ns->uuid);
        if (uuidStr.empty()) {
            UBSE_LOG_ERROR << "RebuildEntry: failed to convert uuid, skip this ns, name=" << name
                           << ", nsId=" << ns->namespaceId;
            continue; // 只跳过当前异常 namespace
        }
        info.nsUuid = uuidStr;
        info.namespaceId = ns->namespaceId;
        // 与CreateDevNameSpaces生成的持久化路径保持一致：/dev/disk/by-id/nvme-uuid.<标准连字符UUID>
        info.nsDevPath = std::string("/dev/disk/by-id/nvme-uuid.") + uuidStr;
        // nsze单位为LBA数量，乘LBA大小换算为字节，与创建路径的nsSize口径保持一致
        info.nsSize = ns->nsze * static_cast<uint64_t>(req.lbaFormat);
        info.lbaFormat = req.lbaFormat;
        allocResult.nameSpaceList.push_back(info);
    }

    UbseSsuLedgerEntry entry;
    entry.name = name;
    entry.allocReq = req;
    if (nsList[0]->namespaceId == 0) {
        UBSE_LOG_ERROR << "Rebuild entry: invalid namespaceId(0) for name=" << name;
        return;
    }
    // 挂载状态由后续将由本地持久化文件记录，不依赖此Rebuild结果，后续添加实现
    entry.state = UbseSsuNsState::CREATED;
    entry.allocResult = allocResult;
    // 硬件恢复标记：恢复收敛窗口守卫据此区分重启前已存在的空间与窗口内新分配的空间
    entry.restoredFromHardware = true;

    if (!ledger.Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_WARN << "Rebuild entry: ledger entry already exists, skip: name=" << name;
    }
}

void UbseSsuDebtLedger::Rebuild(const std::vector<UbseSsuDevInfo> &devices)
{
    auto &ledger = *this;
    std::unordered_map<std::string, std::vector<const UbseSsuDevNameSpace *>> groups;

    for (const auto &dev : devices) {
        for (const auto &ns : dev.nameSpaces) {
            if (ns.customData.name[0] == '\0') {
                continue;
            }
            std::string nsName(ns.customData.name, strnlen(ns.customData.name, sizeof(ns.customData.name)));
            if (nsName.empty()) {
                continue;
            }
            groups[nsName].push_back(&ns);
        }
    }

    for (const auto &[name, nsList] : groups) {
        if (nsList.empty()) {
            UBSE_LOG_WARN << "Rebuild debt ledger entry: nsList is empty: name=" << name;
            continue;
        }
        // 同一个请求name，来自同一个租户，每个namespace的tenant都应相同
        std::string tenant(nsList[0]->customData.tenant,
                           strnlen(nsList[0]->customData.tenant, sizeof(nsList[0]->customData.tenant)));
        RebuildEntry(name, nsList, ledger, tenant);
    }

    // Rebuild完成即视为账本就绪（空账本也是真实状态：硬件上确实无已分配空间）
    SetReady(true);
    // 记录Rebuild完成时间，启动恢复收敛窗口（跨线程可见，与ready_同经同步原语保护）
    {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        rebuildTime_ = std::chrono::steady_clock::now();
    }

    UBSE_LOG_INFO << "Ledger rebuild complete: " << groups.size() << " entries restored";
}

bool UbseSsuDebtLedger::IsInReconcileWindow() const
{
    std::chrono::steady_clock::time_point rebuildTime;
    {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        rebuildTime = rebuildTime_;
    }
    // 从未Rebuild（首启采集成功前）不构成收敛窗口（该阶段由IsReady门禁兜底）
    if (rebuildTime.time_since_epoch().count() == 0) {
        return false;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - rebuildTime).count();
    return elapsed < SSU_RECONCILE_WINDOW_SECONDS;
}

void UbseSsuDebtLedger::ResetRebuildTime()
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    rebuildTime_ = {};
}

bool UbseSsuDebtLedger::IsReady() const
{
    return ready_.load(std::memory_order_acquire);
}

void UbseSsuDebtLedger::SetReady(bool ready)
{
    ready_.store(ready, std::memory_order_release);
}

} // namespace ubse::ssu::debt
